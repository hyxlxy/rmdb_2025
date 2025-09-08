/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#pragma once

#include "../parser/ast.h"
#include "../common/common.h"
#include "../system/sm.h"
#include "../errors.h"
#include <memory>

/**
 * @brief 表达式计算器
 * 用于计算UPDATE语句中的算术表达式，如 score = score + 5.5
 */
class ExpressionEvaluator {
private:
    const std::vector<ColMeta>& cols_;
    const RmRecord* record_;
    
public:
    ExpressionEvaluator(const std::vector<ColMeta>& cols, const RmRecord* record)
        : cols_(cols), record_(record) {}
    
    /**
     * @brief 计算表达式的值
     * @param expr 表达式AST节点
     * @return 计算结果
     */
    Value evaluate(const std::shared_ptr<ast::Expr>& expr) {


        if (auto value_node = std::dynamic_pointer_cast<ast::Value>(expr)) {

            return evaluate_value(value_node);
        } else if (auto col_node = std::dynamic_pointer_cast<ast::Col>(expr)) {

            return evaluate_column(col_node);
        } else if (auto arith_node = std::dynamic_pointer_cast<ast::ArithExpr>(expr)) {

            return evaluate_arithmetic(arith_node);
        } else {
            throw std::runtime_error("Unsupported expression type");
        }
    }
    
private:
    /**
     * @brief 计算值节点
     */
    Value evaluate_value(const std::shared_ptr<ast::Value>& value_node) {
        Value result;
        
        if (auto int_lit = std::dynamic_pointer_cast<ast::IntLit>(value_node)) {
            result.set_int(int_lit->val);
        } else if (auto float_lit = std::dynamic_pointer_cast<ast::FloatLit>(value_node)) {
            result.set_float(float_lit->val);
        } else if (auto str_lit = std::dynamic_pointer_cast<ast::StringLit>(value_node)) {
            result.set_str(str_lit->val);
        } else if (auto bool_lit = std::dynamic_pointer_cast<ast::BoolLit>(value_node)) {
            result.set_int(bool_lit->val ? 1 : 0);
        } else {
            throw std::runtime_error("Unsupported value type");
        }
        
        return result;
    }
    
    /**
     * @brief 计算列引用
     */
    Value evaluate_column(const std::shared_ptr<ast::Col>& col_node) {
        if (record_ == nullptr) {
            throw std::runtime_error("No record context for column evaluation");
        }

        if (record_->data == nullptr) {
            throw std::runtime_error("Record data is null for column evaluation");
        }

        // 查找列元数据
        const ColMeta* col_meta = nullptr;
        for (const auto& col : cols_) {
            if (col.name == col_node->col_name) {
                col_meta = &col;
                break;
            }
        }

        if (col_meta == nullptr) {
            throw std::runtime_error("Column not found: " + col_node->col_name);
        }

        // 从记录中提取值
        Value result;
        char* data_ptr = record_->data + col_meta->offset;
        
        switch (col_meta->type) {
            case TYPE_INT:
                result.set_int(*(int*)data_ptr);
                break;
            case TYPE_FLOAT:
                result.set_float(*(float*)data_ptr);
                break;
            case TYPE_STRING: {
                // 使用与from_raw相同的逻辑
                size_t actual_len = 0;
                while (actual_len < (size_t)col_meta->len && data_ptr[actual_len] != '\0') {
                    actual_len++;
                }
                result.set_str(std::string(data_ptr, actual_len));
                break;
            }
            default:
                throw std::runtime_error("Unsupported column type");
        }
        
        return result;
    }
    
    /**
     * @brief 计算算术表达式
     */
    Value evaluate_arithmetic(const std::shared_ptr<ast::ArithExpr>& arith_node) {
        Value left = evaluate(arith_node->lhs);
        Value right = evaluate(arith_node->rhs);
        
        // 类型转换：确保两个操作数类型兼容
        convert_for_arithmetic(left, right);
        
        Value result;
        
        switch (arith_node->op) {
            case ast::ArithOp::ADD:
                result = add_values(left, right);
                break;
            case ast::ArithOp::SUB:
                result = subtract_values(left, right);
                break;
            case ast::ArithOp::MUL:
                result = multiply_values(left, right);
                break;
            case ast::ArithOp::DIV:
                result = divide_values(left, right);
                break;
            default:
                throw std::runtime_error("Unsupported arithmetic operator");
        }
        
        return result;
    }
    
    /**
     * @brief 为算术运算进行类型转换
     */
    void convert_for_arithmetic(Value& left, Value& right) {
        if (left.type == right.type) {
            return; // 类型相同，无需转换
        }

        // 检查是否都是数值类型
        if ((left.type == TYPE_INT || left.type == TYPE_FLOAT) &&
            (right.type == TYPE_INT || right.type == TYPE_FLOAT)) {
            // 将int转换为float
            if (left.type == TYPE_INT && right.type == TYPE_FLOAT) {
                left.set_float(static_cast<float>(left.int_val));
            } else if (left.type == TYPE_FLOAT && right.type == TYPE_INT) {
                right.set_float(static_cast<float>(right.int_val));
            }
            
        } else {
            throw IncompatibleTypeError(coltype2str(left.type), coltype2str(right.type));
        }
    }
    
    /**
     * @brief 加法运算
     */
    Value add_values(const Value& left, const Value& right) {
        Value result;
        
        if (left.type == TYPE_INT && right.type == TYPE_INT) {
            result.set_int(left.int_val + right.int_val);
        } else if (left.type == TYPE_FLOAT && right.type == TYPE_FLOAT) {
            result.set_float(left.float_val + right.float_val);
        } else if (left.type == TYPE_STRING && right.type == TYPE_STRING) {
            result.set_str(left.str_val + right.str_val);
        } else {
            throw std::runtime_error("Invalid types for addition");
        }
        
        return result;
    }
    
    /**
     * @brief 减法运算
     */
    Value subtract_values(const Value& left, const Value& right) {
        Value result;
        
        if (left.type == TYPE_INT && right.type == TYPE_INT) {
            result.set_int(left.int_val - right.int_val);
        } else if (left.type == TYPE_FLOAT && right.type == TYPE_FLOAT) {
            result.set_float(left.float_val - right.float_val);
        } else {
            throw std::runtime_error("Invalid types for subtraction");
        }
        
        return result;
    }
    
    /**
     * @brief 乘法运算
     */
    Value multiply_values(const Value& left, const Value& right) {
        Value result;
        
        if (left.type == TYPE_INT && right.type == TYPE_INT) {
            result.set_int(left.int_val * right.int_val);
        } else if (left.type == TYPE_FLOAT && right.type == TYPE_FLOAT) {
            result.set_float(left.float_val * right.float_val);
        } else {
            throw std::runtime_error("Invalid types for multiplication");
        }
        
        return result;
    }
    
    /**
     * @brief 除法运算
     */
    Value divide_values(const Value& left, const Value& right) {
        Value result;
        
        if (left.type == TYPE_INT && right.type == TYPE_INT) {
            if (right.int_val == 0) {
                throw std::runtime_error("Division by zero");
            }
            result.set_float(static_cast<float>(left.int_val) / static_cast<float>(right.int_val));
        } else if (left.type == TYPE_FLOAT && right.type == TYPE_FLOAT) {
            if (right.float_val == 0.0f) {
                throw std::runtime_error("Division by zero");
            }
            result.set_float(left.float_val / right.float_val);
        } else {
            throw std::runtime_error("Invalid types for division");
        }
        
        return result;
    }
};
