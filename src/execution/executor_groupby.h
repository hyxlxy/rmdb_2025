#pragma once

#include "execution/fliter.h"
#include "execution/executor_agg.h"
class GroupByExecutor : public AbstractExecutor
{
private:
    std::unique_ptr<AbstractExecutor> prev_;
    int len_;
    std::vector<ColMeta> groupby_col_;
    std::vector<Condition> having_cond_;
    bool father_is_agg_;

    std::vector<std::vector<std::unique_ptr<RmRecord>>> all_record;
    std::vector<std::vector<std::unique_ptr<RmRecord>>> group_record; // 每一层是每一个分组信息
    std::vector<std::unique_ptr<RmRecord>> res_record;

    size_t position;
public:
    GroupByExecutor(std::unique_ptr<AbstractExecutor> prev,std::vector<ColMeta> groupby_col,
        std::vector<Condition> having_cond,bool father_is_agg)
        :prev_(std::move(prev)),groupby_col_(std::move(groupby_col)),
        having_cond_(std::move(having_cond))
    {
        len_ = prev_->tupleLen();
        father_is_agg_ = father_is_agg;
        position = 0;
    }
    virtual std::string getType() { return "GroupByExecutor"; }
    ~GroupByExecutor() = default;
    void beginTuple() override
    {
        for(prev_->beginTuple();!prev_->is_end();prev_->nextTuple())
        {
            auto record = prev_->Next();
            bool is_find = true;

            for (size_t i = 0; i < all_record.size(); i++)
            {
                is_find = true;
                for (auto &gr_meta : groupby_col_)
                {
                    if (ix_compare(record->data + gr_meta.offset,
                                   all_record[i][0]->data + gr_meta.offset, gr_meta.type, gr_meta.len) == 0)
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
                    all_record[i].push_back(std::move(record));
                    break;
                }
            }
            if (!is_find || all_record.size() == 0)
            {
                // 如果没找到 新建并推入all_record

                std::vector<std::unique_ptr<RmRecord>> temp_row;
                temp_row.push_back(std::move(record));
                all_record.push_back(std::move(temp_row));
            }
        }

        //调整顺序
        if (all_record.size() > 0)
        {
            std::vector<bool> is_move(all_record.size(), false);
            group_record.push_back(std::move(all_record[0]));
            is_move[0] = true;
            auto first_col = groupby_col_[0];
            bool is_set = false;

            while (std::find(is_move.begin(), is_move.end(), false) != is_move.end())
            {
                is_set = set_group_by(is_move, group_record, all_record, first_col, is_set);
            }

            // 处理 n-1列的group by
            std::vector<std::vector<std::unique_ptr<RmRecord>>> temp_group;
            for (size_t j = 1; j < groupby_col_.size(); j++)
            {
                temp_group.clear();
                auto ge = groupby_col_[j];

                std::vector<int> split_index = get_pre_gr_split(group_record, groupby_col_[j - 1]); // 找到上一次group_record的组别的分割点
                int left = 0;
                int right = 0;

                for (size_t i = 0; i < split_index.size() - 1; i++)
                {
                    left = split_index[i];
                    right = split_index[i + 1];
                    std::vector<std::vector<std::unique_ptr<RmRecord>>> split_record = split_record_by_index(left, right, group_record); // 根据分割得到子组
                    std::vector<bool> split_move(split_record.size(), false);
                    std::vector<std::vector<std::unique_ptr<RmRecord>>> split_group; // 子组的排列
                    bool split_is_set = false;
                    split_group.push_back(std::move(split_record[0]));
                    split_move[0] = true;

                    while (std::find(split_move.begin(), split_move.end(), false) != split_move.end())
                    {
                        split_is_set = set_group_by(split_move, split_group, split_record, ge, split_is_set);
                    }
                    // 每执行完一次,就推入我们的 group_record
                    for (size_t k = 0; k < split_group.size(); k++)
                    {
                        temp_group.push_back(std::move(split_group[k]));
                    }
                }
                group_record = std::move(temp_group);
            }
        }

        std::vector<Condition> no_agg_cond;
        // 顺序写反了 但是也可以操作 用一个向量来维护正确的层数
        std::vector<int> right_index(group_record.size(), 1);
        for(auto &co:having_cond_)
        {
            if(co.lhs_col.agg_type!=AggType::TYPE_NONE)
            {
                AggType t= co.lhs_col.agg_type;
                ColMeta rel_col;
                ColMeta col_meta;

                for(const auto &F:prev_->cols())
                {
                    if(F.tab_name == co.lhs_col.tab_name && F.name == co.lhs_col.col_name)
                    {
                        col_meta = F;
                    }
                }
                if(t == AggType::TYPE_COUNT)
                {
                    rel_col.type = ColType::TYPE_INT;
                    rel_col.len = sizeof(int);
                }else
                {
                    rel_col.type = col_meta.type;
                    rel_col.len = col_meta.len;
                }
                

                for(size_t i=0;i<group_record.size();++i)
                {
                    auto cur_re = AggregationScanExecutor::get_agg_res(
                        group_record[i],t,col_meta,rel_col
                    );

                    int res = Fliter::compare(cur_re->data,co.rhs_val.raw->data,rel_col.type,
                    co.rhs_val.type,rel_col.len);

                    Fliter::check_op(res,co.op);

                    if(res == 0)
                    {
                        right_index[i] = 0;
                    }
                }
            }
            else
            {
                no_agg_cond.push_back(co);
            }
        }

                if(having_cond_.size() == 0)
        {
            for(size_t i=0;i<group_record.size();++i)
            {
                for(size_t j =0;j<group_record[i].size();++j)
                {
                    res_record.push_back(std::move(group_record[i][j]));
                    if (!father_is_agg_)
                    {
                        break;
                    }
                }
            }
        }else
        {
            std::vector<MiniCol> minico;
            
            for(auto &nc:no_agg_cond)
            {
                for(const auto &F:prev_->cols())
                {
                    if(F.tab_name == nc.lhs_col.tab_name && F.name == nc.lhs_col.col_name)
                    {
                        MiniCol temp_minicol;
                        temp_minicol.len = F.len;
                        temp_minicol.offset = F.offset;
                        temp_minicol.type = F.type;
                        minico.push_back(temp_minicol);
                    }
                    if(!nc.is_rhs_val)
                    {
                        if(F.tab_name == nc.rhs_col.tab_name && F.name == nc.rhs_col.col_name)
                        {
                            MiniCol temp_minicol;
                            temp_minicol.len = F.len;
                            temp_minicol.offset = F.offset;
                            temp_minicol.type = F.type;
                            minico.push_back(temp_minicol);
                        }
                    }
                }
            }

            Fliter *f = new Fliter(no_agg_cond,minico);
            for(size_t i=0;i<group_record.size();++i)
            {
                if(right_index[i] == 1)
                {
                    for(size_t j=0;j<group_record[i].size();++j)
                    {
                        if(f->do_predict(group_record[i][j]))
                        {
                            res_record.push_back(std::move(group_record[i][j]));
                        }
                        if(!father_is_agg_)break;
                    }
                }
            }
            delete f;
        }
    }

    void nextTuple() override
    {
        position++;
    }
    std::unique_ptr<RmRecord> Next() override
    {
        return std::move(res_record[position]);
    }
    Rid &rid() override { return _abstract_rid; }
    bool is_end() const { return (position == res_record.size()); }
    const std::vector<ColMeta> &cols() const
    {
        return prev_->cols();
    }








    bool set_group_by(std::vector<bool> &move_index_, std::vector<std::vector<std::unique_ptr<RmRecord>>> &group_record_,
                      std::vector<std::vector<std::unique_ptr<RmRecord>>> &all_record_,
                      ColMeta group_col_meta_, bool set_or_get)
    {
        // 将还没有move到 group_record 的 根据 col_meta 移动到 group_col
        int current_size = group_record_.size() - 1;
        for (size_t i = 0; i < all_record_.size(); i++)
        {
            if (!move_index_[i])
            {
                if (set_or_get == true)
                {
                    group_record_.push_back(std::move(all_record_[i]));
                    move_index_[i] = true;
                    return false;
                }
                else
                {
                    if (ix_compare(group_record_[current_size][0]->data + group_col_meta_.offset,
                                   all_record_[i][0]->data + group_col_meta_.offset, group_col_meta_.type, group_col_meta_.len) == 0)
                    {
                        group_record_.push_back(std::move(all_record_[i]));
                        move_index_[i] = true;
                    }
                }
            }
        }
        return true;
    }

    std::vector<int> get_pre_gr_split(const std::vector<std::vector<std::unique_ptr<RmRecord>>> &pre_record, ColMeta pre_gr_col)
    {
        std::vector<int> res;
        res.push_back(0);
        int cur_index = 1;

        for (size_t i = 0; i < pre_record.size() - 1; i++)
        {
            if (ix_compare(pre_record[i][0]->data + pre_gr_col.offset, pre_record[i + 1][0]->data + pre_gr_col.offset, pre_gr_col.type, pre_gr_col.len) == 0)
            {
                cur_index++;
            }
            else
            {
                res.push_back(cur_index);
                // cur_index = 1;
                cur_index++;
            }
        }
        res.push_back(pre_record.size());
        return res;
    }

    std::vector<std::vector<std::unique_ptr<RmRecord>>> split_record_by_index(int pre, int end, std::vector<std::vector<std::unique_ptr<RmRecord>>> &gr_re)
    {
        // 将 [pre,end) 的转化为新的数组返回回去
        std::vector<std::vector<std::unique_ptr<RmRecord>>> res;
        for (int i = pre; i < end; i++)
        {
            res.push_back(std::move(gr_re[i]));
        }
        return res;
    }
};
