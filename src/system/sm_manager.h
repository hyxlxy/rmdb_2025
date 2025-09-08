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

#include "index/ix.h"
#include "record/rm_file_handle.h"
#include "sm_defs.h"
#include "sm_meta.h"
// #include "table_stats.h"  // 暂时注释掉
#include "common/context.h"

class Context;

// 前向声明
struct TupleVersion;

struct ColDef {
    std::string name;  // Column name
    ColType type;      // Type of column
    int len;           // Length of column
};

/* 系统管理器，负责元数据管理和DDL（数据定义语言）语句的执行 */
class SmManager {
   public:
    DbMeta db_;             // 当前打开的数据库的元数据
    std::unordered_map<std::string, std::unique_ptr<RmFileHandle>> fhs_;    // file name -> record file handle, 当前数据库中每张表的数据文件
    std::unordered_map<std::string, std::unique_ptr<IxIndexHandle>> ihs_;   // file name -> index file handle, 当前数据库中每个索引的文件

    // 统计信息管理器（暂时注释掉以避免编译错误）
    // TableStatsManager stats_manager_;
    // CardinalityEstimator cardinality_estimator_;
    // JoinOrderOptimizer join_optimizer_;

   private:
    DiskManager* disk_manager_;
    BufferPoolManager* buffer_pool_manager_;
    RmManager* rm_manager_;
    IxManager* ix_manager_;

   public:
    SmManager(DiskManager* disk_manager, BufferPoolManager* buffer_pool_manager, RmManager* rm_manager,
              IxManager* ix_manager)
        : disk_manager_(disk_manager),
          buffer_pool_manager_(buffer_pool_manager),
          rm_manager_(rm_manager),
          ix_manager_(ix_manager) {}

    ~SmManager() {}

    BufferPoolManager* get_bpm() { return buffer_pool_manager_; }

    RmManager* get_rm_manager() { return rm_manager_; }  

    IxManager* get_ix_manager() { return ix_manager_; }

    // 统计信息管理接口
    // TableStatsManager* get_stats_manager() { return &stats_manager_; }
    // CardinalityEstimator* get_cardinality_estimator() { return &cardinality_estimator_; }
    // JoinOrderOptimizer* get_join_optimizer() { return &join_optimizer_; }

    // 获取表的行数
    size_t get_table_row_count(const std::string& table_name);

    // 获取表的统计信息
    // TableStats get_table_stats(const std::string& table_name);

    // 更新所有表的统计信息
    void update_all_table_stats();

    bool is_dir(const std::string& db_name);

    void create_db(const std::string& db_name);

    void drop_db(const std::string& db_name);

    void open_db(const std::string& db_name);

    void close_db();

    void flush_meta();

    void show_tables(Context* context);

    void show_indexs(std::string &table_name, Context *context);

    void desc_table(const std::string& tab_name, Context* context);

    void create_table(const std::string& tab_name, const std::vector<ColDef>& col_defs, Context* context);

    void drop_table(const std::string& tab_name, Context* context);

    void create_index(const std::string& tab_name, const std::vector<std::string>& col_names, Context* context);

    void drop_index(const std::string& tab_name, const std::vector<std::string>& col_names, Context* context);
    
    void drop_index(const std::string& tab_name, const std::vector<ColMeta>& col_names, Context* context);

// TODO  :自己添加的函数从这里开始
     void show_index(const std::string& tab_name, Context* context);

     void set_check_point();

    // 重新加载数据库元数据（用于恢复时的错误处理）
    bool reload_metadata();

    // 获取数据库中表的数量
    size_t get_table_count() const;

    // MVCC版本持久化方法
    void persist_mvcc_version(const Rid& rid, struct TupleVersion* version);


};
