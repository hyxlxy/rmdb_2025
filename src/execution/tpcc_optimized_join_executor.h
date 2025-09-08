#if 0
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

#include "executor_nestedloop_join.h"
#include "mvcc_executor_base.h"
#include "../common/config.h"

/**
 * @brief TPCC优化的连接执行器（未使用组件，已整体注释）
 */
class TPCCOptimizedJoinExecutor : public NestedLoopJoinExecutor, public MVCCExecutorBase
{
    // ... 未使用，整体注释，保留以备后续参考
};
#endif
