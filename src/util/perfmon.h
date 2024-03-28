#ifndef UTIL_PERFMON_H
#define UTIL_PERFMON_H

#include <mutex>
#include <thread>
#include <cassert>
#include <iostream>
#include <stdexcept>

#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>

#ifdef USE_PCM
#include <cpucounters.h>
#endif

#define UTIL_PERFMON_CPUUTILIZATION_PATH    "/proc/self/stat"
#define UTIL_PERFMON_MEMORYSIZE_PATH        "/proc/self/status"

namespace util {

namespace perfmon {

class Clock {

public:
    static uint64_t microsecond() {
        struct timeval tv;
        if (gettimeofday(&tv, nullptr) != 0) {
            throw std::runtime_error("gettimeofday() failed!");
        }
        return tv.tv_sec * 1000000 + tv.tv_usec;
    }

};

class CPUUtilization {

private:
    bool include_user;
    bool include_kernel;
    int fd;
    uint64_t user_time;
    uint64_t kernel_time;
    uint64_t real_time_us;

public:
    CPUUtilization(bool _include_user, bool _include_kernel) :
            include_user(_include_user), include_kernel(_include_kernel) {
        fd = open(UTIL_PERFMON_CPUUTILIZATION_PATH, O_RDONLY);
        if (fd < 0) {
            throw std::runtime_error("failed to open '"
                    UTIL_PERFMON_CPUUTILIZATION_PATH "'!");
        }
    }

    ~CPUUtilization() {
        close(fd);
    }

    void start() {
        glance(user_time, kernel_time);
        real_time_us = Clock::microsecond();
    }

    float end() const {
        uint64_t end_user_time, end_kernel_stime, end_real_time_us;
        glance(end_user_time, end_kernel_stime);
        end_real_time_us = Clock::microsecond();
        uint64_t run_time = 0;
        if (include_user) {
            run_time += end_user_time - user_time;
        }
        if (include_kernel) {
            run_time += end_kernel_stime - kernel_time;
        }
        long hz = sysconf(_SC_CLK_TCK);
        float run_time_us = 1000000.0f * run_time / hz;
        size_t total_time_us = end_real_time_us - real_time_us;
        return run_time_us / total_time_us;
    }

private:
    void glance(uint64_t& user_time, uint64_t& kernel_time) const {
        char buf[256];
        ssize_t len = pread(fd, buf, sizeof(buf), 0);
        if (len <= 0) {
            throw std::runtime_error("failed to read from '"
                    UTIL_PERFMON_CPUUTILIZATION_PATH "'!");
        }
        size_t index = 0;
        char* saved_ptr;
        for (const char *item = strtok_r(buf, " ", &saved_ptr);
                item; item = strtok_r(nullptr, " ", &saved_ptr)) {
            if (index == 13) {
                if (sscanf(item, "%lu", &user_time) != 1) {
                    throw std::runtime_error(std::string("failed to parse "
                            "user_time: '").append(item).append("'!"));
                }
            }
            else if (index == 14) {
                if (sscanf(item, "%lu", &kernel_time) != 1) {
                    throw std::runtime_error(std::string("failed to parse "
                            "kernel_time: '").append(item).append("'!"));
                }
                break;
            }
            index++;
        }
        assert(index == 14);
    }

};

class MemorySize {
private:
    int fd;

public:
    MemorySize() {
        fd = open(UTIL_PERFMON_MEMORYSIZE_PATH, O_RDONLY);
        if (fd < 0) {
            throw std::runtime_error("failed to open '"
                    UTIL_PERFMON_MEMORYSIZE_PATH "'!");
        }
    }

    ~MemorySize() {
        close(fd);
    }

    size_t getAddressSpaceSize() {
        return glance("VmSize:");
    }

    size_t getResidentSetSize() {
        return glance("VmRSS:");
    }

private:
    size_t glance(const char* name) const {
        char buf[1024];
        ssize_t len = pread(fd, buf, sizeof(buf), 0);
        if (len <= 0) {
            throw std::runtime_error("failed to read from '"
                    UTIL_PERFMON_MEMORYSIZE_PATH "'!");
        }
        size_t namelen = strlen(name);
        char* saved_ptr;
        for (const char* line = strtok_r(buf, "\n", &saved_ptr);
                line; line = strtok_r(nullptr, "\n", &saved_ptr)) {
            if (strncmp(line, name, namelen) == 0) {
                size_t value;
                if (sscanf(line + namelen, "%lu", &value) != 1) {
                    throw std::runtime_error(std::string("unrecognizable"
                            " line: '").append(line).append("'!"));
                }
                return value;
            }
        }
        throw std::runtime_error(std::string("no such name: '").append(name)
                .append("'!"));
    }
};

#ifdef USE_PCM
template <typename T>
class PCMInstanceFakeTemplate {

private:
    static std::once_flag init;
    static PCM* instance;

public:
    PCMInstanceFakeTemplate() {
        std::call_once(init, [&] {
            std::ofstream null("/dev/null");
            std::streambuf* nullbuf = null.rdbuf();
            std::streambuf* coutbuf = std::cout.rdbuf(nullbuf);
            std::streambuf* cerrbuf = std::cerr.rdbuf(nullbuf);
            assert(!instance);
            instance = PCM::getInstance();
#ifndef DISABLE_PCM
            PCM::ErrorCode status = instance->program();
#else
            PCM::ErrorCode status = PCM::Success;
#endif
            const char* errmsg = nullptr;
            if (status == PCM::MSRAccessDenied) {
                errmsg = "no MSR or PCI CFG space access!";
            }
            else if (status == PCM::PMUBusy) {
                errmsg = "PMU is occupied by other application!";
            }
            else if (status != PCM::Success) {
                errmsg = "Unknown error of PMU!";
            }
            std::cout.rdbuf(coutbuf);
            std::cerr.rdbuf(cerrbuf);
            null.close();
            if (errmsg) {
                throw std::runtime_error(errmsg);
            }
        });
    }

    PCM* operator ->() {
        return instance;
    }
};

template <typename T>
std::once_flag PCMInstanceFakeTemplate<T>::init;

template <typename T>
PCM* PCMInstanceFakeTemplate<T>::instance = nullptr;

using PCMInstance = PCMInstanceFakeTemplate<int>;

class MemoryBandwidth {

private:
    PCMInstance pcm;
    SystemCounterState states[2];
    SystemCounterState* prev_state;
    SystemCounterState* now_state;
    uint64_t prev_time;
    float total_time;
    float read_bandwidth;
    float write_bandwidth;
    std::thread* thread;
    volatile bool running;

public:
    MemoryBandwidth() : prev_state(states + 0), now_state(states + 1),
            thread(nullptr) {
    }

    void start() {
        assert(!thread);
        prev_time = Clock::microsecond();
        *prev_state = pcm->getSystemCounterState();
        total_time = 0.0f;
        read_bandwidth = 0.0f;
        write_bandwidth = 0.0f;
        running = true;
        thread = new std::thread([&] {
            while (running) {
                sleep(1);
                update();
            }
        });
    }

    void end(float& r_bw, float& w_bw) {
        assert(thread);
        running = false;
        thread->join();
        delete thread;
        thread = nullptr;
        update();
        r_bw = read_bandwidth;
        w_bw = write_bandwidth;
    }

private:
    void update() {
        uint64_t new_time = Clock::microsecond();
        if (new_time == prev_time) {
            return;
        }
        assert(new_time > prev_time);
        float time_delta = (float)(new_time - prev_time);
        *now_state = pcm->getSystemCounterState();
        uint64_t reads = getBytesReadFromMC(*prev_state, *now_state);
        uint64_t writes = getBytesWrittenToMC(*prev_state, *now_state);
        float sudden_r_bw = reads / time_delta;
        float sudden_w_bw = writes / time_delta;
        total_time += time_delta;
        float k = time_delta / total_time;
        float nk = 1.0 - k;
        read_bandwidth = read_bandwidth * nk + sudden_r_bw * k;
        write_bandwidth = write_bandwidth * nk + sudden_w_bw * k;
        prev_time = new_time;
        std::swap(prev_state, now_state);
    }

};
#else
class MemoryBandwidth {

public:
    void start() {}

    void end(float& r_bw, float& w_bw) {
        r_bw = NAN;
        w_bw = NAN;
    }

};
#endif

}

}

#endif