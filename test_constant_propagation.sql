-- 常量传播优化测试
-- 测试将 a=3 AND a=b 优化为 a=3 AND b=3

-- 创建测试表
CREATE TABLE test_table (
    id INT,
    name VARCHAR(20),
    value INT,
    status VARCHAR(10)
);

-- 插入测试数据
INSERT INTO test_table VALUES (1, 'Alice', 100, 'active');
INSERT INTO test_table VALUES (2, 'Bob', 200, 'inactive');
INSERT INTO test_table VALUES (3, 'Charlie', 300, 'active');
INSERT INTO test_table VALUES (4, 'David', 400, 'pending');

-- 创建索引
CREATE INDEX idx_test_table ON test_table(id, value);

-- 测试常量传播优化
-- 原始查询：SELECT * FROM test_table WHERE id = 3 AND id = value;
-- 优化后：SELECT * FROM test_table WHERE id = 3 AND value = 3;

-- 这个查询应该被优化为：
-- SELECT * FROM test_table WHERE id = 3 AND value = 3;
-- 因为 id = 3 是常量条件，可以传播到 id = value 中

-- 测试更复杂的常量传播
-- SELECT * FROM test_table WHERE id = 2 AND status = 'active' AND id = value;
-- 应该被优化为：
-- SELECT * FROM test_table WHERE id = 2 AND status = 'active' AND value = 2;

-- 测试范围查询的常量传播
-- SELECT * FROM test_table WHERE id > 1 AND id = value;
-- 应该被优化为：
-- SELECT * FROM test_table WHERE id > 1 AND value > 1;

-- 清理
DROP INDEX idx_test_table;
DROP TABLE test_table;
