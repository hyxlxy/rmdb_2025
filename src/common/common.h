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

#include <cassert>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "defs.h"
#include "record/rm_defs.h"


struct TabCol {
    std::string tab_name;
    std::string col_name;

    friend bool operator<(const TabCol &x, const TabCol &y) {
        return std::make_pair(x.tab_name, x.col_name) < std::make_pair(y.tab_name, y.col_name);
    }
};

struct Value {
    ColType type;  // type of value
    union {
        int int_val;      // int value
        float float_val;  // float value
    };
    std::string str_val;  // string value

    std::shared_ptr<RmRecord> raw;  // raw record buffer

    void set_int(int int_val_) {
        type = TYPE_INT;
        int_val = int_val_;
    }

    void set_float(float float_val_) {
        type = TYPE_FLOAT;
        float_val = float_val_;
    }

    void set_str(std::string str_val_) {
        type = TYPE_STRING;
        str_val = std::move(str_val_);
    }

    void init_raw(int len) {
        assert(raw == nullptr);
        raw = std::make_shared<RmRecord>(len);
        if (type == TYPE_INT) {
            assert(len == sizeof(int));
            *(int *)(raw->data) = int_val;
        } else if (type == TYPE_FLOAT) {
            assert(len == sizeof(float));
            *(float *)(raw->data) = float_val;
        } else if (type == TYPE_STRING) {
            if (len < (int)str_val.size()) {
                throw StringOverflowError();
            }
            memset(raw->data, 0, len);
            memcpy(raw->data, str_val.c_str(), str_val.size());
        }
    }

    // 添加构造函数
    Value() : type(TYPE_INT), int_val(0) {}
    Value(ColType type_, int int_val_) : type(type_), int_val(int_val_) {}
    Value(ColType type_, float float_val_) : type(type_), float_val(float_val_) {}
    Value(ColType type_, const std::string& str_val_) : type(type_), str_val(str_val_) {}

    // 添加size方法
    size_t size() const {
        switch (type) {
            case TYPE_INT:
                return sizeof(int);
            case TYPE_FLOAT:
                return sizeof(float);
            case TYPE_STRING:
                return str_val.size();
            default:
                return 0;
        }
    }

    // 添加from_raw静态方法
    static Value from_raw(const char* data, ColType type, int len) {
        Value val;
        val.type = type;
        switch (type) {
            case TYPE_INT:
                val.int_val = *(int*)data;
                break;
            case TYPE_FLOAT:
                val.float_val = *(float*)data;
                break;
            case TYPE_STRING:
                // 先找到实际的字符串长度（遇到第一个\0或到达len）
                size_t actual_len = 0;
                while (actual_len < (size_t)len && data[actual_len] != '\0') {
                    actual_len++;
                }
                val.str_val = std::string(data, actual_len);
                break;
        }
        return val;
    }

    // 添加比较运算符
    bool operator==(const Value& other) const {
        if (type != other.type) {
            return false;
        }
        switch (type) {
            case TYPE_INT:
                return int_val == other.int_val;
            case TYPE_FLOAT:
                return float_val == other.float_val;
            case TYPE_STRING:
                return str_val == other.str_val;
            default:
                return false;
        }
    }

    bool operator!=(const Value& other) const {
        return !(*this == other);
    }

    // 添加小于运算符（用于排序和map键）
    bool operator<(const Value& other) const {
        if (type != other.type) {
            return type < other.type;
        }
        switch (type) {
            case TYPE_INT:
                return int_val < other.int_val;
            case TYPE_FLOAT:
                return float_val < other.float_val;
            case TYPE_STRING:
                return str_val < other.str_val;
            default:
                return false;
        }
    }

    // 添加其他比较运算符
    bool operator<=(const Value& other) const {
        return *this < other || *this == other;
    }

    bool operator>(const Value& other) const {
        return !(*this <= other);
    }

    bool operator>=(const Value& other) const {
        return !(*this < other);
    }

    // 添加bool_val方法
    bool bool_val() const {
        switch (type) {
            case TYPE_INT:
                return int_val != 0;
            case TYPE_FLOAT:
                return float_val != 0.0f;
            case TYPE_STRING:
                return !str_val.empty();
            default:
                return false;
        }
    }

    // 添加to_string方法
    std::string to_string() const {
        switch (type) {
            case TYPE_INT:
                return std::to_string(int_val);
            case TYPE_FLOAT:
                return std::to_string(float_val);
            case TYPE_STRING:
                return str_val;
            default:
                return "";
        }
    }

    // 添加is_null方法（简单实现，假设没有NULL值）
    bool is_null() const {
        return false; // 简单实现，假设所有值都不为NULL
    }
};

enum CompOp { OP_EQ, OP_NE, OP_LT, OP_GT, OP_LE, OP_GE };

struct Condition {
    TabCol lhs_col;   // left-hand side column
    CompOp op;        // comparison operator
    bool is_rhs_val;  // true if right-hand side is a value (not a column)
    TabCol rhs_col;   // right-hand side column
    Value rhs_val;    // right-hand side value
};

// 前向声明
namespace ast {
    struct Expr;
}

struct SetClause {
    TabCol lhs;
    Value rhs;//数据实际内容
    std::shared_ptr<ast::Expr> expr; // 支持算术表达式

    // 构造函数
    SetClause() : expr(nullptr) {}
    SetClause(TabCol lhs_, Value rhs_) : lhs(std::move(lhs_)), rhs(std::move(rhs_)), expr(nullptr) {}
    SetClause(TabCol lhs_, Value rhs_, std::shared_ptr<ast::Expr> expr_)
        : lhs(std::move(lhs_)), rhs(std::move(rhs_)), expr(std::move(expr_)) {}
};
