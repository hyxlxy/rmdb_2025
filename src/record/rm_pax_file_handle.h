#pragma once

#include <assert.h>

#include <memory>

#include "bitmap.h"
#include "common/context.h"
#include "rm_defs.h"


//实现pax存储
class RmManager;


struct PaxPageHandle 
{
    const PaxIdx *file_hdr;  // 当前页面所在文件的文件头指针
    Page *page;                 // 页面的实际数据，包括页面存储的数据、元信息等
    RmPageHdr *page_hdr;
    // int *col_idxs;
    char *bitmap;
    char *data;
    int max_record_num;
    PaxPageHandle(const PaxIdx *fhdr_, Page *page_) : file_hdr(fhdr_), page(page_)
    {
        page_hdr = reinterpret_cast<RmPageHdr *>(page->get_data() + page->OFFSET_PAGE_HDR);
        bitmap = page->get_data() + (sizeof(RmPageHdr) + page->OFFSET_PAGE_HDR);
        data = bitmap+file_hdr->file_hdr.bitmap_size;
        max_record_num = file_hdr->file_hdr.num_records_per_page;
    }

    PaxPageHandle() = default;

    char *get_data(int slot_num,int idx,int &copy_size)
    {
        if(idx == 0)
        {
            copy_size = file_hdr->col_idxs[0]/max_record_num;
            return data+(slot_num*(copy_size));
        }
        else
        {
            copy_size = (file_hdr->col_idxs[idx]-file_hdr->col_idxs[idx-1])/max_record_num;
            return data+(file_hdr->col_idxs[idx-1]+slot_num*(copy_size));
        }
    }
};

class PaxFileHandle
{
    friend class RmManager;
    friend class PaxScan;
    private:
    DiskManager *disk_manager_;
    BufferPoolManager *buffer_pool_manager_;
    int fd_;        // 打开文件后产生的文件句柄
    PaxIdx *file_hdr_=nullptr;
    public:
    PaxFileHandle(DiskManager *disk_manager, BufferPoolManager *buffer_pool_manager, int fd)
        : disk_manager_(disk_manager), buffer_pool_manager_(buffer_pool_manager), fd_(fd) 
    {
        char *data_ = new char[PAGE_SIZE];
        disk_manager_->read_page(fd_,RM_FILE_HDR_PAGE,data_,PAGE_SIZE);
        file_hdr_ = new PaxIdx();
        file_hdr_->deserialize(data_);
        delete []data_;
        disk_manager_->set_fd2pageno(fd, file_hdr_->file_hdr.num_pages);
    }

    ~PaxFileHandle()
    {
        if(file_hdr_)
        delete file_hdr_;
    }

    const PaxIdx &get_file_hdr() { return *file_hdr_; }
    void add_record_num(size_t num){file_hdr_->file_hdr.tot_count_+=num;}
    int GetFd() { return fd_; }

public:
    /* 判断指定位置上是否已经存在一条记录，通过Bitmap来判断 */
    bool is_record(const Rid &rid) const {
        PaxPageHandle page_handle = fetch_page_handle(rid.page_no);
        return Bitmap::is_set(page_handle.bitmap, rid.slot_no);  // page的slot_no位置上是否有record
    }

    std::unique_ptr<RmRecord> get_record(const Rid &rid, Context *context) const
    {
        auto page_handle = fetch_page_handle(rid.page_no);
        auto res_ = std::make_unique<RmRecord>(file_hdr_->file_hdr.record_size);
        int offset_ = 0;
        int copy_size = 0;
        for(int i=0;i<file_hdr_->col_num;i++)
        {
            memcpy(res_->data+offset_,page_handle.get_data(rid.slot_no,i,copy_size),copy_size);
            offset_+=copy_size;
        }
        buffer_pool_manager_->unpin_page(page_handle.page->get_page_id(),false);
        return res_;
    }

    RmRecord *get_ptr_record(const Rid &rid, Context *context,bool get_for_dml) const
    {
        auto page_handle = fetch_page_handle(rid.page_no);
        auto res_ = new RmRecord(file_hdr_->file_hdr.record_size);
        int offset_ = 0;
        int copy_size = 0;
        for(int i=0;i<file_hdr_->col_num;i++)
        {
            memcpy(res_->data+offset_,page_handle.get_data(rid.slot_no,i,copy_size),copy_size);
            offset_+=copy_size;
        }
        buffer_pool_manager_->unpin_page(page_handle.page->get_page_id(),false);
        return res_;
    }

    Rid insert_record(char *buf, Context *context)
    {
        PaxPageHandle page_handle = create_page_handle();
        int slot_no = Bitmap::first_bit(false, page_handle.bitmap, 
            file_hdr_->file_hdr.num_records_per_page);
        Rid rid_ = Rid{.page_no = page_handle.page->get_page_id().page_no, .slot_no = slot_no};
        Bitmap::set(page_handle.bitmap,slot_no);
        page_handle.page_hdr->num_records++;
        file_hdr_->file_hdr.tot_count_++;
        int offset_ = 0;
        int copy_size = 0;
        for(int i=0;i<file_hdr_->col_num;i++)
        {
            memcpy(page_handle.get_data(slot_no,i,copy_size),buf+offset_,copy_size);
            offset_+=copy_size;
        }

        if (page_handle.page_hdr->num_records == file_hdr_->file_hdr.num_records_per_page){
            file_hdr_->file_hdr.first_free_page_no = page_handle.page_hdr->next_free_page_no;
            page_handle.page_hdr->next_free_page_no = INVALID_PAGE_ID;
        }

        buffer_pool_manager_->unpin_page(page_handle.page->get_page_id(),true);
        return rid_;

    }

    void insert_record(const Rid &rid, char *buf)
    {
        PaxPageHandle page_handle = fetch_page_handle(rid.page_no);
    //  
        if(!is_record(rid)){
            page_handle.page_hdr->num_records++;
        }

        Bitmap::set(page_handle.bitmap,rid.slot_no);
        file_hdr_->file_hdr.tot_count_++;
        
        int offset_ = 0;
        int copy_size = 0;
        for(int i=0;i<file_hdr_->col_num;i++)
        {
            memcpy(page_handle.get_data(rid.slot_no,i,copy_size),buf+offset_,copy_size);
            offset_+=copy_size;
        }

        if (page_handle.page_hdr->num_records == file_hdr_->file_hdr.num_records_per_page){
            file_hdr_->file_hdr.first_free_page_no = page_handle.page_hdr->next_free_page_no;
            page_handle.page_hdr->next_free_page_no = INVALID_PAGE_ID;
        }

        buffer_pool_manager_->unpin_page(page_handle.page->get_page_id(),true);
    }

    void delete_record(const Rid &rid, Context *context)
    {
        PaxPageHandle page_handle = create_page_handle();
        if (!Bitmap::is_set(page_handle.bitmap, rid.slot_no)) {
            return ;
        }
        Bitmap::reset(page_handle.bitmap, rid.slot_no);
        page_handle.page_hdr->num_records--;

        if (page_handle.page_hdr->num_records == file_hdr_->file_hdr.num_records_per_page - 1) {
            release_page_handle(page_handle);
        }
        file_hdr_->file_hdr.tot_count_--;

        buffer_pool_manager_->unpin_page(page_handle.page->get_page_id(), true);
    }

    void update_record(const Rid &rid, char *buf, Context *context)
    {
        PaxPageHandle page_handle = fetch_page_handle(rid.page_no);
        int offset_ = 0;
        int copy_size = 0;
        for(int i=0;i<file_hdr_->col_num;i++)
        {
            memcpy(page_handle.get_data(rid.slot_no,i,copy_size),buf+offset_,copy_size);
            offset_+=copy_size;
        }
        buffer_pool_manager_->unpin_page(page_handle.page->get_page_id(), true);
    }

    PaxPageHandle create_new_page_handle()
    {
        PageId new_page_id{fd_, file_hdr_->file_hdr.num_pages};
        Page *new_page = buffer_pool_manager_->new_page(&new_page_id);
        if(new_page == nullptr){
            throw UnixError();
        }
        PaxPageHandle page_handle(file_hdr_, new_page);
        // 插入空闲页
        page_handle.page_hdr->next_free_page_no = file_hdr_->file_hdr.first_free_page_no;
        file_hdr_->file_hdr.first_free_page_no = new_page_id.page_no;
        file_hdr_->file_hdr.num_pages++;

        return page_handle;
    }

    PaxPageHandle fetch_page_handle(int page_no) const
    {
        if (page_no < 0 || page_no >= file_hdr_->file_hdr.num_pages) {
            throw RecordNotFoundError(page_no, -1);  // 页面不存在
        }
        PageId page_id = {fd_, page_no};
        Page *page = buffer_pool_manager_->fetch_page(page_id);

        if (page == nullptr || page->get_page_id().page_no == INVALID_PAGE_ID) {
            throw RecordNotFoundError(page_no, -1);  // 缓冲池中未能获取该页
        }

        return PaxPageHandle(file_hdr_, page);
    }

    PaxPageHandle *fetch_ptr_handle(int page_no)const
    {
        if (page_no < 0 || page_no >= file_hdr_->file_hdr.num_pages) {
            throw RecordNotFoundError(page_no, -1);  // 页面不存在
        }
        PageId page_id = {fd_, page_no};
        Page *page = buffer_pool_manager_->fetch_page(page_id);

        if (page == nullptr || page->get_page_id().page_no == INVALID_PAGE_ID) {
            throw RecordNotFoundError(page_no, -1);  // 缓冲池中未能获取该页
        }

        return new PaxPageHandle(file_hdr_, page);
    }


    void get_chunk(page_id_t &page_no,Chunk &chunk)const
    {
        // if(page_no<0 || page_no>file_hdr_.file_hdr.num_pages)
        // {
        //     throw InternalError("Error Page");
        // }
        auto page_hdl = fetch_page_handle(page_no);
        for(size_t i=0;i<chunk.datas_.size();i++)
        {
            int idx = chunk.chunkdata_idx(i);
            int copy_size=0;
            chunk.datas_[i].reset_data(page_hdl.get_data(0,idx,copy_size),page_hdl.page_hdr->num_records);
        }
        buffer_pool_manager_->unpin_page(page_hdl.page->get_page_id(),false);
        page_no++;
        if(page_no==file_hdr_->file_hdr.num_pages)page_no=-1;
    }

    // void load_with_chunk(const Chunk &chunk,int pos_)
    // {
    //     //这里必须保证了列是齐全的
    // }

    private:
    PaxPageHandle create_page_handle()
    {
        if (file_hdr_->file_hdr.first_free_page_no == INVALID_PAGE_ID){
            return create_new_page_handle();
        }
        return fetch_page_handle(file_hdr_->file_hdr.first_free_page_no);
    }

    void release_page_handle(PaxPageHandle &page_handle)
    {
        page_handle.page_hdr->next_free_page_no = file_hdr_->file_hdr.first_free_page_no;
        file_hdr_->file_hdr.first_free_page_no = page_handle.page->get_page_id().page_no;
    }
};


class PaxScan
{
private:
    /* data */
    const PaxFileHandle *handle_;
    // TabMeta tab_meta_;
    // std::vector<Condition> conditions_;
    page_id_t cur_page_no;
public:
    PaxScan(const PaxFileHandle *handle)
    {
        handle_ = handle;
        if(handle_->file_hdr_->file_hdr.num_pages == 1)cur_page_no = -1;
        cur_page_no = 1;
    }
    void next_chunk(Chunk &chunk)
    {
        if(is_end())return;
        handle_->get_chunk(cur_page_no,chunk);
    }
    bool is_end()
    {
        return cur_page_no == -1;
    }
    ~PaxScan() = default;
};

