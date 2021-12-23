#ifndef RING_BUFFER_SPSC_H
#define RING_BUFFER_SPSC_H

#include <cstring>
#include <cstdint>
#include <libpmem.h>

namespace pmem_bench {

struct ring_buffer {
    uint64_t enqueued_, dequeued_;
    uint64_t ram_region_size_, pmem_region_size_;
    uint64_t max_ram_depth_, max_pmem_depth_;
    uint64_t min_consume_depth_;
    uint32_t type_size_;
    char *ram_queue_;
    char *pmem_queue_;

    inline bool Init(uint32_t type_size, void *ram_region, uint64_t ram_region_size, void *pmem_region, uint64_t pmem_region_size, float consume_thresh = 0) {
        ram_region_size_ = ram_region_size;
        pmem_region_size_ = pmem_region_size;
        ram_queue_ = reinterpret_cast<char*>(ram_region);
        pmem_queue_ = reinterpret_cast<char*>(pmem_region);
        Reset();
        SetTypeSize(type_size);
        SetThresh(consume_thresh);
        return true;
    }

    inline void SetTypeSize(uint32_t type_size) {
        type_size_ = type_size;
        max_ram_depth_ = ram_region_size_ / type_size_;
        max_pmem_depth_ = pmem_region_size_ / type_size_;
    }

    inline void SetThresh(float consume_thresh) {
        min_consume_depth_ = max_ram_depth_ * consume_thresh;
    }
    
    inline uint64_t GetDepth() {
        return __atomic_load_n(&enqueued_, __ATOMIC_RELAXED) -  __atomic_load_n(&dequeued_, __ATOMIC_RELAXED);
    }

    inline uint64_t GetMaxDepth() {
        return max_pmem_depth_;
    }

    inline void Reset() {
         enqueued_ = 0;
         dequeued_ = 0;
    }

    inline bool Enqueue(uint32_t nonce) {
        uint64_t entry;
        if (GetDepth() >= max_ram_depth_) {
            return false;
        }
        entry =  enqueued_ %  max_ram_depth_;
        memset(ram_queue_ + OFF(entry), nonce, type_size_);
        ++enqueued_;
        return true;
    }

    inline bool Consume(bool force) {
        uint64_t cur_dequeued = dequeued_, cur_enqueued = enqueued_, cur_depth = cur_enqueued - cur_dequeued;
        if(!force && cur_depth < min_consume_depth_) {
            return false;
        }
        if(cur_depth == 0) {
            return false;
        }

        uint64_t head =  cur_dequeued %  max_ram_depth_;
        uint64_t tail =  (cur_enqueued-1) %  max_ram_depth_;
        if(head <= tail) {
            //[head, tail]
            pmem_memcpy_persist(pmem_queue_ + OFF(dequeued_), ram_queue_ + OFF(head), (tail - head + 1)*type_size_);
            dequeued_ += tail - head + 1;
        }
        else if(tail < head) {
            //[head,max_depth)
            pmem_memcpy_persist(pmem_queue_ + OFF(dequeued_), ram_queue_ + OFF(head), (max_ram_depth_ - head)*type_size_);
            dequeued_ += (max_ram_depth_ - head);
            //[0,tail]
            pmem_memcpy_persist(pmem_queue_ + OFF(dequeued_), ram_queue_, (tail + 1)*type_size_);
            dequeued_ += tail + 1;
        }

        if(cur_dequeued + cur_depth != dequeued_) {
            printf("Logic Error in Dequeue: %lu != %lu + %lu\n", dequeued_, cur_dequeued, cur_depth);
            printf("Enqueued=%lu, Dequeued=%lu, MaxDepth=%lu\n", cur_enqueued, dequeued_, max_ram_depth_);
            printf("Head=%lu, Tail=%lu\n", head, tail);
            exit(1);
        }
        return true;
    }

private:
    inline uint64_t OFF(uint32_t i) {
        return i*type_size_;
    }
};

}
#endif