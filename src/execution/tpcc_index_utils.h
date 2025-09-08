#pragma once
#include <limits>
#include <string>
#include <vector>
#include <memory>
#include "system/sm_manager.h"
#include "index/ix.h"
#include "record/rm.h"

// 通用：在表元数据中按列名顺序查找匹配的复合索引名
inline bool find_index_by_cols(TabMeta &tab, const std::vector<std::string> &cols, std::string &out_name) {
    for (auto &[iname, meta] : tab.indexes) {
        if (meta.col_num != (int)cols.size()) continue;
        bool ok = true;
        for (int i = 0; i < meta.col_num; ++i) {
            if (meta.cols[i].name != cols[i]) { ok = false; break; }
        }
        if (ok) { out_name = iname; return true; }
    }
    return false;
}

// 构造复合键（按索引列顺序），values.size 必须等于 index.col_num
inline std::unique_ptr<char[]> build_comp_key(const TabMeta &tab, const std::string &index_name, const std::vector<int> &values) {
    const auto &meta = tab.indexes.at(index_name);
    std::unique_ptr<char[]> key(new char[meta.col_tot_len]);
    int off = 0;
    for (int i = 0; i < meta.col_num; ++i) {
        memcpy(key.get() + off, &values[i], meta.cols[i].len);
        off += meta.cols[i].len;
    }
    return key;
}

// 通过 orders(o_w_id,o_d_id,o_id) 复合索引判断是否存在该订单
inline bool orders_exists_via_index(SmManager *sm, int w_id, int d_id, int o_id, Transaction *txn) {
    TabMeta &otab = sm->db_.get_table("orders");
    std::string idx_name;
    if (!find_index_by_cols(otab, {"o_w_id","o_d_id","o_id"}, idx_name)) return false;
    auto ih = sm->ihs_.at(idx_name).get();
    auto key = build_comp_key(otab, idx_name, {w_id,d_id,o_id});
    std::vector<Rid> results;
    return ih->get_value(key.get(), &results, txn) && !results.empty();
}

// 使用 orders 复合索引计算该分区的期望 next_o_id（无记录则返回 1）
inline int compute_expected_next_oid_via_index(SmManager *sm, int w_id, int d_id, Context *ctx) {
    try {
        TabMeta &otab = sm->db_.get_table("orders");
        std::string idx_name;
        if (find_index_by_cols(otab, {"o_w_id","o_d_id","o_id"}, idx_name)) {
            const auto &meta = otab.indexes[idx_name];
            auto ih = sm->ihs_.at(idx_name).get();
            // 范围 [low, high)
            std::unique_ptr<char[]> low(new char[meta.col_tot_len]);
            std::unique_ptr<char[]> high(new char[meta.col_tot_len]);
            int off = 0;
            memcpy(low.get()+off, &w_id, meta.cols[0].len); off += meta.cols[0].len;
            memcpy(low.get()+off, &d_id, meta.cols[1].len); off += meta.cols[1].len;
            int oi_min = std::numeric_limits<int>::min();
            memcpy(low.get()+off, &oi_min, meta.cols[2].len);
            off = 0;
            memcpy(high.get()+off, &w_id, meta.cols[0].len); off += meta.cols[0].len;
            memcpy(high.get()+off, &d_id, meta.cols[1].len); off += meta.cols[1].len;
            int oi_max = std::numeric_limits<int>::max();
            memcpy(high.get()+off, &oi_max, meta.cols[2].len);

            Iid lower = ih->lower_bound(low.get());
            Iid upper = ih->upper_bound(high.get());
            IxScan scan(ih, lower, upper, sm->get_bpm());
            Rid last; bool has = false;
            while (!scan.is_end()) { last = scan.rid(); scan.next(); has = true; }
            if (!has) return 1;
            auto ofh = sm->fhs_.at("orders").get();
            auto orec = ofh->get_record(last, ctx);
            int o_id = *(int*)(orec->data + otab.get_col("o_id")->offset);
            return o_id + 1;
        }
    } catch (...) { /* fallback below */ }
    // 回退：顺序扫描
    int max_oid = -1;
    try {
        auto &otab = sm->db_.get_table("orders");
        auto ofh = sm->fhs_.at("orders").get();
        RmScan scan(ofh);
        while (!scan.is_end()) {
            auto orec = ofh->get_record(scan.rid(), ctx);
            int ow = *(int*)(orec->data + otab.get_col("o_w_id")->offset);
            int od = *(int*)(orec->data + otab.get_col("o_d_id")->offset);
            if (ow == w_id && od == d_id) {
                int oi = *(int*)(orec->data + otab.get_col("o_id")->offset);
                if (oi > max_oid) max_oid = oi;
            }
            scan.next();
        }
    } catch (...) {}
    return (max_oid >= 0 ? max_oid + 1 : 1);
}

