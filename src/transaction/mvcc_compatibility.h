// NOTE: Unused component marker
// 当前扫描未发现外部引用，标注为未使用，保留以备后续使用/参考；不移除构建，不影响行为。

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

#include "common/config.h"
#include "mvcc_defs.h"

/**
 * @brief MVCC兼容层 - 确保MVCC启用时不破坏原有逻辑
 */
class MVCCCompatibility {
public:
    /**
     * @brief 检查是否应该使用MVCC逻辑
     * @param context 上下文
     * @return 是否使用MVCC
     */
    static bool should_use_mvcc(Context* context) {
        if (!ENABLE_MVCC) {
            return false;
        }

        // 只有在明确的事务上下文中才使用MVCC
        if (context == nullptr || context->txn_ == nullptr) {
            return false;
        }

        // 检查事务是否有有效的快照
        if (context->txn_->get_snapshot() == nullptr) {
            return false;
        }

        return true;
    }

    /**
     * @brief 安全的MVCC记录读取
     * @param fh 文件句柄
     * @param rid 记录ID
     * @param context 上下文
     * @return 记录指针，如果MVCC失败则回退到原始方法
     */
    static std::unique_ptr<RmRecord> safe_get_record(RmFileHandle* fh, const Rid& rid, Context* context) {
        // 如果不应该使用MVCC，直接使用原始方法
        if (!should_use_mvcc(context)) {
            if (fh->is_record(rid)) {
                return fh->get_record(rid, context);
            }
            return nullptr;
        }

        // 尝试使用MVCC，如果失败则回退
        try {
            // 这里会调用MVCC逻辑
            return get_record_with_mvcc_fallback(fh, rid, context);
        } catch (...) {
            // MVCC失败，回退到原始方法
            if (fh->is_record(rid)) {
                return fh->get_record(rid, context);
            }
            return nullptr;
        }
    }

private:
    /**
     * @brief 带回退机制的MVCC记录读取
     */
    static std::unique_ptr<RmRecord> get_record_with_mvcc_fallback(RmFileHandle* fh, const Rid& rid, Context* context);
};
