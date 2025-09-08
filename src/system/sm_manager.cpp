/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "sm_manager.h"

#include <set> // [唯一性修复标记] 显式唯一性检查所需
#include <sys/stat.h>
#include <unistd.h>

#include <fstream>

#include "index/ix.h"
#include "record/rm.h"
#include "execution/mvcc_executor_base.h" // MVCC执行器基类
#include "record_printer.h"

#include "execution/execution_manager.h"
/**
 * @description: 判断是否为一个文件夹
 * @return {bool} 返回是否为一个文件夹
 * @param {string&} db_name 数据库文件名称，与文件夹同名
 */
bool SmManager::is_dir(const std::string &db_name)
{
    struct stat st;
    return stat(db_name.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

/**
 * @description: 创建数据库，所有的数据库相关文件都放在数据库同名文件夹下
 * @param {string&} db_name 数据库名称
 */
void SmManager::create_db(const std::string &db_name)
{
    if (is_dir(db_name))
    {
        throw DatabaseExistsError(db_name);
    }
    // 为数据库创建一个子目录
    std::string cmd = "mkdir " + db_name;
    if (system(cmd.c_str()) < 0)
    {
        // 创建一个名为db_name的目录
        throw UnixError();
    }
    if (chdir(db_name.c_str()) < 0)
    {
        // 进入名为db_name的目录
        throw UnixError();
    }
    // 创建系统目录
    DbMeta *new_db = new DbMeta();
    new_db->name_ = db_name;

    // 注意，此处ofstream会在当前目录创建(如果没有此文件先创建)和打开一个名为DB_META_NAME的文件
    std::ofstream ofs(DB_META_NAME);

    // 将new_db中的信息，按照定义好的operator<<操作符，写入到ofs打开的DB_META_NAME文件中
    ofs << *new_db; // 注意：此处重载了操作符<<

    delete new_db;

    // 创建日志文件
    disk_manager_->create_file(LOG_FILE_NAME);
    // 创建静态检测点文件
    disk_manager_->create_file(CHECK_POINT_NAME);
    // 初始化检查点条件
    int check_point_fd = disk_manager_->open_file(CHECK_POINT_NAME);
    int *last_check_point = new int();
    *last_check_point = 0;
    disk_manager_->write_check_point(check_point_fd, (char *)last_check_point, sizeof(int));
    delete last_check_point;
    disk_manager_->close_file(check_point_fd);

    // 回到根目录
    if (chdir("..") < 0)
    {
        throw UnixError();
    }
}

/**
 * @description: 删除数据库，同时需要清空相关文件以及数据库同名文件夹
 * @param {string&} db_name 数据库名称，与文件夹同名
 */
void SmManager::drop_db(const std::string &db_name)
{
    if (!is_dir(db_name))
    {
        throw DatabaseNotFoundError(db_name);
    }
    std::string cmd = "rm -r " + db_name;
    if (system(cmd.c_str()) < 0)
    {
        throw UnixError();
    }
}

/**
 * @description: 打开数据库，找到数据库对应的文件夹，并加载数据库元数据和相关文件
 * @param {string&} db_name 数据库名称，与文件夹同名
 */
void SmManager::open_db(const std::string &db_name)
{
    // 数据库不存在
    if (!is_dir(db_name))
    {
        throw DatabaseNotFoundError(db_name);
    }
    // 数据库已经打开
    if (!db_.name_.empty())
    {
        throw DatabaseExistsError(db_name);
    }
    // 将当前工作目录设置为数据库目录
    if (chdir(db_name.c_str()) < 0)
    {
        throw UnixError();
    }

    // 从 DB_META_NAME 文件中加载数据库元数据
    std::ifstream ifs(DB_META_NAME);
    if (ifs.fail())
    {
        throw UnixError();
    }
    ifs >> db_; // 注意：此处重载了操作符>>

    // 打开数据库中每个表的记录文件并读入
    for (auto &[table_name, tab_meta] : db_.tabs_)
    {
        fhs_[table_name] = rm_manager_->open_file(table_name);
        // 待索引完成后更新
        for (auto &[index_name, _] : tab_meta.indexes)
        {
            ihs_[index_name] = ix_manager_->open_index(index_name);
        }
    }
}

/**
 * @description: 把数据库相关的元数据刷入磁盘中
 */
void SmManager::flush_meta()
{
    // 默认清空文件
    std::ofstream ofs(DB_META_NAME);
    ofs << db_;
}

/**
 * @description: 关闭数据库并把数据落盘
 */
void SmManager::close_db()
{
    if (db_.name_.empty())
    {
        throw DatabaseNotOpenError(db_.name_);
    }

    flush_meta();
    db_.name_.clear();
    db_.tabs_.clear();

    // 记录文件落盘
    for (auto &[_, file_handle] : fhs_)
    {
        rm_manager_->close_file(file_handle.get());
    }

    // 索引文件落盘
    for (auto &[_, index_handle] : ihs_)
    {
        ix_manager_->close_index(index_handle.get());
    }

    fhs_.clear();
    ihs_.clear();

    // 将 日志长度写入 静态检测文件
    int check_point_fd = disk_manager_->open_file(CHECK_POINT_NAME);
    int last_check_point = disk_manager_->get_file_size(LOG_FILE_NAME);
    disk_manager_->write_check_point(check_point_fd, reinterpret_cast<char*>(&last_check_point), sizeof(int));
    disk_manager_->close_file(check_point_fd);

    if (chdir("..") < 0)
    {
        throw UnixError();
    }
}

/**
 * @description: 设置检查点，将当前日志大小写入检查点文件
 */
void SmManager::set_check_point()
{
    // 关键修复：在创建检查点之前，必须确保所有脏页都已刷盘
    // 这样才能保证检查点之前的数据真正持久化

    // 刷新所有表文件的脏页
    for (auto &[table_name, fh] : fhs_)
    {
        buffer_pool_manager_->flush_all_pages(fh->GetFd());
    }

    // 刷新所有索引文件的脏页
    for (auto &[index_name, ih] : ihs_)
    {
        buffer_pool_manager_->flush_all_pages(ih->GetFd());
    }

    int check_point_fd = disk_manager_->open_file(CHECK_POINT_NAME);
    int *last_check_point = new int();
    *last_check_point = disk_manager_->get_file_size(LOG_FILE_NAME);

    disk_manager_->write_check_point(check_point_fd, (char *)last_check_point, sizeof(int));
    delete last_check_point;
    disk_manager_->close_file(check_point_fd);
}

/**
 * @description: 重新加载数据库元数据（用于恢复时的错误处理）
 * @return {bool} 是否成功重新加载元数据
 */
bool SmManager::reload_metadata()
{
    try
    {
        // 清空当前元数据
        db_.tabs_.clear();
        fhs_.clear();
        ihs_.clear();

        // 重新从文件加载元数据
        std::ifstream ifs(DB_META_NAME);
        if (ifs.fail())
        {
            std::cerr << "Failed to open metadata file for reloading" << std::endl;
            return false;
        }

        ifs >> db_;

        // 重新打开所有表文件和索引文件
        for (auto &[table_name, tab_meta] : db_.tabs_)
        {
            fhs_[table_name] = rm_manager_->open_file(table_name);
            for (auto &[index_name, _] : tab_meta.indexes)
            {
                ihs_[index_name] = ix_manager_->open_index(index_name);
            }
        }

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to reload metadata: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @description: 获取数据库中表的数量
 * @return {size_t} 表的数量
 */
size_t SmManager::get_table_count() const
{
    return db_.tabs_.size();
}

/**
 * @description: 持久化MVCC版本到底层存储
 * @param {Rid&} rid 记录ID
 * @param {TupleVersion*} version 要持久化的版本
 */
void SmManager::persist_mvcc_version(const Rid &rid, struct TupleVersion *version)
{
    if (version == nullptr)
    {
        return;
    }

    // 查找对应的表文件句柄
    RmFileHandle *target_fh = nullptr;
    std::string target_table;

    // 遍历所有表，找到包含该RID的表
    for (auto &[table_name, fh] : fhs_)
    {
        if (fh->is_record(rid))
        {
            target_fh = fh.get();
            target_table = table_name;
            break;
        }
    }

    if (target_fh == nullptr)
    {
        return;
    }

    try
    {
        // 添加更多安全检查
        if (version->data == nullptr)
        {
            return;
        }

        if (version->size <= 0)
        {
            return;
        }

        // **关键修复：删除版本不删除物理记录**
        if (version->is_deleted)
        {
            // **MVCC删除版本只是逻辑删除，物理记录必须保留**
            // 这样其他事务仍能在其快照中看到记录
            // 物理记录的清理应该由垃圾回收机制处理

            // 不对物理存储做任何操作，只是标记版本已提交
            // 可见性完全由版本链控制
            return;
        }
        else
        {
            // 如果是更新或插入版本，将数据写入底层存储
            target_fh->update_record(rid, version->data, nullptr);
        }

        // 恢复页面刷新以确保数据持久性
        buffer_pool_manager_->flush_page({target_fh->GetFd(), rid.page_no});
    }
    catch (const std::exception &e)
    {
        throw;
    }
}

/**
 * @description: 显示所有的表,通过测试需要将其结果写入到output.txt,详情看题目文档
 * @param {Context*} context
 */
void SmManager::show_tables(Context *context)
{
    std::fstream outfile;
    RecordPrinter printer(1);
    printer.print_separator(context);
    printer.print_record({"Tables"}, context);
    printer.print_separator(context);

    // 输出表名到屏幕
    for (auto &entry : db_.tabs_)
    {
        auto &tab = entry.second;
        printer.print_record({tab.name}, context);
    }

    // 输出到文件，使用简洁格式
    if (output_file_enabled)
    {
        outfile.open("output.txt", std::ios::out | std::ios::app);
        outfile << "| Tables |\n";
        for (auto &entry : db_.tabs_)
        {
            auto &tab = entry.second;
            outfile << "| " << tab.name << " |\n";
        }
        outfile.close();
    }

    printer.print_separator(context);
}

/**
 * @description: 显示该表所有的索引,通过测试需要将其结果写入到output.txt,详情看题目文档
 * @param {String} table_name {Context*} context
 */
void SmManager::show_index(const std::string &tab_name, Context *context)
{
    RecordPrinter printer(3);
    std::vector<std::string> rec_str{tab_name, "unique", ""};
    std::string format = "| " + tab_name + " | unique | (";
    std::fstream outfile;
    std::ostringstream cols_stream;
    if (output_file_enabled)
    {
        outfile.open("output.txt", std::ios::out | std::ios::app);
    }
    for (const auto &[_, index] : db_.tabs_[tab_name].indexes)
    {
        cols_stream << "(" << index.cols[0].name;
        outfile << format << index.cols[0].name;
        for (size_t i = 1; i < index.cols.size(); ++i)
        {
            cols_stream << "," << index.cols[i].name;
            outfile << "," << index.cols[i].name;
        }
        outfile << ") |\n";
        cols_stream << ")";
        rec_str[2] = cols_stream.str();
        printer.print_indexs(rec_str, context);
        cols_stream.str("");
    }
    if (output_file_enabled)
        outfile.close();
}

/**
 * @description: 显示表的元数据
 * @param {string&} tab_name 表名称
 * @param {Context*} context
 */
void SmManager::desc_table(const std::string &tab_name, Context *context)
{
    TabMeta &tab = db_.get_table(tab_name);

    std::vector<std::string> captions = {"Field", "Type", "Index"};
    RecordPrinter printer(captions.size());
    // Print header
    printer.print_separator(context);
    printer.print_record(captions, context);
    printer.print_separator(context);
    // Print fields
    for (auto &col : tab.cols)
    {
        std::vector<std::string> field_info = {col.name, coltype2str(col.type), col.index ? "YES" : "NO"};
        printer.print_record(field_info, context);
    }
    // Print footer
    printer.print_separator(context);
}

/**
 * @description: 创建表
 * @param {string&} tab_name 表的名称
 * @param {vector<ColDef>&} col_defs 表的字段
 * @param {Context*} context
 */
void SmManager::create_table(const std::string &tab_name, const std::vector<ColDef> &col_defs, Context *context)
{
    if (db_.is_table(tab_name))
    {
        throw TableExistsError(tab_name);
    }
    // Create table meta
    int curr_offset = 0;
    TabMeta tab;
    tab.name = tab_name;
    for (auto &col_def : col_defs)
    {
        ColMeta col = {
            .tab_name = tab_name,
            .name = col_def.name,
            .type = col_def.type,
            .len = col_def.len,
            .offset = curr_offset,
            .index = false};
        curr_offset += col_def.len;
        tab.cols.push_back(col);
    }
    // Create & open record file
    int record_size = curr_offset; // record_size就是col meta所占的大小（表的元数据也是以记录的形式进行存储的）
    rm_manager_->create_file(tab_name, record_size);
    db_.tabs_[tab_name] = tab;
    // fhs_[tab_name] = rm_manager_->open_file(tab_name);
    fhs_.emplace(tab_name, rm_manager_->open_file(tab_name));

    flush_meta();
}

/**
 * @description: 删除表
 * @param {string&} tab_name 表的名称
 * @param {Context*} context
 */
void SmManager::drop_table(const std::string &tab_name, Context *context)
{
    if (!db_.is_table(tab_name))
    {
        throw TableNotFoundError(tab_name);
    }

    // **关键修复：清理MVCC版本链**
    // 需要清理该表的所有MVCC版本链数据
    // 通过MVCC执行器基类访问MVCC记录管理器
    auto mvcc_record_manager = MVCCExecutorBase::get_mvcc_record_manager();
    if (mvcc_record_manager)
    {
        auto mvcc_manager = mvcc_record_manager->get_mvcc_manager();
        if (mvcc_manager)
        {
            mvcc_manager->clear_table_versions(tab_name);
        }
    }

    // 先关闭再删除文件
    rm_manager_->close_file(fhs_[tab_name].get());
    rm_manager_->destroy_file(tab_name);

    fhs_.erase(tab_name);
    db_.tabs_.erase(tab_name);

    flush_meta();
}

/**
 * @description: 创建索引
 * @param {string&} tab_name 表的名称
 * @param {vector<string>&} col_names 索引包含的字段名称
 * @param {Context*} context
 */
void SmManager::create_index(const std::string &tab_name, const std::vector<std::string> &col_names, Context *context)
{
    auto &&ix_name = ix_manager_->get_index_name(tab_name, col_names);
    if (disk_manager_->is_file(ix_name))
    {
        throw IndexExistsError(tab_name, col_names);
    }

    auto &table_meta = db_.get_table(tab_name);

    std::vector<ColMeta> col_metas;
    col_metas.reserve(col_names.size());
    auto total_len = 0;
    for (auto &col_name : col_names)
    {
        col_metas.emplace_back(*table_meta.get_col(col_name));
        total_len += col_metas.back().len;
    }

    // auto ix_name = std::move(ix_manager_->get_index_name(tab_name, col_names));
    ix_manager_->create_index(ix_name, col_metas);
    auto &&ih = ix_manager_->open_index(ix_name);
    auto &&fh = fhs_[tab_name];

    int offset = 0;
    char *key = new char[total_len];
    std::set<std::string> unique_keys; // [唯一性修复标记] 显式唯一性检查
    for (auto &&scan = std::make_unique<RmScan>(fh.get()); !scan->is_end(); scan->next())
    {
        auto &&rid = scan->rid();
        auto &&record = fh->get_record(rid, context);
        offset = 0;
        for (auto &col_meta : col_metas)
        {
            memcpy(key + offset, record->data + col_meta.offset, col_meta.len);
            offset += col_meta.len;
        }
        std::string key_str(key, total_len);
        if (unique_keys.count(key_str))
        {
            // [唯一性修复标记] 表内已有重复主键，拒绝建索引
            delete[] key;
            ix_manager_->close_index(ih.get());
            ix_manager_->destroy_index(ix_name);
            throw NonUniqueIndexError(tab_name, col_names);
        }
        unique_keys.insert(key_str);
        // 插入B+树
        if (ih->insert_entry(key, rid, context->txn_) == IX_NO_PAGE)
        {
            // 释放内存
            delete[] key;
            ix_manager_->close_index(ih.get());
            ix_manager_->destroy_index(ix_name);
            throw NonUniqueIndexError(tab_name, col_names);
        }
    }
    // 释放内存
    delete[] key;

    // 更新表元索引数据
    table_meta.indexes.emplace(ix_name, IndexMeta{tab_name, total_len, static_cast<int>(col_names.size()), col_metas});
    // 插入索引句柄
    ihs_[ix_name] = std::move(ih);
    // 持久化
    flush_meta();
}

/**
 * @description: 获取表的行数
 * @param {string&} table_name 表名称
 * @return {size_t} 表的行数
 */
size_t SmManager::get_table_row_count(const std::string &table_name)
{
    auto fh_it = fhs_.find(table_name);
    if (fh_it == fhs_.end())
    {
        throw TableNotFoundError(table_name);
    }

    // 通过扫描表文件实际计算行数
    size_t count = 0;
    try
    {
        for (RmScan scan(fh_it->second.get()); !scan.is_end(); scan.next())
        {
            count++;
        }
    }
    catch (...)
    {
        // 如果扫描失败，返回默认值
        return 1000;
    }
    return count;
}

/**
 * @description: 获取表的统计信息
 * @param {string&} table_name 表名称
 * @return {TableStats} 表的统计信息
 */
// TableStats SmManager::get_table_stats(const std::string& table_name) {
//     auto fh_it = fhs_.find(table_name);
//     if (fh_it == fhs_.end()) {
//         throw TableNotFoundError(table_name);
//     }
//     return stats_manager_.get_table_stats(table_name, fh_it->second.get());
// }

/**
 * @description: 更新所有表的统计信息
 */
void SmManager::update_all_table_stats()
{
    // 暂时注释掉统计信息更新
    // stats_manager_.update_all_stats(fhs_);
}

/**
 * @description: 删除索引
 * @param {string&} tab_name 表名称
 * @param {vector<string>&} col_names 索引包含的字段名称
 * @param {Context*} context
 */
void SmManager::drop_index(const std::string &tab_name, const std::vector<std::string> &col_names, Context *context)
{
    auto &table_meta = db_.get_table(tab_name);
    auto &&ix_name = ix_manager_->get_index_name(tab_name, col_names);
    if (!disk_manager_->is_file(ix_name))
    {
        throw IndexNotFoundError(tab_name, col_names);
    }

    ix_manager_->close_index(ihs_[ix_name].get());
    ix_manager_->destroy_index(ix_name);
    ihs_.erase(ix_name);
    table_meta.indexes.erase(ix_name);
    // 持久化
    flush_meta();
}

/**
 * @description: 删除索引
 * @param {string&} tab_name 表名称
 * @param {vector<ColMeta>&} 索引包含的字段元数据
 * @param {Context*} context
 */
void SmManager::drop_index(const std::string &tab_name, const std::vector<ColMeta> &cols, Context *context)
{
    auto &table_meta = db_.get_table(tab_name);
    auto &&ix_name = ix_manager_->get_index_name(tab_name, cols);
    if (!disk_manager_->is_file(ix_name))
    {
        std::vector<std::string> col_names;
        col_names.reserve(cols.size());
        for (auto &col : cols)
        {
            col_names.emplace_back(col.name);
        }
        throw IndexNotFoundError(tab_name, col_names);
    }

    ix_manager_->close_index(ihs_[ix_name].get());
    ix_manager_->destroy_index(ix_name);
    ihs_.erase(ix_name);
    table_meta.indexes.erase(ix_name);
    // 持久化
    flush_meta();
}
