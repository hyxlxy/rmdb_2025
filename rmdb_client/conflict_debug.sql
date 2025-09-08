-- 测试可能触发冲突的特殊场景
create table conflict_test (id int, data char(20));

-- 插入测试数据
insert into conflict_test values (1, 'original');
insert into conflict_test values (2, 'data');

-- 显示初始状态
select * from conflict_test;

-- 场景1: 尝试在同一事务中多次操作同一记录
begin;
update conflict_test set data = 'updated' where id = 1;
update conflict_test set data = 'updated_again' where id = 1;
select * from conflict_test where id = 1;
commit;

-- 场景2: 删除后立即插入相同ID
begin;
delete from conflict_test where id = 2;
insert into conflict_test values (2, 'new_data');
select * from conflict_test where id = 2;
commit;

-- 查看最终结果
select * from conflict_test;

-- 清理
drop table conflict_test;
