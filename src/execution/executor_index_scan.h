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

#include <float.h>
#include <limits.h>
#include <unordered_set>

#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "mvcc_executor_base.h"
#include "index/ix.h"
#include "system/sm.h"

class IndexScanExecutor : public MVCCExecutorBase
{
private:
    SmManager *sm_manager_;
    std::string tab_name_;                     // 表名称
    TabMeta tab_;                              // 表的元数据
    std::vector<Condition> conds_;             // 扫描条件
    RmFileHandle *fh_;                         // 表的数据文件句柄
    std::vector<ColMeta> cols_;                // 需要读取的字段
    size_t len_;                               // 选取出来的一条记录的长度
    std::vector<Condition> fed_conds_;         // 扫描条件，和conds_字段相同
    std::vector<Condition> raw_conds_;         // 保留原始where条件
    std::vector<std::string> index_col_names_; // index scan涉及到的索引包含的字段
    IndexMeta index_meta_;                     // index scan涉及到的索引元数据
    Rid rid_;
    std::unique_ptr<RecScan> scan_;
    std::unique_ptr<RmRecord> rm_record_;
    constexpr static int int_min_ = INT32_MIN;
    constexpr static int int_max_ = INT32_MAX;
    // 注意：FLT_MIN 是最小正数，不是最小负数，这里下界应为 -FLT_MAX
    constexpr static float float_min_ = -FLT_MAX;
    constexpr static float float_max_ = FLT_MAX;

public:
    IndexScanExecutor(SmManager *sm_manager, std::string tab_name, std::vector<Condition> conds,
                      std::vector<std::string> index_col_names,
                      Context *context) : sm_manager_(sm_manager), tab_name_(std::move(tab_name)),
                                          conds_(conds),     // 用于索引区间生成
                                          raw_conds_(conds), // 保留原始where条件
                                          index_col_names_(std::move(index_col_names))
    {
        context_ = context;
        tab_ = sm_manager_->db_.get_table(tab_name_);
        index_meta_ = tab_.get_index_meta(index_col_names_);
        fh_ = sm_manager_->fhs_.at(tab_name_).get();
        cols_ = tab_.cols;
        len_ = cols_.back().offset + cols_.back().len;
        // 使用不同的操作符映射策略
        auto get_swapped_operator = [](CompOp original_op) -> CompOp
        {
            switch (original_op)
            {
            case OP_EQ:
                return OP_EQ;
            case OP_NE:
                return OP_NE;
            case OP_LT:
                return OP_GT;
            case OP_GT:
                return OP_LT;
            case OP_LE:
                return OP_GE;
            case OP_GE:
                return OP_LE;
            default:
                return original_op;
            }
        };

        for (auto &condition : conds_)
        {
            bool needs_column_swap = (condition.lhs_col.tab_name != tab_name_);
            if (needs_column_swap)
            {
                assert(!condition.is_rhs_val && condition.rhs_col.tab_name == tab_name_);
                std::swap(condition.lhs_col, condition.rhs_col);
                condition.op = get_swapped_operator(condition.op);
            }
        }
        // 保留原始条件用于最终结果过滤
        fed_conds_ = raw_conds_;

        // 重新组织条件以匹配索引列顺序
        std::vector<Condition> index_optimized_conditions;
        for (const auto &index_column_name : index_col_names_)
        {
            for (const auto &condition : conds_)
            {
                bool is_matching_column = (condition.lhs_col.col_name == index_column_name);
                bool is_value_condition = condition.is_rhs_val;

                if (is_matching_column && is_value_condition)
                {
                    index_optimized_conditions.push_back(condition);
                    // 允许同一列有多个条件（范围查询）
                }
            }
        }
        conds_ = index_optimized_conditions;
    }

    void beginTuple() override
    {
        const auto &&index_name = sm_manager_->get_ix_manager()->get_index_name(tab_name_, index_col_names_);

        // 检查索引是否存在
        auto it = sm_manager_->ihs_.find(index_name);
        if (it == sm_manager_->ihs_.end())
        {
            throw IndexNotFoundError(tab_name_, index_col_names_);
        }
        const auto &&ih = it->second.get();

        Iid lower = ih->leaf_begin(), upper = ih->leaf_end();
        std::vector<char> lower_buf(index_meta_.col_tot_len);
        std::vector<char> upper_buf(index_meta_.col_tot_len);
        char *lower_key = lower_buf.data();
        char *upper_key = upper_buf.data();
        // 初始化为全最小/全最大
        set_remaining_all_min(0, 0, lower_key);
        set_remaining_all_max(0, 0, upper_key);

        int offset = 0;
        bool prefix_eq = true;
        for (size_t i = 0; i < index_col_names_.size(); ++i)
        {
            const std::string &col_name = index_col_names_[i];
            Condition *eq_cond = nullptr, *ge_cond = nullptr, *gt_cond = nullptr, *le_cond = nullptr, *lt_cond = nullptr;
            for (auto &cond : conds_)
            {
                if (cond.lhs_col.col_name == col_name && cond.is_rhs_val)
                {
                    switch (cond.op)
                    {
                    case OP_EQ:
                        eq_cond = &cond;
                        break;
                    case OP_GE:
                        ge_cond = &cond;
                        break;
                    case OP_GT:
                        gt_cond = &cond;
                        break;
                    case OP_LE:
                        le_cond = &cond;
                        break;
                    case OP_LT:
                        lt_cond = &cond;
                        break;
                    default:
                        break;
                    }
                }
            }
            auto &col_meta = index_meta_.cols[i];

            // 如果当前字段有等值条件且前缀匹配仍然有效
            if (prefix_eq && eq_cond)
            {
                if (col_meta.type == TYPE_STRING)
                {
                    copy_string_key(lower_key + offset, eq_cond->rhs_val.raw->data, col_meta.len);
                    copy_string_key(upper_key + offset, eq_cond->rhs_val.raw->data, col_meta.len);
                }
                else
                {
                    memcpy(lower_key + offset, eq_cond->rhs_val.raw->data, col_meta.len);
                    memcpy(upper_key + offset, eq_cond->rhs_val.raw->data, col_meta.len);
                }
            }
            else
            {
                // 一旦遇到非等值条件或没有条件，前缀匹配结束
                prefix_eq = false;

                // 处理下界条件
                if (eq_cond)
                {
                    // 即使prefix_eq为false，等值条件仍然可以用于设置边界
                    if (col_meta.type == TYPE_STRING)
                    {
                        copy_string_key(lower_key + offset, eq_cond->rhs_val.raw->data, col_meta.len);
                        copy_string_key(upper_key + offset, eq_cond->rhs_val.raw->data, col_meta.len);
                    }
                    else
                    {
                        memcpy(lower_key + offset, eq_cond->rhs_val.raw->data, col_meta.len);
                        memcpy(upper_key + offset, eq_cond->rhs_val.raw->data, col_meta.len);
                    }
                }
                else if (ge_cond)
                {
                    if (col_meta.type == TYPE_STRING)
                        copy_string_key(lower_key + offset, ge_cond->rhs_val.raw->data, col_meta.len);
                    else
                        memcpy(lower_key + offset, ge_cond->rhs_val.raw->data, col_meta.len);
                }
                else if (gt_cond)
                {
                    // **优化：> val 等价于 >= val+1（整数），直接调整下界，避免逐行过滤**
                    if (col_meta.type == TYPE_INT)
                    {
                        int val = *reinterpret_cast<const int *>(gt_cond->rhs_val.raw->data);
                        if (val < int_max_) val += 1;
                        memcpy(lower_key + offset, &val, sizeof(int));
                    }
                    else if (col_meta.type == TYPE_STRING)
                    {
                        copy_string_key(lower_key + offset, gt_cond->rhs_val.raw->data, col_meta.len);
                    }
                    else
                    {
                        memcpy(lower_key + offset, gt_cond->rhs_val.raw->data, col_meta.len);
                    }
                }
                // 没有下界条件则保持最小值（初始化时已设置）

                // 处理上界条件
                if (eq_cond)
                {
                    // 等值条件已经在上面处理了
                }
                else if (le_cond)
                {
                    if (col_meta.type == TYPE_STRING)
                        copy_string_key(upper_key + offset, le_cond->rhs_val.raw->data, col_meta.len);
                    else
                        memcpy(upper_key + offset, le_cond->rhs_val.raw->data, col_meta.len);
                }
                else if (lt_cond)
                {
                    // **优化：< val 等价于 <= val-1（整数），精确上界避免逐行过滤**
                    if (col_meta.type == TYPE_INT)
                    {
                        int val = *reinterpret_cast<const int *>(lt_cond->rhs_val.raw->data);
                        if (val > int_min_) val -= 1;
                        memcpy(upper_key + offset, &val, sizeof(int));
                    }
                    else if (col_meta.type == TYPE_STRING)
                    {
                        copy_string_key(upper_key + offset, lt_cond->rhs_val.raw->data, col_meta.len);
                    }
                    else
                    {
                        memcpy(upper_key + offset, lt_cond->rhs_val.raw->data, col_meta.len);
                    }
                }
                // 没有上界条件则保持最大值（初始化时已设置）

                // 如果当前字段没有任何条件，后续字段的条件都无法用于索引优化
                if (!eq_cond && !ge_cond && !gt_cond && !le_cond && !lt_cond)
                {
                    // 设置后续字段为全范围，然后跳出循环
                    set_remaining_all_min(offset + col_meta.len, i + 1, lower_key);
                    set_remaining_all_max(offset + col_meta.len, i + 1, upper_key);
                    break;
                }
            }
            offset += col_meta.len;
        }
        lower = ih->lower_bound(lower_key);
        upper = ih->upper_bound(upper_key);
        scan_ = std::make_unique<IxScan>(ih, lower, upper, sm_manager_->get_bpm());
        // 使用 std::vector<char> 管理内存，无需手动释放

        // 保留所有原始where条件进行最终过滤，确保结果正确性
        // 索引范围查询只是预过滤，最终还需要条件过滤
        fed_conds_ = raw_conds_;

        // **优化：收集索引覆盖的条件列，从 fed_conds_ 中剔除，减少逐行过滤开销**
        // 规则：对索引列上的 EQ/GE/LE(含GT→GE+1/LT→LE-1 转换后) 已由区间保证，无需再判断
        {
            // 建立已被索引覆盖的 (col_name, op) 集合
            std::unordered_set<std::string> covered_cols; // 完全等值覆盖的列
            bool prefix_eq_cover = true;
            for (size_t ci = 0; ci < index_col_names_.size(); ++ci)
            {
                const auto &col_name = index_col_names_[ci];
                bool has_eq = false, has_range = false;
                for (auto &cond : conds_) // conds_ 已是索引列条件
                {
                    if (cond.lhs_col.col_name == col_name && cond.is_rhs_val)
                    {
                        if (cond.op == OP_EQ) has_eq = true;
                        else has_range = true;
                    }
                }
                if (prefix_eq_cover && has_eq)
                    covered_cols.insert(col_name);
                else if (has_range || has_eq)
                {
                    covered_cols.insert(col_name); // range bound is exact after GT→GE+1
                    prefix_eq_cover = false;
                    break; // range stops prefix
                }
                else
                    break;
            }
            // 从 fed_conds_ 中移除被索引完全覆盖的单列值条件
            if (!covered_cols.empty())
            {
                auto new_end = std::remove_if(fed_conds_.begin(), fed_conds_.end(),
                    [&](const Condition &c) {
                        return c.is_rhs_val && covered_cols.count(c.lhs_col.col_name) > 0;
                    });
                fed_conds_.erase(new_end, fed_conds_.end());
            }
        }
        while (!scan_->is_end())
        {
            try {
                rid_ = scan_->rid();
                if (context_ != nullptr)
                {
                    context_->current_table_name_ = tab_name_;
                }
                rm_record_ = get_record_mvcc(fh_, rid_, context_, sm_manager_);
                if (!rm_record_)
                {
                    scan_->next();
                    continue;
                }
                if (cmp_conds(rm_record_.get(), fed_conds_, cols_))
                {
                    break;
                }
                scan_->next();
            } catch (const IndexEntryNotFoundError&) {
                // 索引条目不存在，跳过
                scan_->next();
            } catch (const RecordNotFoundError&) {
                // 记录不存在，跳过
                scan_->next();
            }
        }
    }

    void nextTuple() override
    {
        if (scan_->is_end())
        {
            return;
        }

        // 总是进行条件过滤，确保结果正确性
        for (scan_->next(); !scan_->is_end(); scan_->next())
        {
            try {
                rid_ = scan_->rid();
                if (context_ != nullptr)
                {
                    context_->current_table_name_ = tab_name_;
                }
                rm_record_ = get_record_mvcc(fh_, rid_, context_, sm_manager_);
                if (!rm_record_)
                {
                    continue;
                }
                if (cmp_conds(rm_record_.get(), fed_conds_, cols_))
                {
                    break;
                }
            } catch (const IndexEntryNotFoundError&) {
                // 索引条目不存在，跳过
                continue;
            } catch (const RecordNotFoundError&) {
                // 记录不存在，跳过
                continue;
            }
        }
    }

    std::unique_ptr<RmRecord> Next() override
    {
        if (scan_->is_end() || !rm_record_) {
            return nullptr;
        }

        if (context_ != nullptr)
        {
            context_->current_table_name_ = tab_name_;
        }

        // 创建记录的副本而不是移动，避免多次调用时出现空指针
        auto record = std::make_unique<RmRecord>(rm_record_->size);
        if (record && record->data && rm_record_->data) {
            memcpy(record->data, rm_record_->data, rm_record_->size);
        }
        return record;
    }

    Rid &rid() override { return rid_; }

    bool is_end() const { return scan_->is_end(); }

    const std::vector<ColMeta> &cols() const override { return cols_; }

    size_t tupleLen() const override { return len_; }

    // 根据不同的列值类型设置不同的最大值
    // int   类型范围 int_min_ ~ int_max_
    // float 类型范围 float_min_ ~ float_max_
    // char  类型范围 0 ~ 255
    void set_remaining_all_max(int offset, int last_idx, char *&key)
    {
        // 设置成最大值
        for (size_t i = static_cast<size_t>(last_idx); i < index_meta_.cols.size(); ++i)
        {
            auto &col = index_meta_.cols[i];
            if (col.type == TYPE_INT)
            {
                memcpy(key + offset, &int_max_, sizeof(int));
            }
            else if (col.type == TYPE_FLOAT)
            {
                memcpy(key + offset, &float_max_, sizeof(float));
            }
            else if (col.type == TYPE_STRING)
            {
                memset(key + offset, 0xff, col.len);
            }
            else
            {
                throw InternalError("Unexpected data type！");
            }
            offset += col.len;
        }
    }

    // 根据不同的列值类型设置不同的最小值
    // int   类型范围 int_min_ ~ int_max_
    // float 类型范围 float_min_ ~ float_max_
    // char  类型范围 0 ~ 255
    void set_remaining_all_min(int offset, int last_idx, char *&key)
    {
        for (size_t i = static_cast<size_t>(last_idx); i < index_meta_.cols.size(); ++i)
        {
            auto &col = index_meta_.cols[i];
            if (col.type == TYPE_INT)
            {
                memcpy(key + offset, &int_min_, sizeof(int));
            }
            else if (col.type == TYPE_FLOAT)
            {
                memcpy(key + offset, &float_min_, sizeof(float));
            }
            else if (col.type == TYPE_STRING)
            {
                memset(key + offset, 0, col.len);
            }
            else
            {
                throw InternalError("Unexpected data type！");
            }
            offset += col.len;
        }
    }

    static inline int compare(const char *a, const char *b, int col_len, ColType col_type)
    {
        // 使用不同的比较实现策略
        if (col_type == TYPE_INT)
        {
            int left_value = *reinterpret_cast<const int *>(a);
            int right_value = *reinterpret_cast<const int *>(b);
            return (left_value == right_value) ? 0 : ((left_value < right_value) ? -1 : 1);
        }
        else if (col_type == TYPE_FLOAT)
        {
            float left_value = *reinterpret_cast<const float *>(a);
            float right_value = *reinterpret_cast<const float *>(b);
            return (left_value == right_value) ? 0 : ((left_value < right_value) ? -1 : 1);
        }
        else if (col_type == TYPE_STRING)
        {
            return memcmp(a, b, col_len);
        }
        else
        {
            throw InternalError("Unexpected data type！");
        }
    }

    // 判断是否满足单个谓词条件
    bool cmp_cond(const RmRecord *rec, const Condition &cond, const std::vector<ColMeta> &rec_cols)
    {
        const auto &lhs_col_meta = get_col(rec_cols, cond.lhs_col);
        const char *lhs_data = rec->data + lhs_col_meta->offset;
        const char *rhs_data;
        ColType rhs_type;

        // 提取左值与右值的数据和类型
        // 常值
        if (cond.is_rhs_val)
        {
            rhs_type = cond.rhs_val.type;
            rhs_data = cond.rhs_val.raw->data;
        }
        else
        {
            // 列值
            const auto &rhs_col_meta = get_col(rec_cols, cond.rhs_col);
            rhs_type = rhs_col_meta->type;
            rhs_data = rec->data + rhs_col_meta->offset;
        }

        // 类型检查，允许数值类型之间的比较
        int cmp;
        if (lhs_col_meta->type != rhs_type)
        {
            // 检查是否都是数值类型（INT或FLOAT）
            bool lhs_is_numeric = (lhs_col_meta->type == TYPE_INT || lhs_col_meta->type == TYPE_FLOAT);
            bool rhs_is_numeric = (rhs_type == TYPE_INT || rhs_type == TYPE_FLOAT);

            if (!lhs_is_numeric || !rhs_is_numeric)
            {
                throw IncompatibleTypeError(coltype2str(lhs_col_meta->type), coltype2str(rhs_type));
            }

            // 数值类型之间的比较，转换为浮点数进行比较
            float lhs_val, rhs_val;
            if (lhs_col_meta->type == TYPE_INT)
            {
                lhs_val = static_cast<float>(*reinterpret_cast<const int *>(lhs_data));
            }
            else
            {
                lhs_val = *reinterpret_cast<const float *>(lhs_data);
            }

            if (rhs_type == TYPE_INT)
            {
                rhs_val = static_cast<float>(*reinterpret_cast<const int *>(rhs_data));
            }
            else
            {
                rhs_val = *reinterpret_cast<const float *>(rhs_data);
            }

            cmp = (lhs_val == rhs_val) ? 0 : ((lhs_val < rhs_val) ? -1 : 1);
        }
        else
        {
            cmp = compare(lhs_data, rhs_data, lhs_col_meta->len, rhs_type);
        }

        // 调试输出（关闭以避免运行时IO开销）
        #if !DISABLE_DEBUG_OUTPUT
        if (rhs_type == TYPE_STRING && cond.op == OP_NE)
        {
            std::cerr << "  LHS: ";
            for (int i = 0; i < lhs_col_meta->len; i++)
            {
                std::cerr << "'" << lhs_data[i] << "' ";
            }
            std::cerr << std::endl;
            std::cerr << "  RHS: ";
            for (int i = 0; i < lhs_col_meta->len; i++)
            {
                std::cerr << "'" << rhs_data[i] << "' ";
            }
            std::cerr << std::endl;
            std::cerr << "  CMP result: " << cmp << std::endl;
            std::cerr << "  NE result: " << (cmp != 0) << std::endl;
        }
        #endif

        switch (cond.op)
        {
        case OP_EQ:
            return cmp == 0;
        case OP_NE:
            return cmp != 0;
        case OP_LT:
            return cmp < 0;
        case OP_GT:
            return cmp > 0;
        case OP_LE:
            return cmp <= 0;
        case OP_GE:
            return cmp >= 0;
        default:
            throw InternalError("Unexpected op type！");
        }
    }

    bool cmp_conds(const RmRecord *rec, const std::vector<Condition> &conds, const std::vector<ColMeta> &rec_cols)
    {
        return std::all_of(conds.begin(), conds.end(), [&](const Condition &cond)
                           { return cmp_cond(rec, cond, rec_cols); });
    }

    // 工具函数：安全拷贝字符串到 key，右侧补空格
    static void copy_string_key(char *dst, const char *src, int dst_len)
    {
        int actual_len = strnlen(src, dst_len);
        memcpy(dst, src, actual_len);
        if (actual_len < dst_len)
        {
            memset(dst + actual_len, ' ', dst_len - actual_len);
        }
    }

    /**
     * @brief 检查索引是否完全覆盖查询条件
     * 如果所有查询条件都能通过索引范围查询满足，则无需额外过滤
     */
    bool are_conditions_covered_by_index() const
    {
        // 如果没有使用索引条件，说明需要全表扫描后过滤
        if (conds_.empty()) {
            return false;
        }

        // 检查原始条件是否都能被索引条件覆盖
        for (const auto &raw_cond : raw_conds_) {
            if (!raw_cond.is_rhs_val) {
                // 列与列的比较无法通过索引优化
                return false;
            }

            bool covered = false;
            for (const auto &index_cond : conds_) {
                if (raw_cond.lhs_col.col_name == index_cond.lhs_col.col_name &&
                    raw_cond.op == index_cond.op &&
                    raw_cond.rhs_val.raw == index_cond.rhs_val.raw) {
                    covered = true;
                    break;
                }
            }

            if (!covered) {
                return false;
            }
        }

        return true;
    }
public:
    // 检查是否没有过滤条件
    bool has_no_conditions() const {
        return fed_conds_.empty();
    }
    
    // 获取表的文件句柄
    RmFileHandle* get_file_handle() const {
        return fh_;
    }

    // 获取索引列名
    const std::vector<std::string>& get_index_col_names() const {
        return index_col_names_;
    }
};
