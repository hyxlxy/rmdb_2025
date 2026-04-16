#pragma once

#include "storage/buffer_pool_manager.h"

#define INDEX_POOL_SIZE BUFFER_POOL_SIZE/32-40
#define FILE_POOL_SIZE BUFFER_POOL_SIZE/32
#define MAX_POOL_SIZE BUFFER_POOL_SIZE/2
class BufferPool
{
private:
    /* data */
    DiskManager *disk_manager_;
    size_t tot_num;
    size_t use_num = 0;
    std::unordered_map<std::string,BufferPoolManager*> buffers;
    std::mutex latch_;
public:
    BufferPool(size_t pool_size,DiskManager *disk_manager)
    {
        disk_manager_ = disk_manager;
        tot_num = pool_size;
    }
    ~BufferPool()
    {
        for(auto it = buffers.begin();it!=buffers.end();)
        {
            delete it->second;
            it = buffers.erase(it);
        }
    }

    BufferPoolManager *get_pool(const std::string &name,size_t size)
    {
        if(use_num+size>tot_num)
        {
            size = tot_num-use_num;
        }
        if(size == 0)
        {
            throw InternalError("No Pool");
        }
        latch_.lock();
        auto it = buffers.find(name);
        if(it!=buffers.end())
        {
            if(it->second->get_size()>=size)return it->second;
            else
            {
                latch_.unlock();
                adjust_pool(name,size);
                return buffers.find(name)->second;
            }
        }
        BufferPoolManager *pool = new BufferPoolManager(size,disk_manager_);
        buffers[name] = pool;
        use_num+=size;
        latch_.unlock();
        return pool;
    }

    BufferPoolManager *fetch_pool(const std::string &name)
    {
        std::lock_guard<std::mutex> lock(latch_);
        auto it = buffers.find(name);
        if(it == buffers.end())
        {
            throw InternalError("No Pool");
        }
        return it->second;
    }

    void adjust_pool(const std::string &name,size_t size)
    {
        // std::lock_guard<std::mutex> lock(latch_);
        if(use_num+size>tot_num)
        {
            size = tot_num-use_num;
        }
        if(size == 0)
        {
            throw InternalError("No Pool");
        }
        latch_.lock();
        if(!buffers.count(name))
        {
            latch_.unlock();
            get_pool(name,size);
            return;
        }
        auto it = buffers.find(name);
        BufferPoolManager* old_manager = it->second;
        size_t old_size = old_manager->get_size();
        if (old_size == size) {
            return;  // 大小未改变
        }

        BufferPoolManager *pool = new BufferPoolManager(size,disk_manager_);
        int fd = disk_manager_->get_file_fd(name);
        old_manager->flush_all_pages(fd);
        delete old_manager;
        buffers[name] = pool;
        use_num = use_num-old_size+size;
        latch_.unlock();
    }

    void delete_pool(const std::string &name)
    {
        std::lock_guard<std::mutex> lock(latch_);
        auto it = buffers.find(name);
        if (it == buffers.end()) return;
        int fd = disk_manager_->get_file_fd(name);
        it->second->flush_all_pages(fd);
        use_num = use_num - it->second->get_size();
        delete it->second;
        buffers.erase(it);
    }

    BufferPool(const BufferPool&) = delete;
    BufferPool& operator=(const BufferPool&) = delete;
};

