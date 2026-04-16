#pragma once

#include "record/rm_pax_file_handle.h"
#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "index/ix.h"
#include "system/sm.h"
#include "fliter.h"
class ChunkScan: public AbstractExecutor
{
private:
    /* data */
    TabMeta tab_meta_;
    std::vector<Condition> conds_;
    PaxFileHandle *fh_;                  // 表的数据文件句柄

    std::vector<ColMeta> cols_;         // scan后生成的记录的字段
    size_t len_;                        // scan后生成的每条记录的长度

    //Rid rid_;
    SmManager *sm_manager_;

    std::vector<TabCol> prj_cols_;

    std::vector<int> conds_idx;

    std::unique_ptr<PaxScan> scan_;

    Chunk chunk;

    std::unordered_map<std::string,int> idx_;

    std::unique_ptr<RmRecord> res_;

    bool is_end_;

    ColType sum_type_;
    size_t sum_idx_;

    std::vector<ConditionMeta> cond_meta;

public:
    ChunkScan(SmManager *sm_manager, std::string tab_name, 
        std::vector<Condition> conds, Context *context,const std::vector<TabCol> &prj_cols )
        {
            sm_manager_ = sm_manager;
            fh_ = sm_manager_->paxhs_[tab_name].get();
            tab_meta_ = sm_manager_->db_.get_table(tab_name);
            conds_ = std::move(conds);
            context_ = context;
            prj_cols_ = prj_cols;
            is_end_ = false;
            if(prj_cols_.size()!=1 || prj_cols_[0].agg_type != AggType::TYPE_SUM)
            {
                throw InternalError("Only Suport One SUM(Col)");
            }
            cols_.push_back(*tab_meta_.get_col(prj_cols_[0].col_name));
            if(prj_cols_[0].agg_type==AggType::TYPE_COUNT)len_=4;
            else len_=cols_[0].len;

            for(auto &co:conds_)
            {
                ConditionMeta temp_meta;
                temp_meta.op = co.op;
                if(!idx_.count(co.lhs_col.col_name))
                {
                    auto meta_ = tab_meta_.get_col(co.lhs_col.col_name);
                    ChunkData data_(fh_->get_file_hdr().file_hdr.num_records_per_page,meta_->type,meta_->len);
                    chunk.datas_.push_back(std::move(data_));
                    chunk.idxs_.push_back(meta_-tab_meta_.cols.begin());
                    idx_[co.lhs_col.col_name]=chunk.idxs_.size()-1;
                    temp_meta.left_type = meta_->type;
                    temp_meta.lhs_len = meta_->len;
                    temp_meta.lhs_idx = chunk.idxs_.size()-1;
                }

                if(!co.is_rhs_val && !idx_.count(co.rhs_col.col_name))
                {
                    auto meta_ = tab_meta_.get_col(co.rhs_col.col_name);
                    ChunkData data_(fh_->get_file_hdr().file_hdr.num_records_per_page,meta_->type,meta_->len);
                    chunk.datas_.push_back(std::move(data_));
                    chunk.idxs_.push_back(meta_-tab_meta_.cols.begin());
                    idx_[co.rhs_col.col_name]=chunk.idxs_.size()-1;
                    temp_meta.reft_type = meta_->type;
                    temp_meta.rhs_len = meta_->len;
                    temp_meta.rhs_idx = chunk.idxs_.size()-1;
                    temp_meta.is_rhs_val = false;
                }
                if(co.is_rhs_val)
                {
                    temp_meta.reft_type = co.rhs_val.type;
                    temp_meta.rhs_data_ = co.rhs_val.raw->data;
                    temp_meta.is_rhs_val = true;
                }
                cond_meta.push_back(temp_meta);
            }
            if(!idx_.count(prj_cols_[0].col_name))
            {
                auto meta_ = tab_meta_.get_col(prj_cols_[0].col_name);
                ChunkData data_(fh_->get_file_hdr().file_hdr.num_records_per_page,meta_->type,meta_->len);
                chunk.datas_.push_back(std::move(data_));
                chunk.idxs_.push_back(meta_-tab_meta_.cols.begin());
                idx_[prj_cols_[0].col_name]=chunk.idxs_.size()-1;
                sum_type_ = meta_->type;
                sum_idx_=chunk.idxs_.size()-1;
            }else
            {
                sum_type_ = chunk.chunkdata(idx_[prj_cols_[0].col_name]).type_;
                sum_idx_ = idx_[prj_cols_[0].col_name];
            }
            len_ = 4;
        }
    ~ChunkScan() = default;
    size_t tupleLen() const override { return len_; };
    const std::vector<ColMeta> &cols() const override { return cols_; }
    ColMeta get_col_offset(const TabCol &target) {
        ColMeta col = *tab_meta_.get_col(target.col_name);
        return col; 
    };  

    void beginTuple() override 
    {
        scan_ = std::make_unique<PaxScan>(fh_);
        int int_sum=0;
        float float_sum = 0;
        size_t chunk_size = chunk.datas_.size();
        bool add_;
        std::vector<char> valid_mask(fh_->get_file_hdr().file_hdr.num_records_per_page, 1);
        while (!scan_->is_end())
        {
            scan_->next_chunk(chunk);
            //只做SUM
            for(const auto &meta:cond_meta)
            { 
                auto& col_data = chunk.datas_[meta.lhs_idx];
                for (int i = 0; i < chunk.rows(); i++) {
                    if (!valid_mask[i]) continue;  // 跳过已失效行
                    char* left = col_data.get_data(i);
                    char* right = meta.is_rhs_val?meta.rhs_data_:chunk.datas_[meta.rhs_idx].get_data(i);
                    int res = Fliter::compare(left, right, 
                                     meta.left_type, meta.reft_type, 
                                     meta.lhs_len);
                    Fliter::check_op(res, meta.op);
                    valid_mask[i] = res==0?0:1;
                }
            } 
            auto& sum_col = chunk.datas_[sum_idx_];
            for (int i = 0; i < chunk.rows(); i++) 
            {
                if (valid_mask[i]==1) 
                {
                    if (sum_type_ == ColType::TYPE_INT) {
                        int_sum += sum_col.get_int(i);
                    } else {
                        float_sum += sum_col.get_float(i);
                    }
                }
                else valid_mask[i]=1;
        }     
        }
        
        if(sum_type_ == ColType::TYPE_INT)
        {
            int *t = new int(int_sum);
            res_ = std::make_unique<RmRecord>(4,(char*)t);
            delete t;
        }
        else
        {
            float *f = new float(float_sum);
            res_ = std::make_unique<RmRecord>(4,(char*)f);
            delete f;
        }
    }
    void nextTuple() override 
    {}
    bool is_end() const override 
    { 
        return is_end_;
    }
    std::unique_ptr<RmRecord> Next() override 
    {
        return std::move(res_);
    }
    Rid &rid() override {}
};
