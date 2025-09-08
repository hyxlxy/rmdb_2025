/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#pragma once

#include <map>
#include <unordered_map>
#include "log_manager.h"
#include "storage/disk_manager.h"
#include "system/sm_manager.h"

class RedoLogsInPage {
public:
    RedoLogsInPage() { table_file_ = nullptr; }
    RmFileHandle* table_file_;
    std::vector<lsn_t> redo_logs_;   // 在该page上需要redo的操作的lsn
};

class RecoveryManager {
public:
    RecoveryManager(DiskManager* disk_manager, BufferPoolManager* buffer_pool_manager, SmManager* sm_manager) {
        disk_manager_ = disk_manager;
        buffer_pool_manager_ = buffer_pool_manager;
        sm_manager_ = sm_manager;
        current_log_size = 0;
        need_to_fix = false;
        is_recovery = false;
    }

    void analyze();
    void redo();
    void undo();
private:
    std::map<txn_id_t, lsn_t> ATT;
    std::unordered_map<PageId, lsn_t> DPT;
    std::map<lsn_t, int> log_offsets_;

    lsn_t last_lsn_;

    LogBuffer buffer_;                                              // 读入日志
    DiskManager* disk_manager_;                                     // 用来读写文件
    BufferPoolManager* buffer_pool_manager_;                        // 对页面进行读写
    SmManager* sm_manager_;                                         // 访问数据库元数据

    // 添加缺失的成员变量
    std::map<txn_id_t, std::vector<lsn_t>> undo_map_;              // 未完成事务的日志记录
    std::vector<lsn_t> redo_list_;                                  // 需要重做的日志记录
    std::map<lsn_t, int> lsn_offset_;                              // LSN到文件偏移的映射
    std::map<lsn_t, int> lsn_len_;                                 // LSN到日志长度的映射
    int current_log_size;                                           // 当前日志大小
    bool need_to_fix;                                               // 是否需要修复
    bool is_recovery;                                               // 是否处于恢复状态
};
