#pragma once
#include <limits>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <list>
#include <set>

#include "common/config.h"




enum RCInfo {
    RC_SUCCESS,
    RC_RECORD_INVISIBLE,
    RC_LOCKED_CONFLICT
};

struct HeapTupleFields {
    txn_id_t t_xmin;  // 创建事务ID（提交后为正，未提交为负）
    txn_id_t t_xmax;  // 删除事务ID（0表示未删除，正表示已提交删除，负表示未提交删除）
    txn_id_t t_xvac;  // VACUUM事务ID
    std::vector<txn_id_t> old_versions; // 版本链指针
};

struct VersionLink
{
    HeapTupleFields tuple;
    RmRecord *record{nullptr};
    VersionLink *pre_{nullptr};

    ~VersionLink()
    {
        if(record)delete record;
        if(pre_)delete pre_;
    }
};


class MvccMap {
private:
    std::mutex latch_;
    std::unordered_map<int, HeapTupleFields> map_;
    // static std::set<txn_id_t> active_transactions_; // 活跃事务集合

public:
    // // 注册新事务
    // void register_transaction(txn_id_t id) {
    //     std::lock_guard<std::mutex> lock(latch_);
    //     active_transactions_.insert(id);
    // }

    // // 注销事务
    // void unregister_transaction(txn_id_t id) {
    //     std::lock_guard<std::mutex> lock(latch_);
    //     active_transactions_.erase(id);
    // }

    // // 获取事务快照
    // std::set<txn_id_t> get_snapshot() const {
    //     return active_transactions_;
    // }
    RCInfo visit_record(int slot, txn_id_t id,bool is_readolny=false) {
        std::unique_lock<std::mutex> lock(latch_);
        auto it = map_.find(slot);
        if(it == map_.end())return RCInfo::RC_SUCCESS;
        auto xmin = it->second.t_xmin;
        auto xmax = it->second.t_xmax;

        if(xmin>0 && xmax>0)
        {
            if(id>=xmin && id<=xmax)
            {
                return RCInfo::RC_SUCCESS;
            }
            else return RCInfo::RC_RECORD_INVISIBLE;
        }
        else if(xmin<0)
        {
            //插入 但未提交的数据
            return (-xmin == id)? RCInfo::RC_SUCCESS:RCInfo::RC_RECORD_INVISIBLE;
        }
        else if(xmax<0)
        {
            //删除 但未提交
            //仅仅读数据
            if(is_readolny)
            {
                return (-xmax == id)? RCInfo::RC_SUCCESS:RCInfo::RC_RECORD_INVISIBLE;
            }
            else
            {
                return (-xmax!=id)?RCInfo::RC_LOCKED_CONFLICT:RCInfo::RC_RECORD_INVISIBLE;
            }
        }
    }

    // 插入记录（版本链管理）
    RCInfo insert_record(int slot, txn_id_t id) {
        std::lock_guard<std::mutex> lock(latch_);
        
        HeapTupleFields new_hpf;
        new_hpf.t_xmin = -id;  // 未提交状态
        new_hpf.t_xmax = std::numeric_limits<int32_t>::max();    // 初始未删除
        
        // // 保留旧版本指针
        // if (auto old_it = map_.find(slot); old_it != map_.end()) {
        //     new_hpf.old_versions.push_back(old_it->second.t_xmin);
        // }
        
        map_[slot] = std::move(new_hpf);
        return RC_SUCCESS;
    }

    // 删除记录（版本链管理）
    RCInfo delete_record(int slot, txn_id_t id) { 
        std::lock_guard<std::mutex> lock(latch_);
        if (auto it = map_.find(slot); it != map_.end()) {
            it->second.t_xmax = -id;  // 标记未提交删除
            return RC_SUCCESS;
        }
        return RC_RECORD_INVISIBLE;
    }

    // 提交插入
    RCInfo commit_insert(int slot, txn_id_t id) {
        std::lock_guard<std::mutex> lock(latch_);
        if (auto it = map_.find(slot); it != map_.end()) {
            it->second.t_xmin = id;  // 转为正数表示已提交
            map_.erase(it);
            return RC_SUCCESS;
        }
        return RC_RECORD_INVISIBLE;
    }

    // 提交删除
    RCInfo commit_delete(int slot, txn_id_t id) {
        std::lock_guard<std::mutex> lock(latch_);
        if (auto it = map_.find(slot); it != map_.end()) {
            it->second.t_xmax = id;  // 转为正数表示已提交
            map_.erase(it);
            return RC_SUCCESS;
        }
        return RC_RECORD_INVISIBLE;
    }

    // void vacuum_old_versions(txn_id_t oldest_active_txn) {
    //     std::lock_guard<std::mutex> lock(latch_);
        
    //     for (auto it = map_.begin(); it != map_.end(); ) {
    //         auto& hpf = it->second;
            
    //         // 清理条件：
    //         // 1. 记录已提交（t_xmin > 0）
    //         // 2. 已被删除（t_xmax > 0）
    //         // 3. 删除事务早于最老活跃事务
    //         // 4. 没有活跃事务需要访问此版本
    //         if (hpf.t_xmin > 0 && 
    //             hpf.t_xmax > 0 && 
    //             hpf.t_xmax < oldest_active_txn &&
    //             !has_active_references(hpf, oldest_active_txn)) 
    //         {
    //             it = map_.erase(it);
    //         } else {
    //             ++it;
    //         }
    //     }
    // }

private:
    // 检查是否有活跃事务依赖此版本
    // bool has_active_references(const HeapTupleFields& hpf, txn_id_t oldest_active) const {
    //     // 检查主版本
    //     if (hpf.t_xmin >= oldest_active || 
    //        (hpf.t_xmax > 0 && hpf.t_xmax >= oldest_active)) {
    //         return true;
    //     }
        
    //     // 检查版本链中的旧版本
    //     for (txn_id_t old_ver : hpf.old_versions) {
    //         if (old_ver >= oldest_active) return true;
    //     }
        
    //     return false;
    // }
};