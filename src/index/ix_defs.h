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

#include <vector>

#include "defs.h"
#include "storage/buffer_pool_manager.h"

constexpr int IX_NO_PAGE = -1;
constexpr int IX_FILE_HDR_PAGE = 0;
constexpr int IX_LEAF_HEADER_PAGE = 1;
constexpr int IX_INIT_ROOT_PAGE = 2;
constexpr int IX_INIT_NUM_PAGES = 3;
constexpr int IX_MAX_COL_LEN = 512;

class IxFileHdr {
public: 
    page_id_t first_free_page_no_;      // 文件中第一个空闲的磁盘页面的页面号
    int num_pages_;                     // 磁盘文件中页面的数量
    page_id_t root_page_;               // B+树根节点对应的页面号
    int col_num_;                       // 索引包含的字段数量
    std::vector<ColType> col_types_;    // 字段的类型
    std::vector<int> col_lens_;         // 字段的长度
    int col_tot_len_;                   // 索引包含的字段的总长度
    int btree_order_;                   // # children per page 每个结点最多可插入的键值对数量
    int keys_size_;                     // keys_size = (btree_order + 1) * col_tot_len
    // first_leaf初始化之后没有进行修改，只不过是在测试文件中遍历叶子结点的时候用了
    page_id_t first_leaf_;              // 首叶节点对应的页号，在上层IxManager的open函数进行初始化，初始化为root page_no
    page_id_t last_leaf_;               // 尾叶节点对应的页号
    int tot_len_;                       // 记录结构体的整体长度

    IxFileHdr() {
        tot_len_ = col_num_ = 0;
    }
  // 参数构造函数（不包含 col_types 和 col_lens 的初始化）
    IxFileHdr(page_id_t first_free_page_no, int num_pages, page_id_t root_page, int col_num,
                int col_tot_len, int btree_order, int keys_size, page_id_t first_leaf, page_id_t last_leaf)
                : first_free_page_no_(first_free_page_no), num_pages_(num_pages), root_page_(root_page), col_num_(col_num),
                col_tot_len_(col_tot_len), btree_order_(btree_order), keys_size_(keys_size), first_leaf_(first_leaf), last_leaf_(last_leaf) {
                    tot_len_ = 0;
                }

  // 根据当前成员计算 tot_len_
    void update_tot_len() {
        tot_len_ = 0;
        // 先算固定字段：4 个 page_id_t + 6 个 int
        tot_len_ += sizeof(page_id_t) * 4 + sizeof(int) * 6;
        // 再加上 col_types_ 和 col_lens_ 的大小
        tot_len_ += sizeof(ColType) * col_num_ + sizeof(int) * col_num_;
    }
      // 将头信息序列化到 dest 缓冲区（调用前应确保 dest 足够大，至少 >= tot_len_）
    void serialize(char* dest) {
        int offset = 0;

        // 写 tot_len_
        memcpy(dest + offset, &tot_len_, sizeof(int));
        offset += sizeof(int);

        // 写 first_free_page_no_
        memcpy(dest + offset, &first_free_page_no_, sizeof(page_id_t));
        offset += sizeof(page_id_t);

        // 写 num_pages_
        memcpy(dest + offset, &num_pages_, sizeof(int));
        offset += sizeof(int);

        // 写 root_page_
        memcpy(dest + offset, &root_page_, sizeof(page_id_t));
        offset += sizeof(page_id_t);

        // 写 col_num_
        memcpy(dest + offset, &col_num_, sizeof(int));
        offset += sizeof(int);

        // 写 col_types_（共 col_num_ 个 ColType）
        for (int i = 0; i < col_num_; ++i) {
            memcpy(dest + offset, &col_types_[i], sizeof(ColType));
            offset += sizeof(ColType);
        }

        // 写 col_lens_（共 col_num_ 个 int）
        for (int i = 0; i < col_num_; ++i) {
            memcpy(dest + offset, &col_lens_[i], sizeof(int));
            offset += sizeof(int);
        }

        // 写 col_tot_len_
        memcpy(dest + offset, &col_tot_len_, sizeof(int));
        offset += sizeof(int);

        // 写 btree_order_
        memcpy(dest + offset, &btree_order_, sizeof(int));
        offset += sizeof(int);

        // 写 keys_size_
        memcpy(dest + offset, &keys_size_, sizeof(int));
        offset += sizeof(int);

        // 写 first_leaf_
        memcpy(dest + offset, &first_leaf_, sizeof(page_id_t));
        offset += sizeof(page_id_t);

        // 写 last_leaf_
        memcpy(dest + offset, &last_leaf_, sizeof(page_id_t));
        offset += sizeof(page_id_t);

        // 最后做一次断言，确保 offset 正好等于 tot_len_
        assert(offset == tot_len_);
    }
   // 从 src 缓冲区反序列化到成员变量
    void deserialize(char* src) {
        int offset = 0;
        tot_len_ = *reinterpret_cast<const int*>(src + offset);
        offset += sizeof(int);
        first_free_page_no_ = *reinterpret_cast<const page_id_t*>(src + offset);
        offset += sizeof(page_id_t);
        num_pages_ = *reinterpret_cast<const int*>(src + offset);
        offset += sizeof(int);
        root_page_ = *reinterpret_cast<const page_id_t*>(src + offset);
        offset += sizeof(page_id_t);
        col_num_ = *reinterpret_cast<const int*>(src + offset);
        offset += sizeof(int);
        for(int i = 0; i < col_num_; ++i) {
            // col_types_[i] = *reinterpret_cast<const ColType*>(src + offset);
            ColType type = *reinterpret_cast<const ColType*>(src + offset);
            offset += sizeof(ColType);
            col_types_.push_back(type);
        }
        for(int i = 0; i < col_num_; ++i) {
            // col_lens_[i] = *reinterpret_cast<const int*>(src + offset);
            int len = *reinterpret_cast<const int*>(src + offset);
            offset += sizeof(int);
            col_lens_.push_back(len);
        }
        col_tot_len_ = *reinterpret_cast<const int*>(src + offset);
        offset += sizeof(int);
        btree_order_ = *reinterpret_cast<const int*>(src + offset);
        offset += sizeof(int);
        keys_size_ = *reinterpret_cast<const int*>(src + offset);
        offset += sizeof(int);
        first_leaf_ = *reinterpret_cast<const page_id_t*>(src+ offset);
        offset += sizeof(page_id_t);
        last_leaf_ = *reinterpret_cast<const page_id_t*>(src + offset);
        offset += sizeof(page_id_t);
          // 确保反序列化位置匹配
        assert(offset == tot_len_);
    }
};

class IxPageHdr {
public:
    page_id_t next_free_page_no;    // unused
    page_id_t parent;               // 父亲节点所在页面的叶号
    int num_key;                    // # current keys (always equals to #child - 1) 已插入的keys数量，key_idx∈[0,num_key)
    bool is_leaf;                   // 是否为叶节点
    page_id_t prev_leaf;            // previous leaf node's page_no, effective only when is_leaf is true
    page_id_t next_leaf;            // next leaf node's page_no, effective only when is_leaf is true
};

class Iid {
public:
    int page_no;
    int slot_no;

    friend bool operator==(const Iid &x, const Iid &y) { return x.page_no == y.page_no && x.slot_no == y.slot_no; }

    friend bool operator!=(const Iid &x, const Iid &y) { return !(x == y); }
};
