#pragma once

#include<vector>
#include <mutex>

#include "common/config.h"
#include "replacer/replacer.h"
#include "unordered_map"

class ClockReplacer : public Replacer {
    private:
        struct FrameStatus {
            bool in_replacer;
            bool ref;
        };
        std::vector<FrameStatus> frames_;
        size_t clock_hand_;
        size_t size;
        size_t size_;   // 当前在 replacer 里的帧数（避免空时全扫）
        std::mutex mutex_;

    public:
        explicit ClockReplacer(size_t num) {
            frames_.resize(num, {false, false});
            clock_hand_ = 0;
            size = num;
            size_ = 0;
        }

        ~ClockReplacer() override = default;

        bool victim(frame_id_t *frame_id) override {
            if (size_ == 0) return false;  // 没有可驱逐帧，直接返回
            size_t scanned = 0;
            while (scanned < size * 2) {
                if (frames_[clock_hand_].in_replacer) {
                    if (frames_[clock_hand_].ref) {
                        frames_[clock_hand_].ref = false;
                    } else {
                        *frame_id = clock_hand_;
                        frames_[clock_hand_].in_replacer = false;
                        --size_;
                        clock_hand_ = (clock_hand_ + 1) % size;
                        return true;
                    }
                }
                clock_hand_ = (clock_hand_ + 1) % size;
                scanned++;
            }
            return false;
        }

        void pin(frame_id_t frame_id) override {
            if ((size_t)frame_id >= size) return;
            if (frames_[frame_id].in_replacer) {
                frames_[frame_id].in_replacer = false;
                frames_[frame_id].ref = false;
                --size_;
            }
        }

        void unpin(frame_id_t frame_id) override {
            if ((size_t)frame_id >= size) return;
            if (!frames_[frame_id].in_replacer) {
                frames_[frame_id].in_replacer = true;
                ++size_;
            }
            frames_[frame_id].ref = true;
        }

        size_t Size() override { return size_; }
};
