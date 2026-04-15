#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <thread>
#include <sys/time.h>
#include <atomic>
#include <mutex>
#include <chrono>
#include <unordered_map>
#include <iomanip>
#include <algorithm>

std::atomic<bool> stopThreads(false); // 用于控制线程是否停止的标志

#define MAX_MEM_BUFFER_SIZE 1024 * 1024
#define DISTRICT_PER_WARE 10	   // 每个仓库为10个地区提供服务
#define CUSTOMER_PER_DISTRICT 3000 // 每个地区有3000个用户
#define MAXITEMS 100000			   // 有多少个item
								   // 维护一个类
enum TranType
{
	NEW_ORDER,
	PAYMENT,
	ORDER_STATUS,
	DELIVERY,
	STOCK_LEVEL,
};

struct One_Tran
{
	std::vector<std::string> sqls;	   // 该事务的 sql
	std::vector<std::string> labels; // 与 sqls 对应的模板标签
	TranType type;				   // 类型
	bool status;				   // 该事务状态
	bool is_run;
};

struct StatsRecord
{
	uint64_t count = 0;
	uint64_t total_us = 0;
	uint64_t abort_count = 0;
	uint64_t error_count = 0;
};

int max_thread_num = 0;
int total_tran_num = 0;
int new_order = 0;	  // 10
int payment = 0;	  // 10
int order_status = 0; // 1
int delivery = 0;	  // 1
int stock_level = 0;  // 1
std::vector<One_Tran> Trans;
std::vector<One_Tran> Finish_Trans;
std::vector<bool> run_tran;

int init_unix_sock(const char *unix_sock_path)
{
	int sockfd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		fprintf(stderr, "failed to create unix socket. %s", strerror(errno));
		return -1;
	}

	struct sockaddr_un sockaddr;
	memset(&sockaddr, 0, sizeof(sockaddr));
	sockaddr.sun_family = PF_UNIX;
	snprintf(sockaddr.sun_path, sizeof(sockaddr.sun_path), "%s", unix_sock_path);

	if (connect(sockfd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0)
	{
		fprintf(stderr, "failed to connect to server. unix socket path '%s'. error %s", sockaddr.sun_path,
				strerror(errno));
		close(sockfd);
		return -1;
	}
	return sockfd;
}

int init_tcp_sock(const char *server_host, int server_port)
{
	struct hostent *host;
	struct sockaddr_in serv_addr;

	if ((host = gethostbyname(server_host)) == NULL)
	{
		fprintf(stderr, "gethostbyname failed. errmsg=%d:%s\n", errno, strerror(errno));
		return -1;
	}

	int sockfd;
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		fprintf(stderr, "create socket error. errmsg=%d:%s\n", errno, strerror(errno));
		return -1;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(server_port);
	serv_addr.sin_addr = *((struct in_addr *)host->h_addr);
	bzero(&(serv_addr.sin_zero), 8);

	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) == -1)
	{
		fprintf(stderr, "Failed to connect. errmsg=%d:%s\n", errno, strerror(errno));
		close(sockfd);
		return -1;
	}
	return sockfd;
}

int send_recv_sql(int sockfd, std::string sql)
{
	int send_bytes;
	char recv_buf[MAX_MEM_BUFFER_SIZE];
	memset(recv_buf, 0, MAX_MEM_BUFFER_SIZE);
	if ((send_bytes = write(sockfd, sql.c_str(), sql.length() + 1)) == -1)
	{
		std::cerr << "send error: " << errno << ":" << strerror(errno) << " \n"
				  << std::endl;
		exit(1);
	}

	// std::cout << "send bytes: " << send_bytes << std::endl;

	int len = recv(sockfd, recv_buf, MAX_MEM_BUFFER_SIZE, 0);

	// std::cout << recv_buf << std::endl;

	if (len < 0)
	{
		fprintf(stderr, "Connection was broken: %s\n", strerror(errno));
		return 0;
	}
	else if (len == 0)
	{
		printf("Connection has been closed\n");
		return 0;
	}
	else if (strncmp(recv_buf, "abort", 5) == 0)
	{
		return 1;
	}

	return 8;

	// printf("%s\n", recv_buf);
}

int connect_database(const char *unix_sockect_path, const char *server_host, int server_port)
{
	int sockfd;

	if (unix_sockect_path != nullptr)
	{
		sockfd = init_unix_sock(unix_sockect_path);
	}
	else
	{
		sockfd = init_tcp_sock(server_host, server_port);
	}

	if (sockfd < 0)
	{
		exit(1);
	}

	return sockfd;
}

std::atomic<int> new_order_o_id(3001);
std::atomic<int> new_order_o_d_id(1);
std::atomic<int> new_order_o_w_id(1);
// 加一个锁来保证数据的一致性

std::mutex latch_; // 互斥锁
std::mutex stats_latch_;
std::unordered_map<std::string, StatsRecord> sql_stats;
std::unordered_map<int, StatsRecord> txn_stats;
std::unordered_map<std::string, StatsRecord> op_stats;
std::unordered_map<std::string, StatsRecord> access_stats;

const char *tran_type_name(TranType type)
{
	switch (type)
	{
	case NEW_ORDER:
		return "NEW_ORDER";
	case PAYMENT:
		return "PAYMENT";
	case ORDER_STATUS:
		return "ORDER_STATUS";
	case DELIVERY:
		return "DELIVERY";
	case STOCK_LEVEL:
		return "STOCK_LEVEL";
	default:
		return "UNKNOWN";
	}
}

void add_sql(One_Tran &tr, const std::string &label, const std::string &sql)
{
	tr.labels.push_back(label);
	tr.sqls.push_back(sql);
}

std::string sql_operation_group(const std::string &sql)
{
	if (sql.rfind("SELECT", 0) == 0 || sql.rfind("select", 0) == 0)
	{
		return "SELECT";
	}
	if (sql.rfind("UPDATE", 0) == 0 || sql.rfind("update", 0) == 0)
	{
		return "UPDATE";
	}
	if (sql.rfind("INSERT", 0) == 0 || sql.rfind("insert", 0) == 0)
	{
		return "INSERT";
	}
	if (sql.rfind("DELETE", 0) == 0 || sql.rfind("delete", 0) == 0)
	{
		return "DELETE";
	}
	if (sql == "begin;")
	{
		return "BEGIN";
	}
	if (sql == "commit;")
	{
		return "COMMIT";
	}
	return "OTHER";
}

std::string sql_access_group(const std::string &label)
{
	if (label == "TXN_BEGIN" || label == "TXN_COMMIT")
	{
		return "txn_ctl";
	}
	if (label.find("COUNT") != std::string::npos || label.find("SUM") != std::string::npos)
	{
		return "aggregation";
	}
	if (label.find("RANGE") != std::string::npos || label.find("MIN_NEW_ORDER") != std::string::npos)
	{
		return "range_scan";
	}
	if (label.find("INSERT") != std::string::npos || label.find("UPDATE") != std::string::npos || label.find("DELETE") != std::string::npos)
	{
		return "write";
	}
	if (label.find("SELECT") != std::string::npos)
	{
		return "point_lookup";
	}
	return "other";
}

void record_stats_record(StatsRecord &stats, uint64_t elapsed_us, int flag)
{
	stats.count++;
	stats.total_us += elapsed_us;
	if (flag == 0)
	{
		stats.error_count++;
	}
	else if (flag == 1)
	{
		stats.abort_count++;
	}
}

void record_sql_stat(const std::string &label, uint64_t elapsed_us, int flag)
{
	std::lock_guard<std::mutex> guard(stats_latch_);
	record_stats_record(sql_stats[label], elapsed_us, flag);
}

void record_sql_category_stat(const std::string &op_group, const std::string &access_group, uint64_t elapsed_us, int flag)
{
	std::lock_guard<std::mutex> guard(stats_latch_);
	record_stats_record(op_stats[op_group], elapsed_us, flag);
	record_stats_record(access_stats[access_group], elapsed_us, flag);
}

void record_txn_stat(TranType type, uint64_t elapsed_us, bool saw_abort, bool saw_error)
{
	std::lock_guard<std::mutex> guard(stats_latch_);
	auto &stats = txn_stats[static_cast<int>(type)];
	stats.count++;
	stats.total_us += elapsed_us;
	if (saw_abort)
	{
		stats.abort_count++;
	}
	if (saw_error)
	{
		stats.error_count++;
	}
}

void print_timing_summary()
{
	std::vector<std::pair<std::string, StatsRecord>> sql_rows(sql_stats.begin(), sql_stats.end());
	std::sort(sql_rows.begin(), sql_rows.end(), [](const auto &lhs, const auto &rhs) {
		return lhs.second.total_us > rhs.second.total_us;
	});

	auto print_named_stats = [](const std::string &title, const std::unordered_map<std::string, StatsRecord> &source) {
		std::vector<std::pair<std::string, StatsRecord>> rows(source.begin(), source.end());
		std::sort(rows.begin(), rows.end(), [](const auto &lhs, const auto &rhs) {
			return lhs.second.total_us > rhs.second.total_us;
		});
		std::cout << "\n[---------------------------Timing Summary: " << title << "---------------------------]" << std::endl;
		std::cout << std::left << std::setw(24) << "group"
				  << std::right << std::setw(10) << "count"
				  << std::setw(14) << "total_ms"
				  << std::setw(14) << "avg_ms"
				  << std::setw(12) << "aborts"
				  << std::setw(12) << "errors" << std::endl;
		for (const auto &[name, stats] : rows)
		{
			double total_ms = static_cast<double>(stats.total_us) / 1000.0;
			double avg_ms = stats.count == 0 ? 0.0 : total_ms / static_cast<double>(stats.count);
			std::cout << std::left << std::setw(24) << name
					  << std::right << std::setw(10) << stats.count
					  << std::setw(14) << std::fixed << std::setprecision(3) << total_ms
					  << std::setw(14) << std::fixed << std::setprecision(3) << avg_ms
					  << std::setw(12) << stats.abort_count
					  << std::setw(12) << stats.error_count << std::endl;
		}
	};

	std::cout << "\n[---------------------------Timing Summary: Transactions---------------------------]" << std::endl;
	std::cout << std::left << std::setw(18) << "type"
			  << std::right << std::setw(10) << "count"
			  << std::setw(14) << "total_ms"
			  << std::setw(14) << "avg_ms"
			  << std::setw(12) << "aborts"
			  << std::setw(12) << "errors" << std::endl;
	for (int type = NEW_ORDER; type <= STOCK_LEVEL; type++)
	{
		auto it = txn_stats.find(type);
		if (it == txn_stats.end())
		{
			continue;
		}
		const auto &stats = it->second;
		double total_ms = static_cast<double>(stats.total_us) / 1000.0;
		double avg_ms = stats.count == 0 ? 0.0 : total_ms / static_cast<double>(stats.count);
		std::cout << std::left << std::setw(18) << tran_type_name(static_cast<TranType>(type))
				  << std::right << std::setw(10) << stats.count
				  << std::setw(14) << std::fixed << std::setprecision(3) << total_ms
				  << std::setw(14) << std::fixed << std::setprecision(3) << avg_ms
				  << std::setw(12) << stats.abort_count
				  << std::setw(12) << stats.error_count << std::endl;
	}

	print_named_stats("SQL Operations", op_stats);
	print_named_stats("SQL Access Patterns", access_stats);

	std::cout << "\n[---------------------------Timing Summary: SQL Templates---------------------------]" << std::endl;
	std::cout << std::left << std::setw(40) << "sql_template"
			  << std::right << std::setw(10) << "count"
			  << std::setw(14) << "total_ms"
			  << std::setw(14) << "avg_ms"
			  << std::setw(12) << "aborts"
			  << std::setw(12) << "errors" << std::endl;
	for (const auto &[label, stats] : sql_rows)
	{
		double total_ms = static_cast<double>(stats.total_us) / 1000.0;
		double avg_ms = stats.count == 0 ? 0.0 : total_ms / static_cast<double>(stats.count);
		std::cout << std::left << std::setw(40) << label
				  << std::right << std::setw(10) << stats.count
				  << std::setw(14) << std::fixed << std::setprecision(3) << total_ms
				  << std::setw(14) << std::fixed << std::setprecision(3) << avg_ms
				  << std::setw(12) << stats.abort_count
				  << std::setw(12) << stats.error_count << std::endl;
	}
}

/*
   900*500 =  450000
*/

int generate_random_int(int min, int max)
{
	int rand_int;
	int rand_range = max - min + 1;
	rand_int = rand() % rand_range;
	rand_int += min;
	return rand_int;
}

void set_new_order(One_Tran &tr, int w)
{
	tr.type = TranType::NEW_ORDER;
	tr.status = false;
	std::string sql;
	add_sql(tr, "TXN_BEGIN", "begin;");
	int w_id = rand() % w + 1;
	int d_id = rand() % 10 + 1;
	int c_id = rand() % CUSTOMER_PER_DISTRICT + 1;
	sql = "SELECT c_discount, c_last, c_credit, w_tax FROM customer, warehouse WHERE w_id = " + std::to_string(w_id) + " AND c_w_id = w_id " + " AND c_d_id = " + std::to_string(d_id) + " AND c_id = " + std::to_string(c_id) + ";";
	add_sql(tr, "NEW_ORDER_SELECT_CUSTOMER_WAREHOUSE", sql);
	sql = "SELECT d_next_o_id, d_tax FROM district WHERE d_id = " + std::to_string(d_id) + " AND d_w_id = " + std::to_string(w_id) + ";";
	add_sql(tr, "NEW_ORDER_SELECT_DISTRICT", sql);
	sql = "UPDATE district SET d_next_o_id = d_next_o_id + 1 WHERE d_id = " + std::to_string(d_id) + " AND d_w_id = " + std::to_string(w_id) + ";";
	add_sql(tr, "NEW_ORDER_UPDATE_DISTRICT_NEXT_OID", sql);
	latch_.lock();
	int o_id = new_order_o_id;
	int o_d_id = new_order_o_d_id;
	int o_w_id = new_order_o_w_id;
	if (new_order_o_d_id == 10)
	{
		new_order_o_d_id.store(1);
		if (new_order_o_w_id == 50)
		{
			new_order_o_w_id.store(1);
			new_order_o_id.store(new_order_o_id + 1);
		}
		else
		{
			new_order_o_w_id.store(new_order_o_w_id + 1);
		}
	}
	else
	{
		new_order_o_d_id.store(new_order_o_d_id + 1);
	}

	latch_.unlock();
	int *c_ids = new int[CUSTOMER_PER_DISTRICT + 1];
	for (int i = 1; i <= CUSTOMER_PER_DISTRICT; ++i)
		c_ids[i - 1] = i;
	for (int i = 1; i <= CUSTOMER_PER_DISTRICT; ++i)
	{
		int index = generate_random_int(0, CUSTOMER_PER_DISTRICT - 1);
		std::swap(c_ids[i - 1], c_ids[index]);
	}
	int o_c_id = c_ids[o_id];
	delete[] c_ids;
	int o_carrier_id = 0;
	if (o_id < 2001)
	{
		o_carrier_id = generate_random_int(1, 10);
	}
	int o_ol_cnt = 10;
	int o_all_local = 1;
	sql = "INSERT INTO orders  VALUES(" + std::to_string(o_id) + "," + std::to_string(o_d_id) + "," + std::to_string(o_w_id) + "," + std::to_string(o_c_id) + ",'2023-07-22 20:50:31'," + std::to_string(o_ol_cnt) + "," + std::to_string(o_all_local) + "," + std::to_string(o_carrier_id) + ");";
	add_sql(tr, "NEW_ORDER_INSERT_ORDERS", sql);
	sql = "INSERT INTO new_orders VALUES(" + std::to_string(o_id) + "," + std::to_string(o_d_id) + "," + std::to_string(o_w_id) + ");";
	add_sql(tr, "NEW_ORDER_INSERT_NEW_ORDERS", sql);
	int ol_i_id = 0;
	ol_i_id = rand() % MAXITEMS + 1;
	int ol_supply_w_id = 0;
	ol_supply_w_id = w_id;
	sql = "SELECT i_price, i_name, i_data FROM item WHERE i_id = " + std::to_string(ol_i_id) + ";";
	add_sql(tr, "NEW_ORDER_SELECT_ITEM", sql);
	sql = "SELECT s_quantity, s_data, s_dist_01, s_dist_02, s_dist_03, s_dist_04, s_dist_05, s_dist_06, s_dist_07, s_dist_08, s_dist_09, s_dist_10 FROM stock WHERE s_i_id = " + std::to_string(ol_i_id) + " AND s_w_id = " + std::to_string(ol_supply_w_id) + ";";
	add_sql(tr, "NEW_ORDER_SELECT_STOCK", sql);
	sql = "UPDATE stock SET s_quantity = s_quantity - 1 WHERE s_i_id = " +
		  std::to_string(ol_i_id) + " AND s_w_id = " + std::to_string(ol_supply_w_id) + ";";
	add_sql(tr, "NEW_ORDER_UPDATE_STOCK", sql);

	sql = "INSERT INTO order_line VALUES(" + std::to_string(o_id) + "," +
		  std::to_string(o_d_id) + "," + std::to_string(o_w_id) + "," + std::to_string(rand() % 10 + 1) +
		  "," + std::to_string(ol_i_id) + "," + std::to_string(o_w_id) + ",'2023-07-22 20:50:31',5,391.250000,'pvKVc7RMoH7qbl8ew7l8UEYv'); ";
	add_sql(tr, "NEW_ORDER_INSERT_ORDER_LINE", sql);

	add_sql(tr, "TXN_COMMIT", "commit;");
}

void set_payment(One_Tran &tr, int w)
{
	tr.type = TranType::PAYMENT;
	tr.status = false;
	std::string sql = "";
	int w_id = rand() % w + 1;
	add_sql(tr, "TXN_BEGIN", "begin;");
	sql = "UPDATE warehouse SET w_ytd = w_ytd + 1000 WHERE w_id = " + std::to_string(w_id) + ";";
	add_sql(tr, "PAYMENT_UPDATE_WAREHOUSE", sql);
	sql = "SELECT w_street_1, w_street_2, w_city, w_state, w_zip, w_name FROM warehouse WHERE w_id = " + std::to_string(w_id) + ";";
	add_sql(tr, "PAYMENT_SELECT_WAREHOUSE", sql);
	int d_id = rand() % 10 + 1;
	sql = "UPDATE district SET d_ytd = 3819.250000 WHERE d_w_id = " + std::to_string(w_id) + " AND d_id = " + std::to_string(d_id) + ";";
	add_sql(tr, "PAYMENT_UPDATE_DISTRICT", sql);
	sql = "SELECT d_street_1, d_street_2, d_city, d_state, d_zip, d_name FROM district WHERE d_w_id = " + std::to_string(w_id) + " AND d_id = " + std::to_string(d_id) + ";";
	add_sql(tr, "PAYMENT_SELECT_DISTRICT", sql);
	int c_id = rand() % CUSTOMER_PER_DISTRICT + 1;
	sql = "select c_first, c_middle, c_last, c_street_1, c_street_2, c_city, c_state, c_zip, c_phone, c_credit_lim, c_discount, c_balance, c_since from customer where c_w_id=" + std::to_string(w_id) + " and c_d_id=" + std::to_string(d_id) + " and c_id=" + std::to_string(c_id) + ";";
	add_sql(tr, "PAYMENT_SELECT_CUSTOMER", sql);
	sql = "update customer set c_balance=7.250000 where c_w_id=" + std::to_string(w_id) + " and c_d_id=" + std::to_string(d_id) + " and c_id=" + std::to_string(c_id) + ";";
	add_sql(tr, "PAYMENT_UPDATE_CUSTOMER", sql);
	sql = "insert into history values(" + std::to_string(c_id) + ", " + std::to_string(d_id) + ", " + std::to_string(w_id) + ", " + std::to_string(d_id) + ", " + std::to_string(w_id) + ", '2023-07-22 20:50:31', 10.5, 'zyQ3FV9Lm9zPbkgkouVFdS1k');";
	add_sql(tr, "PAYMENT_INSERT_HISTORY", sql);
	add_sql(tr, "TXN_COMMIT", "commit;");
}

void set_order_status(One_Tran &tr, int w)
{
	tr.type = TranType::ORDER_STATUS;
	tr.status = false;
	std::string sql = "";
	int w_id = rand() % w + 1;
	add_sql(tr, "TXN_BEGIN", "begin;");
	int c_id = rand() % CUSTOMER_PER_DISTRICT + 1;
	sql = "SELECT c_balance, c_first, c_middle, c_last FROM customer WHERE c_w_id = " + std::to_string(w_id) + " AND c_d_id = " + std::to_string(rand() % 10 + 1) + " AND c_id = " + std::to_string(c_id) + ";";
	add_sql(tr, "ORDER_STATUS_SELECT_CUSTOMER_BY_ID", sql);
	int o_id = rand() % CUSTOMER_PER_DISTRICT + 1;
	sql = "select o_id, o_entry_d, o_carrier_id from orders where o_w_id=" + std::to_string(w_id) + " and o_d_id=" + std::to_string(rand() % 10 + 1) + " and o_c_id=" + std::to_string(c_id) + " and o_id=" + std::to_string(o_id) + ";";
	add_sql(tr, "ORDER_STATUS_SELECT_ORDERS", sql);
	sql = "select ol_i_id, ol_supply_w_id, ol_quantity, ol_amount, ol_delivery_d from order_line where ol_w_id=" + std::to_string(w_id) + " and ol_d_id=" + std::to_string(rand() % 10 + 1) + " and ol_o_id=" + std::to_string(o_id) + ";";
	add_sql(tr, "ORDER_STATUS_SELECT_ORDER_LINE", sql);
	add_sql(tr, "TXN_COMMIT", "commit;");
}

void set_order_status_2(One_Tran &tr, int w)
{
	tr.type = TranType::ORDER_STATUS;
	tr.status = false;
	std::string sql = "";
	int w_id = rand() % w + 1;
	add_sql(tr, "TXN_BEGIN", "begin;");
	int c_id = rand() % CUSTOMER_PER_DISTRICT + 1;
	sql = "SELECT COUNT(c_id) as count_c_id FROM customer WHERE c_w_id = " + std::to_string(w_id) + " AND c_d_id = " + std::to_string(rand() % 10 + 1) + " AND c_last = 'BARRBARRBARR';";
	add_sql(tr, "ORDER_STATUS_COUNT_CUSTOMER_BY_LAST", sql);
	sql = "SELECT c_balance, c_first, c_middle, c_last FROM customer WHERE c_w_id = " + std::to_string(w_id) + " AND c_d_id = " + std::to_string(rand() % 10 + 1) + " AND c_last = 'BARRBARRBARR' order by c_first;";
	add_sql(tr, "ORDER_STATUS_SELECT_CUSTOMER_BY_LAST", sql);
	add_sql(tr, "TXN_COMMIT", "commit;");
}

void set_stock_level(One_Tran &tr, int w)
{
	tr.type = TranType::STOCK_LEVEL;
	tr.status = false;
	std::string sql = "";
	int w_id = rand() % w + 1;
	int d_id = rand() % 10 + 1;
	add_sql(tr, "TXN_BEGIN", "begin;");
	sql = "SELECT d_next_o_id FROM district WHERE d_id = " + std::to_string(d_id) + " AND d_w_id = " + std::to_string(w_id) + ";";
	add_sql(tr, "STOCK_LEVEL_SELECT_DISTRICT_NEXT_OID", sql);
	int d_next_o_id = 3001;
	sql = "select ol_i_id from order_line where ol_w_id=" + std::to_string(w_id) + " and ol_d_id=" + std::to_string(d_id) +
		  " and ol_o_id<" + std::to_string(d_next_o_id) + " and ol_o_id>=" + std::to_string(d_next_o_id - 20) + ";";
	add_sql(tr, "STOCK_LEVEL_SELECT_ORDER_LINE_RANGE", sql);
	int level = rand() % 10 + 10;
	sql = "select count(*) as count_stock from stock where s_w_id=" + std::to_string(w_id) + " and s_i_id=1 and s_quantity<" + std::to_string(level) + ";";
	add_sql(tr, "STOCK_LEVEL_COUNT_LOW_STOCK", sql);
	add_sql(tr, "TXN_COMMIT", "commit;");
}

void set_delivery(One_Tran &tr, int w)
{
	tr.type = TranType::DELIVERY;
	tr.status = false;
	std::string sql = "";
	int w_id = rand() % w + 1;
	int d_id = rand() % 10 + 1;
	int no_o_id = 0;
	add_sql(tr, "TXN_BEGIN", "begin;");
	sql = "SELECT MIN(no_o_id) from new_orders WHERE no_d_id = " + std::to_string(d_id) + " AND no_w_id = " + std::to_string(w_id) + ";";
	add_sql(tr, "DELIVERY_SELECT_MIN_NEW_ORDER", sql);
	no_o_id = rand() % DISTRICT_PER_WARE + 1;
	sql = "DELETE FROM new_orders WHERE no_o_id = " + std::to_string(no_o_id) + " AND no_d_id = " + std::to_string(d_id) + " AND no_w_id = " + std::to_string(w_id) + ";";
	add_sql(tr, "DELIVERY_DELETE_NEW_ORDER", sql);
	sql = "SELECT o_c_id FROM orders WHERE o_id = " + std::to_string(no_o_id) + " AND o_d_id = " + std::to_string(d_id) + " AND o_w_id = " + std::to_string(w_id) + ";";
	add_sql(tr, "DELIVERY_SELECT_ORDER_CUSTOMER", sql);
	sql = "UPDATE orders SET o_carrier_id = " + std::to_string(rand() % 10 + 1) + " WHERE o_id = " + std::to_string(no_o_id) + " AND o_d_id = " + std::to_string(d_id) + " AND o_w_id = " + std::to_string(w_id) + ";";
	add_sql(tr, "DELIVERY_UPDATE_ORDERS", sql);
	sql = "UPDATE order_line SET ol_delivery_d = '2021-01-01 00:00:00' WHERE ol_o_id = " + std::to_string(no_o_id) + " AND ol_d_id = " + std::to_string(d_id) + " AND ol_w_id = " + std::to_string(w_id) + ";";
	add_sql(tr, "DELIVERY_UPDATE_ORDER_LINE", sql);
	sql = "SELECT SUM(ol_amount) FROM order_line WHERE ol_w_id = " + std::to_string(w_id) + " AND ol_o_id = " + std::to_string(no_o_id) + " AND ol_d_id = " + std::to_string(d_id)+";";
	add_sql(tr, "DELIVERY_SELECT_ORDER_LINE_SUM", sql);
	sql = "UPDATE customer SET c_balance = c_balance + " + std::to_string(rand() % 10000 + 1) + ", c_delivery_cnt = c_delivery_cnt + 1 WHERE c_id = " + std::to_string(rand() % CUSTOMER_PER_DISTRICT + 1) + " AND c_d_id = " + std::to_string(d_id) + " AND c_w_id = " + std::to_string(w_id) + ";";
	add_sql(tr, "DELIVERY_UPDATE_CUSTOMER", sql);
	add_sql(tr, "TXN_COMMIT", "commit;");
}

void set_tran()
{
	for (int i = 0; i < new_order; i++)
	{
		One_Tran temp;
		temp.is_run = false;
		set_new_order(temp, 50);
		Trans.push_back(temp);
		run_tran.push_back(false);
	}
	for (int i = 0; i < payment; i++)
	{
		One_Tran temp;
		temp.is_run = false;
		set_payment(temp, 50);
		Trans.push_back(temp);
		run_tran.push_back(false);
	}
	for (int i = 0; i < order_status; i++)
	{
		One_Tran temp;
		temp.is_run = false;
		set_order_status(temp, 50);
		Trans.push_back(temp);
		run_tran.push_back(false);
	}
	for (int i = 0; i < delivery; i++)
	{
		One_Tran temp;
		temp.is_run = false;
		set_delivery(temp, 50);
		Trans.push_back(temp);
		run_tran.push_back(false);
	}
	for (int i = 0; i < stock_level; i++)
	{
		One_Tran temp;
		temp.is_run = false;
		set_stock_level(temp, 50);
		Trans.push_back(temp);
		run_tran.push_back(false);
	}
}

// 随机获取一个没有运行和没有commit的事务 如果没有 则返回-1
// 我们还是不随机了  从头开始分配
int indexs = 0;
int get_a_tran()
{
	// if(Finish_Trans.size() == Trans.size())
	//     {
	//         return -1;
	//     }
	// 	if(!std::count(run_tran.begin(),run_tran.end(),false))
	// 	{
	//        return -1;
	// 	}
	if (indexs == Trans.size())
	{
		return -1;
	}
	return indexs++;
}

std::atomic<int> total_finish_tran(0);
std::atomic<int> finish_new_order_num(0);

void *threadFunction(void *arg)
{
	// 我们随机挑选一个事务来发送即可
	// 一直发送 直到超时或者已经完成所有事务
	int fd = *(int *)arg;
	while (!stopThreads)
	{
		int index = rand() % 23;
		//  if(index == -1)
		//  {
		// 	break;
		//  }
		One_Tran tr;
		if (index < 10)
		{
			set_new_order(tr, 50);
		}
		else if (index < 20)
		{
			set_payment(tr, 50);
		}
		else if (index == 20)
		{
			set_delivery(tr, 50);
		}
		else if (index == 21)
		{
			int ss = rand() % 10;
			if (ss < 6)
			{
				set_order_status_2(tr, 50);
			}
			else
			{
				set_order_status(tr, 50);
			}
		}
		else if (index == 22)
		{
			set_stock_level(tr, 50);
		}

		bool if_break = false;
		bool saw_abort = false;
		bool saw_error = false;
		auto txn_begin = std::chrono::steady_clock::now();
		while (true)
		{
			if_break = true;
				for (size_t i = 0; i < tr.sqls.size(); i++)
				{
					const auto &sql = tr.sqls[i];
					const auto &label = tr.labels[i];
					const auto op_group = sql_operation_group(sql);
					const auto access_group = sql_access_group(label);
					auto sql_begin = std::chrono::steady_clock::now();
					int flag = send_recv_sql(fd, sql);
					auto sql_end = std::chrono::steady_clock::now();
					uint64_t elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(sql_end - sql_begin).count();
					record_sql_stat(label, elapsed_us, flag);
					record_sql_category_stat(op_group, access_group, elapsed_us, flag);
					if (flag == 0)
				{
					// 事务失败，退出}
					std::cout << "sockfd " << fd << " error" << std::endl;
					saw_error = true;
					break;
				}
				else if (flag == 1)
				{
					// 遇见 abort 重新发送
					std::cout << "sockfd " << fd << " abort" << std::endl;
					saw_abort = true;
					if_break = false;
					break;
				}
			}
			if (if_break)
			{
				break;
			}
		}
		auto txn_end = std::chrono::steady_clock::now();
		uint64_t txn_elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(txn_end - txn_begin).count();
		record_txn_stat(tr.type, txn_elapsed_us, saw_abort, saw_error);
		// Finish_Trans.push_back(tr);
		//total_finish_tran++;
		total_finish_tran.store(total_finish_tran+1);
		if (index < 10)
		{
			//finish_new_order_num++;
			finish_new_order_num.store(finish_new_order_num+1);
		}

		//std::cout<<"Finish total_finish_tran "<<total_finish_tran<<std::endl;

		if (total_finish_tran >= total_tran_num)
		{
			std::cout<<"You have Finish All tran we will stop test;"<<std::endl;
			stopThreads = true;
			break;
		}

	}

	pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		// std::cout << "Usage: " << argv[0] << " max_thread_num and trx_total_num need" << std::endl;
		// exit(1);
		std::cout << "You may not set the max_thread_num and trx_total_num We Will help you to set" << std::endl;
		max_thread_num = 16;
		total_tran_num = 10000000;
	}
	else
	{
		max_thread_num = std::stoi(argv[1]);
		total_tran_num = std::stoi(argv[2]);
	}

	std::cout << "You will use " << max_thread_num << " To Send " << total_tran_num << " Tran" << std::endl;

	pthread_t threads[max_thread_num];

	int avg = total_tran_num / 23;
	new_order = avg * 10;
	payment = avg * 10;
	order_status = avg;
	delivery = avg;
	stock_level = avg;

	std::cout << "[------------------------------Begin set tran------------------------------]" << std::endl;

	// set_tran();
	timeval start_time, current_time, end_time;
	gettimeofday(&start_time, NULL);	  // 获取程序开始时间
	int running_threads = max_thread_num; // 记录正在运行的线程数量

	// 获取程序开始运行的时间
	time_t start = time(nullptr);

	std::cout << "[---------------------------Begin Tpcc Test---------------------------]" << std::endl;
	std::cout << "[--------In Test you will finish new_order " << new_order
			  << " payment " << payment << " order_status " << order_status
			  << " delivery " << delivery << " stock_level " << stock_level
			  << " --------]" << std::endl;

	std::vector<int> sockfds(max_thread_num, -1);
	for (int i = 0; i < max_thread_num; i++)
	{
		sockfds[i] = connect_database(nullptr, "127.0.0.1", 8765);
		int rc = pthread_create(&threads[i], NULL, threadFunction, (void *)&sockfds[i]);
		if (rc)
		{
			std::cout << "Error: unable to create thread, " << rc << std::endl;
			exit(1);
		}
	}

	for (int i = 0; i < max_thread_num; i++)
	{
		pthread_join(threads[i], NULL);
		if (sockfds[i] >= 0)
		{
			close(sockfds[i]);
		}
	}

	gettimeofday(&end_time, NULL); // 获取程序结束时间

	long total_time = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_usec - start_time.tv_usec) / 1000000.0;
	std::cout << "The entire program run for: " << total_time << " seconds." << std::endl;

	std::cout << "You have finish " << total_finish_tran << " In " << total_time << " seconds " << " You have set the new_order_num is " << finish_new_order_num << std::endl;
	print_timing_summary();

	return 0;
}
