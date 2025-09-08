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
	std::vector<std::string> sqls; // 该事务的 sql
	TranType type;				   // 类型
	bool status;				   // 该事务状态
	bool is_run;
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
timeval start_time, current_time, end_time;

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
	//std::cout <<sql<<std::endl;
	//std::cout << "send bytes: " << send_bytes << std::endl;

	int len = recv(sockfd, recv_buf, MAX_MEM_BUFFER_SIZE, 0);

	//std::cout << recv_buf << std::endl;

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
	tr.sqls.push_back("begin;");
	int w_id = rand() % w + 1;
	int d_id = rand() % 10 + 1;
	int c_id = rand() % CUSTOMER_PER_DISTRICT + 1;
	sql = "SELECT c_discount, c_last, c_credit, w_tax FROM customer, warehouse WHERE w_id = " + std::to_string(w_id) + " AND c_w_id = w_id " + " AND c_d_id = " + std::to_string(d_id) + " AND c_id = " + std::to_string(c_id) + ";";
	tr.sqls.push_back(sql);
	sql = "SELECT d_next_o_id, d_tax FROM district WHERE d_id = " + std::to_string(d_id) + " AND d_w_id = " + std::to_string(w_id) + ";";
	tr.sqls.push_back(sql);
	sql = "UPDATE district SET d_next_o_id = d_next_o_id + 1 WHERE d_id = " + std::to_string(d_id) + " AND d_w_id = " + std::to_string(w_id) + ";";
	tr.sqls.push_back(sql);
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
	tr.sqls.push_back(sql);
	sql = "INSERT INTO new_orders VALUES(" + std::to_string(o_id) + "," + std::to_string(o_d_id) + "," + std::to_string(o_w_id) + ");";
	tr.sqls.push_back(sql);
	int ol_i_id = 0;
	ol_i_id = rand() % MAXITEMS + 1;
	int ol_supply_w_id = 0;
	ol_supply_w_id = w_id;
	sql = "SELECT i_price, i_name, i_data FROM item WHERE i_id = " + std::to_string(ol_i_id) + ";";
	tr.sqls.push_back(sql);
	sql = "SELECT s_quantity, s_data, s_dist_01, s_dist_02, s_dist_03, s_dist_04, s_dist_05, s_dist_06, s_dist_07, s_dist_08, s_dist_09, s_dist_10 FROM stock WHERE s_i_id = " + std::to_string(ol_i_id) + " AND s_w_id = " + std::to_string(ol_supply_w_id) + ";";
	tr.sqls.push_back(sql);
	sql = "UPDATE stock SET s_quantity = s_quantity - 1 WHERE s_i_id = " +
		  std::to_string(ol_i_id) + " AND s_w_id = " + std::to_string(ol_supply_w_id) + ";";
	tr.sqls.push_back(sql);

	sql = "INSERT INTO order_line VALUES(" + std::to_string(o_id) + "," +
		  std::to_string(o_d_id) + "," + std::to_string(o_w_id) + "," + std::to_string(rand() % 10 + 1) +
		  "," + std::to_string(ol_i_id) + "," + std::to_string(o_w_id) + ",'2023-07-22 20:50:31',5,391.250000,'pvKVc7RMoH7qbl8ew7l8UEYv'); ";
	tr.sqls.push_back(sql);

	tr.sqls.push_back("commit;");
}

void set_payment(One_Tran &tr, int w)
{
	tr.type = TranType::PAYMENT;
	tr.status = false;
	std::string sql = "";
	int w_id = rand() % w + 1;
	tr.sqls.push_back("begin;");
	sql = "UPDATE warehouse SET w_ytd = w_ytd + 1000 WHERE w_id = " + std::to_string(w_id) + ";";
	tr.sqls.push_back(sql);
	sql = "SELECT w_street_1, w_street_2, w_city, w_state, w_zip, w_name FROM warehouse WHERE w_id = " + std::to_string(w_id) + ";";
	tr.sqls.push_back(sql);
	int d_id = rand() % 10 + 1;
	sql = "UPDATE district SET d_ytd = 3819.250000 WHERE d_w_id = " + std::to_string(w_id) + " AND d_id = " + std::to_string(d_id) + ";";
	tr.sqls.push_back(sql);
	sql = "SELECT d_street_1, d_street_2, d_city, d_state, d_zip, d_name FROM district WHERE d_w_id = " + std::to_string(w_id) + " AND d_id = " + std::to_string(d_id) + ";";
	tr.sqls.push_back(sql);
	int c_id = rand() % CUSTOMER_PER_DISTRICT + 1;
	sql = "select c_first, c_middle, c_last, c_street_1, c_street_2, c_city, c_state, c_zip, c_phone, c_credit_lim, c_discount, c_balance, c_since from customer where c_w_id=" + std::to_string(w_id) + " and c_d_id=" + std::to_string(d_id) + " and c_id=" + std::to_string(c_id) + ";";
	tr.sqls.push_back(sql);
	sql = "update customer set c_balance=7.250000 where c_w_id=" + std::to_string(w_id) + " and c_d_id=" + std::to_string(d_id) + " and c_id=" + std::to_string(c_id) + ";";
	tr.sqls.push_back(sql);
	sql = "insert into history values(" + std::to_string(c_id) + ", " + std::to_string(d_id) + ", " + std::to_string(w_id) + ", " + std::to_string(d_id) + ", " + std::to_string(w_id) + ", '2023-07-22 20:50:31', 10.5, 'zyQ3FV9Lm9zPbkgkouVFdS1k');";
	tr.sqls.push_back(sql);
	tr.sqls.push_back("commit;");
}

void set_order_status(One_Tran &tr, int w)
{
	tr.type = TranType::ORDER_STATUS;
	tr.status = false;
	std::string sql = "";
	int w_id = rand() % w + 1;
	tr.sqls.push_back("begin;");
	int c_id = rand() % CUSTOMER_PER_DISTRICT + 1;
	sql = "SELECT c_balance, c_first, c_middle, c_last FROM customer WHERE c_w_id = " + std::to_string(w_id) + " AND c_d_id = " + std::to_string(rand() % 10 + 1) + " AND c_id = " + std::to_string(c_id) + ";";
	tr.sqls.push_back(sql);
	int o_id = rand() % CUSTOMER_PER_DISTRICT + 1;
	sql = "select o_id, o_entry_d, o_carrier_id from orders where o_w_id=" + std::to_string(w_id) + " and o_d_id=" + std::to_string(rand() % 10 + 1) + " and o_c_id=" + std::to_string(c_id) + " and o_id=" + std::to_string(o_id) + ";";
	tr.sqls.push_back(sql);
	sql = "select ol_i_id, ol_supply_w_id, ol_quantity, ol_amount, ol_delivery_d from order_line where ol_w_id=" + std::to_string(w_id) + " and ol_d_id=" + std::to_string(rand() % 10 + 1) + " and ol_o_id=" + std::to_string(o_id) + ";";
	tr.sqls.push_back(sql);
	tr.sqls.push_back("commit;");
}

void set_order_status_2(One_Tran &tr, int w)
{
	tr.type = TranType::ORDER_STATUS;
	tr.status = false;
	std::string sql = "";
	int w_id = rand() % w + 1;
	tr.sqls.push_back("begin;");
	int c_id = rand() % CUSTOMER_PER_DISTRICT + 1;
	sql = "SELECT COUNT(c_id) as count_c_id FROM customer WHERE c_w_id = " + std::to_string(w_id) + " AND c_d_id = " + std::to_string(rand() % 10 + 1) + " AND c_last = 'BARRBARRBARR';";
	tr.sqls.push_back(sql);
	sql = "SELECT c_balance, c_first, c_middle, c_last FROM customer WHERE c_w_id = " + std::to_string(w_id) + " AND c_d_id = " + std::to_string(rand() % 10 + 1) + " AND c_last = 'BARRBARRBARR' order by c_first;";
	tr.sqls.push_back(sql);
	tr.sqls.push_back("commit;");
}

void set_stock_level(One_Tran &tr, int w)
{
	tr.type = TranType::STOCK_LEVEL;
	tr.status = false;
	std::string sql = "";
	int w_id = rand() % w + 1;
	int d_id = rand() % 10 + 1;
	tr.sqls.push_back("begin;");
	sql = "SELECT d_next_o_id FROM district WHERE d_id = " + std::to_string(d_id) + " AND d_w_id = " + std::to_string(w_id) + ";";
	tr.sqls.push_back(sql);
	int d_next_o_id = 3001;
	sql = "select ol_i_id from order_line where ol_w_id=" + std::to_string(w_id) + " and ol_d_id=" + std::to_string(d_id) + " and ol_o_id<3001 and ol_o_id>=1981;";
	tr.sqls.push_back(sql);
	int level = rand() % 10 + 10;
	sql = "select count(*) as count_stock from stock where s_w_id=" + std::to_string(w_id) + " and s_i_id=1 and s_quantity<" + std::to_string(level) + ";";
	tr.sqls.push_back(sql);
	tr.sqls.push_back("commit;");
}

void set_delivery(One_Tran &tr, int w)
{
	tr.type = TranType::STOCK_LEVEL;
	tr.status = false;
	std::string sql = "";
	int w_id = rand() % w + 1;
	int d_id = rand() % 10 + 1;
	int no_o_id = 0;
	tr.sqls.push_back("begin;");
	sql = "SELECT MIN(no_o_id) from new_orders WHERE no_d_id = " + std::to_string(d_id) + " AND no_w_id = " + std::to_string(w_id) + ";";
	tr.sqls.push_back(sql);
	no_o_id = rand() % DISTRICT_PER_WARE + 1;
	sql = "DELETE FROM new_orders WHERE no_o_id = " + std::to_string(no_o_id) + " AND no_d_id = " + std::to_string(d_id) + " AND no_w_id = " + std::to_string(w_id) + ";";
	tr.sqls.push_back(sql);
	sql = "SELECT o_c_id FROM orders WHERE o_id = " + std::to_string(no_o_id) + " AND o_d_id = " + std::to_string(d_id) + " AND o_w_id = " + std::to_string(w_id) + ";";
	tr.sqls.push_back(sql);
	sql = "UPDATE orders SET o_carrier_id = " + std::to_string(rand() % 10 + 1) + " WHERE o_id = " + std::to_string(no_o_id) + " AND o_d_id = " + std::to_string(d_id) + " AND o_w_id = " + std::to_string(w_id) + ";";
	tr.sqls.push_back(sql);
	sql = "UPDATE order_line SET ol_delivery_d = '2021-01-01 00:00:00' WHERE ol_o_id = " + std::to_string(no_o_id) + " AND ol_d_id = " + std::to_string(d_id) +" AND ol_w_id = "+std::to_string(w_id)+";";
	tr.sqls.push_back(sql);
	sql = "SELECT SUM(ol_amount) FROM order_line WHERE ol_o_id = " + std::to_string(no_o_id) + " AND ol_d_id = " + std::to_string(d_id)+" and ol_w_id="+std::to_string(w_id)+";";
	tr.sqls.push_back(sql);
	sql = "UPDATE customer SET c_balance = c_balance + " + std::to_string(rand() % 10000 + 1) + ", c_delivery_cnt = c_delivery_cnt + 1 WHERE c_id = " + std::to_string(rand() % CUSTOMER_PER_DISTRICT + 1) + " AND c_d_id = " + std::to_string(d_id) + " AND c_w_id = " + std::to_string(w_id) + ";";
	tr.sqls.push_back(sql);
	tr.sqls.push_back("commit;");
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
	while (true)
	{
	        if(stopThreads.load())break;
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
		while (true)
		{
			if_break = true;
			for (auto &sql : tr.sqls)
			{
				//std::cout<<fd<<" "<<sql<<std::endl;
				int flag = send_recv_sql(fd, sql);
				if (flag == 0)
				{
					// 事务失败，退出}
					std::cout << "sockfd " << fd << " error" << std::endl;
					if_break = false;
					break;
				}
				else if (flag == 1)
				{
					// 遇见 abort 重新发送
					std::cout << "sockfd " << fd << " abort" << std::endl;
					if_break = false;
					break;
				}
			}
			if (if_break)
			{
				break;
			}
		}
		// Finish_Trans.push_back(tr);
		//total_finish_tran++;
		total_finish_tran.store(total_finish_tran+1);
		if (index < 10)
		{
			//finish_new_order_num++;
			finish_new_order_num.store(finish_new_order_num+1);
		}

		//std::cout<<"Finish total_finish_tran "<<total_finish_tran<<std::endl;

		if (total_finish_tran.load() >= total_tran_num)
		{
			std::cout<<"You have Finish All tran we will stop test;"<<std::endl;
			gettimeofday(&end_time, NULL); // 获取程序结束时间
			long total_time = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_usec - start_time.tv_usec) / 1000000.0;
			std::cout << "The entire program run for: " << total_time << " seconds." << std::endl;
			std::cout << "You have finish " << total_finish_tran << " In " << total_time << " seconds " << " You have set the new_order_num is " << finish_new_order_num << std::endl;
			exit(1);
			stopThreads.store(true);
			break;
		}

	}
	close(fd);

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

	gettimeofday(&start_time, NULL);	  // 获取程序开始时间
	int running_threads = max_thread_num; // 记录正在运行的线程数量

	// 获取程序开始运行的时间
	time_t start = time(nullptr);

	std::cout << "[---------------------------Begin Tpcc Test---------------------------]" << std::endl;
	std::cout << "[--------In Test you will finish new_order " << new_order
			  << " payment " << payment << " order_status " << order_status
			  << " delivery " << delivery << " stock_level " << stock_level
			  << " --------]" << std::endl;

	for (int i = 0; i < max_thread_num; i++)
	{
		int sockfd = connect_database(nullptr, "127.0.0.1", 8765);
		int rc = pthread_create(&threads[i], NULL, threadFunction, (void *)&sockfd);
		if (rc)
		{
			std::cout << "Error: unable to create thread, " << rc << std::endl;
			exit(1);
		}
	}

	for (int i = 0; i < max_thread_num; i++)
	{
		pthread_join(threads[i], NULL);
	}

	

	return 0;
}
