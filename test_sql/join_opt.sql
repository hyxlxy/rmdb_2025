EXPLAIN SELECT c_discount, c_last, c_credit, w_tax FROM customer, warehouse WHERE w_id = 1  AND c_w_id = 1 AND c_d_id = 1 AND c_id = 1;


Read from client 40: EXPLAIN SELECT c_discount, c_last, c_credit, w_tax FROM customer, warehouse WHERE w_id = 1  AND c_w_id = 1 AND c_d_id = 1 AND c_id = 1;

Parser Error at line 1 column 1: syntax error, unexpected IDENTIFIER


create table students (stu_id int, stu_name char(20), class_id int, score int);
create table classes (class_id int, class_name char(30), teacher char(20));
insert into students values (1, 'anna', 100, 85);
insert into students values (2, 'ben',200, 72);
insert into students values (3, 'carol', 100, 90);
insert into students values (4, 'david', 300, 95);
insert into classes values (100, 'math','smith');
insert into classes values (200, 'history', 'lee');
insert into classes values (300, 'physics', 'smith');
explain select * from students s join classes c on s.class_id = c.class_id where s.score > 80 and c.teacher = 'smith';
drop table students;
drop table classes;