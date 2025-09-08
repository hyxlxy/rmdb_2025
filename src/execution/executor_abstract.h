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

#include "execution_defs.h"
#include "common/common.h"
#include "index/ix.h"
#include "system/sm.h"

class AbstractExecutor {
   public:
    Rid _abstract_rid;

    Context *context_;

    virtual ~AbstractExecutor() = default;

    virtual size_t tupleLen() const { return 0; };

    virtual const std::vector<ColMeta> &cols() const {
        std::vector<ColMeta> *_cols = nullptr;
        return *_cols;
    };

    virtual std::string getType() { return "AbstractExecutor"; };

    virtual void beginTuple(){};

    virtual void nextTuple(){};

    virtual bool is_end() const { return true; };

    virtual Rid &rid() = 0;

    virtual std::unique_ptr<RmRecord> Next() = 0;

    virtual ColMeta get_col_offset(const TabCol &target) { return ColMeta();};

    std::vector<ColMeta>::const_iterator get_col(const std::vector<ColMeta> &rec_cols, const TabCol &target)
     {
        auto pos = std::find_if(rec_cols.begin(), rec_cols.end(), [&](const ColMeta &col) {
            return col.tab_name == target.tab_name && col.name == target.col_name;
        });
        if (pos == rec_cols.end()) {
            throw ColumnNotFoundError(target.tab_name + '.' + target.col_name);
        }
        return pos;
    }

    static void convert(Value &a, Value &b)
    {
        // 数值类型的转化(int, float)
        // int -> float
        if (a.type == b.type)
            return;
        if (a.type == TYPE_FLOAT)
        {
            if (b.type == TYPE_INT)
            {
                b.set_float((double)b.int_val);
                return;
            }
        }
        else if (a.type == TYPE_INT)
        {
            if (b.type == TYPE_FLOAT)
            {
                a.set_float((double)a.int_val);
                return;
            }
        }
        throw InternalError("convert::Unexpected value type");
    }

    Value get_value(ColType p, const char *a)
    {
        Value res;
        switch (p)
        {
        case ColType::TYPE_INT:
        {
            int ia = static_cast<int>(*reinterpret_cast<const int *>(a));
            res.set_int(ia);
            break;
        }

        case ColType::TYPE_FLOAT:
        {
            float fa = static_cast<float>(*reinterpret_cast<const float *>(a));
            res.set_float(fa);
            break;
        }

        case ColType::TYPE_STRING:
        {
            // 需要手动处理string类型的获取
            throw InternalError("get_value::Unexpected string value type at " + getType());
        }
        }
        return res;
    }

    bool is_numeric_type(ColType type) { return type == ColType::TYPE_INT || type == ColType::TYPE_FLOAT; }

    bool eval_cond(const std::vector<ColMeta> &rec_cols, const Condition &cond, const RmRecord *rec)
    {
        auto lhs_col = get_col(rec_cols, cond.lhs_col);
        char *lhs_data = rec->data + lhs_col->offset;
        char *rhs_data;
        ColType rhs_type;
        int rhs_len = 0;

        if (cond.is_rhs_val)
        {
            rhs_data = cond.rhs_val.raw->data;
            rhs_type = cond.rhs_val.type;
            rhs_len = cond.rhs_val.raw->size;
        }
        else
        {
            auto rhs_col = get_col(rec_cols, cond.rhs_col);
            rhs_data = rec->data + rhs_col->offset;
            rhs_type = rhs_col->type;
            rhs_len = rhs_col->len;
        }



        // 类型应该一致或者都是数值类型
        bool is_numeric = is_numeric_type(lhs_col->type) && is_numeric_type(rhs_type);
        if (lhs_col->type != rhs_type && !is_numeric)
        {
            throw IncompatibleTypeError(coltype2str(lhs_col->type), coltype2str(rhs_type));
        }
        
        // 数值类型比较
        if (is_numeric) {
            // 获取左右值（可能需要类型转换）
            float lhs_val_f, rhs_val_f;
            
            // 获取左值
            if (lhs_col->type == TYPE_INT) {
                int lhs_val_i;
                memcpy(&lhs_val_i, lhs_data, sizeof(int));
                lhs_val_f = static_cast<float>(lhs_val_i);
            } else {
                memcpy(&lhs_val_f, lhs_data, sizeof(float));
            }
            
            // 获取右值
            if (cond.is_rhs_val) {
                if (rhs_type == TYPE_INT) {
                    rhs_val_f = static_cast<float>(cond.rhs_val.int_val);
                } else {
                    rhs_val_f = cond.rhs_val.float_val;
                }
            } else {
                if (rhs_type == TYPE_INT) {
                    int rhs_val_i;
                    memcpy(&rhs_val_i, rhs_data, sizeof(int));
                    rhs_val_f = static_cast<float>(rhs_val_i);
                } else {
                    memcpy(&rhs_val_f, rhs_data, sizeof(float));
                }
            }
            
            // 使用浮点数进行比较
            switch (cond.op) {
                case OP_EQ: return lhs_val_f == rhs_val_f;
                case OP_NE: return lhs_val_f != rhs_val_f;
                case OP_LT: return lhs_val_f < rhs_val_f;
                case OP_GT: return lhs_val_f > rhs_val_f;
                case OP_LE: return lhs_val_f <= rhs_val_f;
                case OP_GE: return lhs_val_f >= rhs_val_f;
                default: throw InternalError("Unexpected op type");
            }
        }
        
        // 非数值类型比较（原有逻辑）
        int cmp = 0; // 默认初始化，防止编译器关于未初始化使用的告警
        if (lhs_col->type == ColType::TYPE_STRING)
        {
            size_t len = std::max(lhs_col->len, rhs_len);
            cmp = strncmp(lhs_data, rhs_data, len);
        }

        switch (cond.op)
        {
        case CompOp::OP_EQ:
            return cmp == 0;
        case CompOp::OP_NE:
            return cmp != 0;
        case CompOp::OP_LT:
            return cmp < 0;
        case CompOp::OP_GT:
            return cmp > 0;
        case CompOp::OP_LE:
            return cmp <= 0;
        case CompOp::OP_GE:
            return cmp >= 0;
        default:
            throw InternalError("eval_cond::Unexpected op type at " + getType());
        }
    }

    bool eval_conds(const std::vector<ColMeta> &rec_cols, const std::vector<Condition> &conds, const RmRecord *rec)
    {
        for (const auto &cond : conds)
        {
            if (!eval_cond(rec_cols, cond, rec))
            {
                return false;
            }
        }
        return true;
    }
};
