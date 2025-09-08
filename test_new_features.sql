-- 测试DISTINCT功能
select distinct * from tb;
select distinct a, b from tb;

-- 测试字符串函数
select upper('hello'), lower('WORLD');
select substring('hello world', 1, 5);
select concat('hello', ' world');
select length('hello world');

-- 测试CASE WHEN表达式
select a,
       case when a > 5 then 'high'
            when a > 2 then 'medium'
            else 'low' end as level
from tb;

select case a
       when 1 then 'one'
       when 2 then 'two'
       else 'other' end as number_name
from tb;

-- 测试更多JOIN类型
select x.a, y.b from x left join y on x.id = y.id;
select x.a, y.b from x right join y on x.id = y.id;
select x.a, y.b from x full outer join y on x.id = y.id;
