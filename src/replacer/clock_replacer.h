#pragma once

#include<vector>
#include <mutex>

#include "common/config.h"
#include "replacer/replacer.h"
#include "unordered_map"

class ClockReplacer : public Replacer {
    private:
        struct FrameStatus {  // 修正结构体名
            bool in_replacer;
            bool ref;
        };
        std::vector<FrameStatus> frames_;  // 修正变量名
        size_t clock_hand_;
        size_t size;
        std::mutex mutex_;
    
    public:
        explicit ClockReplacer(size_t num) {
            frames_.resize(num, {false, false});
            clock_hand_ = 0;
            size = num;
        }
    
        ~ClockReplacer() override = default;
    
        bool victim(frame_id_t *frame_id) override {
          //  std::lock_guard<std::mutex> lock(mutex_);
            size_t scanner = 0;
            while (scanner < size * 2) {
                if (frames_[clock_hand_].in_replacer) {
                    if (frames_[clock_hand_].ref) {
                        frames_[clock_hand_].ref = false;
                    } else {
                        *frame_id = clock_hand_;
                        frames_[clock_hand_].in_replacer = false;
                        clock_hand_ = (clock_hand_ + 1) % size;
                        return true;
                    }
                }
                clock_hand_ = (clock_hand_ + 1) % size;
                scanner++;
            }
            return false;
        }
    
        void pin(frame_id_t frame_id) override {
         //   std::lock_guard<std::mutex> lock(mutex_);
            if ((size_t)frame_id >= size) return;  // 添加边界检查
            frames_[frame_id].in_replacer = false;
            frames_[frame_id].ref = false;
        }
    
        void unpin(frame_id_t frame_id) override {
         //   std::lock_guard<std::mutex> lock(mutex_);
            if ((size_t)frame_id >= size) return;  // 添加边界检查
            // 无论是否在替换器中，更新引用位并加入
            frames_[frame_id].in_replacer = true;
            frames_[frame_id].ref = true;
        }
    
        size_t Size() override {
        //    std::lock_guard<std::mutex> lock(mutex_);
            size_t count = 0;
            for (auto &t : frames_) {
                if (t.in_replacer) count++;
            }
            return count;
        }

        // void empty_struc() override
        // {
        //     for (auto &t : frames_) {
        //         t.in_replacer = false;
        //         t.ref = false;
        //     }
        //     clock_hand_ = 0;
        // }
};
