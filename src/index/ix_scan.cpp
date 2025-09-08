/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "ix_scan.h"

IxScan::~IxScan() {
    if (cached_node_ != nullptr) {
        ih_->buffer_pool_manager_->unpin_page(cached_node_->get_page_id(), false);
        cached_node_ = nullptr;
    }
}


/**
 * @brief
 * @todo 加上读锁（需要使用缓冲池得到page）
 */
void IxScan::next() {
    assert(!is_end());
    // 使用缓存的叶子页，若为空则fetch
    IxNodeHandle *node = cached_node_;
    if (node == nullptr || node->get_page_no() != iid_.page_no) {
        if (cached_node_ != nullptr) {
            ih_->buffer_pool_manager_->unpin_page(cached_node_->get_page_id(), false);
        }
        node = ih_->fetch_node(iid_.page_no);
        cached_node_ = node;
    }
    assert(node->is_leaf_page());

    // 修复：检查slot_no是否已经超出范围，如果是则直接跳到下一页
    if (iid_.slot_no >= node->get_size()) {
        // 当前slot已经超出范围，直接跳到下一个叶子
        page_id_t next_leaf = node->get_next_leaf();
        // 注意：索引实现中存在“叶子头结点”IX_LEAF_HEADER_PAGE，遇到它也应视为到达结尾
        if (next_leaf != IX_NO_PAGE && next_leaf != IX_LEAF_HEADER_PAGE) {
            iid_.slot_no = 0;
            iid_.page_no = next_leaf;
            // 切页：更新缓存
            ih_->buffer_pool_manager_->unpin_page(cached_node_->get_page_id(), false);
            cached_node_ = ih_->fetch_node(iid_.page_no);
            return; // 直接返回，不需要再increment
        } else {
            // 没有下一个叶子（或遇到叶子头结点）-> 结束
            iid_ = end_;
            ih_->buffer_pool_manager_->unpin_page(cached_node_->get_page_id(), false);
            cached_node_ = nullptr;
            return;
        }
    }

    // increment slot no
    iid_.slot_no++;
    if (iid_.slot_no == node->get_size()) {
        // 当前叶子已经到末尾，尝试进入下一个叶子，否则结束
        page_id_t next_leaf = node->get_next_leaf();
        // 注意：索引实现中存在“叶子头结点”IX_LEAF_HEADER_PAGE，遇到它也应视为到达结尾
        if (next_leaf != IX_NO_PAGE && next_leaf != IX_LEAF_HEADER_PAGE) {
            iid_.slot_no = 0;
            iid_.page_no = next_leaf;
            // 切页：更新缓存
            ih_->buffer_pool_manager_->unpin_page(cached_node_->get_page_id(), false);
            cached_node_ = ih_->fetch_node(iid_.page_no);
        } else {
            // 没有下一个叶子（或遇到叶子头结点）-> 结束
            iid_ = end_;
            ih_->buffer_pool_manager_->unpin_page(cached_node_->get_page_id(), false);
            cached_node_ = nullptr;
        }
    }
}

Rid IxScan::rid() const {
    // 检查是否已经到达结束位置
    if (is_end()) {
        throw IndexEntryNotFoundError();
    }
    // 使用缓存叶子页，必要时fetch
    IxNodeHandle *node = cached_node_;
    if (node == nullptr || node->get_page_no() != iid_.page_no) {
        if (cached_node_ != nullptr) {
            ih_->buffer_pool_manager_->unpin_page(cached_node_->get_page_id(), false);
        }
        node = ih_->fetch_node(iid_.page_no);
        cached_node_ = node;
    }
    if (!node || !node->is_leaf_page()) {
        throw IndexEntryNotFoundError();
    }
    if (iid_.slot_no < 0 || iid_.slot_no >= node->get_size()) {
        throw IndexEntryNotFoundError();
    }
    return *node->get_rid(iid_.slot_no);
}
