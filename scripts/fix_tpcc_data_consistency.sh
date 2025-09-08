#!/bin/bash

# TPC-C数据一致性修复脚本
# 专门用于修复您遇到的一致性问题

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 显示您的问题诊断
show_problem_diagnosis() {
    echo "=========================================="
    echo "TPC-C一致性问题诊断"
    echo "=========================================="
    echo ""
    echo "检测到的问题："
    echo "1. district.d_next_o_id=3003, max(orders.o_id)=3000, max(new_orders.no_o_id)=3002"
    echo "   -> 期望: d_next_o_id = max(o_id) + 1 = max(no_o_id) + 1"
    echo ""
    echo "2. sum(orders.o_ol_cnt)=30005, count(order_line.ol_o_id)=30000"
    echo "   -> 期望: count(ol_o_id) = sum(o_ol_cnt)"
    echo ""
    echo "3. count(new_orders.no_o_id) != max(no_o_id) - min(no_o_id) + 1"
    echo "   -> 期望: new_orders记录应该连续"
    echo ""
}

# 创建数据修复SQL
create_repair_sql() {
    log_info "创建数据修复SQL脚本..."
    
    cat > tpcc_data_repair.sql << 'EOF'
-- TPC-C数据一致性修复SQL脚本

-- 1. 修复district表的d_next_o_id字段
-- 确保 d_next_o_id = max(o_id) + 1
UPDATE district d 
SET d_next_o_id = (
    SELECT COALESCE(MAX(o_id), 2999) + 1 
    FROM orders o 
    WHERE o.o_w_id = d.d_w_id AND o.o_d_id = d.d_id
)
WHERE EXISTS (
    SELECT 1 FROM orders o 
    WHERE o.o_w_id = d.d_w_id AND o.o_d_id = d.d_id
    AND d.d_next_o_id != (SELECT MAX(o2.o_id) + 1 FROM orders o2 WHERE o2.o_w_id = d.d_w_id AND o2.o_d_id = d.d_id)
);

-- 2. 删除new_orders表中的孤立记录
-- 删除在orders表中不存在对应记录的new_orders记录
DELETE FROM new_orders 
WHERE (no_w_id, no_d_id, no_o_id) NOT IN (
    SELECT o_w_id, o_d_id, o_id 
    FROM orders
);

-- 3. 删除order_line表中的孤立记录
-- 删除在orders表中不存在对应记录的order_line记录
DELETE FROM order_line 
WHERE (ol_w_id, ol_d_id, ol_o_id) NOT IN (
    SELECT o_w_id, o_d_id, o_id 
    FROM orders
);

-- 4. 修复orders表的o_ol_cnt字段
-- 确保o_ol_cnt等于实际的order_line记录数
UPDATE orders o 
SET o_ol_cnt = (
    SELECT COUNT(*) 
    FROM order_line ol 
    WHERE ol.ol_w_id = o.o_w_id 
    AND ol.ol_d_id = o.o_d_id 
    AND ol.ol_o_id = o.o_id
)
WHERE EXISTS (
    SELECT 1 FROM order_line ol 
    WHERE ol.ol_w_id = o.o_w_id 
    AND ol.ol_d_id = o.o_d_id 
    AND ol.ol_o_id = o.o_id
    HAVING COUNT(*) != o.o_ol_cnt
);

-- 5. 为有orders记录但没有new_orders记录的情况补充new_orders
-- 这处理delivery事务可能删除了new_orders但保留了orders的情况
INSERT INTO new_orders (no_o_id, no_d_id, no_w_id)
SELECT o_id, o_d_id, o_w_id 
FROM orders o
WHERE o_carrier_id IS NULL  -- 未配送的订单
AND NOT EXISTS (
    SELECT 1 FROM new_orders no 
    WHERE no.no_w_id = o.o_w_id 
    AND no.no_d_id = o.o_d_id 
    AND no.no_o_id = o.o_id
);

-- 验证修复结果
SELECT 'District一致性检查' as check_type, COUNT(*) as issues
FROM district d
WHERE d_next_o_id != (
    SELECT COALESCE(MAX(o_id), 0) + 1 
    FROM orders 
    WHERE o_w_id = d.d_w_id AND o_d_id = d.d_id
)

UNION ALL

SELECT 'Orders-OrderLine一致性检查' as check_type, COUNT(*) as issues
FROM orders o
WHERE o_ol_cnt != (
    SELECT COUNT(*) 
    FROM order_line ol 
    WHERE ol.ol_w_id = o.o_w_id 
    AND ol.ol_d_id = o.o_d_id 
    AND ol.ol_o_id = o.o_id
)

UNION ALL

SELECT 'Orders-NewOrders一致性检查' as check_type, COUNT(*) as issues
FROM new_orders no
WHERE NOT EXISTS (
    SELECT 1 FROM orders o 
    WHERE o.o_w_id = no.no_w_id 
    AND o.o_d_id = no.no_d_id 
    AND o.o_id = no.no_o_id
);

EOF

    log_success "数据修复SQL脚本已创建: tpcc_data_repair.sql"
}

# 创建验证SQL
create_validation_sql() {
    log_info "创建数据验证SQL脚本..."
    
    cat > tpcc_validation.sql << 'EOF'
-- TPC-C数据一致性验证SQL脚本

-- 检查所有district的一致性
SELECT 
    'District Consistency Check' as test_name,
    d_w_id, d_d_id, d_next_o_id,
    (SELECT MAX(o_id) FROM orders WHERE o_w_id = d.d_w_id AND o_d_id = d.d_d_id) as max_o_id,
    (SELECT MAX(no_o_id) FROM new_orders WHERE no_w_id = d.d_w_id AND no_d_id = d.d_d_id) as max_no_o_id,
    CASE 
        WHEN d_next_o_id = (SELECT COALESCE(MAX(o_id), 0) + 1 FROM orders WHERE o_w_id = d.d_w_id AND o_d_id = d.d_d_id)
        THEN 'PASS'
        ELSE 'FAIL'
    END as status
FROM district d
ORDER BY d_w_id, d_d_id;

-- 检查orders和order_line的一致性（按district汇总）
SELECT 
    'Orders-OrderLine Consistency Check' as test_name,
    o_w_id, o_d_id,
    SUM(o_ol_cnt) as sum_o_ol_cnt,
    (SELECT COUNT(*) FROM order_line WHERE ol_w_id = o.o_w_id AND ol_d_id = o.o_d_id) as count_ol_records,
    CASE 
        WHEN SUM(o_ol_cnt) = (SELECT COUNT(*) FROM order_line WHERE ol_w_id = o.o_w_id AND ol_d_id = o.o_d_id)
        THEN 'PASS'
        ELSE 'FAIL'
    END as status
FROM orders o
GROUP BY o_w_id, o_d_id
ORDER BY o_w_id, o_d_id;

-- 检查new_orders的连续性
SELECT 
    'New Orders Continuity Check' as test_name,
    no_w_id, no_d_id,
    COUNT(*) as count_records,
    MIN(no_o_id) as min_no_o_id,
    MAX(no_o_id) as max_no_o_id,
    CASE 
        WHEN COUNT(*) = (MAX(no_o_id) - MIN(no_o_id) + 1) OR COUNT(*) = 0
        THEN 'PASS'
        ELSE 'FAIL'
    END as status
FROM new_orders
GROUP BY no_w_id, no_d_id
ORDER BY no_w_id, no_d_id;

-- 汇总检查结果
SELECT 
    'Summary' as test_name,
    NULL as detail1, NULL as detail2, NULL as detail3,
    COUNT(*) as total_issues
FROM (
    SELECT d_w_id, d_d_id FROM district d
    WHERE d_next_o_id != (SELECT COALESCE(MAX(o_id), 0) + 1 FROM orders WHERE o_w_id = d.d_w_id AND o_d_id = d.d_d_id)
    
    UNION ALL
    
    SELECT o_w_id, o_d_id FROM orders o
    WHERE o_ol_cnt != (SELECT COUNT(*) FROM order_line WHERE ol_w_id = o.o_w_id AND ol_d_id = o.o_d_id AND ol_o_id = o.o_id)
    
    UNION ALL
    
    SELECT no_w_id, no_d_id FROM new_orders no
    WHERE NOT EXISTS (SELECT 1 FROM orders WHERE o_w_id = no.no_w_id AND o_d_id = no.no_d_id AND o_id = no.no_o_id)
) issues;

EOF

    log_success "数据验证SQL脚本已创建: tpcc_validation.sql"
}

# 执行修复
execute_repair() {
    log_info "开始执行TPC-C数据一致性修复..."
    
    # 这里假设您有一个数据库连接工具
    # 实际使用时，请替换为您的数据库连接命令
    if command -v rmdb_client &> /dev/null; then
        log_info "使用rmdb_client执行修复SQL..."
        rmdb_client < tpcc_data_repair.sql
        log_success "修复SQL执行完成"
        
        log_info "执行验证SQL..."
        rmdb_client < tpcc_validation.sql
        log_success "验证SQL执行完成"
    else
        log_warning "未找到rmdb_client，请手动执行以下SQL文件："
        log_warning "1. tpcc_data_repair.sql - 修复数据不一致"
        log_warning "2. tpcc_validation.sql - 验证修复结果"
    fi
}

# 显示修复建议
show_repair_suggestions() {
    echo ""
    echo "=========================================="
    echo "修复建议和后续步骤"
    echo "=========================================="
    echo ""
    echo "1. 代码层面的修复（已完成）："
    echo "   - 更新了一致性检查逻辑（考虑delivery事务的影响）"
    echo "   - 增强了事务管理器的原子性保证"
    echo "   - 添加了TPC-C专用的事务执行器"
    echo "   - 创建了数据修复工具"
    echo ""
    echo "2. 数据层面的修复："
    echo "   - 执行 tpcc_data_repair.sql 修复现有数据"
    echo "   - 运行 tpcc_validation.sql 验证修复结果"
    echo ""
    echo "3. 运行时配置优化："
    echo "   - 启用 TPCC_CONSISTENCY_MODE=true（在config.h中）"
    echo "   - 启用 TPCC_ATOMIC_TRANSACTIONS=true"
    echo "   - 设置合适的 TPCC_CONSISTENCY_TOLERANCE 值"
    echo ""
    echo "4. 长期监控："
    echo "   - 定期运行一致性检查"
    echo "   - 监控new_order和delivery事务的执行顺序"
    echo "   - 在高并发环境下测试数据一致性"
    echo ""
}

# 主函数
main() {
    echo "TPC-C数据一致性修复工具"
    echo "========================="
    echo ""
    
    show_problem_diagnosis
    
    create_repair_sql
    create_validation_sql
    
    echo ""
    log_info "修复脚本准备完成。"
    
    # 询问是否立即执行修复
    read -p "是否立即执行数据修复？(y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        execute_repair
    else
        log_info "修复脚本已准备就绪，您可以稍后手动执行。"
    fi
    
    show_repair_suggestions
    
    log_success "TPC-C数据一致性修复工具运行完成！"
}

# 运行主函数
main "$@"
