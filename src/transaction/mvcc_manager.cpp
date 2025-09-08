/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "mvcc_manager.h"
#include "system/sm_manager.h"
#include <iostream>

/**
 * @brief 将已提交的版本持久化到底层存储
 */
void MVCCManager::persist_committed_version(const std::string& key, TupleVersion* version) {
    if (version == nullptr || storage_manager_ == nullptr) return;

    // 从字符串key恢复RID
    // 解析格式：table_name_page_no_slot_no
    size_t last_underscore = key.find_last_of('_');
    if (last_underscore == std::string::npos) return;

    size_t second_last_underscore = key.find_last_of('_', last_underscore - 1);
    if (second_last_underscore == std::string::npos) return;

    Rid rid;
    try {
        rid.page_no = std::stoi(key.substr(second_last_underscore + 1, last_underscore - second_last_underscore - 1));
        rid.slot_no = std::stoi(key.substr(last_underscore + 1));
    } catch (const std::exception& e) {
        return;
    }

    try {
        // 调用存储管理器持久化版本
        storage_manager_->persist_mvcc_version(rid, version);
    } catch (const std::exception& e) {
        // 持久化失败，但不影响内存中的MVCC操作
    }
}
