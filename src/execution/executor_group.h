#pragma once

#include "execution_defs.h"
#include "execution_manager.h"
#include "mvcc_executor_base.h"
#include "index/ix.h"
#include "system/sm.h"
#include "parser/ast.h"
#include "executor_aggregation_optimized.h"

/**
 * @brief 分组执行器
 * 
 * 负责GROUP BY和HAVING的处理
 * 输出按组分组的数据，每组输出一个代表记录
 */
class GroupExecutor : public MVCCExecutorBase
{
private:
    std::unique_ptr<AbstractExecutor> child_;
    std::vector<std::shared_ptr<ast::Expr>> group_by_;
    std::shared_ptr<ast::Expr> having_;
    
    struct GroupKey {
        std::vector<Value> keys;
        
        bool operator==(const GroupKey& other) const {
            if (keys.size() != other.keys.size()) return false;
            for (size_t i = 0; i < keys.size(); i++) {
                if (!(keys[i] == other.keys[i])) return false;
            }
            return true;
        }
    };
    
    struct GroupKeyHash {
        size_t operator()(const GroupKey& key) const {
            size_t hash = 0;
            for (const auto& val : key.keys) {
                hash ^= std::hash<std::string>{}(val.to_string()) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            }
            return hash;
        }
    };
    
    struct GroupData {
        std::vector<std::unique_ptr<RmRecord>> records;  // 该组的所有记录
        std::unique_ptr<RmRecord> representative;        // 代表记录（用于输出）
        
        GroupData() = default;
        GroupData(const GroupData&) = delete;
        GroupData& operator=(const GroupData&) = delete;
        GroupData(GroupData&&) = default;
        GroupData& operator=(GroupData&&) = default;
    };
    
    std::unordered_map<GroupKey, GroupData, GroupKeyHash> groups_;
    std::unordered_map<GroupKey, GroupData, GroupKeyHash>::iterator current_group_;
    std::vector<ColMeta> cols_;
    
public:
    GroupExecutor(std::unique_ptr<AbstractExecutor> child,
                  const std::vector<std::shared_ptr<ast::Expr>>& group_by,
                  const std::shared_ptr<ast::Expr>& having)
        : child_(std::move(child)), group_by_(group_by), having_(having) {}
    
    void beginTuple() override {
        child_->beginTuple();
        cols_ = child_->cols();
        groups_.clear();
        
        // 收集所有记录并按组分类
        while (!child_->is_end()) {
            auto rec = child_->Next();
            GroupKey key = extract_group_key(rec.get());
            
            if (groups_.find(key) == groups_.end()) {
                groups_[key] = GroupData{};
                // 设置代表记录（第一条记录）
                groups_[key].representative = std::make_unique<RmRecord>(*rec);
            }
            
            // 将记录添加到对应组
            groups_[key].records.push_back(std::make_unique<RmRecord>(*rec));
            child_->nextTuple();
        }
        
        // 处理无分组的情况
        if (group_by_.empty() && groups_.empty()) {
            // 创建默认组
            GroupKey default_key;
            Value default_val;
            default_val.set_int(1);
            default_key.keys.push_back(default_val);
            groups_[default_key] = GroupData{};
        }
        
        // 应用HAVING过滤
        if (having_) {
            filter_groups_by_having();
        }
        
        current_group_ = groups_.begin();
    }
    
    void nextTuple() override {
        if (current_group_ != groups_.end()) {
            ++current_group_;
        }
    }
    
    bool is_end() const override {
        return current_group_ == groups_.end();
    }
    
    std::unique_ptr<RmRecord> Next() override {
        if (is_end()) return nullptr;
        
        // 返回当前组的代表记录
        return std::make_unique<RmRecord>(*current_group_->second.representative);
    }
    
    const std::vector<ColMeta>& cols() const override {
        // GroupExecutor应该返回子执行器的列信息
        if (child_) {
            return child_->cols();
        }
        static std::vector<ColMeta> empty_cols;
        return empty_cols;
    }
    
    size_t tupleLen() const override {
        return child_->tupleLen();
    }
    
    Rid& rid() override {
        return _abstract_rid;
    }
    
    // 获取当前组的所有记录（供聚合算子使用）
    const std::vector<std::unique_ptr<RmRecord>>& getCurrentGroupRecords() const {
        if (is_end()) {
            static std::vector<std::unique_ptr<RmRecord>> empty;
            return empty;
        }
        return current_group_->second.records;
    }

private:
    int get_col_idx(const std::vector<ColMeta>& cols, const std::string& tab_name, const std::string& col_name) {
        for (size_t i = 0; i < cols.size(); i++) {
            if (cols[i].name == col_name && (tab_name.empty() || cols[i].tab_name == tab_name)) {
                return i;
            }
        }
        throw std::runtime_error("Column not found: " + col_name);
    }

    GroupKey extract_group_key(const RmRecord* rec) {
        GroupKey key;
        
        if (group_by_.empty()) {
            // 无GROUP BY，所有记录在一个组
            Value default_val;
            default_val.set_int(1);
            key.keys.push_back(default_val);
            return key;
        }
        
        for (auto& expr : group_by_) {
            auto col = std::dynamic_pointer_cast<ast::Col>(expr);
            if (col) {
                int idx = get_col_idx(cols_, col->tab_name, col->col_name);
                Value v = Value::from_raw(rec->data + cols_[idx].offset,
                                        cols_[idx].type, cols_[idx].len);
                key.keys.push_back(v);
            }
        }
        return key;
    }
    
    void filter_groups_by_having() {
        auto it = groups_.begin();
        while (it != groups_.end()) {
            if (!evaluate_having_for_group(it->second)) {
                it = groups_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    bool evaluate_having_for_group(const GroupData& group_data) {
        if (!having_) return true;
        
        // 评估HAVING条件
        return evaluate_having_expr(having_, group_data);
    }
    
    bool evaluate_having_expr(const std::shared_ptr<ast::Expr>& expr, const GroupData& group_data) {
        if (auto logical = std::dynamic_pointer_cast<ast::LogicalExpr>(expr)) {
            // 处理逻辑表达式 (AND/OR)
            bool left_val = evaluate_having_expr(logical->lhs, group_data);
            bool right_val = evaluate_having_expr(logical->rhs, group_data);
            
            switch (logical->op) {
                case ast::LogicalExpr::AND: return left_val && right_val;
                case ast::LogicalExpr::OR: return left_val || right_val;
                default: return true;
            }
        } else if (auto compare = std::dynamic_pointer_cast<ast::CompareExpr>(expr)) {
            // 处理比较表达式 (=, <, >, etc.)
            Value left_val = evaluate_having_value(compare->lhs, group_data);
            Value right_val = evaluate_having_value(compare->rhs, group_data);
            
            // 类型转换：如果类型不同但都是数值类型，转换为浮点数比较
            if (left_val.type != right_val.type) {
                bool left_is_numeric = (left_val.type == ColType::TYPE_INT || left_val.type == ColType::TYPE_FLOAT);
                bool right_is_numeric = (right_val.type == ColType::TYPE_INT || right_val.type == ColType::TYPE_FLOAT);
                
                if (left_is_numeric && right_is_numeric) {
                    // 将 INT 转换为 FLOAT 进行比较
                    if (left_val.type == ColType::TYPE_INT && right_val.type == ColType::TYPE_FLOAT) {
                        left_val.set_float(static_cast<float>(left_val.int_val));
                    } else if (left_val.type == ColType::TYPE_FLOAT && right_val.type == ColType::TYPE_INT) {
                        right_val.set_float(static_cast<float>(right_val.int_val));
                    }
                } else {
                    // 非数值类型不匹配，返回 false
                    return false;
                }
            }
            
            switch (compare->op) {
                case ast::SV_OP_EQ: return left_val == right_val;
                case ast::SV_OP_NE: return !(left_val == right_val);
                case ast::SV_OP_LT: return left_val < right_val;
                case ast::SV_OP_LE: return left_val <= right_val;
                case ast::SV_OP_GT: return left_val > right_val;
                case ast::SV_OP_GE: return left_val >= right_val;
                default: return true;
            }
        }
        return true;
    }
    
    Value evaluate_having_value(const std::shared_ptr<ast::Expr>& expr, const GroupData& group_data) {
        if (auto agg = std::dynamic_pointer_cast<ast::AggExpr>(expr)) {
            return compute_agg_for_having(agg, group_data);
        } else if (auto val = std::dynamic_pointer_cast<ast::Value>(expr)) {
            if (auto int_lit = std::dynamic_pointer_cast<ast::IntLit>(val)) {
                Value result;
                result.set_int(int_lit->val);
                return result;
            } else if (auto float_lit = std::dynamic_pointer_cast<ast::FloatLit>(val)) {
                Value result;
                result.set_float(float_lit->val);
                return result;
            } else if (auto str_lit = std::dynamic_pointer_cast<ast::StringLit>(val)) {
                Value result;
                result.set_str(str_lit->val);
                return result;
            }
        } else if (auto col = std::dynamic_pointer_cast<ast::Col>(expr)) {
            // 对于GROUP BY中的列，返回组键值
            if (!group_data.representative) {
                Value default_val;
                default_val.set_int(0);
                return default_val;
            }
            int idx = get_col_idx(cols_, col->tab_name, col->col_name);
            return Value::from_raw(group_data.representative->data + cols_[idx].offset,
                                 cols_[idx].type, cols_[idx].len);
        }
        
        Value default_val;
        default_val.set_int(0);
        return default_val;
    }
    
    Value compute_agg_for_having(const std::shared_ptr<ast::AggExpr>& agg, const GroupData& group_data) {
        Value result;
        
        switch (agg->agg_type) {
            case ast::AGG_COUNT:
                result.set_int(group_data.records.size());
                break;
            case ast::AGG_SUM: {
                if (auto col = std::dynamic_pointer_cast<ast::Col>(agg->arg)) {
                    int idx = get_col_idx(cols_, col->tab_name, col->col_name);
                    
                    if (cols_[idx].type == ColType::TYPE_INT) {
                        // INT列的SUM结果为INT
                        int sum = 0;
                        for (const auto& rec : group_data.records) {
                            Value v = Value::from_raw(rec->data + cols_[idx].offset,
                                                    cols_[idx].type, cols_[idx].len);
                            if (!v.is_null()) {
                                sum += v.int_val;
                            }
                        }
                        result.set_int(sum);
                    } else if (cols_[idx].type == ColType::TYPE_FLOAT) {
                        // FLOAT列的SUM结果为FLOAT
                        float sum = 0.0f;
                        for (const auto& rec : group_data.records) {
                            Value v = Value::from_raw(rec->data + cols_[idx].offset,
                                                    cols_[idx].type, cols_[idx].len);
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
                double sum = 0.0;
                int count = 0;
                if (auto col = std::dynamic_pointer_cast<ast::Col>(agg->arg)) {
                    int idx = get_col_idx(cols_, col->tab_name, col->col_name);
                    for (const auto& rec : group_data.records) {
                        Value v = Value::from_raw(rec->data + cols_[idx].offset,
                                                cols_[idx].type, cols_[idx].len);
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
                result.set_float(count > 0 ? sum / count : 0.0);
                break;
            }
            case ast::AGG_MIN:
            case ast::AGG_MAX: {
                if (auto col = std::dynamic_pointer_cast<ast::Col>(agg->arg)) {
                    int idx = get_col_idx(cols_, col->tab_name, col->col_name);
                    bool first = true;
                    for (const auto& rec : group_data.records) {
                        Value v = Value::from_raw(rec->data + cols_[idx].offset,
                                                cols_[idx].type, cols_[idx].len);
                        if (!v.is_null()) {
                            if (first) {
                                result = v;
                                first = false;
                            } else {
                                if (agg->agg_type == ast::AGG_MIN && v < result) {
                                    result = v;
                                } else if (agg->agg_type == ast::AGG_MAX && v > result) {
                                    result = v;
                                }
                            }
                        }
                    }
                }
                break;
            }
        }
        
        return result;
    }
};
