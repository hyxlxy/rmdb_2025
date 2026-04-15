/**
 * 生成 5个模块的sql语句
 */
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <string>

#define DISTRICT_PER_WARE 10       // 每个仓库为10个地区提供服务
#define CUSTOMER_PER_DISTRICT 3000 // 每个地区有3000个用户
#define MAXITEMS 100000            // 有多少个item

#define TOTAL_TRAN 900
int avg_num = TOTAL_TRAN / 23;
int payment_num = avg_num * 8;
int new_order_num = avg_num * 8;
int order_status_num = avg_num * 1;
int stock_level_num = avg_num * 1;
int delivery_num = avg_num * 3; // 这里的delivery 指的是order_line的更新

void generate_delivery(int w)
{
    std::fstream file("delivery.sql", std::ios::out);

    int d_id = 1;
    std::string sql = "";
    int no_o_id = 0;
    for (int i = 0; i < delivery_num; i++)
    {
        int w_id = rand() % w + 1;
        for (; d_id < 10; d_id++)
        {
            /*EXEC_SQL SELECT COALESCE(MIN(no_o_id),0) INTO :no_o_id
                        FROM new_orders
                        WHERE no_d_id = :d_id AND no_w_id = :w_id;*/
            sql = "SELECT MIN(no_o_id) from new_orders WHERE no_d_id = " + std::to_string(d_id) + " AND no_w_id = " + std::to_string(w_id);
            file << sql << std::endl;

            /*EXEC_SQL DELETE FROM new_orders WHERE no_o_id = :no_o_id AND no_d_id = :d_id
              AND no_w_id = :w_id;*/
            no_o_id = rand() % DISTRICT_PER_WARE + 1;
            sql = "DELETE FROM new_orders WHERE no_o_id = " + std::to_string(no_o_id) + " AND no_d_id = " + std::to_string(d_id) + " AND no_w_id = " + std::to_string(w_id);
            file << sql << std::endl;

            /*EXEC_SQL SELECT o_c_id INTO :c_id FROM orders
                            WHERE o_id = :no_o_id AND o_d_id = :d_id
                    AND o_w_id = :w_id;*/
            sql = "SELECT o_c_id FROM orders WHERE o_id = " + std::to_string(no_o_id) + " AND o_d_id = " + std::to_string(d_id) + " AND o_w_id = " + std::to_string(w_id);
            file << sql << std::endl;

            /*EXEC_SQL UPDATE orders SET o_carrier_id = :o_carrier_id
                            WHERE o_id = :no_o_id AND o_d_id = :d_id AND
                    o_w_id = :w_id;*/
            sql = "UPDATE orders SET o_carrier_id = " + std::to_string(rand() % 10 + 1) + " WHERE o_id = " + std::to_string(no_o_id) + " AND o_d_id = " + std::to_string(d_id) + " AND o_w_id = " + std::to_string(w_id);
            file << sql << std::endl;

            /*EXEC_SQL UPDATE order_line
                            SET ol_delivery_d = :datetime
                            WHERE ol_o_id = :no_o_id AND ol_d_id = :d_id AND
                    ol_w_id = :w_id;*/
            sql = "UPDATE order_line SET ol_delivery_d = '2021-01-01 00:00:00' WHERE ol_o_id = " + std::to_string(no_o_id) + " AND ol_d_id = " + std::to_string(d_id) + " AND ol_w_id = " + std::to_string(w_id);
            file << sql << std::endl;

            /*EXEC_SQL SELECT SUM(ol_amount) INTO :ol_total
                            FROM order_line
                            WHERE ol_o_id = :no_o_id AND ol_d_id = :d_id
                    AND ol_w_id = :w_id;*/
            sql = "SELECT SUM(ol_amount) FROM order_line WHERE ol_o_id = " + std::to_string(no_o_id) + " AND ol_d_id = " + std::to_string(d_id) + " AND ol_w_id = " + std::to_string(w_id);
            file << sql << std::endl;

            /*EXEC_SQL UPDATE customer SET c_balance = c_balance + :ol_total ,
                                               c_delivery_cnt = c_delivery_cnt + 1
                                  WHERE c_id = :c_id AND c_d_id = :d_id AND
                          c_w_id = :w_id;*/
            sql = "UPDATE customer SET c_balance = c_balance + " + std::to_string(rand() % 10000 + 1) + ", c_delivery_cnt = c_delivery_cnt + 1 WHERE c_id = " + std::to_string(rand() % CUSTOMER_PER_DISTRICT + 1) + " AND c_d_id = " + std::to_string(d_id) + " AND c_w_id = " + std::to_string(w_id);
            file << sql << std::endl;
        }
    }

    file.close();
}

int generate_random_int(int min, int max)
{
    int rand_int;
    int rand_range = max - min + 1;
    rand_int = rand() % rand_range;
    rand_int += min;
    return rand_int;
}

static char *makeInt(char *output, int value, int digits)
{
    char *last = output + digits;
    char *next = last;
    for (int i = 0; i < digits; ++i)
    {
        int digit = value % 10;
        value = value / 10;
        next -= 1;
        *next = static_cast<char>('0' + digit);
    }
    return last;
}

void generate_new_order(int w)
{

    std::fstream file("new_order.sql", std::ios::out);

    std::string sql = "";

    for (int i = 0; i < new_order_num; i++)
    {
        int w_id = rand() % w + 1;
        int d_id = rand() % 10 + 1;
        int c_id = rand() % CUSTOMER_PER_DISTRICT + 1;
        /*EXEC_SQL SELECT c_discount, c_last, c_credit, w_tax
      INTO :c_discount, :c_last, :c_credit, :w_tax
          FROM customer, warehouse
          WHERE w_id = :w_id
      AND c_w_id = w_id
      AND c_d_id = :d_id
      AND c_id = :c_id;*/
        sql = "SELECT c_discount, c_last, c_credit, w_tax FROM customer, warehouse WHERE w_id = " + std::to_string(w_id) + " AND c_w_id = " + std::to_string(w_id) + " AND c_d_id = " + std::to_string(d_id) + " AND c_id = " + std::to_string(c_id);
        file << sql << std::endl;

        /*EXEC_SQL SELECT d_next_o_id, d_tax INTO :d_next_o_id, :d_tax
            FROM district
            WHERE d_id = :d_id
        AND d_w_id = :w_id
        FOR UPDATE;*/
        sql = "SELECT d_next_o_id, d_tax FROM district WHERE d_id = " + std::to_string(d_id) + " AND d_w_id = " + std::to_string(w_id) + " FOR UPDATE";
        file << sql << std::endl;

        /*EXEC_SQL UPDATE district SET d_next_o_id = :d_next_o_id + 1
                WHERE d_id = :d_id
            AND d_w_id = :w_id;*/

        sql = "UPDATE district SET d_next_o_id = d_next_o_id + 1 WHERE d_id = " + std::to_string(d_id) + " AND d_w_id = " + std::to_string(w_id);
        file << sql << std::endl;
        /*EXEC_SQL INSERT INTO orders (o_id, o_d_id, o_w_id, o_c_id,
                         o_entry_d, o_ol_cnt, o_all_local)
        VALUES(:o_id, :d_id, :w_id, :c_id,
               :datetime,
                       :o_ol_cnt, :o_all_local);*/
        int o_id = rand() % CUSTOMER_PER_DISTRICT + 1;
        int o_d_id = rand() % 10 + 1;
        int o_w_id = w_id;
        int c_ids[CUSTOMER_PER_DISTRICT + 1];
        for (int i = 1; i <= CUSTOMER_PER_DISTRICT; ++i)
            c_ids[i - 1] = i;
        for (int i = 1; i <= CUSTOMER_PER_DISTRICT; ++i)
        {
            int index = generate_random_int(0, CUSTOMER_PER_DISTRICT - 1);
            std::swap(c_ids[i - 1], c_ids[index]);
        }
        int o_c_id = c_ids[o_id];
        char o_entry_d[20];
        // std::strftime(o_entry_d, sizeof(o_entry_d), "%Y-%m-%d %H:%M:%S", std::localtime(&std::time(nullptr)));

        auto loc_time = std::time(nullptr);

        char *next = makeInt(o_entry_d, std::localtime(&loc_time)->tm_year + 1900, 14);
        *next = '-';
        next += 1;
        next = makeInt(next, std::localtime(&loc_time)->tm_mon + 1, 4);
        *next = '-';
        next += 1;
        next = makeInt(next, std::localtime(&loc_time)->tm_mday, 2);
        *next = ' ';
        next += 1;
        next = makeInt(next, std::localtime(&loc_time)->tm_hour, 2);
        *next = ':';
        next += 1;
        next = makeInt(next, std::localtime(&loc_time)->tm_min, 2);
        *next = ':';
        next += 1;
        next = makeInt(next, std::localtime(&loc_time)->tm_sec, 2);
        *next = '\0';

        int o_carrier_id = 0;

        if (o_id < 2001)
        {
            o_carrier_id = generate_random_int(1, 10);
        }

        int o_ol_cnt = 10;
        int o_all_local = 1;
        sql = "INSERT INTO orders (o_id, o_d_id, o_w_id, o_c_id, o_entry_d, o_ol_cnt, o_all_local, o_carrier_id) VALUES(" + std::to_string(o_id) + "," + std::to_string(o_d_id) + "," + std::to_string(o_w_id) + "," + std::to_string(o_c_id) + ",'" + std::string(o_entry_d) + "'," + std::to_string(o_ol_cnt) + "," + std::to_string(o_all_local) + "," + std::to_string(o_carrier_id) + ")";
        file << sql << std::endl;

        /* EXEC_SQL INSERT INTO new_orders (no_o_id, no_d_id, no_w_id)
           VALUES (:o_id,:d_id,:w_id); */

        sql = "INSERT INTO new_orders (no_o_id, no_d_id, no_w_id) VALUES(" + std::to_string(o_id) + "," + std::to_string(o_d_id) + "," + std::to_string(o_w_id) + ")";
        file << sql << std::endl;

        int ol_number = 1;
        int ol_supply_w_id = 0;
        int ol_i_id = 0;

        ol_supply_w_id = w_id;

        for (; ol_number <= o_ol_cnt; ol_number++)
        {
            ol_i_id = rand() % MAXITEMS + 1;

            /*EXEC_SQL SELECT i_price, i_name, i_data
                    INTO :i_price, :i_name, :i_data
                        FROM item
                        WHERE i_id = :ol_i_id;*/
            sql = "SELECT i_price, i_name, i_data FROM item WHERE i_id = " + std::to_string(ol_i_id);
            file << sql << std::endl;

            /*EXEC_SQL SELECT s_quantity, s_data, s_dist_01, s_dist_02,
                            s_dist_03, s_dist_04, s_dist_05, s_dist_06,
                            s_dist_07, s_dist_08, s_dist_09, s_dist_10
                INTO :s_quantity, :s_data, :s_dist_01, :s_dist_02,
                         :s_dist_03, :s_dist_04, :s_dist_05, :s_dist_06,
                         :s_dist_07, :s_dist_08, :s_dist_09, :s_dist_10
                    FROM stock
                    WHERE s_i_id = :ol_i_id
                AND s_w_id = :ol_supply_w_id
                ;*/

            sql = "SELECT s_quantity, s_data, s_dist_01, s_dist_02, s_dist_03, s_dist_04, s_dist_05, s_dist_06, s_dist_07, s_dist_08, s_dist_09, s_dist_10 FROM stock WHERE s_i_id = " + std::to_string(ol_i_id) + " AND s_w_id = " + std::to_string(ol_supply_w_id);
            file << sql << std::endl;

            /*EXEC_SQL UPDATE stock SET s_quantity = :s_quantity
                    WHERE s_i_id = :ol_i_id
                AND s_w_id = :ol_supply_w_id;*/
            sql = "UPDATE stock SET s_quantity = s_quantity - 1 WHERE s_i_id = " + std::to_string(ol_i_id) + " AND s_w_id = " + std::to_string(ol_supply_w_id);
            file << sql << std::endl;

            /*EXEC_SQL INSERT INTO order_line (ol_o_id, ol_d_id, ol_w_id,
                         ol_number, ol_i_id,
                         ol_supply_w_id, ol_quantity,
                         ol_amount, ol_dist_info)
            VALUES (:o_id, :d_id, :w_id, :ol_number, :ol_i_id,
                :ol_supply_w_id, :ol_quantity, :ol_amount,
                :ol_dist_info);*/
            // 我们暂时不写这一个
        }
    }

    file.close();
}
void generate_order_status(int w, int byname, char last_name[])
{
    std::fstream file("order_status.sql", std::ios::out);
    std::string sql;

    char c_last[17];

    for (int i = 0; i < order_status_num; i++)
    {
        int w_id = rand() % w + 1;
        /*EXEC_SQL SELECT c_balance, c_first, c_middle, c_last
    INTO :c_balance, :c_first, :c_middle, :c_last
        FROM customer
        WHERE c_w_id = :c_w_id
    AND c_d_id = :c_d_id
    AND c_id = :c_id;*/

        int c_id = rand() % CUSTOMER_PER_DISTRICT + 1;
        sql = "SELECT c_balance, c_first, c_middle, c_last FROM customer WHERE c_w_id = " + std::to_string(w_id) + " AND c_d_id = " + std::to_string(rand() % 10 + 1) + " AND c_id = " + std::to_string(c_id);
        file << sql << std::endl;

        /*
        EXEC_SQL
        select o_id, o_entry_d, o_carrier_id from orders where o_w_id=:c_w_id and
o_d_id=:c_d_id and o_c_id=:c_id and o_id=:o_id;
        */
        int o_id = rand() % CUSTOMER_PER_DISTRICT + 1;
        sql = "select o_id, o_entry_d, o_carrier_id from orders where o_w_id=" + std::to_string(w_id) + " and o_d_id=" + std::to_string(rand() % 10 + 1) + " and o_c_id=" + std::to_string(c_id) + " and o_id=" + std::to_string(o_id);
        file << sql << std::endl;

        /*
        EXEC_SQL
        select ol_i_id, ol_supply_w_id, ol_quantity, ol_amount, ol_delivery_d from order_line
where ol_w_id=:c_w_id and ol_d_id=:c_d_id and ol_o_id=:o_id;
        */
        sql = "select ol_i_id, ol_supply_w_id, ol_quantity, ol_amount, ol_delivery_d from order_line where ol_w_id=" + std::to_string(w_id) + " and ol_d_id=" + std::to_string(rand() % 10 + 1) + " and ol_o_id=" + std::to_string(o_id);
        file << sql << std::endl;
    }
    //     if (byname == 1)
    //     {
    //         memcpy(c_last, last_name, 17);
    //         /*EXEC_SQL SELECT count(c_id)
    //             INTO :namecnt
    //                 FROM customer
    //             WHERE c_w_id = :c_w_id
    //             AND c_d_id = :c_d_id
    //                 AND c_last = :c_last;*/
    //         sql = "SELECT count(c_id) FROM customer WHERE c_w_id = " + std::to_string(w_id) + " AND c_d_id = " + std::to_string(rand() % 10 + 1) + " AND c_last = '" + std::string(c_last) + "'";
    //         file << sql << std::endl;

    //         /*
    //         EXEC_SQL
    //         select c_balance, c_first, c_middle, c_last from customer where c_w_id=:c_w_id and
    // c_d_id=:c_d_id and c_last=:c_last order by c_fisrt;
    //         */
    //         sql = "select c_balance, c_first, c_middle, c_last from customer where c_w_id=" + std::to_string(w_id) + " and c_d_id=" + std::to_string(rand() % 10 + 1) + " and c_last='" + std::string(c_last) + "' order by c_fisrt";
    //         file << sql << std::endl;
    //     }
    // else
    // {

    // }
    file.close();
}
void generate_stock_level(int w)
{
    std::fstream file("stock_level.sql", std::ios::out);
    std::string sql;

    for (int i = 0; i < stock_level_num; i++)
    {
        int w_id = rand() % w + 1;

        /*EXEC_SQL SELECT d_next_o_id
                        INTO :d_next_o_id
                        FROM district
                        WHERE d_id = :d_id
                AND d_w_id = :w_id;*/
        int d_id = rand() % 10 + 1;
        sql = "SELECT d_next_o_id FROM district WHERE d_id = " + std::to_string(d_id) + " AND d_w_id = " + std::to_string(w_id);
        file << sql << std::endl;

        /*EXEC_SQL DECLARE ord_line CURSOR FOR
                    SELECT DISTINCT ol_i_id
                    FROM order_line
                    WHERE ol_w_id = :w_id
            AND ol_d_id = :d_id
            AND ol_o_id < :d_next_o_id
            AND ol_o_id >= (:d_next_o_id - 20);*/

        /*
        select ol_i_id from order_line where ol_w_id=:w_id and ol_d_id=:d_id and
    ol_o_id<:d_next_o_id and ol_o_id>=(:d_next_o_id-20);
        */
        int d_next_o_id = 3001;
        sql = "select ol_i_id from order_line where ol_w_id=" + std::to_string(w_id) + " and ol_d_id=" + std::to_string(d_id) + " and ol_o_id<3001 and ol_o_id>=1981";
        file << sql << std::endl;

        /*
        select count(*) as count_stock from stock where s_w_id=:w_id and s_i_id=:ol_i_id and
    s_quantity<:level;
        */
        int level = rand() % 10 + 10;
        sql = "select count(*) as count_stock from stock where s_w_id=" + std::to_string(w_id) + " and s_i_id=1 and s_quantity<" + std::to_string(level);
        file << sql << std::endl;
    }

    file.close();
}
void generate_payment(int w)
{
    std::fstream file("payment.sql", std::ios::out);
    std::string sql;
    for (int i = 0; i < payment_num; i++)
    {
        int w_id = rand() % w + 1;

        /*EXEC_SQL UPDATE warehouse SET w_ytd = w_ytd + :h_amount
          WHERE w_id =:w_id;*/
        sql = "UPDATE warehouse SET w_ytd = w_ytd + 1000 WHERE w_id = " + std::to_string(w_id);
        file << sql << std::endl;

        /*EXEC_SQL SELECT w_street_1, w_street_2, w_city, w_state, w_zip,
                    w_name
                    INTO :w_street_1, :w_street_2, :w_city, :w_state,
                :w_zip, :w_name
                    FROM warehouse
                    WHERE w_id = :w_id;*/
        sql = "SELECT w_street_1, w_street_2, w_city, w_state, w_zip, w_name FROM warehouse WHERE w_id = " + std::to_string(w_id);
        file << sql << std::endl;

        /*EXEC_SQL UPDATE district SET d_ytd = d_ytd + :h_amount
            WHERE d_w_id = :w_id
            AND d_id = :d_id;*/

        int d_id = rand() % 10 + 1;
        sql = "UPDATE district SET d_ytd = d_ytd + 1000 WHERE d_w_id = " + std::to_string(w_id) + " AND d_id = " + std::to_string(d_id);
        file << sql << std::endl;
        /*EXEC_SQL SELECT d_street_1, d_street_2, d_city, d_state, d_zip,
                    d_name
                    INTO :d_street_1, :d_street_2, :d_city, :d_state,
                :d_zip, :d_name
                    FROM district
                    WHERE d_w_id = :w_id
            AND d_id = :d_id;*/
        sql = "SELECT d_street_1, d_street_2, d_city, d_state, d_zip, d_name FROM district WHERE d_w_id = " + std::to_string(w_id) + " AND d_id = " + std::to_string(d_id);
        file << sql << std::endl;

        /*EXEC_SQL select c_first, c_middle, c_last, c_street_1, c_street_2, c_city, c_state, c_zip,
    c_phone, c_credit, c_credit_lim, c_discount, c_balance, c_since from customer where
    c_w_id=:w_id and c_d_id=:d_id and c_id=:c_id;*/
        int c_id = rand() % CUSTOMER_PER_DISTRICT + 1;
        sql = "select c_first, c_middle, c_last, c_street_1, c_street_2, c_city, c_state, c_zip, c_phone, c_credit_lim, c_discount, c_balance, c_since from customer where c_w_id=" + std::to_string(w_id) + " and c_d_id=" + std::to_string(d_id) + " and c_id=" + std::to_string(c_id);
        file << sql << std::endl;

        /*
        update customer set c_balance=:c_balance where c_w_id=:w_id and c_d_id=:d_id and
    c_id=:c_id;
        */
        sql = "update customer set c_balance=c_balance-1000 where c_w_id=" + std::to_string(w_id) + " and c_d_id=" + std::to_string(d_id) + " and c_id=" + std::to_string(c_id);
        file << sql << std::endl;

        /*
        insert into history values(:h_c_id, :h_c_d_id, :h_c_w_id, :h_d_id, :h_w_id, :h_date,
    :h_amount, :h_data);
        */
        sql = "insert into history values(" + std::to_string(c_id) + ", " + std::to_string(d_id) + ", " + std::to_string(w_id) + ", " + std::to_string(d_id) + ", " + std::to_string(w_id) + ", '2021-09-01', 1000, 'payment')";
        file << sql << std::endl;
    }

    file.close();
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cout << "Usage: " << argv[0] << " warehouse_num need" << std::endl;
        exit(1);
    }

    int warehouse_num = std::stoi(argv[1]);

    // 这里仅仅是生成sql文件即可，后续跑负载 我们另写一个脚本来跑负载
    // 设置数量
    /*
    payment 8/23
    new_order 8/23
    order_status 1/23
    stock_level 1/23
    delivery 3/23
    */

    generate_payment(warehouse_num);
    generate_new_order(warehouse_num);
    generate_order_status(warehouse_num, 0, NULL);
    generate_stock_level(warehouse_num);
    generate_delivery(warehouse_num);
}
