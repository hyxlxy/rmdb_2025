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
#include "mvcc_executor_base.h"
#include <set>
#include <vector>

class DistinctExecutor : public MVCCExecutorBase {
private:
    std::unique_ptr<AbstractExecutor> child_;
    std::set<std::vector<std::string>> seen_records_;  // 用于去重
    std::vector<std::unique_ptr<RmRecord>> distinct_results_;
    size_t current_index_;
    bool is_end_;

public:
    DistinctExecutor(std::unique_ptr<AbstractExecutor> child)
        : child_(std::move(child)), current_index_(0), is_end_(false) {}

    void beginTuple() override {
        child_->beginTuple();
        distinct_results_.clear();
        seen_records_.clear();
        current_index_ = 0;
        
        // 收集所有记录并去重
        while (!child_->is_end()) {
            auto record = child_->Next();
            if (record) {
                // 将记录转换为字符串向量用于比较
                std::vector<std::string> record_key = record_to_key(record.get());
                
                // 如果没有见过这个记录，添加到结果中
                if (seen_records_.find(record_key) == seen_records_.end()) {
                    seen_records_.insert(record_key);
                    distinct_results_.push_back(std::move(record));
                }
            }
            child_->nextTuple();
        }
        
        is_end_ = distinct_results_.empty();
    }

    void nextTuple() override {
        current_index_++;
        if (current_index_ >= distinct_results_.size()) {
            is_end_ = true;
        }
    }

    bool is_end() const override {
        return is_end_;
    }

    std::unique_ptr<RmRecord> Next() override {
        if (is_end()) return nullptr;
        
        // 返回当前记录的拷贝
        auto& current_record = distinct_results_[current_index_];
        auto result = std::make_unique<RmRecord>(current_record->size);
        memcpy(result->data, current_record->data, current_record->size);
        return result;
    }

    const std::vector<ColMeta>& cols() const override {
        return child_->cols();
    }

    size_t tupleLen() const override {
        return child_->tupleLen();
    }

private:
    // 将记录转换为可比较的字符串向量
    std::vector<std::string> record_to_key(RmRecord* record) {
        std::vector<std::string> key;
        const auto& col_metas = cols();
        
        for (const auto& col : col_metas) {
            std::string value;
            char* data_ptr = record->data + col.offset;
            
            switch (col.type) {
                case TYPE_INT:
                    value = std::to_string(*(int*)data_ptr);
                    break;
                case TYPE_FLOAT:
                    value = std::to_string(*(float*)data_ptr);
                    break;
                case TYPE_STRING:
                    value = std::string(data_ptr, col.len);
                    // 去除尾部空字符
                    value.erase(value.find_last_not_of('\0') + 1);
                    break;
            }
            key.push_back(value);
        }
        return key;
    }
};
