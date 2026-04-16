#pragma once
#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"

struct MiniCol
{
    int offset;
    int len;
    ColType type;
};

struct ConditionMeta
{
    int lhs_idx;
    ColType left_type;
    int lhs_len;
    int rhs_idx;
    ColType reft_type;
    int rhs_len;
    char *rhs_data_;
    CompOp op;
    bool is_rhs_val;
};

class Fliter
{
private:
    std::vector<Condition> conds_;
    std::vector<MiniCol> cols_;
public:
    Fliter(const std::vector<Condition> &conds,const std::vector<MiniCol> &cols)
    :conds_(conds),cols_(cols)
    {}
    ~Fliter() = default;
    bool do_predict(const RmRecord *record)
    {
        if(conds_.size()==0)return true;
        char *record_data = record->data;
        ColType left_tp;
        ColType right_tp;
        int cond_col_index = 0;
        for(auto &co:conds_)
        {
            auto &meta = cols_[cond_col_index++];
            __builtin_prefetch(record_data+meta.offset);
            char *left_data = (record_data+meta.offset);
            left_tp = meta.type;
            char *right_data;
            if(co.is_rhs_val)
            {
                right_data = co.rhs_val.raw->data;
                right_tp = co.rhs_val.type;
            }
            else
            {
                auto &meta2 = cols_[cond_col_index++];
                __builtin_prefetch(record_data+meta2.offset);
                right_data = (record_data+meta2.offset);
                right_tp = meta2.type;
            }

            int res = Fliter::compare(left_data,right_data,left_tp,right_tp,meta.len);
            Fliter::check_op(res,co.op);
            if(res == 0)return false;
        }
        return true;
    }

    bool do_predict(const std::unique_ptr<RmRecord> &record)
    {
        return do_predict(record.get());
    }  

    bool do_predict(const std::unique_ptr<RmRecord> &left_,const std::unique_ptr<RmRecord> &right_)
    {  
        if(conds_.size()==0)return true;
        int col_left_index = 0;
        int col_right_index = 1;
        for(auto &co:conds_)
        {
            char *left_data = left_->data+cols_[col_left_index].offset;
            char *right_data = right_->data+cols_[col_right_index].offset;
            int res = Fliter::compare(left_data,right_data,
            cols_[col_left_index].type,cols_[col_right_index].type,cols_[col_left_index].len);
            col_left_index+=2;
            col_right_index+=2;

            Fliter::check_op(res,co.op);
            if(res == 0)return false;
        }
        return true;
    }
    public:
    static int compare(char *left_data, char *right_data, 
    const ColType &left_type, const ColType &right_type, size_t len)
    {
        switch (left_type)
        {
        case ColType::TYPE_INT:
        {
            int left_int = *(int *)left_data;
            if (right_type == ColType::TYPE_INT)
            {
                int right_int = *(int *)right_data;
                return left_int > right_int ? 1 : (left_int < right_int ? -1 : 0);
            }
            else if (right_type == ColType::TYPE_FLOAT)
            {
                float right_float = *(float *)right_data;
                return left_int > right_float ? 1 : (left_int < right_float ? -1 : 0);
            }
        }
        break;
        case ColType::TYPE_FLOAT:
        {
            float left_float = *(float *)left_data;
            if (right_type == ColType::TYPE_FLOAT)
            {
                float right_float = *(float *)right_data;
                return left_float > right_float ? 1 : (left_float < right_float ? -1 : 0);
            }
            if (right_type == ColType::TYPE_INT)
            {
                int right_int = *(int *)right_data;
                return left_float > right_int ? 1 : (left_float < right_int ? -1 : 0);
            }
        }
        break;
        case ColType::TYPE_STRING:
        {
            //if (right_type == ColType::TYPE_STRING)
           // {
               // return memcmp(left_data, right_data, len);
            //}
            return strncmp(left_data, right_data, len);
        }
        break;
        default:
            break;
        }
        return -100;
    }

    inline static void check_op(int &res,CompOp op)
    {
        switch (op)
        {
            case CompOp::OP_EQ:res = (res==0);break;
            case CompOp::OP_GE: res = (res>=0);break;
            case CompOp::OP_GT: res = (res>0);break;
            case CompOp::OP_LE: res = (res<=0);break;
            case CompOp::OP_LT: res = (res<0);break;
            case CompOp::OP_NE: res = (res!=0);break;
            default:
                break;
        }
    }
};


// class ChunkFliter
// {
// private:
//     /* data */
//     using ConditionFunc = std::function<bool(const Chunk&)>;
//     std::vector<ConditionFunc> conds_funcs;
//     std::vector<ConditionFunc> rclco_funcs;
  
// public:
//     ChunkFliter(std::vector<ConditionMeta> co)
//     {
//         // for(auto &c:co)
//         // {
//         //     if(c.is_rhs_val)conds.push_back(std::move(c));
//         //     else rclco.push_back(std::move(c));
//         // }
//         for (auto& c : co) {
//             auto func = create_condition_func(c);
//             if (c.is_rhs_val) {
//                 conds_funcs.push_back(func);
//             } else {
//                 rclco_funcs.push_back(func);
//             }
//         }
        
//     }
//     // bool do_predict(const Chunk &chunk)
//     // {
//         // const auto& chunk_data = chunk.datas_;
//         // for(auto &co:conds)
//         // {
//         //     int res = Fliter::compare(chunk_data[co.lhs_idx].data_, co.rhs_data_, 
//         //                              co.left_type, co.reft_type, 
//         //                              co.lhs_len);
//         //     Fliter::check_op(res, co.op);
//         //     if(res==0)return false;
//         // }

//         // for(auto &co:rclco)
//         // {
//         //     int res = Fliter::compare(chunk_data[co.lhs_idx].data_, chunk_data[co.rhs_idx].data_, 
//         //                              co.left_type, co.reft_type, 
//         //                              co.lhs_len);
//         //     Fliter::check_op(res, co.op);
//         //     if(res==0)return false;
//         // }
//         // return true;

//     // }
//     bool do_predict(const Chunk& chunk) {
//         for (const auto& func : conds_funcs) {
//             if (!func(chunk)) return false;
//         }
//         for (const auto& func : rclco_funcs) {
//             if (!func(chunk)) return false;
//         }
//         return true;
//     }
//     ~ChunkFliter() = default;



//     // 条件函数工厂实现
// ConditionFunc create_condition_func(const ConditionMeta& co) {
//     // 处理字符串类型
//     if (co.left_type == ColType::TYPE_STRING) {
//         // 对于右侧是常量值的情况，创建字符串副本
//         if (co.is_rhs_val) {
//             std::string rhs_str(co.rhs_data_, co.lhs_len);
//             return [=](const Chunk& chunk) {
//                 const char* lhs_str = chunk.datas_[co.lhs_idx].data_;
//                 int res = strncmp(lhs_str, rhs_str.c_str(), co.lhs_len);
//                 return check_op_result(res, co.op);
//             };
//         } 
//         // 右侧是另一列的情况
//         else {
//             return [=](const Chunk& chunk) {
//                 const char* lhs_str = chunk.datas_[co.lhs_idx].data_;
//                 const char* rhs_str = chunk.datas_[co.rhs_idx].data_;
//                 int res = strncmp(lhs_str, rhs_str, co.lhs_len);
//                 return check_op_result(res, co.op);
//             };
//         }
//     }

//     // 处理整数类型
//     if (co.left_type == ColType::TYPE_INT) {
//         // 右侧是常量值
//         if (co.is_rhs_val) {
//             if (co.reft_type == ColType::TYPE_INT) {
//                 int rhs_val = *reinterpret_cast<const int*>(co.rhs_data_);
//                 return [=](const Chunk& chunk) {
//                     int lhs_val = *reinterpret_cast<const int*>(chunk.datas_[co.lhs_idx].data_);
//                     return compare_and_check(lhs_val, rhs_val, co.op);
//                 };
//             } 
//             else if (co.reft_type == ColType::TYPE_FLOAT) {
//                 float rhs_val = *reinterpret_cast<const float*>(co.rhs_data_);
//                 return [=](const Chunk& chunk) {
//                     int lhs_val = *reinterpret_cast<const int*>(chunk.datas_[co.lhs_idx].data_);
//                     return compare_and_check(static_cast<float>(lhs_val), rhs_val, co.op);
//                 };
//             }
//         }
//         // 右侧是另一列
//         else {
//             return [=](const Chunk& chunk) {
//                 int lhs_val = *reinterpret_cast<const int*>(chunk.datas_[co.lhs_idx].data_);
//                 int rhs_val = *reinterpret_cast<const int*>(chunk.datas_[co.rhs_idx].data_);
//                 return compare_and_check(lhs_val, rhs_val, co.op);
//             };
//         }
//     }

//     // 处理浮点数类型
//     if (co.left_type == ColType::TYPE_FLOAT) {
//         // 右侧是常量值
//         if (co.is_rhs_val) {
//             if (co.reft_type == ColType::TYPE_FLOAT) {
//                 float rhs_val = *reinterpret_cast<const float*>(co.rhs_data_);
//                 return [=](const Chunk& chunk) {
//                     float lhs_val = *reinterpret_cast<const float*>(chunk.datas_[co.lhs_idx].data_);
//                     return compare_and_check(lhs_val, rhs_val, co.op);
//                 };
//             } 
//             else if (co.reft_type == ColType::TYPE_INT) {
//                 int rhs_val = *reinterpret_cast<const int*>(co.rhs_data_);
//                 return [=](const Chunk& chunk) {
//                     float lhs_val = *reinterpret_cast<const float*>(chunk.datas_[co.lhs_idx].data_);
//                     return compare_and_check(lhs_val, static_cast<float>(rhs_val), co.op);
//                 };
//             }
//         }
//         // 右侧是另一列
//         else {
//             return [=](const Chunk& chunk) {
//                 float lhs_val = *reinterpret_cast<const float*>(chunk.datas_[co.lhs_idx].data_);
//                 float rhs_val = *reinterpret_cast<const float*>(chunk.datas_[co.rhs_idx].data_);
//                 return compare_and_check(lhs_val, rhs_val, co.op);
//             };
//         }
//     }

//     // 默认返回true
//     return [](const Chunk&) { return true; };
// }


//     // 比较和操作检查模板
// template <typename L, typename R>
// inline bool compare_and_check(L lhs, R rhs, CompOp op) {
//     switch (op) {
//     case CompOp::OP_EQ: return lhs == rhs;
//     case CompOp::OP_GE: return lhs >= rhs;
//     case CompOp::OP_GT: return lhs > rhs;
//     case CompOp::OP_LE: return lhs <= rhs;
//     case CompOp::OP_LT: return lhs < rhs;
//     case CompOp::OP_NE: return lhs != rhs;
//     default: return true;
//     }
// }

// // 字符串操作检查
// inline bool check_op_result(int res, CompOp op) {
//     switch (op) {
//     case CompOp::OP_EQ: return res == 0;
//     case CompOp::OP_GE: return res >= 0;
//     case CompOp::OP_GT: return res > 0;
//     case CompOp::OP_LE: return res <= 0;
//     case CompOp::OP_LT: return res < 0;
//     case CompOp::OP_NE: return res != 0;
//     default: return true;
//     }
// }

// };

// Minimal Chunk type for ChunkFliter (avoids circular include with chunk_scan.h)
struct ColData { char *data_; };
struct Chunk { std::vector<ColData> datas_; };

class ChunkFliter
{
private:
    /* data */
    std::vector<ConditionMeta> conds;
    std::vector<ConditionMeta> rclco;
public:
    ChunkFliter(std::vector<ConditionMeta> co)
    {
        for(auto &c:co)
        {
            if(c.is_rhs_val)conds.push_back(std::move(c));
            else rclco.push_back(std::move(c));
        }
        
    }
    bool do_predict(const Chunk &chunk)
    {
        const auto& chunk_data = chunk.datas_;
        for(auto &co:conds)
        {
            int res = Fliter::compare(chunk_data[co.lhs_idx].data_, co.rhs_data_, 
                                     co.left_type, co.reft_type, 
                                     co.lhs_len);
            // Fliter::check_op(res, co.op);
            // if(res==0)return false;
            if(!check_op(res,co.op))return false;
        }

        for(auto &co:rclco)
        {
            int res = Fliter::compare(chunk_data[co.lhs_idx].data_, chunk_data[co.rhs_idx].data_, 
                                     co.left_type, co.reft_type, 
                                     co.lhs_len);
           if(!check_op(res,co.op))return false;
        }
        return true;
    }
    ~ChunkFliter() = default;

    inline static bool check_op(int res,CompOp op)
    {
        switch (op)
        {
            case CompOp::OP_EQ:return res==0;
            case CompOp::OP_GE:return res>=0;
            case CompOp::OP_GT:return res>0;
            case CompOp::OP_LE:return res<=0;
            case CompOp::OP_LT:return res<0;
            case CompOp::OP_NE:return res!=0;
            default:
                break;
        }
        return true;
    }
};


