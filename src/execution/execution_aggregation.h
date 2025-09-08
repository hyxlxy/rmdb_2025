// #pragma once
// #include "executor_abstract.h"
// #include "parser/ast.h"
// #include "common/common.h"
// #include <unordered_map>
// #include <vector>
// #include <memory>
// #include <iostream>
// #include <stdexcept>
// #include <functional>
// #include "executor_seq_scan.h"

// int get_col_idx(const std::vector<ColMeta> &cols, const std::string &tab_name, const std::string &col_name)
// {
//     // 首先尝试精确匹配（表名+列名）
//     if (!tab_name.empty())
//     {
//         for (size_t i = 0; i < cols.size(); ++i)
//         {
//             if (cols[i].name == col_name && cols[i].tab_name == tab_name)
//             {
//                 return i;
//             }
//         }
//     }

//     // 如果精确匹配失败或没有表名，尝试只匹配列名
//     for (size_t i = 0; i < cols.size(); ++i)
//     {
//         if (cols[i].name == col_name)
//         {
//             return i;
//         }
//     }

//     // 如果列名匹配失败，抛出异常，提供更详细的错误信息
//     std::string full_col_name = tab_name.empty() ? col_name : tab_name + "." + col_name;
//     throw std::runtime_error("Column not found: " + full_col_name);
// }

// // 聚合分组的 key，可以用字符串拼接或 tuple/hash
// struct AggGroupKey
// {
//     std::vector<Value> keys;
//     bool operator==(const AggGroupKey &other) const { return keys == other.keys; }
// };

// namespace std
// {
//     template <>
//     struct hash<AggGroupKey>
//     {
//         size_t operator()(const AggGroupKey &k) const
//         {
//             size_t h = 0;
//             for (const auto &v : k.keys)
//                 h ^= std::hash<std::string>{}(v.to_string());
//             return h;
//         }
//     };
// }

// // 聚合状态
// struct AggState
// {
//     std::vector<Value> agg_vals; // 每个聚合表达式的当前值
//     std::vector<int> counts;     // 用于AVG/COUNT
//     AggState() = default;        // 默认构造函数
//     AggState(size_t n) : agg_vals(n), counts(n, 0) {}
// };

// class AggExecutor : public AbstractExecutor
// {
// public:
//     AggExecutor(std::unique_ptr<AbstractExecutor> child,
//                 const std::vector<std::shared_ptr<ast::Expr>> &select_exprs,
//                 const std::vector<std::shared_ptr<ast::Expr>> &group_by,
//                 const std::shared_ptr<ast::Expr> &having)
//         : child_(std::move(child)), select_exprs_(select_exprs), group_by_(group_by), having_(having)
//     {

//         // 在构造函数中初始化基本的输出列信息
//         output_cols_.clear();
//         for (size_t i = 0; i < select_exprs_.size(); i++)
//         {
//             ColMeta col_meta;
//             col_meta.tab_name = ""; // 聚合后的列没有表名

//             // 根据表达式类型推断输出列类型和名称
//             auto expr = select_exprs_[i];
//             std::string col_name = "col_" + std::to_string(i); // 默认列名

//             // 处理别名表达式
//             if (auto alias = std::dynamic_pointer_cast<ast::AliasExpr>(expr))
//             {
//                 // 使用别名作为列名
//                 col_name = alias->alias;
//                 expr = alias->expr;
//             }
//             else if (auto col = std::dynamic_pointer_cast<ast::Col>(expr))
//             {
//                 // 普通列，使用列名
//                 col_name = col->col_name;
//             }
//             else if (auto agg = std::dynamic_pointer_cast<ast::AggExpr>(expr))
//             {
//                 // 聚合函数，生成描述性名称
//                 switch (agg->agg_type)
//                 {
//                 case ast::AGG_COUNT:
//                     col_name = "COUNT(*)";
//                     break;
//                 case ast::AGG_SUM:
//                     if (agg->arg)
//                     {
//                         if (auto arg_col = std::dynamic_pointer_cast<ast::Col>(agg->arg))
//                         {
//                             col_name = "SUM(" + arg_col->col_name + ")";
//                         }
//                     }
//                     break;
//                 case ast::AGG_MIN:
//                     if (agg->arg)
//                     {
//                         if (auto arg_col = std::dynamic_pointer_cast<ast::Col>(agg->arg))
//                         {
//                             col_name = "MIN(" + arg_col->col_name + ")";
//                         }
//                     }
//                     break;
//                 case ast::AGG_MAX:
//                     if (agg->arg)
//                     {
//                         if (auto arg_col = std::dynamic_pointer_cast<ast::Col>(agg->arg))
//                         {
//                             col_name = "MAX(" + arg_col->col_name + ")";
//                         }
//                     }
//                     break;
//                 case ast::AGG_AVG:
//                     if (agg->arg)
//                     {
//                         if (auto arg_col = std::dynamic_pointer_cast<ast::Col>(agg->arg))
//                         {
//                             col_name = "AVG(" + arg_col->col_name + ")";
//                         }
//                     }
//                     break;
//                 }
//             }
//             col_meta.name = col_name;

//             // 根据聚合函数类型设置输出列类型
//             if (auto agg = std::dynamic_pointer_cast<ast::AggExpr>(expr))
//             {
//                 switch (agg->agg_type)
//                 {
//                 case ast::AGG_COUNT:
//                     col_meta.type = TYPE_INT;
//                     col_meta.len = sizeof(int);
//                     break;
//                 case ast::AGG_AVG:
//                     col_meta.type = TYPE_FLOAT; // AVG 总是返回浮点数
//                     col_meta.len = sizeof(float);
//                     break;
//                 case ast::AGG_SUM:
//                 case ast::AGG_MIN:
//                 case ast::AGG_MAX:
//                     // 根据参数类型确定
//                     if (agg->arg)
//                     {
//                         auto col = std::dynamic_pointer_cast<ast::Col>(agg->arg);
//                         if (col)
//                         {
//                             // 获取子执行器的列元数据
//                             try
//                             {
//                                 auto child_cols = child_->cols();
//                                 for (const auto &child_col : child_cols)
//                                 {
//                                     if (child_col.name == col->col_name &&
//                                         (col->tab_name.empty() || child_col.tab_name == col->tab_name))
//                                     {
//                                         col_meta.type = child_col.type;
//                                         col_meta.len = child_col.len;
//                                         break;
//                                     }
//                                 }
//                             }
//                             catch (const std::exception &e)
//                             {
//                                 // 如果获取子执行器列信息失败，使用默认类型
//                                 // 这在构造函数阶段是可能的
//                             }
//                         }
//                     }
//                     // 默认类型
//                     if (col_meta.len == 0)
//                     {
//                         col_meta.type = TYPE_INT;
//                         col_meta.len = sizeof(int);
//                     }
//                     break;
//                 default:
//                     col_meta.type = TYPE_INT;
//                     col_meta.len = sizeof(int);
//                     break;
//                 }
//             }
//             else if (auto col = std::dynamic_pointer_cast<ast::Col>(expr))
//             {
//                 // 普通列，获取实际的列类型
//                 try
//                 {
//                     auto child_cols = child_->cols();
//                     bool found = false;
//                     for (const auto &child_col : child_cols)
//                     {
//                         if (child_col.name == col->col_name &&
//                             (col->tab_name.empty() || child_col.tab_name == col->tab_name))
//                         {
//                             col_meta.type = child_col.type;
//                             col_meta.len = child_col.len;
//                             found = true;
//                             break;
//                         }
//                     }
//                     if (!found)
//                     {
//                         // 如果没找到，默认为 INT
//                         col_meta.type = TYPE_INT;
//                         col_meta.len = sizeof(int);
//                     }
//                 }
//                 catch (const std::exception &e)
//                 {
//                     // 如果获取子执行器列信息失败，使用默认类型
//                     col_meta.type = TYPE_INT;
//                     col_meta.len = sizeof(int);
//                 }
//             }
//             else
//             {
//                 // 其他表达式，默认为 INT
//                 col_meta.type = TYPE_INT;
//                 col_meta.len = sizeof(int);
//             }

//             // 计算偏移量
//             if (i == 0)
//             {
//                 col_meta.offset = 0;
//             }
//             else
//             {
//                 col_meta.offset = output_cols_[i - 1].offset + output_cols_[i - 1].len;
//             }
//             col_meta.index = false;
//             output_cols_.push_back(col_meta);
//         }
//     }

//     void beginTuple() override
//     {  
//         // 快速路径1：简单 COUNT(*) 查询优化
//         if (is_simple_count_query()) {
            
//             execute_count_optimization();
//             return;
//         }
//         // 普通聚合路径
//         execute_normal_aggregation();
//     }

//     void nextTuple() override
//     {
//         if (output_iter_ == group_map_.end())
//             return;
//         ++output_iter_;
//         find_next_valid();
//     }

//     bool is_end() const override
//     {
//         return output_iter_ == group_map_.end();
//     }

//     std::unique_ptr<RmRecord> Next() override
//     {
//         if (is_end())
//             return nullptr;

 
//         // 构造输出记录
//         auto &state = output_iter_->second;

//         // 计算记录总长度
//         size_t len = 0;
//         for (const auto &col : output_cols_)
//         {
//             len += col.len;
//         }

//         auto rec = std::make_unique<RmRecord>(len);

//         // 填充记录数据
//         for (size_t i = 0; i < select_exprs_.size(); i++)
//         {
//             Value final_val = state.agg_vals[i];
//             const auto &col = output_cols_[i];

//             // 处理 AVG 的最终计算
//             auto expr = select_exprs_[i];
//             if (auto alias = std::dynamic_pointer_cast<ast::AliasExpr>(expr))
//             {
//                 expr = alias->expr;
//             }
//             if (auto agg = std::dynamic_pointer_cast<ast::AggExpr>(expr))
//             {
//                 if (agg->agg_type == ast::AGG_AVG && state.counts[i] > 0)
//                 {
//                     // 计算平均值
//                     if (final_val.type == TYPE_INT)
//                     {
//                         final_val.set_float(static_cast<float>(final_val.int_val) / state.counts[i]);
//                     }
//                     else if (final_val.type == TYPE_FLOAT)
//                     {
//                         final_val.set_float(final_val.float_val / state.counts[i]);
//                     }
//                 }
//             }

//             // 检查类型是否匹配，如果不匹配则进行转换
//             if (final_val.type != col.type)
//             {
//                 // 类型转换
//                 if (final_val.type == TYPE_INT && col.type == TYPE_FLOAT)
//                 {
//                     float float_val = static_cast<float>(final_val.int_val);
//                     memcpy(rec->data + col.offset, &float_val, sizeof(float));
//                 }
//                 else if (final_val.type == TYPE_FLOAT && col.type == TYPE_INT)
//                 {
//                     int int_val = static_cast<int>(final_val.float_val);
//                     memcpy(rec->data + col.offset, &int_val, sizeof(int));
//                 }
//                 else
//                 {
//                     // 其他类型转换...
//                 }
//             }
//             else
//             {
//                 // 类型匹配，直接复制
//                 switch (final_val.type)
//                 {
//                 case TYPE_INT:
//                     memcpy(rec->data + col.offset, &final_val.int_val, sizeof(int));
//                     break;
//                 case TYPE_FLOAT:
//                     memcpy(rec->data + col.offset, &final_val.float_val, sizeof(float));
//                     break;
//                 case TYPE_STRING:
//                     memcpy(rec->data + col.offset, final_val.str_val.c_str(), final_val.str_val.length() + 1);
//                     break;
//                 default:
//                     break;
//                 }
//             }
//         }

//         return rec;
//     }

//     const std::vector<ColMeta> &cols() const override
//     {
//         return output_cols_;
//     }

//     std::string getType() override { return "AggExecutor"; }

//     Rid &rid() override { return _abstract_rid; }

// private:
//     std::unique_ptr<AbstractExecutor> child_;
//     std::vector<std::shared_ptr<ast::Expr>> select_exprs_;
//     std::vector<std::shared_ptr<ast::Expr>> group_by_;
//     std::shared_ptr<ast::Expr> having_;
//     std::unordered_map<AggGroupKey, AggState> group_map_;
//     decltype(group_map_.begin()) output_iter_;
//     std::vector<ColMeta> cols_;        // 子执行器的列元数据
//     std::vector<ColMeta> output_cols_; // 输出的列元数据

//     AggGroupKey extract_group_key(const RmRecord *rec)
//     {
//         AggGroupKey key;
//         // 如果没有GROUP BY，所有记录都在一个组
//         if (group_by_.empty())
//         {
//             Value default_key;
//             default_key.set_int(1); // 使用常量1作为默认分组键
//             key.keys.push_back(default_key);
//             return key;
//         }

//         for (auto &expr : group_by_)
//         {
//             // 目前只支持列作为分组键
//             auto col = std::dynamic_pointer_cast<ast::Col>(expr);
//             if (!col)
//             {
//                 throw std::runtime_error("Only column expressions are supported in GROUP BY");
//             }

//             int idx = get_col_idx(cols_, col->tab_name, col->col_name);
//             Value v = Value::from_raw(rec->data + cols_[idx].offset, cols_[idx].type, cols_[idx].len);
//             key.keys.push_back(v);
//         }
//         return key;
//     }

//     // 这个方法已经不需要了，因为在 beginTuple 中已经处理了所有逻辑

//     void update_agg_state(AggState &state, const RmRecord *rec)
//     {
//         for (size_t i = 0; i < select_exprs_.size(); ++i)
//         {
//             auto expr = select_exprs_[i];

//             // 处理别名表达式
//             if (auto alias = std::dynamic_pointer_cast<ast::AliasExpr>(expr))
//             {
//                 expr = alias->expr;
//             }

//             // 处理聚合表达式
//             if (auto agg = std::dynamic_pointer_cast<ast::AggExpr>(expr))
//             {
//                 // 获取聚合函数的参数值
//                 Value arg_val;
//                 if (agg->arg)
//                 {
//                     auto col = std::dynamic_pointer_cast<ast::Col>(agg->arg);
//                     if (!col)
//                     {
//                         throw std::runtime_error("Only column expressions are supported in aggregate functions");
//                     }

//                     int idx = get_col_idx(cols_, col->tab_name, col->col_name);
//                     arg_val = Value::from_raw(rec->data + cols_[idx].offset, cols_[idx].type, cols_[idx].len);
//                 }

//                 // 根据聚合类型更新状态
//                 switch (agg->agg_type)
//                 {
//                 case ast::AGG_COUNT:
//                     if (!agg->arg || !arg_val.is_null())
//                     {
//                         // COUNT(*) 或 COUNT(col) 且值不为NULL
//                         state.agg_vals[i].set_int(state.agg_vals[i].int_val + 1);
//                         state.counts[i]++; // 更新计数
//                     }
//                     break;

//                 case ast::AGG_SUM:
//                     if (!arg_val.is_null())
//                     {
//                         if (state.counts[i] == 0)
//                         {
//                             // 第一次更新，直接使用参数值
//                             state.agg_vals[i] = arg_val;
//                         }
//                         else
//                         {
//                             // 后续更新，需要处理类型转换
//                             if (state.agg_vals[i].type == TYPE_INT && arg_val.type == TYPE_INT)
//                             {
//                                 // 检查是否会溢出
//                                 long long result = static_cast<long long>(state.agg_vals[i].int_val) + static_cast<long long>(arg_val.int_val);
//                                 if (result > 2147483647 || result < -2147483647)
//                                 {
//                                     // 只有在溢出时才转换为浮点数
//                                     state.agg_vals[i].set_float(static_cast<double>(state.agg_vals[i].int_val) + static_cast<double>(arg_val.int_val));
//                                 }
//                                 else
//                                 {
//                                     // 否则保持整数类型
//                                     state.agg_vals[i].set_int(static_cast<int>(result));
//                                 }
//                             }
//                             else if (state.agg_vals[i].type == TYPE_FLOAT && arg_val.type == TYPE_FLOAT)
//                             {
//                                 state.agg_vals[i].set_float(state.agg_vals[i].float_val + arg_val.float_val);
//                             }
//                             else if (state.agg_vals[i].type == TYPE_INT && arg_val.type == TYPE_FLOAT)
//                             {
//                                 // INT + FLOAT = FLOAT
//                                 state.agg_vals[i].set_float(static_cast<double>(state.agg_vals[i].int_val) + arg_val.float_val);
//                             }
//                             else if (state.agg_vals[i].type == TYPE_FLOAT && arg_val.type == TYPE_INT)
//                             {
//                                 // FLOAT + INT = FLOAT
//                                 state.agg_vals[i].set_float(state.agg_vals[i].float_val + static_cast<double>(arg_val.int_val));
//                             }
//                         }
//                         state.counts[i]++;
//                     }
//                     break;

//                 case ast::AGG_AVG:
//                     if (!arg_val.is_null())
//                     {
//                         if (state.counts[i] == 0)
//                         {
//                             // 第一次更新，直接使用参数值
//                             state.agg_vals[i] = arg_val;
//                         }
//                         else
//                         {
//                             // 后续更新，需要处理类型转换
//                             if (state.agg_vals[i].type == TYPE_INT && arg_val.type == TYPE_INT)
//                             {
//                                 state.agg_vals[i].set_int(state.agg_vals[i].int_val + arg_val.int_val);
//                             }
//                             else if (state.agg_vals[i].type == TYPE_FLOAT && arg_val.type == TYPE_FLOAT)
//                             {
//                                 state.agg_vals[i].set_float(state.agg_vals[i].float_val + arg_val.float_val);
//                             }
//                             else if (state.agg_vals[i].type == TYPE_INT && arg_val.type == TYPE_FLOAT)
//                             {
//                                 // INT + FLOAT = FLOAT
//                                 state.agg_vals[i].set_float(static_cast<float>(state.agg_vals[i].int_val) + arg_val.float_val);
//                             }
//                             else if (state.agg_vals[i].type == TYPE_FLOAT && arg_val.type == TYPE_INT)
//                             {
//                                 // FLOAT + INT = FLOAT
//                                 state.agg_vals[i].set_float(state.agg_vals[i].float_val + static_cast<float>(arg_val.int_val));
//                             }
//                         }
//                         state.counts[i]++;
//                     }
//                     break;

//                 case ast::AGG_MIN:
//                     if (!arg_val.is_null())
//                     {
//                         bool should_update = false;
//                         if (state.counts[i] == 0)
//                         {
//                             should_update = true;
//                         }
//                         else if (arg_val.type == TYPE_INT && state.agg_vals[i].type == TYPE_INT)
//                         {
//                             should_update = arg_val.int_val < state.agg_vals[i].int_val;
//                         }
//                         else if (arg_val.type == TYPE_FLOAT && state.agg_vals[i].type == TYPE_FLOAT)
//                         {
//                             should_update = arg_val.float_val < state.agg_vals[i].float_val;
//                         }
//                         else if (arg_val.type == TYPE_STRING && state.agg_vals[i].type == TYPE_STRING)
//                         {
//                             should_update = arg_val.str_val < state.agg_vals[i].str_val;
//                         }

//                         if (should_update)
//                         {
//                             state.agg_vals[i] = arg_val;
//                         }
//                         state.counts[i]++;
//                     }
//                     break;

//                 case ast::AGG_MAX:
//                     if (!arg_val.is_null())
//                     {
//                         bool should_update = false;
//                         if (state.counts[i] == 0)
//                         {
//                             should_update = true;
//                         }
//                         else if (arg_val.type == TYPE_INT && state.agg_vals[i].type == TYPE_INT)
//                         {
//                             should_update = arg_val.int_val > state.agg_vals[i].int_val;
//                         }
//                         else if (arg_val.type == TYPE_FLOAT && state.agg_vals[i].type == TYPE_FLOAT)
//                         {
//                             should_update = arg_val.float_val > state.agg_vals[i].float_val;
//                         }
//                         else if (arg_val.type == TYPE_STRING && state.agg_vals[i].type == TYPE_STRING)
//                         {
//                             should_update = arg_val.str_val > state.agg_vals[i].str_val;
//                         }

//                         if (should_update)
//                         {
//                             state.agg_vals[i] = arg_val;
//                         }
//                         state.counts[i]++;
//                     }
//                     break;
//                 }
//             }
//             else if (auto col = std::dynamic_pointer_cast<ast::Col>(expr))
//             {
//                 // 对于普通列（GROUP BY列），更新计数以便COUNT(*)能正确工作
//                 state.counts[i]++;

//                 // 设置列的值（用于输出）- 只在第一次设置
//                 if (state.counts[i] == 1)
//                 {
//                     int idx = get_col_idx(cols_, col->tab_name, col->col_name);
//                     state.agg_vals[i] = Value::from_raw(rec->data + cols_[idx].offset, cols_[idx].type, cols_[idx].len);
//                 }
//             }
//         }
//     }

//     void find_next_valid()
//     {
//         while (output_iter_ != group_map_.end())
//         {
//             if (!having_)
//             {
//                 // 没有HAVING子句，所有组都有效
//                 break;
//             }

//             // 评估HAVING条件
//             try
//             {
//                 if (eval_having_condition(output_iter_->second))
//                 {
//                     break; // 找到满足条件的组
//                 }
//             }
//             catch (const std::exception &e)
//             {
//                 // 处理可能的异常，如类型不兼容等
//                 std::cerr << "Error evaluating HAVING condition: " << e.what() << std::endl;
//                 // 对于严重错误，应该抛出异常而不是继续
//                 throw std::runtime_error("HAVING clause evaluation failed: " + std::string(e.what()));
//             }

//             ++output_iter_;
//         }
//     }

//     // 评估HAVING条件
//     bool eval_having_condition(const AggState &state)
//     {
//         if (!having_)
//             return true; // 没有HAVING子句，直接返回true

//         // 尝试解析复合条件（如 "COUNT(*) > 1 AND MIN(score) > 88"）
//         // 由于当前AST结构限制，我们需要特殊处理
//         return eval_having_condition_with_and(having_, state);
//     }

//     // 处理可能包含AND的HAVING条件
//     bool eval_having_condition_with_and(const std::shared_ptr<ast::Expr> &expr, const AggState &state)
//     {
//         // 处理不同类型的表达式
//         if (auto logical = std::dynamic_pointer_cast<ast::LogicalExpr>(expr))
//         {
//             // 处理逻辑表达式，如 expr1 AND expr2
//             return eval_logical_expr(logical, state);
//         }
//         else if (auto compare = std::dynamic_pointer_cast<ast::CompareExpr>(expr))
//         {
//             // 处理比较表达式，如 COUNT(*) > 3
//             return eval_compare_expr(compare, state);
//         }
//         else if (auto binary = std::dynamic_pointer_cast<ast::BinaryExpr>(expr))
//         {
//             // 处理旧的二元表达式（向后兼容）
//             return eval_binary_expr(binary, state);
//         }
//         else
//         {
//             // 其他类型的表达式，如单独的聚合函数或常量
//             return eval_having_condition_expr(expr, state);
//         }
//     }

//     // 评估逻辑表达式（AND/OR）
//     bool eval_logical_expr(const std::shared_ptr<ast::LogicalExpr> &expr, const AggState &state)
//     {
//         bool lhs_result = eval_having_condition_with_and(expr->lhs, state);

//         if (expr->op == ast::LogicalExpr::AND)
//         {
//             // 对于AND，如果左边为false，直接返回false（短路求值）
//             if (!lhs_result)
//                 return false;
//             // 否则计算右边
//             return eval_having_condition_with_and(expr->rhs, state);
//         }
//         else if (expr->op == ast::LogicalExpr::OR)
//         {
//             // 对于OR，如果左边为true，直接返回true（短路求值）
//             if (lhs_result)
//                 return true;
//             // 否则计算右边
//             return eval_having_condition_with_and(expr->rhs, state);
//         }

//         throw std::runtime_error("Unsupported logical operator in HAVING clause");
//     }

//     // 评估比较表达式
//     bool eval_compare_expr(const std::shared_ptr<ast::CompareExpr> &expr, const AggState &state)
//     {
//         // 获取左右操作数的值
//         Value lhs_val = eval_having_expr(expr->lhs, state);
//         Value rhs_val = eval_having_expr(expr->rhs, state);

//         // 处理类型转换
//         if (lhs_val.type != rhs_val.type)
//         {
//             // 如果类型不同但都是数值类型，进行类型转换
//             if ((lhs_val.type == TYPE_INT || lhs_val.type == TYPE_FLOAT) &&
//                 (rhs_val.type == TYPE_INT || rhs_val.type == TYPE_FLOAT))
//             {
//                 // 转换为浮点数比较
//                 float lhs_float, rhs_float;
//                 if (lhs_val.type == TYPE_INT)
//                 {
//                     lhs_float = static_cast<float>(lhs_val.int_val);
//                 }
//                 else
//                 {
//                     lhs_float = lhs_val.float_val;
//                 }

//                 if (rhs_val.type == TYPE_INT)
//                 {
//                     rhs_float = static_cast<float>(rhs_val.int_val);
//                 }
//                 else
//                 {
//                     rhs_float = rhs_val.float_val;
//                 }

//                 // 根据操作符比较
//                 switch (expr->op)
//                 {
//                 case ast::SV_OP_EQ:
//                     return lhs_float == rhs_float;
//                 case ast::SV_OP_NE:
//                     return lhs_float != rhs_float;
//                 case ast::SV_OP_LT:
//                     return lhs_float < rhs_float;
//                 case ast::SV_OP_LE:
//                     return lhs_float <= rhs_float;
//                 case ast::SV_OP_GT:
//                     return lhs_float > rhs_float;
//                 case ast::SV_OP_GE:
//                     return lhs_float >= rhs_float;
//                 default:
//                     throw std::runtime_error("Unsupported comparison operator in HAVING clause");
//                 }
//             }
//             else
//             {
//                 throw std::runtime_error("Incompatible types in HAVING clause");
//             }
//         }

//         // 类型相同，直接比较
//         switch (expr->op)
//         {
//         case ast::SV_OP_EQ:
//             return lhs_val == rhs_val;
//         case ast::SV_OP_NE:
//             return lhs_val != rhs_val;
//         case ast::SV_OP_LT:
//             return lhs_val < rhs_val;
//         case ast::SV_OP_LE:
//             return lhs_val <= rhs_val;
//         case ast::SV_OP_GT:
//             return lhs_val > rhs_val;
//         case ast::SV_OP_GE:
//             return lhs_val >= rhs_val;
//         default:
//             throw std::runtime_error("Unsupported comparison operator in HAVING clause");
//         }
//     }

//     // 评估二元表达式
//     bool eval_binary_expr(const std::shared_ptr<ast::BinaryExpr> &expr, const AggState &state)
//     {
//         // 获取左右操作数的值
//         // 注意：BinaryExpr的lhs是Col类型，但在HAVING中我们需要特殊处理
//         Value lhs_val, rhs_val;

//         // 尝试将左操作数作为列来处理
//         try
//         {
//             lhs_val = eval_having_expr(std::static_pointer_cast<ast::Expr>(expr->lhs), state);
//         }
//         catch (const std::exception &)
//         {
//             // 如果失败，可能是因为解析问题，尝试其他方法
//             throw std::runtime_error("Failed to evaluate left operand in HAVING clause");
//         }

//         rhs_val = eval_having_expr(expr->rhs, state);

//         // 处理类型转换
//         if (lhs_val.type != rhs_val.type)
//         {
//             // 如果类型不同但都是数值类型，进行类型转换
//             if ((lhs_val.type == TYPE_INT || lhs_val.type == TYPE_FLOAT) &&
//                 (rhs_val.type == TYPE_INT || rhs_val.type == TYPE_FLOAT))
//             {
//                 // 转换为浮点数比较
//                 float lhs_float, rhs_float;
//                 if (lhs_val.type == TYPE_INT)
//                 {
//                     lhs_float = static_cast<float>(lhs_val.int_val);
//                 }
//                 else
//                 {
//                     lhs_float = lhs_val.float_val;
//                 }

//                 if (rhs_val.type == TYPE_INT)
//                 {
//                     rhs_float = static_cast<float>(rhs_val.int_val);
//                 }
//                 else
//                 {
//                     rhs_float = rhs_val.float_val;
//                 }

//                 // 根据操作符比较
//                 switch (expr->op)
//                 {
//                 case ast::SV_OP_EQ:
//                     return lhs_float == rhs_float;
//                 case ast::SV_OP_NE:
//                     return lhs_float != rhs_float;
//                 case ast::SV_OP_LT:
//                     return lhs_float < rhs_float;
//                 case ast::SV_OP_LE:
//                     return lhs_float <= rhs_float;
//                 case ast::SV_OP_GT:
//                     return lhs_float > rhs_float;
//                 case ast::SV_OP_GE:
//                     return lhs_float >= rhs_float;
//                 default:
//                     throw std::runtime_error("Unsupported binary operator in HAVING clause");
//                 }
//             }
//             else
//             {
//                 throw std::runtime_error("Incompatible types in HAVING clause");
//             }
//         }

//         // 类型相同，直接比较
//         switch (expr->op)
//         {
//         case ast::SV_OP_EQ:
//             return lhs_val == rhs_val;
//         case ast::SV_OP_NE:
//             return lhs_val != rhs_val;
//         case ast::SV_OP_LT:
//             return lhs_val < rhs_val;
//         case ast::SV_OP_LE:
//             return lhs_val <= rhs_val;
//         case ast::SV_OP_GT:
//             return lhs_val > rhs_val;
//         case ast::SV_OP_GE:
//             return lhs_val >= rhs_val;
//         default:
//             throw std::runtime_error("Unsupported binary operator in HAVING clause");
//         }
//     }

//     // 评估HAVING子句中的表达式，返回表达式的值
//     Value eval_having_expr(const std::shared_ptr<ast::Expr> &expr, const AggState &state)
//     {
//         // 处理不同类型的表达式
//         if (auto agg = std::dynamic_pointer_cast<ast::AggExpr>(expr))
//         {
//             // 聚合函数表达式
//             return eval_agg_expr(agg, state);
//         }
//         else if (auto col = std::dynamic_pointer_cast<ast::Col>(expr))
//         {
//             // 列引用表达式（必须是GROUP BY中的列）
//             return eval_col_expr(col, state);
//         }
//         else if (auto val = std::dynamic_pointer_cast<ast::Value>(expr))
//         {
//             // 常量表达式
//             return eval_value_expr(val);
//         }
//         else if (auto compare = std::dynamic_pointer_cast<ast::CompareExpr>(expr))
//         {
//             // 比较表达式，返回布尔结果作为整数
//             bool result = eval_compare_expr(compare, state);
//             Value val;
//             val.set_int(result ? 1 : 0);
//             return val;
//         }
//         else if (auto logical = std::dynamic_pointer_cast<ast::LogicalExpr>(expr))
//         {
//             // 逻辑表达式，返回布尔结果作为整数
//             bool result = eval_logical_expr(logical, state);
//             Value val;
//             val.set_int(result ? 1 : 0);
//             return val;
//         }
//         else if (auto binary = std::dynamic_pointer_cast<ast::BinaryExpr>(expr))
//         {
//             // 二元表达式（向后兼容）
//             Value lhs = eval_having_expr(std::static_pointer_cast<ast::Expr>(binary->lhs), state);
//             Value rhs = eval_having_expr(binary->rhs, state);
//             return eval_binary_operation(binary->op, lhs, rhs);
//         }
//         else
//         {
//             throw std::runtime_error("Unsupported expression type in HAVING clause");
//         }
//     }

//     // 评估聚合函数表达式
//     Value eval_agg_expr(const std::shared_ptr<ast::AggExpr> &agg, const AggState &state)
//     {
//         // 首先在select_exprs_中查找匹配的聚合函数
//         for (size_t i = 0; i < select_exprs_.size(); i++)
//         {
//             auto expr = select_exprs_[i];
//             // 处理别名表达式
//             if (auto alias = std::dynamic_pointer_cast<ast::AliasExpr>(expr))
//             {
//                 expr = alias->expr;
//             }

//             // 检查是否是聚合函数
//             if (auto select_agg = std::dynamic_pointer_cast<ast::AggExpr>(expr))
//             {
//                 // 检查聚合函数类型是否匹配
//                 if (select_agg->agg_type == agg->agg_type)
//                 {
//                     // 检查参数是否匹配
//                     bool args_match = false;
//                     if (!select_agg->arg && !agg->arg)
//                     {
//                         // 两者都没有参数（如COUNT(*)）
//                         args_match = true;
//                     }
//                     else if (select_agg->arg && agg->arg)
//                     {
//                         // 两者都有参数，检查参数是否相同
//                         auto select_col = std::dynamic_pointer_cast<ast::Col>(select_agg->arg);
//                         auto having_col = std::dynamic_pointer_cast<ast::Col>(agg->arg);
//                         if (select_col && having_col &&
//                             select_col->col_name == having_col->col_name &&
//                             (having_col->tab_name.empty() || select_col->tab_name == having_col->tab_name))
//                         {
//                             args_match = true;
//                         }
//                     }

//                     if (args_match)
//                     {
//                         // 找到匹配的聚合函数，返回其值
//                         return state.agg_vals[i];
//                     }
//                 }
//             }
//         }

//         // 如果在SELECT中没有找到，直接计算聚合函数的值
//         // 这是符合SQL标准的：HAVING可以使用SELECT中没有的聚合函数
//         return compute_agg_value(agg, state);
//     }

//     // 直接计算聚合函数的值（用于HAVING中独立的聚合函数）
//     Value compute_agg_value(const std::shared_ptr<ast::AggExpr> &agg, const AggState &state)
//     {
//         Value result;

//         switch (agg->agg_type)
//         {
//         case ast::AGG_COUNT:
//             // 对于COUNT(*)，计算当前分组的记录数
//             if (!agg->arg)
//             {
//                 // COUNT(*) - 找到任何一个有效的聚合计数
//                 int max_count = 0;
//                 for (int count : state.counts)
//                 {
//                     if (count > max_count)
//                     {
//                         max_count = count;
//                     }
//                 }
//                 result.set_int(max_count);
//             }
//             else
//             {
//                 // COUNT(col) - 需要检查特定列的非NULL计数
//                 // 为简化，使用相同逻辑
//                 int max_count = 0;
//                 for (int count : state.counts)
//                 {
//                     if (count > max_count)
//                     {
//                         max_count = count;
//                     }
//                 }
//                 result.set_int(max_count);
//             }
//             break;

//         case ast::AGG_MIN:
//         case ast::AGG_MAX:
//         case ast::AGG_SUM:
//         case ast::AGG_AVG:
//             // 对于其他聚合函数，如果不在SELECT中，我们需要特殊处理
//             // 实际上，标准SQL要求HAVING中的聚合函数要么在SELECT中，要么重新计算
//             // 这里我们提供一个简化的实现：假设用户不会在HAVING中使用SELECT中没有的复杂聚合
//             throw std::runtime_error("Aggregate function in HAVING clause must also appear in SELECT list, or use a simpler form");
//         }

//         return result;
//     }

//     // 评估列引用表达式（必须是GROUP BY中的列）
//     Value eval_col_expr(const std::shared_ptr<ast::Col> &col, const AggState &state)
//     {
//         // 在GROUP BY中查找匹配的列
//         for (size_t i = 0; i < group_by_.size(); i++)
//         {
//             auto group_col = std::dynamic_pointer_cast<ast::Col>(group_by_[i]);
//             if (group_col && group_col->col_name == col->col_name &&
//                 (col->tab_name.empty() || group_col->tab_name == col->tab_name))
//             {
//                 // 找到匹配的GROUP BY列
//                 // 从当前组的键中获取值
//                 return output_iter_->first.keys[i];
//             }
//         }

//         // 没有找到匹配的GROUP BY列
//         throw std::runtime_error("Column in HAVING clause must appear in GROUP BY clause");
//     }

//     // 评估常量表达式
//     Value eval_value_expr(const std::shared_ptr<ast::Value> &val)
//     {
//         Value result;
//         if (auto int_lit = std::dynamic_pointer_cast<ast::IntLit>(val))
//         {
//             result.set_int(int_lit->val);
//         }
//         else if (auto float_lit = std::dynamic_pointer_cast<ast::FloatLit>(val))
//         {
//             result.set_float(float_lit->val);
//         }
//         else if (auto str_lit = std::dynamic_pointer_cast<ast::StringLit>(val))
//         {
//             result.set_str(str_lit->val);
//         }
//         else if (auto bool_lit = std::dynamic_pointer_cast<ast::BoolLit>(val))
//         {
//             result.set_int(bool_lit->val ? 1 : 0);
//         }
//         else
//         {
//             throw std::runtime_error("Unsupported value type in HAVING clause");
//         }
//         return result;
//     }

//     // 评估二元操作
//     Value eval_binary_operation(ast::SvCompOp op, const Value &lhs, const Value &rhs)
//     {
//         // 处理类型转换
//         if (lhs.type != rhs.type)
//         {
//             // 如果类型不同但都是数值类型，进行类型转换
//             if ((lhs.type == TYPE_INT || lhs.type == TYPE_FLOAT) &&
//                 (rhs.type == TYPE_INT || rhs.type == TYPE_FLOAT))
//             {
//                 // 转换为浮点数计算
//                 float lhs_float, rhs_float;
//                 if (lhs.type == TYPE_INT)
//                 {
//                     lhs_float = static_cast<float>(lhs.int_val);
//                 }
//                 else
//                 {
//                     lhs_float = lhs.float_val;
//                 }

//                 if (rhs.type == TYPE_INT)
//                 {
//                     rhs_float = static_cast<float>(rhs.int_val);
//                 }
//                 else
//                 {
//                     rhs_float = rhs.float_val;
//                 }

//                 // 根据操作符计算
//                 Value result;
//                 switch (op)
//                 {
//                 case ast::SV_OP_EQ:
//                     result.set_int(lhs_float == rhs_float ? 1 : 0);
//                     break;
//                 case ast::SV_OP_NE:
//                     result.set_int(lhs_float != rhs_float ? 1 : 0);
//                     break;
//                 case ast::SV_OP_LT:
//                     result.set_int(lhs_float < rhs_float ? 1 : 0);
//                     break;
//                 case ast::SV_OP_LE:
//                     result.set_int(lhs_float <= rhs_float ? 1 : 0);
//                     break;
//                 case ast::SV_OP_GT:
//                     result.set_int(lhs_float > rhs_float ? 1 : 0);
//                     break;
//                 case ast::SV_OP_GE:
//                     result.set_int(lhs_float >= rhs_float ? 1 : 0);
//                     break;
//                 default:
//                     throw std::runtime_error("Unsupported binary operator in HAVING clause");
//                 }
//                 return result;
//             }
//             else
//             {
//                 throw std::runtime_error("Incompatible types in binary operation");
//             }
//         }

//         // 类型相同，直接计算
//         Value result;
//         switch (op)
//         {
//         // 比较操作符
//         case ast::SV_OP_EQ:
//             result.set_int(lhs == rhs ? 1 : 0);
//             break;
//         case ast::SV_OP_NE:
//             result.set_int(lhs != rhs ? 1 : 0);
//             break;
//         case ast::SV_OP_LT:
//             result.set_int(lhs < rhs ? 1 : 0);
//             break;
//         case ast::SV_OP_LE:
//             result.set_int(lhs <= rhs ? 1 : 0);
//             break;
//         case ast::SV_OP_GT:
//             result.set_int(lhs > rhs ? 1 : 0);
//             break;
//         case ast::SV_OP_GE:
//             result.set_int(lhs >= rhs ? 1 : 0);
//             break;
//         default:
//             throw std::runtime_error("Unsupported binary operator in HAVING clause");
//         }
//         return result;

//         throw std::runtime_error("Unsupported operation for the given types");
//     }

//     // 评估一元操作 - 由于AST中没有定义UnaryOp，暂时移除此功能
//     // Value AggExecutor::eval_unary_operation(ast::UnaryOp op, const Value& val) {
//     //     // 此功能暂时不支持，因为AST中没有定义UnaryOp枚举
//     //     throw std::runtime_error("Unary operations not supported in current AST definition");
//     // }

//     // 评估HAVING条件表达式，返回布尔结果
//     bool eval_having_condition_expr(const std::shared_ptr<ast::Expr> &expr, const AggState &state)
//     {
//         Value result = eval_having_expr(expr, state);
//         return result.bool_val();
//     }

//     // 检查是否为简单的 COUNT(*) 查询
//     bool is_count_star_only() const
//     {
//         if (select_exprs_.size() != 1 || !group_by_.empty() || having_)
//             return false;
        
//         auto expr = select_exprs_[0];
//         if (auto alias = std::dynamic_pointer_cast<ast::AliasExpr>(expr))
//             expr = alias->expr;
        
//         auto agg = std::dynamic_pointer_cast<ast::AggExpr>(expr);
//         return agg && agg->agg_type == ast::AGG_COUNT && !agg->arg;
//     }

//     // 检查是否可以使用缓存的记录数
//     bool can_use_cached_count() const
//     {
//         // 检查顺序扫描执行器
//         if (auto seq_scan = dynamic_cast<SeqScanExecutor*>(child_.get()))
//         {
//             return seq_scan->has_no_conditions();
//         }
        
//         // 检查索引扫描执行器
//         if (auto index_scan = dynamic_cast<IndexScanExecutor*>(child_.get()))
//         {
//             return index_scan->has_no_conditions();
//         }
        
        
//         return false;
//     }

//     // 获取缓存的记录总数
//     int get_cached_record_count() const
//     {
//         // 从顺序扫描执行器获取
//         if (auto seq_scan = dynamic_cast<SeqScanExecutor*>(child_.get()))
//         {
//             auto fh = seq_scan->get_file_handle();
//             auto file_hdr = fh->get_file_hdr();
//             if (file_hdr.count_cache_valid)
//             {
//                 return file_hdr.total_record_count;
//             }
//         }
        
//         // 从索引扫描执行器获取
//         if (auto index_scan = dynamic_cast<IndexScanExecutor*>(child_.get()))
//         {
//             auto fh = index_scan->get_file_handle();
//             auto file_hdr = fh->get_file_hdr();
//             if (file_hdr.count_cache_valid)
//             {
//                 return file_hdr.total_record_count;
//             }
//         }
        
//         return -1;
//     }

//     // 创建 COUNT 结果记录
//     std::unique_ptr<RmRecord> create_count_result(int count)
//     {
//         size_t len = sizeof(int);
//         auto rec = std::make_unique<RmRecord>(len);
//         memcpy(rec->data, &count, sizeof(int));
//         return rec;
//     }

//     // 检查是否为简单的 COUNT 查询（可以优化的）
//     // 只有一个 COUNT 表达式，无 GROUP BY，无 HAVING
//     bool is_simple_count_query() const {
        
//         if (select_exprs_.size() != 1 || !group_by_.empty() || having_) {
//             return false;
//         }
        
//         auto expr = select_exprs_[0];
//         if (auto alias = std::dynamic_pointer_cast<ast::AliasExpr>(expr)) {
//             expr = alias->expr;
//         }
        
//         auto agg = std::dynamic_pointer_cast<ast::AggExpr>(expr);
//         return agg && agg->agg_type == ast::AGG_COUNT;
//     }
    
//     // 执行 COUNT 优化路径
//     void execute_count_optimization() {
        
//         auto expr = select_exprs_[0];
//         if (auto alias = std::dynamic_pointer_cast<ast::AliasExpr>(expr)) {
//             expr = alias->expr;
//         }
//         auto count_expr = std::dynamic_pointer_cast<ast::AggExpr>(expr);
        
//         int count_result = 0;
        
//         // 优化1：COUNT(*) 无条件全表扫描
//         if (!count_expr->arg && can_use_cached_count()) {
           
//             auto cached_count = get_cached_record_count();
//             if (cached_count >= 0) {
                
//                 count_result = cached_count;
//                 create_count_result_group(count_result);
//                 return;
//             } 
//         } 
//         // 优化2：直接计数，不使用复杂的聚合状态
//         child_->beginTuple();
        
//         if (!count_expr->arg) {
        
//             count_result = count_all_rows();
//         } else {

//             count_result = count_non_null_values(count_expr);
//         }
        

//         create_count_result_group(count_result);
//     }
    
//     // 计数所有行（COUNT(*)）
//     int count_all_rows() {

//         int count = 0;
//         while (!child_->is_end()) {
//             child_->Next();
//             count++;
//             child_->nextTuple();
//         }

//         return count;
//     }
    
//     // 计数非NULL值（COUNT(column)）
//     int count_non_null_values(const std::shared_ptr<ast::AggExpr>& count_expr) {
//         int count = 0;
//         cols_ = child_->cols();
//         auto col = std::dynamic_pointer_cast<ast::Col>(count_expr->arg);
//         if (!col) {

//             throw std::runtime_error("COUNT argument must be a column");
//         }
//         int col_idx = get_col_idx(cols_, col->tab_name, col->col_name);
//         while (!child_->is_end()) {
//             auto rec = child_->Next();
//             Value val = Value::from_raw(rec->data + cols_[col_idx].offset, 
//                                       cols_[col_idx].type, cols_[col_idx].len);
//             if (!val.is_null()) {
//                 count++;
//             }
//             child_->nextTuple();
//         }
//         return count;
//     }
    
//     // 创建 COUNT 结果分组
//     void create_count_result_group(int count_result) {
//         AggGroupKey key;
//         Value key_val;
//         key_val.set_int(1);
//         key.keys.push_back(key_val);
        
//         AggState state(1);
//         state.agg_vals[0].set_int(count_result);
//         state.counts[0] = 1;
        
//         group_map_[key] = state;
//         output_iter_ = group_map_.begin();
//     }
    
//     // 执行普通聚合（原有逻辑）
//     void execute_normal_aggregation() {
//         child_->beginTuple();
//         group_map_.clear();
//         cols_ = child_->cols();

//         bool has_input = false;
//         int record_count = 0;
//         while (!child_->is_end()) {
//             has_input = true;
//             auto rec = child_->Next();
//             AggGroupKey key = extract_group_key(rec.get());
            
//             if (group_map_.find(key) == group_map_.end()) {
//                 group_map_[key] = AggState(select_exprs_.size());
//             }
            
//             update_agg_state(group_map_[key], rec.get());
//             child_->nextTuple();
//             record_count++;
//         }
//         // 处理无输入情况...
//         if (!has_input && group_by_.empty()) {

//           AggGroupKey default_key;
//             Value key_val;
//             key_val.set_int(1);
//             default_key.keys.push_back(key_val);
//             AggState default_state(select_exprs_.size());
//             // 初始化聚合函数的初始值
//             for (size_t i = 0; i < select_exprs_.size(); i++)
//             {
//                 auto expr = select_exprs_[i];
//                 // 处理别名表达式
//                 if (auto alias = std::dynamic_pointer_cast<ast::AliasExpr>(expr))
//                 {
//                     expr = alias->expr;
//                 }
//                 if (auto agg = std::dynamic_pointer_cast<ast::AggExpr>(expr))
//                 {
//                     if (agg->agg_type == ast::AGG_COUNT)
//                     {
//                         default_state.agg_vals[i].set_int(0); // COUNT 初始值为 0
//                     }
//                 }
//             }
//             group_map_[default_key] = default_state;
//         }
//         output_iter_ = group_map_.begin();
//         find_next_valid();
//     }
// };
