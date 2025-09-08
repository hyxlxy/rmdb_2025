create table item (i_id int, i_im_id int, i_name char(25), i_price float, i_data char(51));
load ../../src/test/performance_test/table_data/item.csv into item;
select count(*) from item;












