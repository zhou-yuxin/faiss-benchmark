#include <list>
#include <mutex>
#include <queue>
#include <thread>

#include "util/vecs.h"
#include "util/vector.h"

template <typename TBase, typename TQuery, typename TDistance,
        typename TIndex>
std::vector<TIndex> Generate(
        const std::list<std::vector<TBase>>& base_vectors,
        const std::vector<TQuery>& query_vector,
        util::vector::DistanceAlgo<TBase, TQuery, TDistance>& dis_algo,
        size_t top_n) {
    size_t count = base_vectors.size();
    if (top_n > count) {
        char buf[256];
        sprintf(buf, "argument <top_n = %lu> is larger than vector count!",
                count);
        throw std::runtime_error(buf);
    }
    struct Entry {
        TIndex index;
        TDistance distance;

        bool operator <(const Entry& another) const {
            return distance < another.distance;
        }
    };
    std::priority_queue<Entry> tops;
    auto iter = base_vectors.begin();
    for (size_t i = 0; i < count; i++, iter++) {
        assert(iter != base_vectors.end());
        Entry entry = {
            .index = static_cast<TIndex>(i),
            .distance = dis_algo(*iter, query_vector),
        };
        tops.push(entry);
        if (tops.size() > top_n) {
            tops.pop();
        }
    }
    assert(tops.size() == top_n);
    std::vector<TIndex> gt;
    gt.resize(top_n);
    size_t rindex = top_n - 1;
    for (size_t i = 0; i < top_n; i++) {
        gt[rindex--] = tops.top().index;
        tops.pop();
    }
    return gt;
}

template <typename TBase, typename TQuery, typename TDistance,
        typename TIndex>
std::vector<std::vector<TIndex>> Generate(
        const std::list<std::vector<TBase>>& base_vectors,
        const std::list<std::vector<TQuery>>& query_vectors,
        util::vector::DistanceAlgo<TBase, TQuery, TDistance>& dis_algo,
        size_t top_n, size_t thread_count) {
    if (thread_count == 0) {
        throw std::runtime_error("<thread_count = 0> is invalid!");
    }
    size_t count = query_vectors.size();
    std::vector<std::vector<TIndex>> gts;
    gts.resize(count);
    auto iter = query_vectors.begin();
    size_t cursor = 0;
    std::mutex mutex;
    std::vector<std::thread> threads;
    for (size_t i = 0; i < thread_count; i++) {
        threads.emplace_back([&] {
            while (true) {
                mutex.lock();
                size_t index = cursor;
                if (index >= count) {
                    mutex.unlock();
                    break;
                }
                assert(iter != query_vectors.end());
                const std::vector<TQuery>& vector = *iter;
                iter++;
                cursor++;
                mutex.unlock();
                gts[index] = Generate<TBase, TQuery, TDistance, TIndex>
                        (base_vectors, vector, dis_algo, top_n);
            }
        });
    }
    for (size_t i = 0; i < thread_count; i++) {
        threads[i].join();
    }
    assert(cursor == count);
    assert(iter == query_vectors.end());
    return gts;
}

template <typename TBase, typename TQuery, typename TDistance,
        typename TIndex>
void Generate(util::vecs::File* gt_file,
        util::vecs::File* base_file, util::vecs::File* query_file,
        util::vector::DistanceAlgo<TBase, TQuery, TDistance>& dis_algo,
        size_t top_n, size_t thread_count) {
    util::vecs::Formater<TBase> base_reader(base_file);
    std::list<std::vector<TBase>> base_vectors;
    while (true) {
        std::vector<TBase> vector = base_reader.read();
        if (vector.size() == 0) {
            break;
        }
        base_vectors.emplace_back(std::move(vector));
    }
    size_t batch_size = thread_count * 1000;
    util::vecs::Formater<TQuery> query_reader(query_file);
    util::vecs::Formater<TIndex> gt_writer(gt_file);
    while (true) {
        std::list<std::vector<TQuery>> query_vectors;
        for (size_t i = 0; i < batch_size; i++) {
            std::vector<TQuery> vector = query_reader.read();
            if (vector.size() == 0) {
                break;
            }
            query_vectors.emplace_back(std::move(vector));
        }
        if (query_vectors.size() == 0) {
            break;
        }
        std::vector<std::vector<TIndex>> gts = Generate
                <TBase, TQuery, TDistance, TIndex>
                (base_vectors, query_vectors, dis_algo, top_n, thread_count);
        for (auto iter = gts.begin(); iter != gts.end(); iter++) {
            gt_writer.write(*iter);
        }
    }
}

template <typename TBase, typename TQuery, typename TDistance,
        typename TIndex>
void Generate(util::vecs::File* gt_file,
        util::vecs::File* base_file, util::vecs::File* query_file,
        const char* metric_type, size_t top_n, size_t thread_count) {
    std::unique_ptr<util::vector::DistanceAlgo<TBase, TQuery, TDistance>> algo;
    if (strcmp(metric_type, "l1") == 0) {
        algo.reset(new util::vector::DistanceL1<TBase, TQuery, TDistance>);
    }
    else if (strcmp(metric_type, "l2") == 0) {
        algo.reset(new util::vector::DistanceL2Sqr<TBase, TQuery, TDistance>);
    }
    else if (strcmp(metric_type, "ip") == 0) {
        algo.reset(new util::vector::DistanceIP<TBase, TQuery, TDistance>);
    }
    else {
        throw std::runtime_error(std::string("unsupported metric type: '")
                .append(metric_type).append("'!"));
    }
    Generate<TBase, TQuery, TDistance, TIndex>(gt_file, base_file,
            query_file, *algo, top_n, thread_count);
}

void Generate(const char* gt_fpath, const char* base_fpath,
        const char* query_fpath, const char* metric_type,
        size_t top_n, size_t thread_count) {
    util::vecs::SuffixWrapper base(base_fpath, true);
    util::vecs::SuffixWrapper query(query_fpath, true);
    util::vecs::SuffixWrapper gt(gt_fpath, false);
    typedef void (*func_t)(
            util::vecs::File*, util::vecs::File*, util::vecs::File*,
            const char*, size_t, size_t);
    static const struct Entry {
        char base_type;
        char query_type;
        char gt_type;
        func_t func;
    }
    entries[] = {
        {'b', 'b', 'i', Generate<uint8_t, uint8_t, int64_t, int32_t>},
        {'b', 'i', 'i', Generate<uint8_t, int32_t, int64_t, int32_t>},
        {'b', 'f', 'i', Generate<uint8_t, float, float, int32_t>},
        {'i', 'b', 'i', Generate<int32_t, uint8_t, int64_t, int32_t>},
        {'i', 'i', 'i', Generate<int32_t, int32_t, int64_t, int32_t>},
        {'i', 'f', 'i', Generate<int32_t, float, float, int32_t>},
        {'f', 'b', 'i', Generate<float, uint8_t, float, int32_t>},
        {'f', 'i', 'i', Generate<float, int32_t, float, int32_t>},
        {'f', 'f', 'i', Generate<float, float, float, int32_t>},
    };
    for (size_t i = 0; i < sizeof(entries) / sizeof(Entry); i++) {
        const Entry* entry = entries + i;
        if (base.getDataType() == entry->base_type &&
                query.getDataType() == entry->query_type &&
                gt.getDataType() == entry->gt_type) {
            entry->func(gt.getFile(), base.getFile(), query.getFile(),
                    metric_type, top_n, thread_count);
            return;
        }
    }
    throw std::runtime_error("unsupported format!");
}

int main(int argc, char** argv) {
    size_t top_n;
    size_t thread_count;
    if (argc != 7 || sscanf(argv[5], "%lu", &top_n) != 1 ||
            sscanf(argv[6], "%lu", &thread_count) != 1) {
        fprintf(stderr, "%s <gt> <base> <query> <metric> <top_n> <thread>\n"
                "Calculate the groundtruth for vectors in <query>. "
                "For each vector in <query>, find the <top_n> nearest vectors"
                " from <base>. Output result to <gt>. Use <metric> to "
                "calculate the distances, now 'l1', 'l2' and 'ip' "
                "are supported. "
                "Accelerate the process with <thread> threads. "
                "The formats of <base> and <query> can be any combination "
                "of .[b/i/f]vecs.(gz). While the format of <gt> should be "
                ".ivecs or .ivecs.gz.\n",
                argv[0]);
        return 1;
    }
    const char* gt = argv[1];
    const char* base = argv[2];
    const char* query = argv[3];
    const char* metric = argv[4];
    try {
        Generate(gt, base, query, metric, top_n, thread_count);
    }
    catch (const std::exception& e) {
        fprintf(stderr, "ERROR: %s\n", e.what());
        return 1;
    }
    return 0;
}