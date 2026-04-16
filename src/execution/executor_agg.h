#pragma once
#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "index/ix.h"
#include "system/sm.h"
#include "../deps/thread_pool/thread_pool.h"

class AggregationScanExecutor : public AbstractExecutor
{
private:
    std::unique_ptr<AbstractExecutor> prev_; // 投影节点的儿子节点
    int len_;
    std::vector<int> pre_col_pos_; //pos index
    std::vector<TabCol> tab_cols_; // select cols
    std::vector<ColMeta> tab_col_metas_;//cols meta
    std::vector<ColMeta> pre_cols;
    std::vector<ColMeta> groupby_col;
    bool has_groupby;
    
    std::vector<std::unique_ptr<RmRecord>> all_record_;
    //std::unique_ptr<RmRecord> res_record_;
    std::vector<std::unique_ptr<RmRecord>> res_record_;
    size_t _pos;

    bool all_count_;
    bool all_table_;
    size_t all_table_count_;

    int scan_count = 0;
public:
    AggregationScanExecutor(std::unique_ptr<AbstractExecutor> prev,const std::vector<TabCol> &tab_cols,
        std::vector<int> pre_col_pos,const std::vector<ColMeta> &tab_col_metas,std::vector<ColMeta> groupby_col_
    ,bool all_count = false,bool all_table = false,size_t all_table_count=0)
    :prev_(std::move(prev)),pre_col_pos_(std::move(pre_col_pos)),tab_cols_(tab_cols)
    ,tab_col_metas_(tab_col_metas),groupby_col(groupby_col_)
    {
        len_ = 0;
        for(auto &M:tab_col_metas_)len_+=M.len;
        pre_cols = prev_->cols();
        //res_record_ = std::make_unique<RmRecord>(len_);
        _pos = 0;
        all_count_ = all_count;
        all_table_ = all_table;
        all_table_count_ = all_table_count;

        has_groupby = (groupby_col.size()>0);
    }
    ~AggregationScanExecutor() = default;
    virtual std::string getType() { return "AggregationScanExecutor"; };
    void beginTuple() override
    {
        if(has_groupby)
        {
            for(prev_->beginTuple();!prev_->is_end();prev_->nextTuple())
            {
                all_record_.push_back(std::move(prev_->Next()));
            }
            std::vector<std::vector<std::unique_ptr<RmRecord>>> group_data;
                        for (size_t i = 0; i < all_record_.size(); ++i)
            {
                bool is_find = true;
                for (size_t j = 0; j < group_data.size(); ++j)
                {
                    is_find = true;
                    for (auto &gr_meta : groupby_col)
                    {
                        if (ix_compare(all_record_[i]->data + gr_meta.offset,
                            group_data[j][0]->data + gr_meta.offset, gr_meta.type, gr_meta.len) == 0)
                            {
                                continue;
                            }
                        else
                            {
                                is_find = false;
                            }
                    }
                    if (is_find)
                    {
                        group_data[j].push_back(std::move(all_record_[i]));
                        break;
                    }
                }
                if (!is_find || group_data.size() == 0)
                {
                    // 如果没找到 新建并推入all_record
                    std::vector<std::unique_ptr<RmRecord>> temp_row;
                    temp_row.push_back(std::move(all_record_[i]));
                    group_data.push_back(std::move(temp_row));
                }
            }

                        for (size_t k = 0; k < group_data.size(); ++k)
            {
                auto record = std::make_unique<RmRecord>(len_);
                for (size_t i = 0; i < tab_cols_.size(); i++)
                {
                    if(tab_cols_[i].agg_type == AggType::TYPE_NONE)
                    {
                        memcpy(record->data+tab_col_metas_[i].offset,
                            group_data[k][0]->data+pre_cols[pre_col_pos_[i]].offset,
                            tab_col_metas_[i].len);
                    }
                    else
                    {
                        auto cur_re = get_agg_res(
                            group_data[k],tab_cols_[i].agg_type,pre_cols[pre_col_pos_[i]],tab_col_metas_[i]
                        );
                        memcpy(record->data+tab_col_metas_[i].offset,cur_re->data,tab_col_metas_[i].len);
                    }
                }
                res_record_.push_back(std::move(record));
            }
        }
        else
        {
            if(all_count_&&all_table_)
            {
                auto _record = std::make_unique<RmRecord>(len_);
                int t = 0;
                while (t<len_)
                {
                    memcpy(_record->data+(4*t),(char*)&all_table_count_,4);
                    t+=4;
                }
                res_record_.push_back(std::move(_record));
                return;
            }
            else if(all_count_)
            {
                auto _record = std::make_unique<RmRecord>(len_);
                int count_ = 0;
                for(prev_->beginTuple();!prev_->is_end();prev_->nextTuple())count_++;
                int t = 0;
                while (t<len_)
                {
                    memcpy(_record->data+(4*t),(char*)&count_,4);
                    t+=4;
                }
                res_record_.push_back(std::move(_record));
                return;
            }

            // for(prev_->beginTuple();!prev_->is_end();prev_->nextTuple())
            // {
            //     all_record_.push_back(std::move(prev_->Next()));
            // }
            // auto _record = std::make_unique<RmRecord>(len_);
            // if(all_record_.size() == 0)
            // {
            //         for(size_t i=0;i<tab_cols_.size();++i)
            //         {
            //             if(tab_cols_[i].agg_type!=AggType::TYPE_COUNT)
            //             {
            //                 return;
            //             }
            //             int *ze = new int(0);
            //             memcpy(_record->data+tab_col_metas_[i].offset,ze,sizeof(int));
            //             delete ze;
            //         }
            //         res_record_.push_back(std::move(_record));
            //         return;
            // }
            // for(size_t i=0;i<tab_cols_.size();++i)
            // {
            //     auto _temp_re =get_agg_res(
            //         all_record_,tab_cols_[i].agg_type,pre_cols[pre_col_pos_[i]],tab_col_metas_[i]
            //     );
            //     memcpy(_record->data+tab_col_metas_[i].offset,_temp_re->data,tab_col_metas_[i].len);
            // }
            auto _record = std::make_unique<RmRecord>(len_);
            memset(_record->data,0,len_);
            prev_->beginTuple();
            if(prev_->is_end())
            {
                for(size_t i=0;i<tab_cols_.size();++i)
                {
                   if(tab_cols_[i].agg_type!=AggType::TYPE_COUNT)
                    {
                        return;
                    }
                    int *ze = new int(0);
                    memcpy(_record->data+tab_col_metas_[i].offset,ze,sizeof(int));
                    delete ze;
                }
                res_record_.push_back(std::move(_record));
                return;
            }
            else
            {
                for(;!prev_->is_end();prev_->nextTuple())
                {
                    set_agg(prev_->Next(),_record->data);
                    scan_count++;
                }
                for(size_t i=0;i<tab_cols_.size();i++)
                {
                    if(tab_cols_[i].agg_type==AggType::TYPE_AVG)
                    {
                        if(pre_cols[pre_col_pos_[i]].type==ColType::TYPE_INT)
                        {
                            int res = *(int*)(_record->data+tab_col_metas_[i].offset);
                            float flres_ = (float)res/scan_count;
                            memcpy(_record->data+tab_col_metas_[i].offset,&flres_,4);
                        }
                        else
                        {
                            float flres_ = *(float*)(_record->data+tab_col_metas_[i].offset);
                            flres_ = flres_/scan_count;
                            memcpy(_record->data+tab_col_metas_[i].offset,&flres_,4);
                        }
                    }
                }
            }
            res_record_.push_back(std::move(_record));
        }
    }
    void nextTuple() override
    {

        _pos++;
    }
    std::unique_ptr<RmRecord> Next() override
    {
        return std::move(res_record_[_pos]);
    }
    Rid &rid() override { return _abstract_rid; }
    bool is_end() const { return (_pos == res_record_.size()); }
    const std::vector<ColMeta> &cols() const
    {
        return tab_col_metas_;
    }

    /**
     * @param records_:记录数组
     * @param agg_type:求取的聚合函数类别
     * @param tab_col:涉及到的列的相关信息,需要包括:类型,偏移量,长度
     * @param rel_col:只需涉及到返回结果的长度(由于COUNT所需长度与列长度不一致导致需要此字段)
     */
    static std::unique_ptr<RmRecord> get_agg_res
    (
        const std::vector<std::unique_ptr<RmRecord>> &records_,
        AggType agg_type,
        const ColMeta &tab_col,
        const ColMeta &rel_col
    )
    {
        auto res = std::make_unique<RmRecord>(rel_col.len);
        ThreadPool pool(std::thread::hardware_concurrency());
        const size_t min_batch_size = 10000;
        const size_t min_thread_ = 1;
        const size_t num_batches = std::max(min_thread_, std::min((unsigned long)std::thread::hardware_concurrency(), records_.size() / min_batch_size));
        const size_t batch_size = records_.size() / num_batches;
            switch (agg_type)
            {
            case AggType::TYPE_AVG:
            {
                if(tab_col.type == TYPE_INT)
                {
                    std::vector<std::future<std::pair<int, size_t>>> futures;
                    std::atomic<int> total(0);
                    std::atomic<size_t> count(0);
                    for (size_t i = 0; i < num_batches; ++i) 
                    {
                        size_t start = i * batch_size;
                        size_t end = (i == num_batches - 1) ? records_.size() : (i + 1) * batch_size;
                        futures.emplace_back(pool.enqueue([&records_, &tab_col, start, end] {
                        int local_total = 0;
                        for (size_t j = start; j < end; ++j) {
                            int cur_val = *(int*)(records_[j]->data+tab_col.offset);
                            local_total += cur_val;
                        }
                        return std::make_pair(local_total, end - start);
                        }));
                    }
                    for(auto& future : futures) {
                        auto result = future.get();
                        total += result.first;
                        count += result.second;
                    }
                    float val = static_cast<float>(total) / count;
                    memcpy(res->data, &val, sizeof(float));
                }else if(tab_col.type == TYPE_FLOAT)
                {
                    std::vector<std::future<std::pair<float, size_t>>> futures;
                    std::atomic<float> total(0.0f);
                    std::atomic<size_t> count(0);
                    for (size_t i = 0; i < num_batches; ++i) {
                    size_t start = i * batch_size;
                    size_t end = (i == num_batches - 1) ? records_.size() : (i + 1) * batch_size;
                    futures.emplace_back(pool.enqueue([&records_, &tab_col, start, end] {
                        float local_total = 0;
                        for (size_t j = start; j < end; ++j) {
                            float cur_val = *(float*)(records_[j]->data+tab_col.offset);
                            local_total += cur_val;
                        }
                        return std::make_pair(local_total, end - start);
                    }));
                    for (auto& future : futures) {
                        auto result = future.get();
                        total.store(result.first+total.load());
                        count += result.second;
                    }
                    float avg = total / count;
                    memcpy(res->data, &avg, sizeof(float));
                }
                }
            }break;
            case AggType::TYPE_COUNT:
            {
                size_t size = records_.size();
                int *new_int = new int(size);
                memcpy(res->data,new_int,sizeof(int));
                delete new_int;
            }break;
            case AggType::TYPE_MAX:
            {
                res->SetData(records_[0]->data+tab_col.offset);
                for(size_t i=1;i<records_.size();++i)
                {
                    if(ix_compare(records_[i]->data+tab_col.offset,res->data,tab_col.type,tab_col.len
                    )>0)
                    {
                        res->SetData(records_[i]->data+tab_col.offset);
                    }
                }
            }break;
            case AggType::TYPE_MIN:
            {
                res->SetData(records_[0]->data+tab_col.offset);
                for(size_t i=1;i<records_.size();++i)
                {
                    if(ix_compare(records_[i]->data+tab_col.offset,res->data,tab_col.type,tab_col.len
                    )<0)
                    {
                        res->SetData(records_[i]->data+tab_col.offset);
                    }
                }
            }break;
            case AggType::TYPE_SUM:
            {
                if(tab_col.type == TYPE_INT)
                {
                    int total = 0;
                    for(auto &R:records_)
                    {
                        int cur_val = *(int*)(R->data+tab_col.offset);
                        total+=cur_val;
                    }
                    int *new_int = new int(total);
                    memcpy(res->data,new_int,sizeof(int));
                    delete new_int;
                }else if(tab_col.type == TYPE_FLOAT)
                {
                    float total = 0;
                    for(auto &R:records_)
                    {
                        float cur_val = *(float*)(R->data+tab_col.offset);
                        total+=cur_val;
                    }
                    float *new_fl = new float(total);
                    memcpy(res->data,new_fl,sizeof(float));
                    delete new_fl;
                }
            }break;
            default:
                break;
            }
            return res;
    }

    template<typename T>
    inline void add(char *res,const char *r)
    {
        *(T*)res=*(T*)res+*(T*)r;
    }

    inline void set_agg(const std::unique_ptr<RmRecord> &re,char *res_data)
    {
        for (size_t i = 0; i < tab_cols_.size(); i++)
        {
            const auto &tab_col = pre_cols[pre_col_pos_[i]];
            const auto &tab_meta = tab_col_metas_[i];
            switch (tab_cols_[i].agg_type)
            {
            case AggType::TYPE_AVG:
            {
                if(tab_col.type==TYPE_INT)add<int>(res_data+tab_meta.offset,re->data+tab_col.offset);
                else add<float>(res_data+tab_meta.offset,re->data+tab_col.offset);
            }break;
            case AggType::TYPE_COUNT:
            {
                int cur_size = *(int*)(res_data+tab_meta.offset);
                *(int*)(res_data+tab_meta.offset)=cur_size+1;
            }break;
            case AggType::TYPE_MAX:
            {
                if(scan_count==0)
                {
                    //res_data+tab_meta.offset=re->data+tab_col.offset;
                    memcpy(res_data+tab_meta.offset,re->data+tab_col.offset,tab_col.len);
                }
                else if(ix_compare(re->data+tab_col.offset,res_data+tab_meta.offset,tab_col.type,tab_col.len)>0)
                {
                    memcpy(res_data+tab_meta.offset,re->data+tab_col.offset,tab_col.len);
                }
            }break;
            case AggType::TYPE_MIN:
            {
                if(scan_count==0)
                {
                    //res_data+tab_meta.offset=re->data+tab_col.offset;
                    memcpy(res_data+tab_meta.offset,re->data+tab_col.offset,tab_col.len);
                }
                else if(ix_compare(re->data+tab_col.offset,res_data+tab_meta.offset,tab_col.type,tab_col.len)<0)
                {
                    memcpy(res_data+tab_meta.offset,re->data+tab_col.offset,tab_col.len);
                }
            }break;
            case AggType::TYPE_SUM:
            {
                if(tab_col.type==TYPE_INT)add<int>(res_data+tab_meta.offset,re->data+tab_col.offset);
                else add<float>(res_data+tab_meta.offset,re->data+tab_col.offset);
            }break;
            default:
                break;
            }
        }
        
    }
};
