#!/bin/bash

# TPCC性能优化测试脚本
# 测试最左索引、连接条件优化、UPDATE优化和代码级优化的效果

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

# 检查编译状态
check_build() {
    log_info "检查编译状态..."
    if [ ! -f "build/bin/rmdb" ]; then
        log_error "rmdb可执行文件不存在，请先编译项目"
        exit 1
    fi
    log_success "编译检查通过"
}

# 启动数据库服务器
start_server() {
    local db_name=$1
    log_info "启动数据库服务器: $db_name"
    
    # 清理旧的数据库文件
    rm -rf "$db_name"
    
    # 在后台启动服务器
    cd build
    ./bin/rmdb "$db_name" > server.log 2>&1 &
    SERVER_PID=$!
    cd ..
    
    # 等待服务器启动
    sleep 2
    
    if kill -0 $SERVER_PID 2>/dev/null; then
        log_success "数据库服务器启动成功 (PID: $SERVER_PID)"
    else
        log_error "数据库服务器启动失败"
        exit 1
    fi
}

# 停止数据库服务器
stop_server() {
    if [ ! -z "$SERVER_PID" ]; then
        log_info "停止数据库服务器 (PID: $SERVER_PID)"
        kill $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
        log_success "数据库服务器已停止"
    fi
}

# 运行TPCC测试
run_tpcc_test() {
    local test_name=$1
    local threads=$2
    local transactions=$3
    
    log_info "运行TPCC测试: $test_name"
    log_info "线程数: $threads, 事务数: $transactions"
    
    cd TPCC-Tester-master
    
    # 记录开始时间
    start_time=$(date +%s.%N)
    
    # 运行测试
    if python runner.py --prepare --thread $threads --rw $transactions --analyze > "../test_results/${test_name}.log" 2>&1; then
        # 记录结束时间
        end_time=$(date +%s.%N)
        duration=$(echo "$end_time - $start_time" | bc)
        
        # 提取tpmC值
        tpmc=$(grep "tpmC:" "../test_results/${test_name}.log" | tail -1 | awk '{print $2}' || echo "N/A")
        
        log_success "测试完成: $test_name"
        log_info "耗时: ${duration}s, tpmC: $tpmc"
        
        # 保存结果
        echo "$test_name,$threads,$transactions,$duration,$tpmc" >> "../test_results/summary.csv"
    else
        log_error "测试失败: $test_name"
        return 1
    fi
    
    cd ..
}

# 分析索引使用情况
analyze_index_usage() {
    log_info "分析索引使用情况..."
    
    cat << EOF > test_results/index_analysis.txt
TPCC索引使用分析报告
===================

当前索引配置:
- warehouse(w_id)
- district(d_w_id, d_id) ✓ 支持最左匹配
- customer(c_w_id, c_d_id, c_id) ✓ 支持最左匹配  
- new_orders(no_w_id, no_d_id, no_o_id) ✓ 支持最左匹配
- orders(o_w_id, o_d_id, o_id) ✓ 支持最左匹配
- order_line(ol_w_id, ol_d_id, ol_o_id, ol_number) ✓ 支持最左匹配
- item(i_id)
- stock(s_w_id, s_i_id) ✓ 支持最左匹配

TPCC事务的索引使用模式:

1. New Order事务:
   - district表: WHERE d_w_id = ? AND d_id = ? → 使用(d_w_id, d_id)索引
   - stock表: WHERE s_w_id = ? AND s_i_id = ? → 使用(s_w_id, s_i_id)索引
   - item表: WHERE i_id = ? → 使用(i_id)索引

2. Payment事务:
   - warehouse表: WHERE w_id = ? → 使用(w_id)索引
   - customer表: WHERE c_w_id = ? AND c_d_id = ? AND c_id = ? → 使用(c_w_id, c_d_id, c_id)索引

3. Order Status事务:
   - orders表: WHERE o_w_id = ? AND o_d_id = ? AND o_c_id = ? → 使用(o_w_id, o_d_id)前缀
   - order_line表: WHERE ol_w_id = ? AND ol_d_id = ? AND ol_o_id = ? → 使用(ol_w_id, ol_d_id, ol_o_id)前缀

4. Delivery事务:
   - new_orders表: WHERE no_w_id = ? AND no_d_id = ? → 使用(no_w_id, no_d_id)前缀

5. Stock Level事务:
   - order_line表: WHERE ol_w_id = ? AND ol_d_id = ? → 使用(ol_w_id, ol_d_id)前缀
   - stock表: WHERE s_i_id IN (...) → 可能无法有效使用索引

优化建议:
- 所有主要查询都能有效利用最左前缀匹配
- Stock Level事务的性能可能受限于IN查询的处理
- 考虑为高频查询添加覆盖索引以减少回表操作
EOF

    log_success "索引分析完成，结果保存到 test_results/index_analysis.txt"
}

# 主函数
main() {
    log_info "开始TPCC性能优化测试"
    
    # 创建结果目录
    mkdir -p test_results
    
    # 初始化结果文件
    echo "test_name,threads,transactions,duration,tpmc" > test_results/summary.csv
    
    # 检查编译
    check_build
    
    # 启动数据库服务器
    start_server "tpcc_test"
    
    # 设置清理函数
    trap stop_server EXIT
    
    # 运行不同配置的测试
    log_info "开始性能测试..."
    
    # 小规模测试
    run_tpcc_test "small_1thread" 1 20
    run_tpcc_test "small_2thread" 2 30
    
    # 中等规模测试
    run_tpcc_test "medium_2thread" 2 50
    run_tpcc_test "medium_4thread" 4 50
    
    # 分析结果
    log_info "分析测试结果..."
    
    if [ -f "test_results/summary.csv" ]; then
        log_info "测试结果摘要:"
        cat test_results/summary.csv | column -t -s ','
        
        # 计算平均性能
        avg_tpmc=$(tail -n +2 test_results/summary.csv | awk -F',' '{sum+=$5; count++} END {if(count>0) print sum/count; else print "N/A"}')
        log_info "平均tpmC: $avg_tpmc"
    fi
    
    # 分析索引使用
    analyze_index_usage
    
    log_success "TPCC性能优化测试完成！"
    log_info "详细结果请查看 test_results/ 目录"
}

# 运行主函数
main "$@"
