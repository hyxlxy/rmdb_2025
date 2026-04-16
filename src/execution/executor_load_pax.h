#pragma once

#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "index/ix.h"
#include "system/sm.h"
#include <sstream>

class LoadPaxExecutor : public AbstractExecutor
{
private:
    TabMeta tab_;           // 表的元数据
    std::string file_name_; // 文件名
    std::string tab_name_;  // 表名称
    SmManager *sm_manager_;
    PaxFileHandle *fh_; // 表的数据文件句柄

    IndexMeta index_; // 操作索引
    bool have_index;
    IxIndexHandle *ix_;

    size_t affect_row_;

    std::unordered_map<std::string,int> index_idx_;
public:
    LoadPaxExecutor(SmManager *sm_manager, const std::string &file_name, const std::string &tab_name, Context *context)
    {
        sm_manager_ = sm_manager;
        tab_ = sm_manager_->db_.get_table(tab_name);
        file_name_ = file_name;
        tab_name_ = tab_name;

        // 我们认为一个表只有一个索引
        have_index = false;
        if (tab_.indexes.size() > 0)
        {
            have_index = true;
            index_ = tab_.indexes[0];
            ix_ = sm_manager_->ihs_.at(sm_manager_->get_ix_manager()->get_index_name(tab_.name, index_.cols)).get();
            for(auto &ix:index_.cols)
            {
                index_idx_[ix.name]=tab_.get_col(ix.name)-tab_.cols.begin();
            }
        }

        have_index = false;

        fh_ = sm_manager_->paxhs_.at(tab_name).get();
        context_ = context;

        affect_row_=0;
    }
        Rid &rid() override { return _abstract_rid; }
        std::unique_ptr<RmRecord> Next() override
        {
            //int recore_size = fh_->get_file_hdr().file_hdr.record_size; // 记录大小
        // int num_page = fh_->get_file_hdr().num_pages;      // 页数
            int num_records_per_page = fh_->get_file_hdr().file_hdr.num_records_per_page;
            int first_free_page_no = fh_->get_file_hdr().file_hdr.first_free_page_no;
            Chunk chunk;
            for(size_t i=0;i<tab_.cols.size();i++)
            {
                ChunkData data_(num_records_per_page,tab_.cols[i].type,tab_.cols[i].len);
                chunk.datas_.push_back(std::move(data_));
                chunk.idxs_.push_back(i);
            }

            if (first_free_page_no == -1)
            {
                fh_->create_new_page_handle();
                sm_manager_->get_bpm(tab_name_)->unpin_page({fh_->GetFd(), fh_->get_file_hdr().file_hdr.first_free_page_no}, true);
                first_free_page_no = fh_->get_file_hdr().file_hdr.first_free_page_no;
            }

            int index_init_len = 1;
            if (have_index)
            {
                index_init_len = index_.col_tot_len;
            }
            char *index_data = new char[index_init_len * num_records_per_page];
            int index_data_offset = 0;

            std::ifstream csv_data(file_name_, std::ios::in);
            std::vector<std::string> words; // 声明一个字符串向量
            std::string line;

            if (!csv_data.is_open())
            {
                throw InternalError("Error: opening file fail");
            }

            std::istringstream sin;
            std::getline(csv_data, line); // 头信息
            int j = 0;
            int real_read = 0;
            char *p; // use strtok to splic the val

            while (1)
            {
                auto page_hdl = fh_->fetch_page_handle(first_free_page_no);
                int current_read_line = num_records_per_page - page_hdl.page_hdr->num_records;
                real_read = 0;
                chunk.reset_data();
                for (int i = 0; i < current_read_line; i++)
                {
                    if (!std::getline(csv_data, line))
                    {
                        break;
                    }
                    real_read++;
                    p = strtok((char *)line.c_str(), ",");
                    j = 0;
                    while (p)
                    {
                        //chunk.datas_[j].append_data(p,1);
                        auto &col = tab_.cols[j];
                        switch (col.type)
                        {
                            case ColType::TYPE_INT:
                            {
                                *(int *)(chunk.get_value(j,i)) = atoi(p);
                                chunk.datas_[j].count_++;
                                break;
                            }
                            case ColType::TYPE_FLOAT:
                            {
                                *(float *)(chunk.get_value(j,i)) = atof(p);
                                chunk.datas_[j].count_++;
                                break;
                            }
                            case ColType::TYPE_STRING:
                            {
                                int copy_len = strlen(p);
                                memcpy(chunk.get_value(j,i), p, copy_len);
                                chunk.datas_[j].count_++;
                                break;
                            }
                        }
                        j++;
                        p = strtok(NULL, ",");
                    }
                }
                if (real_read == 0)
                {
                    sm_manager_->get_bpm(tab_name_)->unpin_page(page_hdl.page->get_page_id(),true);
                    break;
                }
                int get_pos = (page_hdl.page_hdr->num_records == 0) ? 0 : page_hdl.page_hdr->num_records + 1;
                // fh_->load_with_chunk(chunk,get_pos);
                int copy_size = 0;
                for(size_t i=0;i<chunk.datas_.size();i++)
                {
                    char *begin_ = page_hdl.get_data(get_pos,i,copy_size);
                    char *copy_data_ = chunk.get_value(i,0);
                    memcpy(begin_,copy_data_,copy_size*chunk.rows());
                }
                // 设置 bitmap
                for (int i = get_pos; i < real_read + get_pos; i++)
                {
                    Bitmap::set(page_hdl.bitmap, i);
                }
                if (have_index)
                {
                    memset(index_data, 0, num_records_per_page * index_.col_tot_len);
                    index_data_offset = 0;
                    for (int k = 0; k < real_read; k++)
                    {
                        for (auto &index_col : index_.cols)
                        {
                            memcpy(index_data+index_data_offset,chunk.get_value(index_idx_[index_col.name],k),index_col.len);
                            index_data_offset += index_col.len;
                        }
                    }
                    ix_->insert_one_page_data(index_data, first_free_page_no, get_pos,
                                                            real_read + get_pos);
                }

                            // 设置新页面
                page_hdl.page_hdr->num_records += real_read;
                assert(page_hdl.page_hdr->num_records <= num_records_per_page);
                if (page_hdl.page_hdr->num_records == num_records_per_page)
                {
                    fh_->create_new_page_handle();
                    first_free_page_no = fh_->get_file_hdr().file_hdr.first_free_page_no;
                    sm_manager_->get_bpm(tab_name_)->unpin_page({fh_->GetFd(), first_free_page_no}, true);
                    
                }
                sm_manager_->get_bpm(tab_name_)->unpin_page(page_hdl.page->get_page_id(), true);

                affect_row_+=real_read;
            }
            delete[] index_data;
            csv_data.close();
            fh_->add_record_num(affect_row_);
            sm_manager_->get_rm_manager()->flush_table_page(fh_);
            if (have_index)
            {
                sm_manager_->get_ix_manager()->flush_index_page(ix_);
            }
            return nullptr;
        }
};