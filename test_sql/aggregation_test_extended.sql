-- 创建学生成绩表
create table student_scores (
    student_id int,
    course_name char(30),
    semester char(10),
    score float,
    credit int
);

-- 插入测试数据
insert into student_scores values(1001, 'Database', 'Fall2023', 92.5, 3);
insert into student_scores values(1001, 'Algorithm', 'Fall2023', 88.0, 4);
insert into student_scores values(1001, 'OS', 'Spring2024', 95.5, 3);
insert into student_scores values(1001, 'Network', 'Spring2024', 87.5, 3);

insert into student_scores values(1002, 'Database', 'Fall2023', 78.5, 3);
insert into student_scores values(1002, 'Algorithm', 'Fall2023', 92.0, 4);
insert into student_scores values(1002, 'OS', 'Spring2024', 85.0, 3);
insert into student_scores values(1002, 'Network', 'Spring2024', 90.5, 3);

insert into student_scores values(1003, 'Database', 'Fall2023', 96.0, 3);
insert into student_scores values(1003, 'Algorithm', 'Fall2023', 94.5, 4);
insert into student_scores values(1003, 'OS', 'Spring2024', 89.0, 3);

-- 基础聚合测试
select student_id, COUNT(*) as course_count, AVG(score) as avg_score, 
       MAX(score) as highest, MIN(score) as lowest, SUM(score) as total_score
from student_scores group by student_id;
-- 预期输出:
-- student_id | course_count | avg_score | highest | lowest | total_score
-- 1001       | 4           | 90.875    | 95.5    | 87.5   | 363.5
-- 1002       | 4           | 86.5      | 92.0    | 78.5   | 346.0
-- 1003       | 3           | 93.167    | 96.0    | 89.0   | 279.5

-- 多条件HAVING测试
select student_id, AVG(score) as avg_score, COUNT(*) as course_num
from student_scores 
group by student_id 
having COUNT(*) >= 3 and AVG(score) > 85;
-- 预期输出:
-- student_id | avg_score | course_num
-- 1001       | 90.875    | 4
-- 1002       | 86.5      | 4
-- 1003       | 93.167    | 3

-- 按课程聚合
select course_name, COUNT(*) as student_count, AVG(score) as class_avg,
       MAX(score) as top_score, MIN(score) as lowest_score
from student_scores group by course_name;
-- 预期输出:
-- course_name | student_count | class_avg | top_score | lowest_score
-- Algorithm   | 3            | 91.5      | 94.5      | 88.0
-- Database    | 3            | 89.0      | 96.0      | 78.5
-- Network     | 2            | 89.0      | 90.5      | 87.5
-- OS          | 3            | 89.833    | 95.5      | 85.0

-- 按学期聚合
select semester, COUNT(*) as total_courses, AVG(score) as semester_avg,
       SUM(credit) as total_credits
from student_scores group by semester;
-- 预期输出:
-- semester    | total_courses | semester_avg | total_credits
-- Fall2023    | 6            | 90.25        | 21
-- Spring2024  | 5            | 89.5         | 15

-- 复杂HAVING条件
select course_name, COUNT(*) as enrolled, AVG(score) as avg_score
from student_scores 
group by course_name 
having COUNT(*) > 2 and AVG(score) between 85 and 95;
-- 预期输出:
-- course_name | enrolled | avg_score
-- Algorithm   | 3        | 91.5
-- Database    | 3        | 89.0
-- OS          | 3        | 89.833

-- 测试NULL值处理
insert into student_scores values(1004, 'Database', 'Fall2023', NULL, 3);
insert into student_scores values(1004, 'Algorithm', 'Fall2023', 85.0, 4);

select student_id, COUNT(*) as total_records, COUNT(score) as valid_scores,
       AVG(score) as avg_score, SUM(score) as sum_score
from student_scores group by student_id;
-- 预期输出:
-- student_id | total_records | valid_scores | avg_score | sum_score
-- 1001       | 4            | 4            | 90.875    | 363.5
-- 1002       | 4            | 4            | 86.5      | 346.0
-- 1003       | 3            | 3            | 93.167    | 279.5
-- 1004       | 2            | 1            | 85.0      | 85.0

-- 边界情况测试
select course_name, MIN(score) as min_score, MAX(score) as max_score
from student_scores 
where score is not null
group by course_name
having MIN(score) > 80;
-- 预期输出:
-- course_name | min_score | max_score
-- Algorithm   | 85.0      | 94.5
-- Network     | 87.5      | 90.5
-- OS          | 85.0      | 95.5

-- 创建销售数据表进行更多测试
create table sales_data (
    region char(20),
    product char(30),
    quarter int,
    sales_amount float,
    units_sold int
);

insert into sales_data values('North', 'Laptop', 1, 15000.0, 50);
insert into sales_data values('North', 'Laptop', 2, 18000.0, 60);
insert into sales_data values('North', 'Phone', 1, 12000.0, 120);
insert into sales_data values('North', 'Phone', 2, 14000.0, 140);

insert into sales_data values('South', 'Laptop', 1, 13000.0, 43);
insert into sales_data values('South', 'Laptop', 2, 16000.0, 53);
insert into sales_data values('South', 'Phone', 1, 11000.0, 110);
insert into sales_data values('South', 'Phone', 2, 13500.0, 135);

insert into sales_data values('East', 'Laptop', 1, 17000.0, 57);
insert into sales_data values('East', 'Phone', 1, 15000.0, 150);

-- 按地区聚合销售数据
select region, COUNT(*) as record_count, SUM(sales_amount) as total_sales,
       AVG(sales_amount) as avg_sales, SUM(units_sold) as total_units
from sales_data group by region;
-- 预期输出:
-- region | record_count | total_sales | avg_sales | total_units
-- East   | 2           | 32000.0     | 16000.0   | 207
-- North  | 4           | 59000.0     | 14750.0   | 370
-- South  | 4           | 53500.0     | 13375.0   | 341

-- 按产品聚合
select product, COUNT(*) as quarters_sold, MAX(sales_amount) as peak_sales,
       MIN(sales_amount) as lowest_sales, AVG(units_sold) as avg_units
from sales_data group by product;
-- 预期输出:
-- product | quarters_sold | peak_sales | lowest_sales | avg_units
-- Laptop  | 5            | 18000.0    | 13000.0      | 52.6
-- Phone   | 5            | 15000.0    | 11000.0      | 131.0

-- 复合条件聚合
select region, product, SUM(sales_amount) as total_revenue, COUNT(*) as quarters
from sales_data 
group by region, product
having SUM(sales_amount) > 25000;
-- 预期输出:
-- region | product | total_revenue | quarters
-- North  | Laptop  | 33000.0      | 2
-- North  | Phone   | 26000.0      | 2
-- South  | Laptop  | 29000.0      | 2

-- 测试空结果集聚合
select region, COUNT(*) as count, AVG(sales_amount) as avg_sales
from sales_data 
where sales_amount > 50000
group by region;
-- 预期输出: (空结果集)

-- 单行聚合（无GROUP BY）
select COUNT(*) as total_records, AVG(sales_amount) as overall_avg,
       MAX(sales_amount) as highest_sale, MIN(sales_amount) as lowest_sale,
       SUM(sales_amount) as grand_total
from sales_data;
-- 预期输出:
-- total_records | overall_avg | highest_sale | lowest_sale | grand_total
-- 10           | 14450.0     | 18000.0      | 11000.0     | 144500.0

-- 测试COUNT(*)与COUNT(column)的区别
insert into sales_data values('West', 'Tablet', 1, NULL, 0);

select region, COUNT(*) as all_records, COUNT(sales_amount) as valid_sales,
       COUNT(units_sold) as valid_units
from sales_data group by region;
-- 预期输出:
-- region | all_records | valid_sales | valid_units
-- East   | 2          | 2           | 2
-- North  | 4          | 4           | 4
-- South  | 4          | 4           | 4
-- West   | 1          | 0           | 1

-- 清理测试数据
drop table student_scores;
drop table sales_data;

-- 员工薪资表测试
create table employee_salary (
    dept_id int,
    emp_id int,
    salary float,
    bonus float,
    years_exp int
);

insert into employee_salary values(10, 101, 5000.0, 1000.0, 2);
insert into employee_salary values(10, 102, 6000.0, 1200.0, 3);
insert into employee_salary values(10, 103, 7000.0, NULL, 5);

insert into employee_salary values(20, 201, 5500.0, 1100.0, 3);
insert into employee_salary values(20, 202, 6500.0, 1300.0, 4);
insert into employee_salary values(20, 203, 8000.0, 1600.0, 7);
insert into employee_salary values(20, 204, 4500.0, 900.0, 1);

-- 部门薪资统计
select dept_id, COUNT(*) as emp_count, AVG(salary) as avg_salary,
       MAX(salary) as max_salary, MIN(salary) as min_salary,
       SUM(salary) as total_payroll
from employee_salary group by dept_id;
-- 预期输出:
-- dept_id | emp_count | avg_salary | max_salary | min_salary | total_payroll
-- 10      | 3        | 6000.0     | 7000.0     | 5000.0     | 18000.0
-- 20      | 4        | 6125.0     | 8000.0     | 4500.0     | 24500.0

-- 高薪部门筛选
select dept_id, COUNT(*) as emp_count, AVG(salary) as avg_salary
from employee_salary 
group by dept_id 
having AVG(salary) > 5500 and COUNT(*) >= 3;
-- 预期输出:
-- dept_id | emp_count | avg_salary
-- 10      | 3        | 6000.0
-- 20      | 4        | 6125.0

-- 奖金统计（处理NULL）
select dept_id, COUNT(bonus) as bonus_recipients, AVG(bonus) as avg_bonus,
       SUM(bonus) as total_bonus
from employee_salary group by dept_id;
-- 预期输出:
-- dept_id | bonus_recipients | avg_bonus | total_bonus
-- 10      | 2               | 1100.0    | 2200.0
-- 20      | 4               | 1225.0    | 4900.0

drop table employee_salary;
