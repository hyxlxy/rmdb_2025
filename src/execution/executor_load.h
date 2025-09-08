#pragma once

#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "mvcc_executor_base.h"
#include "index/ix.h"
#include "system/sm.h"
#include <sstream>
#include <chrono>
#include <iostream>

class LoadExecutor : public MVCCExecutorBase
{
private:
    TabMeta tab_; // 表元数据
    std::string file_name_;
    std::string tab_name_;
    SmManager *sm_manager_; // 系统管理器
    RmFileHandle *fh_;      // 文件控制头 对文件数据进行操作
    IndexMeta index_;       // 索引元数据
    IxIndexHandle *ih_;     // 索引控制头
    bool have_index;
    bool loaded_ = false;

    // 叶子节点缓存优化
    IxNodeHandle *cached_leaf_node_ = nullptr;  // 缓存的叶子节点
    bool is_root_locked_ = false;               // 根锁状态
    std::vector<char> last_key_buffer_;         // 上一个插入的key
    bool has_last_key_ = false;                 // 是否有上一个key

    // 绕过缓冲池管理器的批量插入，返回插入记录的RID列表
    std::vector<Rid> insert_records(char *buf, int record_size, int count) // count 是每批次的记录数
    {
        std::vector<Rid> rids;
        rids.reserve(count); // 预分配空间，避免重复扩容

        RmFileHdr file_hdr = fh_->get_file_hdr();
        auto page_no = file_hdr.first_free_page_no;
        if (page_no == -1)
        {
            auto new_page_handle = fh_->create_new_page_handle();
            page_no = new_page_handle.page->get_page_id().page_no;
            file_hdr.first_free_page_no = page_no;                                 // 更新文件头
            fh_->get_bpm()->unpin_page(new_page_handle.page->get_page_id(), true); // 释放新页面
        }

        auto max_record = file_hdr.num_records_per_page; // 每页的插槽数
        int current_read = 0;                            // 当前读取的记录数
        while (count > 0)
        {
            auto page_handle = fh_->fetch_page_handle(page_no);
            int avail = max_record - page_handle.page_hdr->num_records; // 可用的槽
            int pos = page_handle.page_hdr->num_records;                // 直接使用当前记录数作为位置

            // 确保不会复制超过剩余记录数的数据
            int to_load = std::min(avail, count);

            char *data = page_handle.get_slot(pos);
            memcpy(data, buf + current_read * record_size, to_load * record_size); // 将数据拷贝到槽中

            // 设置bitmap并记录真实RID
            for (int i = 0; i < to_load; i++)
            {
                Bitmap::set(page_handle.bitmap, pos + i); // 设置bitmap
                rids.push_back(Rid{page_no, pos + i});    // 记录真实RID
            }

            count -= to_load;                             // 减少剩余记录数
            current_read += to_load;                      // 更新已读取的记录数
            page_handle.page_hdr->num_records += to_load; // 更新页的记录数

            if (page_handle.page_hdr->num_records >= max_record)
            {
                auto temp = fh_->create_new_page_handle();
                page_no = temp.page->get_page_id().page_no;
                fh_->get_bpm()->unpin_page(temp.page->get_page_id(), true); // 释放新页面
            }
            fh_->get_bpm()->unpin_page(page_handle.page->get_page_id(), true); // 释放新页面
        }

        return rids;
    }

    // 简单的批量插入优化：检查键值是否有序
    bool is_key_ordered(const char *key)
    {
        if (!has_last_key_)
        {
            return true;  // 第一个key总是有序的
        }

        // 构建类型和长度向量用于比较
        std::vector<ColType> col_types;
        std::vector<int> col_lens;
        for (const auto &col : index_.cols)
        {
            col_types.push_back(col.type);
            col_lens.push_back(col.len);
        }

        // 比较当前key和上一个key
        int cmp = ix_compare(last_key_buffer_.data(), key, col_types, col_lens);
        return cmp <= 0;  // 当前key >= 上一个key
    }

    // 更新最后插入的key
    void update_last_key(const char *key)
    {
        if (last_key_buffer_.size() < (size_t)index_.col_tot_len)
        {
            last_key_buffer_.resize(index_.col_tot_len);
        }
        memcpy(last_key_buffer_.data(), key, index_.col_tot_len);
        has_last_key_ = true;
    }

    // 优化的批量插入索引方法，减少页面fetch/unpin操作
    void batch_insert_index(char *data, int record_size, const std::vector<Rid> &rids, int count)
    {
        if (count <= 0) return;

        int key_size = index_.col_tot_len;
        char *key_buffer = new char[key_size];

        try {
            for (int i = 0; i < count; i++)
            {
                char *current_record = data + (i * record_size);

                // 构建索引键
                char *key_ptr = key_buffer;
                for (size_t j = 0; j < index_.cols.size(); j++)
                {
                    auto &col = index_.cols[j];
                    memcpy(key_ptr, current_record + col.offset, col.len);
                    key_ptr += col.len;
                }

                // 使用缓存优化的插入
                optimized_insert_entry(key_buffer, rids[i]);
            }
        } catch (...) {
            delete[] key_buffer;
            cleanup_cached_node();
            throw;
        }

        delete[] key_buffer;
    }

    // 优化的单个索引插入，利用叶子节点缓存
    void optimized_insert_entry(const char *key, const Rid &value)
    {
        // 如果没有缓存节点，或者key不适合当前缓存节点
        if (!cached_leaf_node_ || !can_insert_to_cached_node(key))
        {
            // 清理旧缓存
            cleanup_cached_node();

            // 查找新的叶子节点并缓存
            auto [leaf_node, is_root_locked] = ih_->find_leaf_page(key, Operation::INSERT, nullptr, false);
            cached_leaf_node_ = leaf_node;
            is_root_locked_ = is_root_locked;
        }

        // 直接在缓存的叶子节点中插入
        int old_size = cached_leaf_node_->get_size();
        auto [new_size, pos] = cached_leaf_node_->insert(key, value);

        // 如果插入失败（重复键），清理缓存
        if (new_size == old_size)
        {
            cleanup_cached_node();
            return;
        }

        // 如果插入在第一个位置，需要更新父节点
        if (pos == 0)
        {
            ih_->maintain_parent(cached_leaf_node_);
        }

        // 如果节点满了，需要分裂，清理缓存让下次重新查找
        if (cached_leaf_node_->isFull())
        {
            // 处理节点分裂
            auto new_sibling_node = ih_->split(cached_leaf_node_);

            // 维护最右叶子节点
            if (cached_leaf_node_->get_page_no() == ih_->file_hdr_->last_leaf_)
            {
                ih_->file_hdr_->last_leaf_ = new_sibling_node->get_page_no();
            }

            // 插入到父节点
            ih_->insert_into_parent(cached_leaf_node_, new_sibling_node->get_key(0), new_sibling_node, nullptr);

            // 释放新兄弟节点
            ih_->buffer_pool_manager_->unpin_page(new_sibling_node->get_page_id(), true);
            delete new_sibling_node;

            // 清理缓存，下次重新查找
            cleanup_cached_node();
        }
    }

    // 检查key是否可以插入到缓存的节点
    bool can_insert_to_cached_node(const char *key)
    {
        if (!cached_leaf_node_ || cached_leaf_node_->isFull())
            return false;

        // 简单策略：如果key大于等于节点中的最后一个key，可能适合插入
        if (cached_leaf_node_->get_size() == 0)
            return true;

        // 检查key的插入位置
        int pos = cached_leaf_node_->lower_bound(key);
        return pos <= cached_leaf_node_->get_size();
    }

    // 清理缓存的叶子节点
    void cleanup_cached_node()
    {
        if (cached_leaf_node_)
        {
            ih_->buffer_pool_manager_->unpin_page(cached_leaf_node_->get_page_id(), true);
            delete cached_leaf_node_;
            cached_leaf_node_ = nullptr;
        }
        if (is_root_locked_)
        {
            ih_->root_latch_.unlock();
            is_root_locked_ = false;
        }
    }

public:
    LoadExecutor(SmManager *sm_manager, const std::string &file_name, const std::string &tab_name, Context *context)
    {
        sm_manager_ = sm_manager;
        tab_ = sm_manager_->db_.get_table(tab_name);
        file_name_ = file_name;
        tab_name_ = tab_name;
        have_index = false; // 默认不含索引
        ih_ = nullptr;      // 初始化索引句柄为空

        if (tab_.indexes.size() > 0) // 如果有索引
        {
            have_index = true;
            index_ = tab_.indexes.begin()->second; // 取第一个索引
            std::string index_name = sm_manager_->get_ix_manager()->get_index_name(tab_name, index_.cols);
            if (sm_manager_->ihs_.find(index_name) != sm_manager_->ihs_.end())
            {
                ih_ = sm_manager_->ihs_.at(index_name).get(); // 获得当前索引的控制头
            }
            else
            {
                have_index = false; // 索引句柄不存在，禁用索引
            }
        }
        fh_ = sm_manager_->fhs_.at(tab_name).get(); // 获取表的文件控制头
        context_ = nullptr;                         // 批量加载模式：不使用context，避免所有日志和事务开销
    };

    ~LoadExecutor()
    {
        cleanup_cached_node();
    }

    Rid &rid() override { return _abstract_rid; }
    std::unique_ptr<RmRecord> Next() override
    {
        // 开始计时
        auto start_time = std::chrono::high_resolution_clock::now();
        int total_records_loaded = 0;

        int record_size = fh_->get_file_hdr().record_size;                   // 获取记录大小
        int num_records_per_page = fh_->get_file_hdr().num_records_per_page; // 每页记录数

        // 缓冲区 + 内存预热
        const int TARGET_BUFFER_SIZE_MB = 128; // 增加到128MB缓冲区
        const int TARGET_BUFFER_SIZE = TARGET_BUFFER_SIZE_MB * 1024 * 1024;
        int optimal_batch_size = std::max(TARGET_BUFFER_SIZE / record_size, num_records_per_page * 16);
        int max_batch_size = optimal_batch_size;

        // 分配对齐的内存以优化缓存性能，只分配一块data缓冲区，后续所有批次直接复用
        std::vector<char> data_buffer;
        if (data_buffer.size() < (size_t)record_size * max_batch_size)
        {
            data_buffer.resize(record_size * max_batch_size);
        }
        char *data = data_buffer.data();

        // 内存预热：触发页面分配，减少后续的缺页中断
        std::ifstream csv_data;

        int index_init_len = 1;
        char *index_data = nullptr;
        char *key_buffer = nullptr;
        if (have_index)
        {
            index_init_len = index_.col_tot_len;
            // 优化：为更大的批处理分配索引数据缓冲区
            index_data = new char[index_init_len * max_batch_size];
            // 分配批量key缓冲区
            key_buffer = new char[index_init_len];
        }

        // 添加异常安全的文件处理
        csv_data.open(file_name_, std::ios::in);
        if (!csv_data.is_open())
        {
            if (index_data)
                delete[] index_data;
            if (key_buffer)
                delete[] key_buffer;
            throw InternalError("LoadExecutor: Failed to open CSV file");
        }

        // 超高性能CSV解析：直接在原始字符串上操作，避免内存分配
        // 修复说明：优化了索引插入流程，先插入数据获得真实RID，再插入索引
        auto ultra_fast_parse = [&](const char *line_data, int line_len, char *record_data) -> bool
        {
            const char *ptr = line_data;
            const char *end = line_data + line_len;
            int col_idx = 0;

            int size = static_cast<int>(tab_.cols.size());
            while (ptr < end && col_idx < size)
            {
                const char *field_start = ptr;
                // 找到字段结束位置
                while (ptr < end && *ptr != ',')
                    ptr++;
                int field_len = ptr - field_start;

                auto &col = tab_.cols[col_idx];
                char *field_ptr = record_data + col.offset;

                if (col.type == ColType::TYPE_INT)
                {
                    *(int *)field_ptr = atoi(field_start);
                }
                else if (col.type == ColType::TYPE_FLOAT)
                {
                    *(float *)field_ptr = atoi(field_start);
                }
                else if (col.type == ColType::TYPE_STRING)
                {
                    int copy_len = std::min(field_len, col.len);
                    if (copy_len > 0)
                        memcpy(field_ptr, field_start, copy_len);
                    if (copy_len < col.len)
                        memset(field_ptr + copy_len, 0, col.len - copy_len);
                }
                col_idx++;
                if (ptr < end)
                    ptr++; // 跳过逗号
            }
            return col_idx == static_cast<int>(tab_.cols.size());
        };

        std::string line;

        // 跳过CSV文件的头行（字段名行）
        if (!std::getline(csv_data, line))
        {
            if (index_data)
                delete[] index_data;
            if (key_buffer)
                delete[] key_buffer;
            csv_data.close();
            throw InternalError("LoadExecutor: Empty CSV file or failed to read header");
        }

        bool csv_eof = false;
        int total_batches_processed = 0;

        while (!csv_eof)
        {
            int batch_size = max_batch_size;
            int has_read = 0;

            // 解析CSV数据到内存缓冲区
            for (int i = 0; i < batch_size; i++)
            {
                if (!std::getline(csv_data, line))
                {
                    csv_eof = true;
                    break;
                }
                char *current_record = data + (has_read * record_size);
                if (ultra_fast_parse(line.c_str(), line.length(), current_record))
                {
                    has_read++;
                }
            }
            if (has_read == 0)
            {
                break;
            }

            // 批量插入数据到表中，获得RID列表
            std::vector<Rid> rids = insert_records(data, record_size, has_read);
            if (have_index)
            {
                // 使用批量插入优化
                batch_insert_index(data, record_size, rids, has_read);
            }
            total_records_loaded += has_read;
            total_batches_processed++;
        }

        // 后期处理
        // data缓冲区为vector，自动释放
        if (index_data)
        {
            delete[] index_data;
        }
        if (key_buffer)
        {
            delete[] key_buffer;
        }
        csv_data.close();

        // 只在最后刷新所有页面到磁盘，避免频繁I/O操作
        // 完全跳过所有日志记录，实现最大性能
        sm_manager_->get_bpm()->flush_all_pages(fh_->GetFd());

        // 结束计时，输出调试信息
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        double seconds = duration.count() / 1000.0;
        double speed = seconds > 0 ? total_records_loaded / seconds : 0;

        // 更新文件头的记录统计
        RmFileHdr file_hdr = fh_->get_file_hdr();
        file_hdr.total_record_count = total_records_loaded;
        file_hdr.count_cache_valid = true;
        // 需要将更新后的文件头写回磁盘
        fh_->update_file_hdr(file_hdr);

        std::cout << "[LoadExecutor] 加载完成: 总耗时 " << seconds << " 秒, 总记录数 " << total_records_loaded << ", 平均速率 " << speed << " 条/秒" << std::endl;

        return nullptr;
    }
};
