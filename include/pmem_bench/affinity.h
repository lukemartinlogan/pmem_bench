
#ifndef PMEM_BENCH_AFFINITY_H
#define PMEM_BENCH_AFFINITY_H

#include <vector>
#include <dirent.h>
#include <sys/sysinfo.h>
#include <sched.h>
#include <string>

namespace pmem_bench {

class ProcessAffiner {
private:
    int n_cpu_;
    cpu_set_t *cpus_;
public:
    ProcessAffiner() {
        n_cpu_ = get_nprocs_conf();
        cpus_ = new cpu_set_t[n_cpu_];
        CPU_ZERO(cpus_);
    }

    ~ProcessAffiner() {
        delete cpus_;
    }

    inline bool isdigit(char digit) {
        return ('0' <= digit && digit <= '9');
    }

    inline int GetNumCPU() {
        return n_cpu_;
    }

    inline void SetCpu(int cpu) {
        CPU_SET(cpu, cpus_);
    }

    inline void ClearCpu(int cpu) {
        CPU_CLR(cpu, cpus_);
    }

    inline void ClearAllCpu() {
        CPU_ZERO(cpus_);
    }

    int AffineAll(void) {
        DIR *procdir;
        FILE *fp;
        struct dirent *entry;
        int proc_pid, count = 0;

        // Open /proc directory.
        procdir = opendir("/proc");
        if (!procdir) {
            perror("opendir failed");
            return 0;
        }

        // Iterate through all files and folders of /proc.
        while ((entry = readdir(procdir))) {
            // Skip anything that is not a PID folder.
            if (!is_pid_folder(entry))
                continue;
            //Get the PID of the running process
            int proc_pid = atoi(entry->d_name);
            //Set the affinity of all running process to this mask
            count += Affine(proc_pid);
        }
        closedir(procdir);
        return count;
    }
    int Affine(std::vector<pid_t> &&pids) {
        return Affine(pids);
    }
    int Affine(std::vector<pid_t> &pids) {
        //Set the affinity of all running process to this mask
        int count = 0;
        for(pid_t &pid : pids) {
            count += Affine(pid);
        }
        return count;
    }
    int Affine(int pid) {
        return SetAffinitySafe(pid, n_cpu_, cpus_);
    }

    void PrintAffinity(int pid) {
        PrintAffinity("", pid);
    }
    void PrintAffinity(std::string prefix, int pid) {
        cpu_set_t cpus[n_cpu_];
        sched_getaffinity(pid, n_cpu_, cpus);
        PrintAffinity(prefix, pid, cpus);
    }

    void PrintAffinity(std::string prefix, int pid, cpu_set_t *cpus) {
        std::string affinity = "";
        for (int i = 0; i < n_cpu_; ++i) {
            if (CPU_ISSET(i, cpus)) {
                affinity += std::to_string(i) + ", ";
            }
        }
        printf("%s: CPU affinity[pid=%d]: %s\n", prefix.c_str(), pid, affinity.c_str());
    }

private:
    int SetAffinitySafe(int pid, int n_cpu, cpu_set_t *cpus) {
        int ret = sched_setaffinity(pid, n_cpu, cpus);
        if (ret == -1) {
            return 0;
        }
        return 1;
    }

    // Helper function to check if a struct dirent from /proc is a PID folder.
    int is_pid_folder(const struct dirent *entry) {
        const char *p;
        for (p = entry->d_name; *p; p++) {
            if (!isdigit(*p))
                return false;
        }
        return true;
    }
};

}

#endif // PMEM_BENCH_AFFINITY_H
