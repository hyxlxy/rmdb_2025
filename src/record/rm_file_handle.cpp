/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "rm_file_handle.h"
#include "recovery/log_manager.h"

std::mutex latch_;
/**
 * @description: 获取当前表中记录号为rid的记录
 * @param {Rid&} rid 记录号，指定记录的位置
 * @param {Context*} context
 * @return {unique_ptr<RmRecord>} rid对应的记录对象指针
 */
std::unique_ptr<RmRecord> RmFileHandle::get_record(const Rid &rid, Context *context) const
{
    // Todo:
    // 1. 获取指定记录所在的page handle
    auto page_handle = fetch_page_handle(rid.page_no);
    if (!Bitmap::is_set(page_handle.bitmap, rid.slot_no))
    {
        // 记录不存在，需要unpin页面
        buffer_pool_manager_->unpin_page(page_handle.page->get_page_id(), false);
        throw RecordNotFoundError(rid.page_no, rid.slot_no);
    }
    // 2. 初始化一个指向RmRecord的指针（赋值其内部的data和size）
    char *record_data = page_handle.get_slot(rid.slot_no); // 这个文件可以开始写入数据的地址

    // 创建RmRecord的副本，这样就不依赖页面内存了
    auto record = std::make_unique<RmRecord>(record_data, file_hdr_.record_size);

    // 重要：unpin页面，因为我们已经复制了数据
    buffer_pool_manager_->unpin_page(page_handle.page->get_page_id(), false);

    return record;
}

/**
 * @description: 在当前表中插入一条记录，不指定插入位置
 * @param {char*} buf 要插入的记录的数据
 * @param {Context*} context
 * @return {Rid} 插入的记录的记录号（位置）
 */
Rid RmFileHandle::insert_record(char *buf, Context *context)
{
    // 使用更细粒度的锁来避免死锁
    std::unique_lock<std::mutex> lock(latch_);

    Page *page = nullptr;
    RmPageHandle page_handle{&file_hdr_, page};

    // 获取或创建页面
    if (file_hdr_.first_free_page_no == -1)
    {
        page_handle = create_new_page_handle();
    }
    else
    {
        page_handle = fetch_page_handle(file_hdr_.first_free_page_no);
    }

    // 查找空闲slot
    int slot_no = Bitmap::first_bit(false, page_handle.bitmap, file_hdr_.num_records_per_page);

    if (slot_no == -1)
    {
        // 当前页面已满，需要创建新页面
        buffer_pool_manager_->unpin_page(page_handle.page->get_page_id(), false);
        page_handle = create_new_page_handle();
        slot_no = Bitmap::first_bit(false, page_handle.bitmap, file_hdr_.num_records_per_page);

        if (slot_no == -1)
        {
            buffer_pool_manager_->unpin_page(page_handle.page->get_page_id(), false);
            throw std::runtime_error("No free slot found even in new page");
        }
    }

    Rid rid = Rid{.page_no = page_handle.page->get_page_id().page_no, .slot_no = slot_no};

    // 执行实际的插入操作
    std::memcpy(page_handle.get_slot(slot_no), buf, file_hdr_.record_size);

    // 原子性地更新所有元数据（关键修复点：确保原子性）
    Bitmap::set(page_handle.bitmap, slot_no);
    page_handle.page_hdr->num_records++;

    // 检查页面是否已满，原子性地更新空闲页链表
    if (page_handle.page_hdr->num_records == file_hdr_.num_records_per_page)
    {
        file_hdr_.first_free_page_no = page_handle.page_hdr->next_free_page_no;
        page_handle.page_hdr->next_free_page_no = -1;
    }

    // 在成功插入后记录日志
    if (context && context->log_mgr_)
    {
        RmRecord insert_record(buf, file_hdr_.record_size);
        Rid rid_copy = rid;

        if (!DISABLE_DEBUG_OUTPUT) {
            if (context->current_table_name_.empty())
            {
                std::cerr << "Warning: current_table_name_ is empty when creating INSERT log" << std::endl;
            }
        }

        InsertLogRecord *insert_log = new InsertLogRecord(
            context->txn_ ? context->txn_->get_transaction_id() : INVALID_TXN_ID,
            insert_record,
            rid_copy,
            context->current_table_name_);

        context->log_mgr_->add_log_to_buffer(insert_log);
        delete insert_log;
    }

    buffer_pool_manager_->unpin_page(page_handle.page->get_page_id(), true);

    // 释放锁（RAII会自动处理）
    return rid;
}

/**
 * @description: 在当前表中的指定位置插入一条记录
 * @param {Rid&} rid 要插入记录的位置
 * @param {char*} buf 要插入记录的数据
 */
void RmFileHandle::insert_record(const Rid &rid, char *buf)
{
    std::scoped_lock lock(latch_); // 添加锁保护
    RmPageHandle page_handle = fetch_page_handle(rid.page_no);
    if (Bitmap::is_set(page_handle.bitmap, rid.slot_no))
    {
        buffer_pool_manager_->unpin_page(page_handle.page->get_page_id(), false);
        throw std::runtime_error("Slot already occupied");
    }

    // 原子性地执行所有更新操作
    std::memcpy(page_handle.get_slot(rid.slot_no), buf, file_hdr_.record_size);
    Bitmap::set(page_handle.bitmap, rid.slot_no);
    page_handle.page_hdr->num_records++;

    // 检查页面是否变满，需要更新空闲页链表（修复关键问题）
    if (page_handle.page_hdr->num_records == file_hdr_.num_records_per_page)
    {
        // 从空闲链表中移除此页面
        if (file_hdr_.first_free_page_no == rid.page_no)
        {
            file_hdr_.first_free_page_no = page_handle.page_hdr->next_free_page_no;
            page_handle.page_hdr->next_free_page_no = -1;
        }
    }

    buffer_pool_manager_->unpin_page(page_handle.page->get_page_id(), true);
}

/**
 * @description: 删除记录文件中记录号为rid的记录
 * @param {Rid&} rid 要删除的记录的记录号（位置）
 * @param {Context*} context
 */
void RmFileHandle::delete_record(const Rid &rid, Context *context)
{
    std::unique_lock<std::mutex> lock(latch_); // 添加锁保护

    // Todo:
    // 1. 获取指定记录所在的page handle
    RmPageHandle page_handle = fetch_page_handle(rid.page_no); // 获取句柄

    if (!Bitmap::is_set(page_handle.bitmap, rid.slot_no)) // 判断该位置是否有记录
    {
        buffer_pool_manager_->unpin_page(page_handle.page->get_page_id(), false);
        throw RecordNotFoundError(rid.page_no, rid.slot_no);
    }

    // 记录DELETE日志（在删除之前记录原始数据）
    if (context && context->log_mgr_)
    {
        char *record_data = page_handle.get_slot(rid.slot_no);
        RmRecord delete_record(record_data, file_hdr_.record_size);
        Rid rid_copy = rid; // 创建副本，因为构造函数需要引用
        DeleteLogRecord *delete_log = new DeleteLogRecord(
            context->txn_ ? context->txn_->get_transaction_id() : INVALID_TXN_ID,
            rid_copy,
            context->current_table_name_,
            delete_record);
        context->log_mgr_->add_log_to_buffer(delete_log);
        delete delete_log;
    }

    // 2. 更新page_handle.page_hdr中的数据结构
    // 注意考虑删除一条记录后页面未满的情况，需要调用release_page_handle()
    bool flag = page_handle.page_hdr->num_records == file_hdr_.num_records_per_page;
    Bitmap::reset(page_handle.bitmap, rid.slot_no);
    page_handle.page_hdr->num_records--;

    if (flag)
    {
        release_page_handle(page_handle); // 更新当前页面的句柄
    }
    buffer_pool_manager_->unpin_page(page_handle.page->get_page_id(), true); // 变为0
}

/**
 * @description: 更新记录文件中记录号为rid的记录
 * @param {Rid&} rid 要更新的记录的记录号（位置）
 * @param {char*} buf 新记录的数据
 * @param {Context*} context
 */
void RmFileHandle::update_record(const Rid &rid, char *buf, Context *context)
{
    std::unique_lock<std::mutex> lock(latch_); // 添加锁保护

    // Todo:
    // 1. 获取指定记录所在的page handle
    RmPageHandle page_handle = fetch_page_handle(rid.page_no);
    if (!Bitmap::is_set(page_handle.bitmap, rid.slot_no))
    {
        buffer_pool_manager_->unpin_page(page_handle.page->get_page_id(), false);
        throw RecordNotFoundError(rid.page_no, rid.slot_no);
    }

    // 记录UPDATE日志（在更新之前记录原始数据和新数据）
    if (context && context->log_mgr_)
    {
        char *old_record_data = page_handle.get_slot(rid.slot_no);
        RmRecord old_record(old_record_data, file_hdr_.record_size);
        RmRecord new_record(buf, file_hdr_.record_size);
        Rid rid_copy = rid; // 创建副本，因为构造函数需要引用
        UpdateLogRecord *update_log = new UpdateLogRecord(
            context->txn_ ? context->txn_->get_transaction_id() : INVALID_TXN_ID,
            old_record,
            new_record,
            rid_copy,
            context->current_table_name_);
        context->log_mgr_->add_log_to_buffer(update_log);
        delete update_log;
    }

    // 2. 更新记录
    std::memcpy(page_handle.get_slot(rid.slot_no), buf, file_hdr_.record_size); // 位置 内容 大小
    buffer_pool_manager_->unpin_page(page_handle.page->get_page_id(), true);    // 标记修改过 成为脏页
}

/**
 * 以下函数为辅助函数，仅提供参考，可以选择完成如下函数，也可以删除如下函数，在单元测试中不涉及如下函数接口的直接调用
 */
/**
 * @description: 获取指定页面的页面句柄
 * @param {int} page_no 页面号
 * @return {RmPageHandle} 指定页面的句柄
 */
RmPageHandle RmFileHandle::fetch_page_handle(int page_no) const // 读取当前页柄
{
    // Todo:
    // 使用缓冲池获取指定页面，并生成page_handle返回给上层
    // if page_no is invalid, throw PageNotExistError exception
    if (page_no < 0)
    {
        throw RecordNotFoundError(page_no, -1); // slot=-1代表整个页面不存在
    }
    if (page_no >= file_hdr_.num_pages) // file_hdr_.num_page是元文件头，包含所有信息 当页面号数比分配到的大则为非法
    {
        throw RecordNotFoundError(page_no, -1);
    }

    Page *page = buffer_pool_manager_->fetch_page({fd_, page_no}); // 查看page是否在缓冲池中
    if (!page)
    {
        throw RecordNotFoundError(page_no, -1);
    }

    return RmPageHandle(&file_hdr_, page);
}

/**
 * @description: 创建一个新的page handle
 * @return {RmPageHandle} 新的PageHandle
 */
RmPageHandle RmFileHandle::create_new_page_handle()
{
    // Todo:
    // 1.使用缓冲池来创建一个新page
    // 2.更新page handle中的相关信息
    // 3.更新file_hdr_
    PageId new_page_id = {fd_, INVALID_PAGE_ID}; // 让BufferPoolManager分配页面号

    Page *page = buffer_pool_manager_->new_page(&new_page_id);
    if (!page)
    {
        throw std::runtime_error("fail to create_new_page");
    }

    // 关键修复：初始化页面内容，确保一致性
    memset(page->get_data(), 0, PAGE_SIZE);

    // 2.更新page handle中的相关信息
    RmPageHandle page_handle(&file_hdr_, page);
    page_handle.page_hdr->num_records = 0; // 确保初始化为0
    page_handle.page_hdr->next_free_page_no = file_hdr_.first_free_page_no;

    // 确保位图被正确初始化
    memset(page_handle.bitmap, 0, file_hdr_.bitmap_size);

    // 3.更新file_hdr_
    file_hdr_.first_free_page_no = new_page_id.page_no;
    file_hdr_.num_pages = std::max(file_hdr_.num_pages, new_page_id.page_no + 1); // 确保num_pages正确

    return page_handle;
}

/**
 * @brief 创建或获取一个空闲的page handle
 *
 * @return RmPageHandle 返回生成的空闲page handle
 * @note pin the page, remember to unpin it outside!
 */
RmPageHandle RmFileHandle::create_page_handle()
{
    // Todo:
    // 1. 判断file_hdr_中是否还有空闲页
    //     1.1 没有空闲页：使用缓冲池来创建一个新page；可直接调用create_new_page_handle()
    //     1.2 有空闲页：直接获取第一个空闲页
    // 2. 生成page handle并返回给上层
    if (file_hdr_.first_free_page_no == -1)
    {
        return create_new_page_handle();
    }
    else
    {
        return fetch_page_handle(file_hdr_.first_free_page_no);
    }
}

/**
 * @description: 当一个页面从没有空闲空间的状态变为有空闲空间状态时，更新文件头和页头中空闲页面相关的元数据
 */
void RmFileHandle::release_page_handle(RmPageHandle &page_handle)
{
    // Todo:
    // 当page从已满变成未满，考虑如何更新：
    // 1. page_handle.page_hdr->next_free_page_no
    // 2. file_hdr_.first_free_page_no

    // 关键修复：确保页面不会重复加入空闲链表
    int current_page_no = page_handle.page->get_page_id().page_no;

    // 检查当前页面是否已经在空闲链表中
    if (file_hdr_.first_free_page_no != current_page_no)
    {
        page_handle.page_hdr->next_free_page_no = file_hdr_.first_free_page_no; // 将下一页传给next_free_page_no
        file_hdr_.first_free_page_no = current_page_no;                         // 第一个空的为本页
    }
}

/**
 * @description: 批量插入多条记录
 * @param {char*} buf 记录批量数据的首地址（每条record_size字节）
 * @param {int} record_size 单条记录大小
 * @param {int} count 插入的记录条数
 * @param {Context*} context 上下文
 * @return {std::vector<Rid>} 返回所有插入记录的Rid
 */
std::vector<Rid> RmFileHandle::insert_records_batch(char *buf, int record_size, int count, Context *context)
{
    std::vector<Rid> result;
    result.reserve(count);

    // 批量插入时使用事务级别的锁
    std::unique_lock<std::mutex> lock(latch_);

    // 如果有事务，确保批量操作的一致性
    bool has_transaction = (context && context->txn_);
    std::vector<std::unique_ptr<InsertLogRecord>> log_records;

    if (has_transaction)
    {
        log_records.reserve(count);
    }

    int inserted = 0;
    try
    {
        while (inserted < count)
        {
            Page *page = nullptr;
            RmPageHandle page_handle{&file_hdr_, page};
            if (file_hdr_.first_free_page_no == -1)
            {
                page_handle = create_new_page_handle();
            }
            else
            {
                page_handle = fetch_page_handle(file_hdr_.first_free_page_no);
            }

            int slots_per_page = file_hdr_.num_records_per_page;
            int remain = slots_per_page - page_handle.page_hdr->num_records;
            int batch = std::min(remain, count - inserted);

            // 关键修复：批量操作时确保所有更新的原子性
            bool page_modified = false;
            for (int i = 0; i < slots_per_page && batch > 0; ++i)
            {
                if (!Bitmap::is_set(page_handle.bitmap, i))
                {
                    char *src = buf + inserted * record_size;
                    Rid rid{page_handle.page->get_page_id().page_no, i};

                    // 先准备日志记录
                    if (has_transaction && context->log_mgr_)
                    {
                        RmRecord insert_record(src, record_size);
                        auto insert_log = std::make_unique<InsertLogRecord>(
                            context->txn_->get_transaction_id(),
                            insert_record,
                            rid,
                            context->current_table_name_);
                        log_records.push_back(std::move(insert_log));
                    }

                    // 原子性地执行插入
                    std::memcpy(page_handle.get_slot(i), src, record_size);
                    Bitmap::set(page_handle.bitmap, i);
                    page_handle.page_hdr->num_records++;
                    result.push_back(rid);

                    inserted++;
                    batch--;
                    page_modified = true;
                    if (inserted >= count)
                        break;
                }
            }

            // 原子性地更新页面状态
            if (page_handle.page_hdr->num_records == slots_per_page)
            {
                file_hdr_.first_free_page_no = page_handle.page_hdr->next_free_page_no;
                page_handle.page_hdr->next_free_page_no = -1;
            }
            buffer_pool_manager_->unpin_page(page_handle.page->get_page_id(), page_modified);
        }

        // 批量写入日志记录
        if (has_transaction && context->log_mgr_)
        {
            for (auto &log_record : log_records)
            {
                context->log_mgr_->add_log_to_buffer(log_record.get());
            }
        }
    }
    catch (const std::exception &e)
    {
        // 如果插入失败，清理已插入的记录
        // 这里应该实现回滚逻辑
        throw;
    }

    return result;
}