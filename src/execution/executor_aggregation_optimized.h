#pragma once

#include "execution_defs.h"
#include "execution_manager.h"
#include "mvcc_executor_base.h"
#include "executor_group.h"
#include "index/ix.h"
#include "system/sm.h"
#include "parser/ast.h"

/**
 * @brief 优化的聚合执行器
 * 
 * 专门处理聚合函数计算，接收来自GroupExecutor的分组数据
 */
class OptimizedAggregationExecutor : public MVCCExecutorBase
{
private:
    std::unique_ptr<AbstractExecutor> child_;  // GroupExecutor
    std::vector<std::shared_ptr<ast::Expr>> select_exprs_;
    
    struct AggResult {
        std::vector<Value> values;
        AggResult(size_t size) : values(size) {}
    };
    
    std::vector<AggResult> results_;
    size_t current_result_;
    std::vector<ColMeta> output_cols_;
    
public:
    OptimizedAggregationExecutor(std::unique_ptr<AbstractExecutor> child,
                               const std::vector<std::shared_ptr<ast::Expr>>& select_exprs)
        : child_(std::move(child)), select_exprs_(select_exprs), current_result_(0) {
        build_output_columns();
    }
    
    void beginTuple() override {
        child_->beginTuple();
        results_.clear();
        current_result_ = 0;
        
        auto group_executor = dynamic_cast<GroupExecutor*>(child_.get());
        if (group_executor) {
            // 处理分组聚合
            while (!group_executor->is_end()) {
                const auto& group_records = group_executor->getCurrentGroupRecords();  // 使用引用
                AggResult result = compute_aggregation_for_group(group_records);
                results_.push_back(std::move(result));
                group_executor->nextTuple();
            }
        } else {
            // 流式无分组聚合
            AggResult result = compute_streaming_aggregation();
            if (!result.values.empty()) {
                results_.push_back(std::move(result));
            }
        }
    }
    
    void nextTuple() override {
        current_result_++;
    }
    
    bool is_end() const override {
        return current_result_ >= results_.size();
    }
    
    std::unique_ptr<RmRecord> Next() override {
        if (is_end()) return nullptr;
        
        const auto& result = results_[current_result_];
        size_t len = tupleLen();
        auto rec = std::make_unique<RmRecord>(len);
        
        // 构建输出记录，返回给projecitonexecutor
        for (size_t i = 0; i < result.values.size(); i++) {
            const auto& col = output_cols_[i];
            const auto& val = result.values[i];
            
            if (val.type == ColType::TYPE_INT) {
                *(int*)(rec->data + col.offset) = val.int_val;
            } else if (val.type == ColType::TYPE_FLOAT) {
                *(float*)(rec->data + col.offset) = val.float_val;
            } else if (val.type == ColType::TYPE_STRING) {
                memset(rec->data + col.offset, 0, col.len);  // 先清零
                memcpy(rec->data + col.offset, val.str_val.c_str(), 
                       std::min(val.str_val.length(), (size_t)col.len));
            }
        }
        
        return rec;
    }
    
    const std::vector<ColMeta>& cols() const override {
        return output_cols_;
    }
    
    size_t tupleLen() const override {
        size_t len = 0;
        for (const auto& col : output_cols_) {
            len += col.len;
        }
        return len;
    }
    
    Rid& rid() override {
        return _abstract_rid;
    }

private:
    AggResult compute_streaming_aggregation() {
        // 单个MIN/MAX查询的索引优化
        if (select_exprs_.size() == 1) {
            auto expr = select_exprs_[0];
            if (auto alias = std::dynamic_pointer_cast<ast::AliasExpr>(expr)) {
                expr = alias->expr;
            }
            auto agg = std::dynamic_pointer_cast<ast::AggExpr>(expr);
            if (agg && (agg->agg_type == ast::AGG_MIN || agg->agg_type == ast::AGG_MAX)) {
                Value result;
                if (try_index_min_max_optimization(agg, result)) {
                    AggResult agg_result(1);
                    agg_result.values[0] = result;
                    return agg_result;
                }
            }else if (agg && agg->agg_type == ast::AGG_COUNT) {//先不用那招
                return compute_single_count_aggregation();

                // if (!agg->arg) {
                //     // COUNT(*) - 优先尝试缓存
                //     if (can_use_cached_count()) {
                //         // std::cout<<"cached_count"<<std::endl;
                //         int cached_count = get_cached_record_count();
                //         if (cached_count >= 0) {
                //             // std::cout<<"cached_count"<<cached_count<<std::endl;
                //             AggResult result(1);
                //             result.values[0].set_int(cached_count);
                //             return result;
                //         }
                //     }
                //     // 缓存失败，使用单COUNT优化
                //     return compute_single_count_aggregation();
                // } else {
                //     // COUNT(column) - 直接使用单COUNT优化
                //     return compute_single_count_aggregation();
                // }
            }
        }
        
        // 原有的COUNT优化逻辑...
        if (select_exprs_.size() == 1) {
            auto expr = select_exprs_[0];
            if (auto alias = std::dynamic_pointer_cast<ast::AliasExpr>(expr)) {
                expr = alias->expr;
            }
            auto agg = std::dynamic_pointer_cast<ast::AggExpr>(expr);
 
        }
        
        AggResult result(select_exprs_.size());
        
        // 初始化聚合状态
        std::vector<int> counts(select_exprs_.size(), 0);
        std::vector<int> int_sums(select_exprs_.size(), 0);      // INT类型的SUM
        std::vector<float> float_sums(select_exprs_.size(), 0.0f); // FLOAT类型的SUM
        std::vector<Value> min_vals;
        std::vector<Value> max_vals;
        std::vector<bool> first_record(select_exprs_.size(), true);
        
        min_vals.reserve(select_exprs_.size());
        max_vals.reserve(select_exprs_.size());
        for (size_t i = 0; i < select_exprs_.size(); i++) {
            min_vals.emplace_back();
            max_vals.emplace_back();
        }
        
        bool has_records = false;
        
        // 流式处理每条记录
        while (!child_->is_end()) {
            auto rec = child_->Next();
            has_records = true;
            
            // 更新聚合状态（不保存记录）
            update_streaming_agg_state(rec.get(), counts, int_sums, float_sums, min_vals, max_vals, first_record);
            child_->nextTuple();
        }
        
        // 检查空结果集的COUNT(*)情况
        if (!has_records) {
            bool has_count_star = check_has_count();
            if (!has_count_star) {
                return AggResult(0); // 返回空结果
            }
        }
        
        // 计算最终结果
        compute_final_results(result, counts, int_sums, float_sums, min_vals, max_vals, has_records);
        return result;
    }

    void update_streaming_agg_state(const RmRecord* rec, 
                                   std::vector<int>& counts,
                                   std::vector<int>& int_sums,
                                   std::vector<float>& float_sums,
                                   std::vector<Value>& min_vals,
                                   std::vector<Value>& max_vals,
                                   std::vector<bool>& first_record) {
        for (size_t i = 0; i < select_exprs_.size(); i++) {
            auto expr = select_exprs_[i];
            if (auto alias = std::dynamic_pointer_cast<ast::AliasExpr>(expr)) {
                expr = alias->expr;
            }
            
            if (auto agg = std::dynamic_pointer_cast<ast::AggExpr>(expr)) {
                switch (agg->agg_type) {
                    case ast::AGG_COUNT:
                        if (!agg->arg) {
                            counts[i]++; // COUNT(*)
                        } else {
                            // COUNT(column) - 直接检查NULL，不创建Value对象
                            if (auto col = std::dynamic_pointer_cast<ast::Col>(agg->arg)) {
                                auto cols = child_->cols();
                                int idx = get_col_idx(cols, col->tab_name, col->col_name);
                                if (!is_null_direct(rec->data + cols[idx].offset, cols[idx].type, cols[idx].len)) {
                                    counts[i]++;
                                }
                            }
                        }
                        break;
                        
                    case ast::AGG_SUM:
                    case ast::AGG_AVG:
                        if (auto col = std::dynamic_pointer_cast<ast::Col>(agg->arg)) {
                            auto cols = child_->cols();
                            int idx = get_col_idx(cols, col->tab_name, col->col_name);
                            const char* data_ptr = rec->data + cols[idx].offset;
                            
                            if (!is_null_direct(data_ptr, cols[idx].type, cols[idx].len)) {
                                counts[i]++;
                                if (cols[idx].type == ColType::TYPE_INT) {
                                    int_sums[i] += *(int*)data_ptr;  // 直接读取INT值
                                } else if (cols[idx].type == ColType::TYPE_FLOAT) {
                                    float_sums[i] += *(float*)data_ptr;  // 直接读取FLOAT值
                                }
                            }
                        }
                        break;
                        
                    case ast::AGG_MIN:
                    case ast::AGG_MAX:
                        if (auto col = std::dynamic_pointer_cast<ast::Col>(agg->arg)) {
                            auto cols = child_->cols();
                            int idx = get_col_idx(cols, col->tab_name, col->col_name);
                            const char* data_ptr = rec->data + cols[idx].offset;
                            
                            if (!is_null_direct(data_ptr, cols[idx].type, cols[idx].len)) {
                                if (first_record[i]) {
                                    // 第一次需要创建Value对象来存储
                                    min_vals[i] = Value::from_raw(data_ptr, cols[idx].type, cols[idx].len);
                                    max_vals[i] = min_vals[i];
                                    first_record[i] = false;
                                } else {
                                    // 后续比较时直接比较内存，避免创建Value
                                    if (compare_values_direct(data_ptr, cols[idx].type, cols[idx].len,
                                                            agg->agg_type == ast::AGG_MIN ? min_vals[i] : max_vals[i],
                                                            agg->agg_type)) {  // 传入聚合类型
                                        if (agg->agg_type == ast::AGG_MIN) {
                                            min_vals[i] = Value::from_raw(data_ptr, cols[idx].type, cols[idx].len);
                                        } else {
                                            max_vals[i] = Value::from_raw(data_ptr, cols[idx].type, cols[idx].len);
                                        }
                                    }
                                }
                            }
                        }
                        break;
                }
            }
        }
    }

    void compute_final_results(AggResult& result,
                               const std::vector<int>& counts,
                               const std::vector<int>& int_sums,
                               const std::vector<float>& float_sums,
                               const std::vector<Value>& min_vals,
                               const std::vector<Value>& max_vals,
                               bool has_records) {
        for (size_t i = 0; i < select_exprs_.size(); i++) {
            auto expr = select_exprs_[i];
            if (auto alias = std::dynamic_pointer_cast<ast::AliasExpr>(expr)) {
                expr = alias->expr;
            }
            
            if (auto agg = std::dynamic_pointer_cast<ast::AggExpr>(expr)) {
                switch (agg->agg_type) {
                    case ast::AGG_COUNT:
                        result.values[i].set_int(counts[i]);
                        break;
                    case ast::AGG_SUM: {
                        // 根据原始列类型决定SUM结果类型
                        if (auto col = std::dynamic_pointer_cast<ast::Col>(agg->arg)) {
                            auto cols = child_->cols();
                            int idx = get_col_idx(cols, col->tab_name, col->col_name);
                            if (cols[idx].type == ColType::TYPE_INT) {
                                result.values[i].set_int(int_sums[i]);  // INT列 → INT结果
                            } else if (cols[idx].type == ColType::TYPE_FLOAT) {
                                result.values[i].set_float(float_sums[i]);  // FLOAT列 → FLOAT结果
                            }
                        }
                        break;
                    }
                    case ast::AGG_AVG: {
                        // AVG始终返回FLOAT
                        if (auto col = std::dynamic_pointer_cast<ast::Col>(agg->arg)) {
                            auto cols = child_->cols();
                            int idx = get_col_idx(cols, col->tab_name, col->col_name);
                            if (cols[idx].type == ColType::TYPE_INT) {
                                result.values[i].set_float(counts[i] > 0 ? static_cast<float>(int_sums[i]) / counts[i] : 0.0f);
                            } else if (cols[idx].type == ColType::TYPE_FLOAT) {
                                result.values[i].set_float(counts[i] > 0 ? float_sums[i] / counts[i] : 0.0f);
                            }
                        }
                        break;
                    }
                    case ast::AGG_MIN:
                        result.values[i] = min_vals[i];
                        break;
                    case ast::AGG_MAX:
                        result.values[i] = max_vals[i];
                        break;
                }
            }
        }
    }

    bool check_has_count() {
        for (const auto& expr : select_exprs_) {
            auto actual_expr = expr;
            if (auto alias = std::dynamic_pointer_cast<ast::AliasExpr>(expr)) {
                actual_expr = alias->expr;
            }
            if (auto agg = std::dynamic_pointer_cast<ast::AggExpr>(actual_expr)) {
                if (agg->agg_type == ast::AGG_COUNT ) {
                    return true;
                }
            }
        }
        return false;
    }

    AggResult compute_aggregation_for_group(const std::vector<std::unique_ptr<RmRecord>>& records) {
        AggResult result(select_exprs_.size());
        
        for (size_t i = 0; i < select_exprs_.size(); i++) {
            auto expr = select_exprs_[i];
            
            // 处理别名表达式
            if (auto alias = std::dynamic_pointer_cast<ast::AliasExpr>(expr)) {
                expr = alias->expr;
            }
            
            if (auto agg = std::dynamic_pointer_cast<ast::AggExpr>(expr)) {
                result.values[i] = compute_single_aggregation(agg, records);
            } else if (auto col = std::dynamic_pointer_cast<ast::Col>(expr)) {
                // 非聚合列（必须在GROUP BY中）
                if (!records.empty()) {
                    auto cols = child_->cols();
                    int idx = get_col_idx(cols, col->tab_name, col->col_name);
                    result.values[i] = Value::from_raw(records[0]->data + cols[idx].offset,
                                                     cols[idx].type, cols[idx].len);
                }
            } 
        }
        
        return result;
    }
    
    
    Value compute_single_aggregation(const std::shared_ptr<ast::AggExpr>& agg, 
                                   const std::vector<std::unique_ptr<RmRecord>>& records) {
        Value result;
        
        switch (agg->agg_type) {
            case ast::AGG_COUNT:
                if (!agg->arg) {
                    // COUNT(*)
                    result.set_int(records.size());
                } else {
                    // COUNT(column) - 计算非NULL值
                    int count = 0;
                    if (auto col = std::dynamic_pointer_cast<ast::Col>(agg->arg)) {
                        auto cols = child_->cols();
                        int idx = get_col_idx(cols, col->tab_name, col->col_name);
                        for (const auto& rec : records) {
                            Value v = Value::from_raw(rec->data + cols[idx].offset,
                                                    cols[idx].type, cols[idx].len);
                            if (!v.is_null()) {
                                count++;
                            }
                        }
                    }
                    result.set_int(count);
                }
                break;
                
            case ast::AGG_SUM: {
                // 如果没有记录，SUM应该返回NULL（这里用0表示，但在空结果集情况下不会被使用）
                if (records.empty()) {
                    result.set_int(0);
                    break;
                }

                if (auto col = std::dynamic_pointer_cast<ast::Col>(agg->arg)) {
                    auto cols = child_->cols();
                    int idx = get_col_idx(cols, col->tab_name, col->col_name);
                    
                    if (cols[idx].type == ColType::TYPE_INT) {
                        // INT列的SUM结果为INT
                        int sum = 0;
                        for (const auto& rec : records) {
                            Value v = Value::from_raw(rec->data + cols[idx].offset,
                                                    cols[idx].type, cols[idx].len);
                            if (!v.is_null()) {
                                sum += v.int_val;
                            }
                        }
                        result.set_int(sum);
                    } else if (cols[idx].type == ColType::TYPE_FLOAT) {
                        // FLOAT列的SUM结果为FLOAT
                        float sum = 0.0f;
                        for (const auto& rec : records) {
                            Value v = Value::from_raw(rec->data + cols[idx].offset,
                                                    cols[idx].type, cols[idx].len);
                            if (!v.is_null()) {
                                sum += v.float_val;
                            }
                        }
                        result.set_float(sum);
                    }
                }
                break;
            }
            
            case ast::AGG_AVG: {
                // 如果没有记录，AVG应该返回NULL（这里用0表示，但在空结果集情况下不会被使用）
                if (records.empty()) {
                    result.set_float(0.0f);
                    break;
                }

                double sum = 0.0;
                int count = 0;

                if (auto col = std::dynamic_pointer_cast<ast::Col>(agg->arg)) {
                    auto cols = child_->cols();
                    int idx = get_col_idx(cols, col->tab_name, col->col_name);

                    for (const auto& rec : records) {
                        Value v = Value::from_raw(rec->data + cols[idx].offset,
                                                cols[idx].type, cols[idx].len);
                        if (!v.is_null()) {
                            if (v.type == ColType::TYPE_INT) {
                                sum += v.int_val;
                            } else if (v.type == ColType::TYPE_FLOAT) {
                                sum += v.float_val;
                            }
                            count++;
                        }
                    }
                }

                // AVG 结果为 FLOAT，避免除零
                result.set_float(count > 0 ? static_cast<float>(sum / count) : 0.0f);
                break;
            }
            
            case ast::AGG_MIN:
            case ast::AGG_MAX: {
                // 如果没有记录，MIN/MAX应该返回NULL（这里用默认值表示，但在空结果集情况下不会被使用）
                if (records.empty()) {
                    result.set_int(0);  // 默认值，实际不会被使用
                    break;
                }

                if (auto col = std::dynamic_pointer_cast<ast::Col>(agg->arg)) {
                    auto cols = child_->cols();
                    int idx = get_col_idx(cols, col->tab_name, col->col_name);

                    bool found_valid = false;
                    Value min_max_val;

                    for (const auto& rec : records) {
                        Value v = Value::from_raw(rec->data + cols[idx].offset,
                                                cols[idx].type, cols[idx].len);
                        if (!v.is_null()) {
                            if (!found_valid) {
                                min_max_val = v;
                                found_valid = true;
                            } else {
                                bool should_update = false;
                                if (agg->agg_type == ast::AGG_MIN) {
                                    should_update = (v < min_max_val);
                                } else { // AGG_MAX
                                    should_update = (v > min_max_val);
                                }

                                if (should_update) {
                                    min_max_val = v;
                                }
                            }
                        }
                    }

                    if (found_valid) {
                        result = min_max_val;
                    } else {
                        // 没有找到有效值，根据列类型设置默认值
                        if (cols[idx].type == ColType::TYPE_INT) {
                            result.set_int(0);
                        } else if (cols[idx].type == ColType::TYPE_FLOAT) {
                            result.set_float(0.0f);
                        } else {
                            result.set_str("");
                        }
                    }
                } else {
                    // 没有参数，设置默认值
                    result.set_int(0);
                }
                break;
            }
        }
        
        return result;
    }
    
    void build_output_columns() {
        output_cols_.clear();
        
        for (size_t i = 0; i < select_exprs_.size(); i++) {
            ColMeta col_meta;
            col_meta.tab_name = "";
            col_meta.index = false;
            
            auto expr = select_exprs_[i];
            std::string col_name = "col_" + std::to_string(i);
            
            // 处理别名表达式
            if (auto alias = std::dynamic_pointer_cast<ast::AliasExpr>(expr)) {
                col_name = alias->alias;
                expr = alias->expr;
            }
            
            // 根据表达式类型设置列信息
            if (auto agg = std::dynamic_pointer_cast<ast::AggExpr>(expr)) {
                // 使用新的类型推导函数
                col_meta.type = get_agg_result_type(agg);
                col_meta.len = (col_meta.type == ColType::TYPE_FLOAT) ? sizeof(float) : sizeof(int);
                
                if (col_name == "col_" + std::to_string(i)) {
                    switch (agg->agg_type) {
                        case ast::AGG_COUNT: col_name = "COUNT(*)"; break;
                        case ast::AGG_SUM: col_name = "SUM"; break;
                        case ast::AGG_AVG: col_name = "AVG"; break;
                        case ast::AGG_MIN: col_name = "MIN"; break;
                        case ast::AGG_MAX: col_name = "MAX"; break;
                    }
                }
            } else if (auto col = std::dynamic_pointer_cast<ast::Col>(expr)) {
                // 从子执行器获取列类型
                auto child_cols = child_->cols();
                int idx = get_col_idx(child_cols, col->tab_name, col->col_name);
                col_meta.type = child_cols[idx].type;
                col_meta.len = child_cols[idx].len;
                
                if (col_name == "col_" + std::to_string(i)) {
                    col_name = col->col_name;
                }
            }
            
            col_meta.name = col_name;
            col_meta.offset = (i == 0) ? 0 : output_cols_[i-1].offset + output_cols_[i-1].len;
            output_cols_.push_back(col_meta);
        }
    }
private:  
    // 添加缺失的辅助函数
    int get_col_idx(const std::vector<ColMeta>& cols, const std::string& tab_name, const std::string& col_name) {
        for (size_t i = 0; i < cols.size(); i++) {
            if (cols[i].name == col_name && (tab_name.empty() || cols[i].tab_name == tab_name)) {
                return i;
            }
        }

        throw std::runtime_error("Column not found: " + col_name);
    }
  
    // 获取聚合函数参数的类型
    ColType get_agg_result_type(const std::shared_ptr<ast::AggExpr>& agg) {
        switch (agg->agg_type) {
            case ast::AGG_COUNT:
                return ColType::TYPE_INT;
            case ast::AGG_AVG:
                return ColType::TYPE_FLOAT;
            case ast::AGG_SUM:
                if (agg->arg) {
                    if (auto col = std::dynamic_pointer_cast<ast::Col>(agg->arg)) {
                        auto cols = child_->cols();
                        int idx = get_col_idx(cols, col->tab_name, col->col_name);
                        return cols[idx].type; // 保持原始类型
                    }
                }
                return ColType::TYPE_INT; // 默认
            case ast::AGG_MIN:
            case ast::AGG_MAX:
                if (agg->arg) {
                    if (auto col = std::dynamic_pointer_cast<ast::Col>(agg->arg)) {
                        auto cols = child_->cols();
                        int idx = get_col_idx(cols, col->tab_name, col->col_name);
                        return cols[idx].type;
                    }
                }
                return ColType::TYPE_INT; // 默认
        }
        return ColType::TYPE_INT;
    }
private:
    // 检查是否为简单的COUNT(*)查询
    bool is_simple_count_star_query() const {
        if (select_exprs_.size() != 1) return false;
        
        auto expr = select_exprs_[0];
        if (auto alias = std::dynamic_pointer_cast<ast::AliasExpr>(expr)) {
            expr = alias->expr;
        }
        
        auto agg = std::dynamic_pointer_cast<ast::AggExpr>(expr);
        return agg && agg->agg_type == ast::AGG_COUNT && !agg->arg;
    }
    
    // 检查是否可以使用缓存的记录数
    bool can_use_cached_count() const {
        // 检查顺序扫描执行器是否无条件
        if (auto seq_scan = dynamic_cast<SeqScanExecutor*>(child_.get())) {
            return seq_scan->has_no_conditions();
        }
        return false;
    }
    
    // 获取缓存的记录总数
    int get_cached_record_count() const {
        if (auto seq_scan = dynamic_cast<SeqScanExecutor*>(child_.get())) {
            auto fh = seq_scan->get_file_handle();
            auto file_hdr = fh->get_file_hdr();
            if (file_hdr.count_cache_valid) {
                // std::cout<<"get_cached_record"<<std::endl;
                return file_hdr.total_record_count;
            }
        }
        return -1;
    }
private:
    // 直接检查NULL，不创建Value对象
    inline bool is_null_direct(const char* data, ColType type, int len) const {
        switch (type) {
            case ColType::TYPE_INT:
                return false; // 简化实现，假设INT不为NULL
                
            case ColType::TYPE_FLOAT:
                return false; // 简化实现
                
            case ColType::TYPE_STRING:
                // 字符串NULL通常表示为空字符串或全零
                return len > 0 && data[0] == '\0';
                
            default:
                return false;
        }
    }
    
    // 直接比较值，用于MIN/MAX，返回true表示需要更新
    inline bool compare_values_direct(const char* data, ColType type, int len, 
                                     const Value& current_val, ast::AggType agg_type) const {
        switch (type) {
            case ColType::TYPE_INT: {
                int new_val = *(int*)data;
                if (agg_type == ast::AGG_MIN) {
                    return new_val < current_val.int_val;  // MIN: 新值更小时更新
                } else { // AGG_MAX
                    return new_val > current_val.int_val;  // MAX: 新值更大时更新
                }
            }
            case ColType::TYPE_FLOAT: {
                float new_val = *(float*)data;
                if (agg_type == ast::AGG_MIN) {
                    return new_val < current_val.float_val;
                } else { // AGG_MAX
                    return new_val > current_val.float_val;
                }
            }
            case ColType::TYPE_STRING: {
                std::string new_str(data, len);
                size_t actual_len = strnlen(data, len);
                new_str.resize(actual_len);
                if (agg_type == ast::AGG_MIN) {
                    return new_str < current_val.str_val;
                } else { // AGG_MAX
                    return new_str > current_val.str_val;
                }
            }
            default:
                return false;
        }
    }
private:
    // 单个COUNT的极速实现
    AggResult compute_single_count_aggregation() {
        // std::cout<<"single_cout_optimized"<<std::endl;
        auto expr = select_exprs_[0];
        if (auto alias = std::dynamic_pointer_cast<ast::AliasExpr>(expr)) {
            expr = alias->expr;
        }
        
        auto agg = std::dynamic_pointer_cast<ast::AggExpr>(expr);
        int count = 0;
        
        if (agg && agg->agg_type == ast::AGG_COUNT) {
            if (!agg->arg) {
                // COUNT(*) - 最简单的计数，不需要获取记录内容
                while (!child_->is_end()) {
                    count++;
                    child_->nextTuple();
                }
            } else {
                // COUNT(column) - 需要检查NULL
                if (auto col = std::dynamic_pointer_cast<ast::Col>(agg->arg)) {
                    auto cols = child_->cols();
                    int idx = get_col_idx(cols, col->tab_name, col->col_name);
                    while (!child_->is_end()) {
                        auto rec = child_->Next();
                        if (!is_null_direct(rec->data + cols[idx].offset, cols[idx].type, cols[idx].len)) {
                            count++;
                        }
                        child_->nextTuple();
                    }
                }
            }
        }
        
        AggResult result(1);
        result.values[0].set_int(count);
        return result;
    }
private:
    // 尝试通过索引直接获取MIN/MAX值
    bool try_index_min_max_optimization(const std::shared_ptr<ast::AggExpr>& agg, Value& result) {
        // 检查是否是IndexScanExecutor且只有一个聚合函数
        auto index_scan = dynamic_cast<IndexScanExecutor*>(child_.get());
        if (!index_scan || select_exprs_.size() != 1) {
            // std::cout<<"!index_scan"<<std::endl;
            return false;
        }
        
        // 检查聚合函数是否是MIN/MAX且参数是索引的最后一列
        if (agg->agg_type != ast::AGG_MIN && agg->agg_type != ast::AGG_MAX) {
            return false;
        }
        
        auto col = std::dynamic_pointer_cast<ast::Col>(agg->arg);
        if (!col) return false;
        
        // 获取索引的列信息，检查聚合列是否是索引的最后一列
     
            // 从IndexScanExecutor获取索引列名
        const auto& index_col_names = index_scan->get_index_col_names();
        if (index_col_names.empty()) {
            return false;
        }
            
            // 检查聚合列是否是索引的最后一列
        const std::string& last_index_col = index_col_names.back();
        if (col->col_name != last_index_col) {
            return false;
        }
            
        
        if (agg->agg_type == ast::AGG_MIN) {
            // 直接从IndexScan的第一个记录获取值
            if (!child_->is_end()) {
                auto rec = child_->Next();
                auto cols = child_->cols();
                int idx = get_col_idx(cols, col->tab_name, col->col_name);
                result = Value::from_raw(rec->data + cols[idx].offset, 
                                       cols[idx].type, cols[idx].len);
                return true;
            }
        } else { // AGG_MAX
            // 需要扫描到最后一个记录
            Value max_val;
            bool found = false;
            while (!child_->is_end()) {
                auto rec = child_->Next();
                auto cols = child_->cols();
                int idx = get_col_idx(cols, col->tab_name, col->col_name);
                max_val = Value::from_raw(rec->data + cols[idx].offset,
                                        cols[idx].type, cols[idx].len);
                found = true;
                child_->nextTuple();
            }
            if (found) {
                result = max_val;
                return true;
            }
        }
        
        return false;
    }
};
