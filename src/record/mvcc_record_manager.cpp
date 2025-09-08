/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "mvcc_record_manager.h"
#include "../common/exception.h"

// 这个文件主要包含一些复杂的实现逻辑，目前大部分逻辑已经在头文件中实现
// 这里可以添加一些辅助函数或复杂的实现

/**
 * @brief 辅助函数：从原始记录创建MVCC版本
 */
TupleVersion* create_version_from_record(const RmRecord& record, txn_id_t txn_id) {
    return new TupleVersion(record.data, record.size, txn_id);
}

/**
 * @brief 辅助函数：验证MVCC操作的有效性
 */
bool validate_mvcc_operation(const Rid& rid, Transaction* txn, const std::string& operation) {
    if (txn == nullptr) {
        return false;
    }
    
    if (txn->get_read_ts() == INVALID_TIMESTAMP) {
        return false;
    }
    
    return true;
}
