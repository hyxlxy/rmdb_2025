#pragma once
#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "index/ix.h"
#include "system/sm.h"
#include "fliter.h"

class SemiJoinExecutor : public AbstractExecutor {
private:
    std::shared_ptr<AbstractExecutor> left_;
    std::shared_ptr<AbstractExecutor> right_;
    std::vector<Condition> conds_;

    size_t len_;
    std::vector<ColMeta> cols_;
    std::vector<std::unique_ptr<RmRecord>> right_records_;
    bool isend;
    Fliter *fliter_;
    size_t right_position;
    std::unique_ptr<RmRecord> left_tuple_;
public:
    SemiJoinExecutor(std::unique_ptr<AbstractExecutor> left, std::unique_ptr<AbstractExecutor> right, 
                            std::vector<Condition> conds)
    {
        left_ = std::move(left);
        right_ = std::move(right);
        len_ = left_->tupleLen() + right_->tupleLen();
        cols_ = left_->cols();
        auto right_cols = right_->cols();
        for (auto &col : right_cols) {
            col.offset += left_->tupleLen();
        }

        cols_.insert(cols_.end(), right_cols.begin(), right_cols.end());
        isend = false;
        conds_ = std::move(conds);

        size_t left_len = left_->tupleLen();
        std::vector<MiniCol> cond_cols;
        for(auto &co:conds_)
        {
            auto left_it = cols_.begin();
            auto right_it = cols_.begin();
            bool left_find = false;
            bool right_find = false;
            //因为是一定能找到的
            while (right_it!=cols_.end())
            {
                if(!left_find&&left_it->tab_name == co.lhs_col.tab_name
                &&left_it->name == co.lhs_col.col_name)
                {
                    left_find = true;
                }
                else if(!left_find)left_it++;

                if(!right_find&&right_it->tab_name == co.rhs_col.tab_name
                &&right_it->name == co.rhs_col.col_name)
                {
                    right_find = true;
                }
                else if(!right_find) right_it++;

                if(left_find && right_find)break;
            }
            
            if(left_it == cols_.end())
            {
                throw InternalError("Condition Left Col:"+
                co.lhs_col.col_name+" Not Find");
            }
            MiniCol temp1;
            temp1.type = left_it->type;
            temp1.offset = left_it->offset;
            temp1.len = left_it->len;
            //TOOD:
            

            cond_cols.push_back(temp1);

            if(right_it == cols_.end())
            {
                throw InternalError("Condition right Col:"+
                co.rhs_col.col_name+" Not Find");
            }
            MiniCol temp2;
            temp2.type = right_it->type;
            temp2.offset = right_it->offset-left_len;
            temp2.len = right_it->len;
            cond_cols.push_back(temp2);
        }

        fliter_ = new Fliter(conds_,cond_cols);
        right_position = 0;

        //右表为空
        // if(right_->is_end())
        // {
        //     throw InternalError("Semi Join,Right Table Can Not Be Empty");
        // }
    }

    bool get_next_tuple()
   {
    while(!left_->is_end())
    {
       // left_tuple_ = std::move(left_->record());
        while (right_position<right_records_.size())
        {   
            if(fliter_->do_predict(left_tuple_,right_records_[right_position]))return false;
            right_position++;
        }
        right_position = 0;
        left_->nextTuple();
        if(!left_->is_end())left_tuple_ = std::move(left_->Next());
    }
    return true;
   }

    void beginTuple() override 
    {
        left_->beginTuple();
        for(right_->beginTuple();!right_->is_end();right_->nextTuple())
        {
            right_records_.emplace_back(std::move(right_->Next()));
        }
        if(!left_->is_end())left_tuple_ = std::move(left_->Next());
        isend  = get_next_tuple();
    }
    
    void nextTuple()override
    {
        isend = get_next_tuple();
    }
    bool is_end()const override
    {
        return isend;
    }

    std::unique_ptr<RmRecord> Next() override
    {
        if(isend)return nullptr;
        auto right = right_records_[right_position++].get();
        auto res_ = std::make_unique<RmRecord>(len_);
        memcpy(res_->data,left_tuple_->data,left_->tupleLen());
        memcpy(res_->data+left_->tupleLen(),right->data,len_-left_->tupleLen());
        if(!left_->is_end())
        {
            left_->nextTuple();
            left_tuple_=std::move(left_->Next());
        }
        else isend = true;
        return res_;
    }

    const std::vector<ColMeta> &cols() const override{ return cols_; }
    size_t tupleLen() const override{ return left_->tupleLen() + right_->tupleLen(); }
    Rid &rid() override { return _abstract_rid; }

    ColMeta get_col_offset(const TabCol &target)
    {
        for (auto col : cols_)
        {
            if (col.tab_name == target.tab_name && col.name == target.col_name)
            {
                return col;
            }
        }
        return ColMeta();
    }
};