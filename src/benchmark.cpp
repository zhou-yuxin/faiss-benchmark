#include <mutex>
#include <atomic>
#include <thread>
#include <iostream>
#include <algorithm>

#include <pthread.h>

#include <AutoTune.h>
#include <index_io.h>

#include "util/vecs.h"
#include "util/string.h"
#include "util/vector.h"
#include "util/perfmon.h"
#include "util/statistics.h"

void Evaluate(size_t count, size_t top_n,
        const faiss::Index::idx_t* groundtruths,
        faiss::Index::idx_t* labels,
        util::statistics::Percentile<float>& percentile_rate) {
    size_t thread_count = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;
    std::atomic<size_t> cursor(0);
    std::mutex mutex;
    for (size_t i = 0; i < thread_count; i++) {
        threads.emplace_back([&] {
            while (true) {
                size_t index = cursor++;
                if (index >= count) {
                    break;
                }
                size_t offset = index * top_n;
                const faiss::Index::idx_t* gs = groundtruths + offset;
                faiss::Index::idx_t* ls = labels + offset;
                std::sort(ls, ls + top_n);
                size_t ig = 0, il = 0, correct = 0;
                while (ig < top_n && il < top_n) {
                    ssize_t diff = (ssize_t)gs[ig] - (ssize_t)ls[il];
                    if (diff < 0) {
                        ig++;
                    }
                    else if (diff > 0) {
                        il++;
                    }
                    else {
                        ig++;
                        il++;
                        correct++;
                    }
                }
                float rate = (float)correct / top_n;
                mutex.lock();
                percentile_rate.add(rate);
                mutex.unlock();
            }
        });
    }
    for (size_t i = 0; i < thread_count; i++) {
        threads[i].join();
    }
}

struct TestCase {
    std::string parameters;
    size_t loop;
    size_t batch_size;
    std::vector<int> threads;
};

template <typename T>
T* NewZeroOutArray(size_t n) {
    T* array = new T[n];
    memset(array, 0, n * sizeof(T));
    return array;
}

void SetCPU(int cpu) {
    if (cpu >= 0) {
        cpu_set_t cpus;
        CPU_ZERO(&cpus);
        CPU_SET(cpu, &cpus);
        int ret = pthread_setaffinity_np(pthread_self(),
                sizeof(cpus), &cpus);
        if (ret) {
            throw std::runtime_error("failed to "
                    "pthread_setaffinity_np()");
        }
    }
}

void Benchmark(const faiss::Index* index, size_t count, size_t top_n,
        const float* queries, const faiss::Index::idx_t* groundtruths,
        const TestCase& test_case,
        float& qps, float& cpu_util, float& mem_r_bw, float& mem_w_bw,
        util::statistics::Percentile<uint32_t>& percentile_latency,
        util::statistics::Percentile<float>& percentile_rate) {
    size_t loop = test_case.loop;
    if (loop == 0) {
        throw std::runtime_error ("<loop = 0> is invalid!");
    }
    size_t vcount = loop * count;
    size_t batch_size = test_case.batch_size;
    if (batch_size == 0) {
        throw std::runtime_error("<batch_size = 0> is invalid!");
    }
    size_t thread_count = test_case.threads.size();
    if (thread_count == 0) {
        throw std::runtime_error("<thread_count = 0> is invalid!");
    }
    size_t dim = index->d;
    std::unique_ptr<uint32_t> latencies(NewZeroOutArray<uint32_t>(vcount));
    std::unique_ptr<faiss::Index::idx_t> labels(
            NewZeroOutArray<faiss::Index::idx_t>(count * top_n));
    std::atomic<size_t> cursor(0);
    std::vector<std::thread> threads;
    util::perfmon::CPUUtilization cpu_mon(true, true);
    util::perfmon::MemoryBandwidth mem_mon;
    cpu_mon.start();
    mem_mon.start();
    uint64_t all_start_us = util::perfmon::Clock::microsecond();
    for (size_t t = 0; t < thread_count; t++) {
        int cpu = test_case.threads[t];
        SetCPU(cpu);
        threads.emplace_back([&](int cpu) {
            SetCPU(cpu);
            std::unique_ptr<float> distances(
                    NewZeroOutArray<float>(batch_size * top_n));
            while (true) {
                size_t voffset = cursor.fetch_add(batch_size);
                if (voffset >= vcount) {
                    break;
                }
                size_t offset = voffset % count;
                size_t nquery1 = std::min (batch_size, count - offset);
                size_t nquery2 = batch_size - nquery1;
                const float* queries1 = queries + offset * dim;
                const float* queries2 = queries;
                faiss::Index::idx_t* labels1 = labels.get() + offset * top_n;
                faiss::Index::idx_t* labels2 = labels.get();
                float* ds = distances.get();
                uint64_t start_us = util::perfmon::Clock::microsecond();
                index->search(nquery1, queries1, top_n, ds, labels1);
                if (nquery2) {
                    index->search(nquery2, queries2, top_n, ds, labels2);
                }
                uint64_t end_us = util::perfmon::Clock::microsecond();
                uint64_t latency = end_us - start_us;
                uint32_t* lats = latencies.get();
                size_t lat_end = std::min(vcount, voffset + batch_size);
                for (size_t i = voffset; i < lat_end; i++) {
                    lats[i] = (uint32_t)latency;
                }
            }
        }, cpu);
    }
    for (size_t t = 0; t < thread_count; t++) {
        threads[t].join();
    }
    uint64_t all_end_us = util::perfmon::Clock::microsecond();
    cpu_util = cpu_mon.end();
    mem_mon.end(mem_r_bw, mem_w_bw);
    threads.clear();
    qps = 1000000.0f * vcount / (all_end_us - all_start_us);
    percentile_latency.add(latencies.get(), vcount);
    latencies.reset();
    Evaluate(count, top_n, groundtruths, labels.get(), percentile_rate);
#ifdef PRINT_LABELS
    faiss::Index::idx_t* plabel = labels.get (); 
    for (size_t i = 0; i < count; i++) {
        for (size_t j = 0; j < top_n; j++) {
            std::cerr << *plabel << ",";
            plabel++;
        }
        std::cerr << std::endl;
    }
#endif
}

template <typename T>
std::shared_ptr<float> PrepareQueries(util::vecs::File* file, size_t dim,
        size_t& count) {
    util::vecs::Formater<T> reader(file);
    count = 0;
    while (reader.skip()) {
        count++;
    }
    reader.reset();
    float* cursor = new float[count * dim];
    std::shared_ptr<float> queries(cursor);
    util::vector::Converter<T, float> converter;
    for (size_t i = 0; i < count; i++) {
        std::vector<T> vector = reader.read();
        if (vector.size() != dim) {
            char buf[256];
            sprintf(buf, "query vector is not %luD!", dim);
            throw std::runtime_error(buf);
        }
        converter(cursor, vector);
        cursor += dim;
    }
    return queries;
}

std::shared_ptr<float> PrepareQueries(const char* fpath, size_t dim,
        size_t& count) {
    util::vecs::SuffixWrapper query(fpath, true);
    typedef std::shared_ptr<float> (*func_t)(util::vecs::File*,
            size_t, size_t&);
    static const struct Entry {
        char type;
        func_t func;
    }
    entries[] = {
        {'b', PrepareQueries<uint8_t>},
        {'i', PrepareQueries<int32_t>},
        {'f', PrepareQueries<float>},
    };
    for (size_t i = 0; i < sizeof(entries) / sizeof(Entry); i++) {
        const Entry* entry = entries + i;
        if (query.getDataType() == entry->type) {
            return entry->func(query.getFile(), dim, count);
        }
    }
    throw std::runtime_error("unsupported format of query vectors!");
}

template <typename T>
std::shared_ptr<faiss::Index::idx_t> PrepareGroundTruths(size_t count,
        size_t top_n, util::vecs::File* gt_file) {
    faiss::Index::idx_t* cursor = new faiss::Index::idx_t[count * top_n];
    std::shared_ptr<faiss::Index::idx_t> gts(cursor);
    util::vecs::Formater<T> reader(gt_file);
    util::vector::Converter<T, faiss::Index::idx_t> converter;
    for (size_t i = 0; i < count; i++) {
        std::vector<T> gt = reader.read();
        if (gt.size() < top_n) {
            char buf[256];
            sprintf(buf, "groundtruth vector is less than %luD!", top_n);
            throw std::runtime_error(buf);
        }
        gt.resize(top_n);
        std::sort(gt.begin(), gt.end());
        converter(cursor, gt);
        cursor += top_n;
    }
    return gts;
}

std::shared_ptr<faiss::Index::idx_t> PrepareGroundTruths(size_t count,
        size_t top_n, const char* fpath) {
    util::vecs::SuffixWrapper gt(fpath, true);    
    typedef std::shared_ptr<faiss::Index::idx_t> (*func_t)(size_t, size_t,
            util::vecs::File*);
    static const struct Entry {
        char type;
        func_t func;
    }
    entries[] = {
        {'i', PrepareGroundTruths<int32_t>},
    };
    for (size_t i = 0; i < sizeof(entries) / sizeof(Entry); i++) {
        const Entry* entry = entries + i;
        if (gt.getDataType() == entry->type) {
            return entry->func(count, top_n, gt.getFile());
        }
    }
    throw std::runtime_error("unsupported format of groundtruth vectors!");
}

template <typename T>
void OutputValue(const char* name, T value) {
    std::cout << name << ": " << value << std::endl;
}

struct Percentage {
    std::string str;
    double value;
};

template <typename T>
void OutputStatistics(const char* name,
        const std::vector<Percentage>& percentages,
        util::statistics::Percentile<T>& percentile) {
    std::cout << name << ": best=" << percentile.best() << " worst=" <<
            percentile.worst() << " average=" << percentile.average();
    for (auto it = percentages.begin(); it != percentages.end(); it++) {
        std::cout << " P(" << it->str << "%)=" << percentile(it->value);
    }
    std::cout << std::endl;
}

std::vector<Percentage> ParsePercentages(const char* joint_percentages) {
    std::vector<Percentage> percentages;
    auto func = [&](const char* item, size_t len) -> int {
        double value;
        if (sscanf(item, "%lf", &value) != 1) {
            throw std::runtime_error(std::string("unrecognizable "
                    "percentage: '").append(item).append("'!"));
        }
        Percentage p;
        p.str.assign(item, len);
        p.value = value;
        percentages.emplace_back(p);
        return 0;
    };
    util::string::split(joint_percentages, ",", &func);
    return percentages;
}

std::vector<TestCase> ParseTestCases(const char* joint_cases) {
    std::vector<TestCase> test_cases;
    auto case_func = [&](const char* case_item, size_t case_len) -> int {
        std::string case_str(case_item, case_len);
        case_item = case_str.data();
        size_t loop, batch_size, thread_count;
        const char* pos1 = strstr(case_item, "/");
        if (!pos1 || sscanf(pos1, "/%lux%lux%lu",
                &loop, &batch_size, &thread_count) != 3) {
            throw std::runtime_error(std::string("unrecognizable case: '")
                    .append(case_item, case_len).append("'!"));
        }
        TestCase t;
        t.parameters.assign(case_item, pos1 - case_item);
        t.loop = loop;
        t.batch_size = batch_size;
        const char* pos2 = strstr(pos1, ":");
        if (!pos2) {
            for (size_t i = 0; i < thread_count; i++) {
                t.threads.emplace_back(-1);
            }
        }
        else {
            auto cpu_func = [&](const char* cpu_item, size_t cpu_len) -> int {
                int cpu;
                if (sscanf(cpu_item, "%d", &cpu) != 1) {
                    throw std::runtime_error(std::string("unrecognizable "
                            "cpu: '").append(cpu_item, cpu_len).append("'!"));
                }
                t.threads.emplace_back(cpu);
                return 0;
            };
            util::string::split(pos2 + 1, ",", &cpu_func);
            if (t.threads.size() != thread_count) {
                throw std::runtime_error(std::string("length of cpu list"
                        " is not equal to thread count!"));
            }
        }
        test_cases.emplace_back(t);
        return 0;
    };
    util::string::split(joint_cases, ";", &case_func);
    return test_cases;
}

void Benchmark(const char* index_fpath, const char* query_fpath,
        const char* gt_fpath, size_t top_n, const char* joint_percentages,
        const char* joint_cases) {
    std::unique_ptr<faiss::Index> index(faiss::read_index(index_fpath));
    size_t dim = index->d;
    size_t count;
    std::shared_ptr<float> queries = PrepareQueries(query_fpath, dim, count);
    std::shared_ptr<faiss::Index::idx_t> gts = PrepareGroundTruths(count,
            top_n, gt_fpath);
    std::vector<Percentage> percentages = ParsePercentages(joint_percentages);
    std::vector<TestCase> test_cases = ParseTestCases(joint_cases);
    faiss::ParameterSpace ps;
    for (auto iter = test_cases.begin(); iter != test_cases.end(); iter++) {
        float qps, cpu_util, mem_r_bw, mem_w_bw;
        util::statistics::Percentile<uint32_t> latencies(true);
        util::statistics::Percentile<float> rates(false);
        ps.set_index_parameters(index.get(), iter->parameters.data());
        Benchmark(index.get(), count, top_n, queries.get(), gts.get(),
                *iter, qps, cpu_util, mem_r_bw, mem_w_bw, latencies, rates);
        OutputValue("qps", qps);
        OutputValue("cpu-util", cpu_util);
        OutputValue("mem-r-bw", mem_r_bw);
        OutputValue("mem-w-bw", mem_w_bw);
        OutputStatistics("latency", percentages, latencies);
        OutputStatistics("recall", percentages, rates);
    }
}

int main(int argc, char** argv) {
    size_t top_n;
    if (argc != 7 || sscanf(argv[4], "%lu", &top_n) != 1) {
        fprintf(stderr, "%s <index> <query> <gt> <top_n> <percentages> "
                "<cases>\n"
                "Load index from <index> if it exists. Then run several "
                "cases of benchmarks. The vectors to query are from <query>,"
                " the groundtruth vectors are from <gt>. Find <top_n> nearest"
                " neighbors for each query vector. The result is consist of "
                "statistics of latency and recall rate. Besides the best, "
                "worst and average, percentiles at <percentages> will be "
                "displayed additionally. For example, if <percentages> = '50,"
                "99,99.9', then 50-percentile, 99-percentile and "
                "99.9-percentile of latency and recall rates will be "
                "displayed. <cases> is a semicolon-split string of serval "
                "benchmark cases, each is in format of "
                "[parameters]/<loop>x<batch_size>x<thread_count>[:<cpu-list>] "
                "(e.g. 'nprobe=32/10x1x4' or 'nprobe=64/10x4x4:0,1,2,3')\n",
                argv[0]);
        return 1;
    }
    const char* index_fpath = argv[1];
    const char* query_fpath = argv[2];
    const char* gt_fpath = argv[3];
    const char* percentages = argv[5];
    const char* cases = argv[6];
    try {
        Benchmark(index_fpath, query_fpath, gt_fpath, top_n, percentages,
                cases);
    }
    catch (const std::exception& e) {
        fprintf(stderr, "ERROR: %s\n", e.what());
        return 1;
    }
    return 0;
}
