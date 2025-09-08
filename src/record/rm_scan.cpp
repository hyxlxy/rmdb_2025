/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "rm_scan.h"
#include "rm_file_handle.h"

/**
 * @brief 初始化file_handle和rid
 * @param file_handle
 */
RmScan::RmScan(const RmFileHandle *file_handle) : file_handle_(file_handle)
{
    // Todo:
    // 初始化file_handle和rid（指向第一个存放了记录的位置）
    rid_ = {RM_FIRST_RECORD_PAGE, RM_NO_PAGE}; // 第0页为文件头 第1页才是真正内容
    next();
}

/**
 * @brief 找到文件中下一个存放了记录的位置
 */
void RmScan::next()
{
    // Todo:
    // 找到文件中下一个存放了记录的非空闲位置，用rid_来指向这个位置
    for (int page_no = rid_.page_no;
         page_no < file_handle_->file_hdr_.num_pages; page_no++) // 小于页柄的最大页就遍历
    {
        RmPageHandle rm_page_handle = file_handle_->fetch_page_handle(page_no);
        int max_n = file_handle_->file_hdr_.num_records_per_page; // 页的最大内容
        int slot_no =
            Bitmap::next_bit(true, rm_page_handle.bitmap, max_n, rid_.slot_no); // 找1

        if (slot_no < max_n)
        {
            rid_ = {page_no, slot_no};
            // 关键修复：找到记录后unpin页面
            file_handle_->buffer_pool_manager_->unpin_page(rm_page_handle.page->get_page_id(), false);
            return;
        }

        // 关键修复：没找到记录时也要unpin页面
        file_handle_->buffer_pool_manager_->unpin_page(rm_page_handle.page->get_page_id(), false);
        rid_.slot_no = RM_NO_PAGE;
    }
    rid_.page_no = RM_NO_PAGE;
}

/**
 * @brief ​ 判断是否到达文件末尾
 */
bool RmScan::is_end() const
{
    // Todo: 修改返回值
    return rid_.page_no == RM_NO_PAGE; //若rid_.page_no为-1 ，说明已经遍历完所有页面
}

/**
 * @brief RmScan内部存放的rid
 */
Rid RmScan::rid() const
{
    return rid_;
}