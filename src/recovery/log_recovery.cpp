/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "log_recovery.h"
#include "../record/rm_manager.h"
#include <unistd.h>
#include <set>

/**
 * @description: analyze阶段，需要获得脏页表（DPT）和未完成的事务列表（ATT）
 */
void RecoveryManager::analyze()
{
        // 第一步 检测是否有静态检测点文件,读取最后一个静态检测点
        // 读取文件的offset
        int file_offset = 0;

        // 读取检查点文件
        int checkpoint_offset = 0;
        bool has_checkpoint = false;
        if (disk_manager_->is_file(CHECK_POINT_NAME)) {
            int fd = disk_manager_->get_file_fd(CHECK_POINT_NAME);
            char *last_check_point = new char[sizeof(int)];

            // 使用直接的文件读取而不是read_page
            lseek(fd, 0, SEEK_SET);
            ssize_t bytes_read = read(fd, last_check_point, sizeof(int));
            if (bytes_read == sizeof(int)) {
                checkpoint_offset = *(int *)last_check_point;
                has_checkpoint = true;
            }

            disk_manager_->close_file(fd);
            delete []last_check_point;
        }

        // 修复的恢复策略：
        // 检查点的作用是标记已持久化的数据，但在恢复时我们仍需要重放所有已提交的操作
        // 因为磁盘上的数据可能在崩溃时丢失或不完整
        file_offset = 0;

        // 检查数据库元数据是否正确加载

        // 检查当前工作目录


        // 如果元数据为空，尝试重新加载
        if (sm_manager_->get_table_count() == 0) {
            sm_manager_->reload_metadata();
        }

        // 静态检测点里面我们存储的是一个
        // 好像仅仅靠静态检测点的 offset并不能很好的加快性能

        // 读取日志信息
        char *buffer_log = new char[LOG_BUFFER_SIZE];
        // 每一次读取的offset
        int buffer_offset = 0;

        // 读取的字节数
        int read_bytes = 0;

        current_log_size = file_offset;
        
        need_to_fix=false;

        // 检查日志文件是否存在
        if (!disk_manager_->is_file(LOG_FILE_NAME)) {
            delete[] buffer_log;
            return;
        }

        // 获取日志文件大小
        int log_file_size = disk_manager_->get_file_size(LOG_FILE_NAME);

        while (1)
        {
                memset(buffer_log, 0, LOG_BUFFER_SIZE);
                buffer_offset = 0;
                read_bytes = disk_manager_->read_log(buffer_log, LOG_BUFFER_SIZE, file_offset);
                if (read_bytes < 0)
                {
                        throw InternalError("READ LOG ERROR !");
                }

                // current_log_size += read_bytes;

                while (buffer_offset + OFFSET_LOG_TOT_LEN + sizeof(uint32_t) < read_bytes)
                {
                        uint32_t log_tot_len_ = *reinterpret_cast<uint32_t *>(buffer_log + buffer_offset + OFFSET_LOG_TOT_LEN);
                        if (log_tot_len_ + buffer_offset > read_bytes)
                        {
                                break;
                        }
                        else
                        {
                                LogType log_type = *reinterpret_cast<LogType *>(buffer_log + buffer_offset + OFFSET_LOG_TYPE);
                                switch (log_type)
                                {
                                case LogType::begin:
                                {
                                        BeginLogRecord *begin_log = new BeginLogRecord();
                                        // 反序列化 取出记录中的值
                                        begin_log->deserialize(buffer_log + buffer_offset);
                                        lsn_offset_[begin_log->lsn_] = file_offset;
                                        lsn_len_[begin_log->lsn_] = begin_log->log_tot_len_;
                                        buffer_offset += begin_log->log_tot_len_;
                                        file_offset += begin_log->log_tot_len_;
                                        undo_map_[begin_log->log_tid_].push_back(begin_log->lsn_);
                                        delete begin_log;
                                        break;
                                }
                                case LogType::commit:
                                {
                                        CommitLogRecord *commit_log = new CommitLogRecord();
                                        commit_log->deserialize(buffer_log + buffer_offset);
                                        lsn_offset_[commit_log->lsn_] = file_offset;
                                        lsn_len_[commit_log->lsn_] = commit_log->log_tot_len_;
                                        buffer_offset += commit_log->log_tot_len_;
                                        file_offset += commit_log->log_tot_len_;
                                        undo_map_.erase(commit_log->log_tid_);
                                        delete commit_log;
                                        break;
                                }
                                case LogType::ABORT:
                                {
                                        AbortLogRecord *abort_log = new AbortLogRecord();
                                        abort_log->deserialize(buffer_log + buffer_offset);
                                        lsn_offset_[abort_log->lsn_] = file_offset;
                                        lsn_len_[abort_log->lsn_] = abort_log->log_tot_len_;
                                        buffer_offset += abort_log->log_tot_len_;
                                        file_offset += abort_log->log_tot_len_;
                                        undo_map_.erase(abort_log->log_tid_);
                                        delete abort_log;
                                        break;
                                }
                                case LogType::INSERT:
                                {
                                        InsertLogRecord *insert_log = new InsertLogRecord();
                                        insert_log->deserialize(buffer_log + buffer_offset);
                                        lsn_offset_[insert_log->lsn_] = file_offset;
                                        lsn_len_[insert_log->lsn_] = insert_log->log_tot_len_;
                                        buffer_offset += insert_log->log_tot_len_;
                                        file_offset += insert_log->log_tot_len_;
                                        undo_map_[insert_log->log_tid_].push_back(insert_log->lsn_);
                                        redo_list_.emplace_back(insert_log->lsn_);
                                        delete insert_log;
                                        break;
                                }
                                case LogType::DELETE:
                                {
                                        DeleteLogRecord *delete_log = new DeleteLogRecord();
                                        delete_log->deserialize(buffer_log + buffer_offset);
                                        lsn_offset_[delete_log->lsn_] = file_offset;
                                        lsn_len_[delete_log->lsn_] = delete_log->log_tot_len_;
                                        buffer_offset += delete_log->log_tot_len_;
                                        file_offset += delete_log->log_tot_len_;
                                        undo_map_[delete_log->log_tid_].push_back(delete_log->lsn_);
                                        redo_list_.emplace_back(delete_log->lsn_);
                                        delete delete_log;
                                        break;
                                }
                                case LogType::UPDATE:
                                {
                                        UpdateLogRecord *update_log = new UpdateLogRecord();
                                        update_log->deserialize(buffer_log + buffer_offset);
                                        lsn_offset_[update_log->lsn_] = file_offset;
                                        lsn_len_[update_log->lsn_] = update_log->log_tot_len_;
                                        buffer_offset += update_log->log_tot_len_;
                                        file_offset += update_log->log_tot_len_;
                                        undo_map_[update_log->log_tid_].push_back(update_log->lsn_);
                                        redo_list_.emplace_back(update_log->lsn_);
                                        delete update_log;
                                        break;
                                }

                                default:
                                        break;
                                }
                        }
                }
                if (read_bytes < LOG_BUFFER_SIZE)
                {
                        break;
                }
        }

        if (redo_list_.size() > 0 || undo_map_.size() > 0)
        {
                is_recovery = true;
        }
        delete[] buffer_log;
}

/**
 * @description: 重做所有未落盘的操作
 */
void RecoveryManager::redo()
{
        for (auto &redo_lsn : redo_list_)
        {
                int log_len = lsn_len_[redo_lsn];
                char *buffer = new char[log_len];
                memset(buffer, 0, log_len);
                disk_manager_->read_log(buffer, log_len, lsn_offset_[redo_lsn]);
                LogType log_type = *(LogType *)buffer;
                switch (log_type)
                {
                case LogType::INSERT:
                {
                        InsertLogRecord *insert_log = new InsertLogRecord();
                        insert_log->deserialize(buffer);
                        std::string tab_name(insert_log->table_name_, insert_log->table_name_size_);

                        // 检查表是否存在于数据库元数据中
                        if (!sm_manager_->db_.is_table(tab_name)) {
                                delete insert_log;
                                continue; // 跳过这个日志记录，继续处理下一个
                        }

                        // 检查表文件句柄是否存在，如果不存在则打开文件
                        if (sm_manager_->fhs_.find(tab_name) == sm_manager_->fhs_.end()) {
                                sm_manager_->fhs_[tab_name] = sm_manager_->get_rm_manager()->open_file(tab_name);
                        }
                        auto fh = sm_manager_->fhs_[tab_name].get();
                        // Rid rid = fh->insert_record(insert_log->insert_value_.data, nullptr);
                        // 插入 指定位置,并且 解决 page_no=1的情况

                        if (fh->get_file_hdr().num_pages <= insert_log->rid_.page_no)
                        {
                                auto new_file_hand = fh->create_new_page_handle();
                                buffer_pool_manager_->unpin_page(new_file_hand.page->get_page_id(), true);
                        }

                        // 在恢复过程中，我们直接插入记录，不需要特殊的Context
                        fh->insert_record(insert_log->rid_, insert_log->insert_value_.data);

                        // 立即刷新页面到磁盘
                        buffer_pool_manager_->flush_page(PageId{fh->GetFd(), insert_log->rid_.page_no});
                        Rid rid = insert_log->rid_;
                        for (auto &index : sm_manager_->db_.get_table(tab_name).indexes)
                        {
                                std::string index_name = sm_manager_->get_ix_manager()->get_index_name(tab_name, index.second.cols);

                                // 检查索引句柄是否存在，如果不存在则打开索引
                                if (sm_manager_->ihs_.find(index_name) == sm_manager_->ihs_.end()) {
                                        sm_manager_->ihs_[index_name] = sm_manager_->get_ix_manager()->open_index(index_name);
                                }
                                auto ih = sm_manager_->ihs_[index_name].get();
                                char *key = new char[index.second.col_tot_len];
                                int offset = 0;
                                for (size_t i = 0; i < index.second.col_num; ++i)
                                {
                                        memcpy(key + offset, insert_log->insert_value_.data + index.second.cols[i].offset, index.second.cols[i].len);
                                        offset += index.second.cols[i].len;
                                }
                                ih->insert_entry(key, rid, nullptr);
                                delete[] key;
                        }
                        delete insert_log;
                        break;
                }
                case LogType::DELETE:
                {
                        DeleteLogRecord *delete_log = new DeleteLogRecord();
                        delete_log->deserialize(buffer);

                        // 检查表名是否有效
                        if (delete_log->table_name_size_ == 0 || delete_log->table_name_ == nullptr) {
                                delete delete_log;
                                continue;
                        }

                        std::string tab_name(delete_log->table_name_, delete_log->table_name_size_);

                        // 检查表是否存在于数据库元数据中
                        if (tab_name.empty() || !sm_manager_->db_.is_table(tab_name)) {
                                delete delete_log;
                                continue; // 跳过这个日志记录，继续处理下一个
                        }

                        // 检查表文件句柄是否存在，如果不存在则打开文件
                        if (sm_manager_->fhs_.find(tab_name) == sm_manager_->fhs_.end()) {
                                sm_manager_->fhs_[tab_name] = sm_manager_->get_rm_manager()->open_file(tab_name);
                        }
                        auto fh = sm_manager_->fhs_[tab_name].get();

                        try {
                                // 检查记录是否存在
                                auto existing_record = fh->get_record(delete_log->rid_, nullptr);
                                fh->delete_record(delete_log->rid_, nullptr);
                        } catch (const std::exception& e) {
                                // 记录不存在或删除失败，继续处理
                        }
                        for (auto &index : sm_manager_->db_.get_table(tab_name).indexes)
                        {
                                std::string index_name = sm_manager_->get_ix_manager()->get_index_name(tab_name, index.second.cols);
                                // 检查索引句柄是否存在，如果不存在则打开索引
                                if (sm_manager_->ihs_.find(index_name) == sm_manager_->ihs_.end()) {
                                        sm_manager_->ihs_[index_name] = sm_manager_->get_ix_manager()->open_index(index_name);
                                }
                                auto ih = sm_manager_->ihs_[index_name].get();
                                char *key = new char[index.second.col_tot_len];
                                int offset = 0;
                                for (size_t i = 0; i < index.second.col_num; ++i)
                                {
                                        memcpy(key + offset, delete_log->delete_value_.data + index.second.cols[i].offset, index.second.cols[i].len);
                                        offset += index.second.cols[i].len;
                                }
                                ih->delete_entry(key, nullptr);
                                delete[] key;
                        }
                        delete delete_log;
                        break;
                }
                case LogType::UPDATE:
                {
                        UpdateLogRecord *update_log = new UpdateLogRecord();
                        update_log->deserialize(buffer);
                        std::string tab_name(update_log->table_name_, update_log->table_name_size_);

                        // 检查表是否存在于数据库元数据中
                        if (!sm_manager_->db_.is_table(tab_name)) {
                                delete update_log;
                                continue; // 跳过这个日志记录，继续处理下一个
                        }

                        // 检查表文件句柄是否存在，如果不存在则打开文件
                        if (sm_manager_->fhs_.find(tab_name) == sm_manager_->fhs_.end()) {
                                sm_manager_->fhs_[tab_name] = sm_manager_->get_rm_manager()->open_file(tab_name);
                        }
                        auto fh = sm_manager_->fhs_[tab_name].get();

                        try {
                                // 检查记录是否存在
                                auto existing_record = fh->get_record(update_log->rid_, nullptr);

                                fh->update_record(update_log->rid_, update_log->new_value_.data, nullptr);
                        } catch (const std::exception& e) {
                                // 记录不存在或更新失败，继续处理
                        }
                        for (auto &index : sm_manager_->db_.get_table(tab_name).indexes)
                        {
                                std::string index_name = sm_manager_->get_ix_manager()->get_index_name(tab_name, index.second.cols);
                                // 检查索引句柄是否存在，如果不存在则打开索引
                                if (sm_manager_->ihs_.find(index_name) == sm_manager_->ihs_.end()) {
                                        sm_manager_->ihs_[index_name] = sm_manager_->get_ix_manager()->open_index(index_name);
                                }
                                auto ih = sm_manager_->ihs_[index_name].get();
                                char *insert_key = new char[index.second.col_tot_len];
                                char *delete_key = new char[index.second.col_tot_len];
                                int offset = 0;
                                for (size_t i = 0; i < index.second.col_num; ++i)
                                {
                                        memcpy(insert_key + offset, update_log->new_value_.data + index.second.cols[i].offset, index.second.cols[i].len);
                                        memcpy(delete_key + offset, update_log->old_value_.data + index.second.cols[i].offset, index.second.cols[i].len);
                                        offset += index.second.cols[i].len;
                                }
                                ih->delete_entry(delete_key, nullptr);
                                ih->insert_entry(insert_key, update_log->rid_, nullptr);
                                delete[] insert_key;
                                delete[] delete_key;
                        }
                        delete update_log;
                        break;
                }
                default:
                        break;
                }

                delete[] buffer;
        }
}

/**
 * @description: 回滚未完成的事务
 */
void RecoveryManager::undo()
{
        /**
         * 对于undo的事务，我们该怎么确保其在恢复后不在让其重复恢复
         *
         */
        for (auto it = undo_map_.begin(); it != undo_map_.end(); ++it)
        {
                auto &list_queue = it->second;
                while (!list_queue.empty())
                {
                        lsn_t lsn = list_queue.back();
                        int log_len = lsn_len_[lsn];
                        char *buffer = new char[log_len];
                        memset(buffer, 0, log_len);
                        disk_manager_->read_log(buffer, log_len, lsn_offset_[lsn]);
                        LogType log_type = *(LogType *)buffer;
                        switch (log_type)
                        {
                        case LogType::begin:
                        {
                                break;
                        }
                        case LogType::INSERT:
                        {
                                InsertLogRecord *insert_log = new InsertLogRecord();
                                insert_log->deserialize(buffer);

                                // 检查表名是否有效
                                if (insert_log->table_name_size_ == 0 || insert_log->table_name_ == nullptr) {
                                        delete insert_log;
                                        continue;
                                }

                                std::string tab_name(insert_log->table_name_, insert_log->table_name_size_);

                                // 检查表是否存在于数据库元数据中
                                if (tab_name.empty() || !sm_manager_->db_.is_table(tab_name)) {
                                        delete insert_log;
                                        continue; // 跳过这个日志记录，继续处理下一个
                                }

                                // 检查表文件句柄是否存在，如果不存在则打开文件
                                if (sm_manager_->fhs_.find(tab_name) == sm_manager_->fhs_.end()) {
                                        sm_manager_->fhs_[tab_name] = sm_manager_->get_rm_manager()->open_file(tab_name);
                                }
                                auto fh = sm_manager_->fhs_[tab_name].get();
                                fh->delete_record(insert_log->rid_, nullptr);
                                for (auto &index : sm_manager_->db_.get_table(tab_name).indexes)
                                {
                                        std::string index_name = sm_manager_->get_ix_manager()->get_index_name(tab_name, index.second.cols);
                                        // 检查索引句柄是否存在，如果不存在则打开索引
                                        if (sm_manager_->ihs_.find(index_name) == sm_manager_->ihs_.end()) {
                                                sm_manager_->ihs_[index_name] = sm_manager_->get_ix_manager()->open_index(index_name);
                                        }
                                        auto ih = sm_manager_->ihs_[index_name].get();
                                        char *key = new char[index.second.col_tot_len];
                                        int offset = 0;
                                        for (size_t i = 0; i < index.second.col_num; ++i)
                                        {
                                                memcpy(key + offset, insert_log->insert_value_.data + index.second.cols[i].offset, index.second.cols[i].len);
                                                offset += index.second.cols[i].len;
                                        }
                                        ih->delete_entry(key, nullptr);
                                        delete[] key;
                                }
                                delete insert_log;
                                break;
                        }
                        case LogType::DELETE:
                        {
                                DeleteLogRecord *delete_log = new DeleteLogRecord();
                                delete_log->deserialize(buffer);

                                // 检查表名是否有效
                                if (delete_log->table_name_size_ == 0 || delete_log->table_name_ == nullptr) {
                                        delete delete_log;
                                        continue;
                                }

                                std::string tab_name(delete_log->table_name_, delete_log->table_name_size_);

                                // 检查表是否存在于数据库元数据中
                                if (tab_name.empty() || !sm_manager_->db_.is_table(tab_name)) {
                                        delete delete_log;
                                        continue; // 跳过这个日志记录，继续处理下一个
                                }

                                // 检查表文件句柄是否存在，如果不存在则打开文件
                                if (sm_manager_->fhs_.find(tab_name) == sm_manager_->fhs_.end()) {
                                        sm_manager_->fhs_[tab_name] = sm_manager_->get_rm_manager()->open_file(tab_name);
                                }
                                auto fh = sm_manager_->fhs_[tab_name].get();
                                // Rid rid = fh->insert_record(delete_log->delete_value_.data, nullptr);
                                //  fh->insert_record(delete_log->rid_, delete_log->delete_value_.data);
                                if (fh->get_file_hdr().num_pages <= delete_log->rid_.page_no)
                                {
                                        auto new_page_handl = fh->create_new_page_handle();
                                        buffer_pool_manager_->unpin_page(new_page_handl.page->get_page_id(), true);
                                }
                                fh->insert_record(delete_log->rid_, delete_log->delete_value_.data);
                                Rid rid = delete_log->rid_;
                                for (auto &index : sm_manager_->db_.get_table(tab_name).indexes)
                                {
                                        std::string index_name = sm_manager_->get_ix_manager()->get_index_name(tab_name, index.second.cols);
                                        // 检查索引句柄是否存在，如果不存在则打开索引
                                        if (sm_manager_->ihs_.find(index_name) == sm_manager_->ihs_.end()) {
                                                sm_manager_->ihs_[index_name] = sm_manager_->get_ix_manager()->open_index(index_name);
                                        }
                                        auto ih = sm_manager_->ihs_[index_name].get();
                                        char *key = new char[index.second.col_tot_len];
                                        int offset = 0;
                                        for (size_t i = 0; i < index.second.col_num; ++i)
                                        {
                                                memcpy(key + offset, delete_log->delete_value_.data + index.second.cols[i].offset, index.second.cols[i].len);
                                                offset += index.second.cols[i].len;
                                        }
                                        ih->insert_entry(key, rid, nullptr);
                                        delete[] key;
                                }
                                delete delete_log;
                                break;
                        }
                        case LogType::UPDATE:
                        {
                                UpdateLogRecord *update_log = new UpdateLogRecord();
                                update_log->deserialize(buffer);

                                // 检查表名是否有效
                                if (update_log->table_name_size_ == 0 || update_log->table_name_ == nullptr) {
                                        delete update_log;
                                        continue;
                                }

                                std::string tab_name(update_log->table_name_, update_log->table_name_size_);

                                // 检查表是否存在于数据库元数据中
                                if (tab_name.empty() || !sm_manager_->db_.is_table(tab_name)) {
                                        delete update_log;
                                        continue; // 跳过这个日志记录，继续处理下一个
                                }

                                // 检查表文件句柄是否存在，如果不存在则打开文件
                                if (sm_manager_->fhs_.find(tab_name) == sm_manager_->fhs_.end()) {
                                        sm_manager_->fhs_[tab_name] = sm_manager_->get_rm_manager()->open_file(tab_name);
                                }
                                auto fh = sm_manager_->fhs_[tab_name].get();
                                fh->update_record(update_log->rid_, update_log->old_value_.data, nullptr);
                                for (auto &index : sm_manager_->db_.get_table(tab_name).indexes)
                                {
                                        std::string index_name = sm_manager_->get_ix_manager()->get_index_name(tab_name, index.second.cols);
                                        // 检查索引句柄是否存在，如果不存在则打开索引
                                        if (sm_manager_->ihs_.find(index_name) == sm_manager_->ihs_.end()) {
                                                sm_manager_->ihs_[index_name] = sm_manager_->get_ix_manager()->open_index(index_name);
                                        }
                                        auto ih = sm_manager_->ihs_[index_name].get();
                                        char *insert_key = new char[index.second.col_tot_len];
                                        char *delete_key = new char[index.second.col_tot_len];
                                        int offset = 0;
                                        for (size_t i = 0; i < index.second.col_num; ++i)
                                        {
                                                memcpy(delete_key + offset, update_log->new_value_.data + index.second.cols[i].offset, index.second.cols[i].len);
                                                memcpy(insert_key + offset, update_log->old_value_.data + index.second.cols[i].offset, index.second.cols[i].len);
                                                offset += index.second.cols[i].len;
                                        }
                                        ih->delete_entry(delete_key, nullptr);
                                        ih->insert_entry(insert_key, update_log->rid_, nullptr);
                                        delete[] insert_key;
                                        delete[] delete_key;
                                }
                                delete update_log;
                                break;
                        }
                        default:
                                break;
                        }
                        list_queue.pop_back();
                        delete[] buffer;
                }
        }

        if (is_recovery)
        {
            // 刷新所有表文件的页面到磁盘
            for (auto& [table_name, fh] : sm_manager_->fhs_) {
                sm_manager_->get_bpm()->flush_all_pages(fh->GetFd());
            }
            sm_manager_->set_check_point();
        }
}

