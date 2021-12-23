
/*
 * This test aims to identify the optimal consumption threshold for appending integers to an in-memory log
 * and persisting the log to a PMEM backend before it overflows.
 * */

#define _GNU_SOURCE
#include <stdio.h>
#include <omp.h>
#include <sched.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <cstdint>
#include <cstdlib>

//#ifndef SYS_gettid
//#error "SYS_gettid unavailable on this system"
//#endif
//#define gettid() ((pid_t)syscall(SYS_gettid))

#include <pmem_bench/ring_buffer.h>
#include <pmem_bench/affinity.h>

template<typename ... Args>
void printflush(const char *fmt, Args ...args) {
    printf(fmt, args...);
    fflush(stdout);
}

bool produce(pmem_bench::ring_buffer &rbuf, bool *future) {
    uint32_t interval = rbuf.GetMaxDepth() / 100;
    for (uint32_t i = 0; i < rbuf.GetMaxDepth(); ++i) {
        if(!rbuf.Enqueue(i)) {
            *future = true;
            return false;
        }
        if((i % interval) == 0) {
            printflush("%d/%d\n", i, rbuf.GetMaxDepth());
        }
    }
    *future = true;
    return true;
}

void consume(pmem_bench::ring_buffer &rbuf, bool *future) {
    uint32_t interval = rbuf.GetMaxDepth() / 100;
    while(!__atomic_load_n(future, __ATOMIC_RELAXED)) {
        rbuf.Consume(false);
    }
    rbuf.Consume(true);
}

int main(int argc, char **argv) {
    if(argc != 10) {
        printflush("USAGE: ./test_spsc [pmem_path] [pmem_size_gb] [ram_size_gb] [cpu1] [cpu2] [max_iter] [thresh_min] [thresh_max] [type_size_bytes]\n");
        exit(1);
    }

    //Input variables
    char *pmem_path = argv[1];
    uint64_t pmem_size = atoi(argv[2])*(1<<30);
    uint64_t ram_size = atoi(argv[3])*(1<<30);
    int cpu[] = {atoi(argv[4]), atoi(argv[5])};
    int max_iter = atoi(argv[6]);
    float thresh_min = atof(argv[7]);
    float thresh_max= atof(argv[8]);
    float thresh_cur = (thresh_max - thresh_min) / 2;
    uint32_t type_size = atoi(argv[9]);
    bool future = false;

    printflush("Starting test\n");

    //Input checks
    if(pmem_size < ram_size) {
        printflush("PMEM size (%s) must be at least RAM size (%s)\n", argv[2], argv[3]);
        exit(1);
    }
    if(thresh_min > thresh_max) {
        printflush("Thresh min must be at least thresh_max: min=%lf, max=%lf\n", thresh_min, thresh_max);
        exit(1);
    }
    if(thresh_min < 0 || thresh_max > 1) {
        printflush("Thresh min and max must be between 0 and 1: min=%lf, max=%lf\n", thresh_min, thresh_max);
        exit(1);
    }

    //Open PMEM chardev
    int fd = open(pmem_path, O_RDWR);
    if(fd < 0) {
        perror(pmem_path);
        exit(1);
    }
    void *ram_region = malloc(ram_size);
    if(ram_region == NULL) {
        printflush("Failed to allocate memory\n");
        exit(1);
    }
    void *pmem_region = mmap(NULL, pmem_size, PROT_READ | PROT_WRITE, MAP_SHARED_VALIDATE, fd, 0);
    if(pmem_region == MAP_FAILED) {
        perror("Mmap PMEM");
        printf("PMEM SIZE: %lu, fd: %d\n", pmem_size, fd);
        exit(1);
    }

    //Initialize ring buffer
    pmem_bench::ring_buffer rbuf;
    rbuf.Init(type_size, ram_region, ram_size, pmem_region, pmem_size);

    //Isolate CPUs as much as possible
    pmem_bench::ProcessAffiner isol;
    isol.SetCpu(isol.GetNumCPU() - 1);
    isol.SetCpu(isol.GetNumCPU() - 2);
    isol.SetCpu(isol.GetNumCPU() - 3);
    isol.SetCpu(isol.GetNumCPU() - 4);
    printflush("%lu processes affined\n", isol.AffineAll());
    printflush("Moving unnecessary processes to be on cores %d:%d on CPU with %d cores\n", isol.GetNumCPU()-4, isol.GetNumCPU()-1, isol.GetNumCPU());

    omp_set_dynamic(0);
#pragma omp parallel shared(rbuf, thresh_cur, thresh_min, thresh_max, future) num_threads(2)
    {
        int rank;
        rank = omp_get_thread_num();
        pmem_bench::ProcessAffiner mask;
        mask.SetCpu(cpu[rank]);
        mask.Affine(gettid());
        mask.PrintAffinity(gettid());

        for(int i = 0; i < max_iter; ++i) {
            //Initialize test
#pragma omp barrier
            if (rank == 0) {
                thresh_cur = thresh_min + (thresh_max - thresh_min) / 2;
                rbuf.SetThresh(thresh_cur);
                rbuf.Reset();
                if(thresh_cur < 0 || thresh_cur > 1) {
                    printf("Thresh cur is not between 0 and 1: %lf\n", thresh_cur);
                    printf("Thresh min: %lf\n", thresh_min);
                    printf("Thresh max: %lf\n", thresh_max);
                    printf("Midpoint: %lf\n", (thresh_max - thresh_min) / 2);
                    exit(1);
                }
            }
#pragma omp barrier

            //The producer thread
            if (rank == 0) {
                //If production was faster than consumption, make digestion occur more frequently
                if(!produce(rbuf, &future)) {
                    printflush("Failed! Production is faster than consumption: consume after %lf full\n", thresh_cur);
                    thresh_max = thresh_cur;
                }

                //If production was slower than consumption, make digestion occur less frequently
                else {
                    printflush("Success! Consumption is faster than production: consume after %lf full\n", thresh_cur);
                    thresh_min = thresh_cur;
                }
            }
            //The consumer thread
            if (rank == 1) {
                consume(rbuf, &future);
            }
        }
#pragma omp barrier
    }


}