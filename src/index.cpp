#include <AutoTune.h>
#include <index_io.h>
#include <index_factory.h>

#include "util/vecs.h"
#include "util/random.h"
#include "util/vector.h"
#include "util/perfmon.h"

template <typename T>
std::shared_ptr<faiss::Index> Build(const char* key, faiss::MetricType metric,
        const char* parameters, util::vecs::File* base_file,
        float train_ratio) {
    util::vecs::Formater<T> reader(base_file);
    size_t dim = reader.read().size();
    if (dim == 0) {
        throw std::runtime_error("empty file of base vectors!");
    }
    size_t base_count = 1;
    while (reader.skip()) {
        base_count++;
    }
    reader.reset();
    size_t train_count = std::min<>(base_count,
            std::max<>(1UL, (size_t)(base_count * train_ratio)));
    float* train_vectors = new float[dim * train_count];
    std::unique_ptr<float> train_vectors_deleter(train_vectors);
    size_t cursor = 0;
    util::random::Sequence<size_t> seq_rand(0, base_count, train_count);
    util::vector::Converter<T, float> converter;
    for (size_t i = 0; i < train_count; i++) {
        size_t index = seq_rand.next();
        while (cursor < index) {
            reader.skip();
            cursor++;
        }
        assert(cursor == index);
        std::vector<T> vector = reader.read();
        cursor++;
        converter(train_vectors + dim * i, vector);
    }
    assert(cursor <= base_count);
    reader.reset();
    std::shared_ptr<faiss::Index> index(faiss::index_factory(dim, key,
            metric));
    faiss::ParameterSpace().set_index_parameters(index.get(), parameters);
    index->train(train_count, train_vectors);
    train_vectors_deleter.reset();
    for (size_t i = 0; i < base_count; i++) {
        std::vector<T> vector = reader.read();
        if (vector.size() != dim) {
            char buf[256];
            sprintf(buf, "index is %luD, but this vector is %luD!",
                    dim, vector.size());
            throw std::runtime_error(buf);
        }
        index->add(1, converter(vector).data());
    }
    return index;
}

void Build(const char* fpath, const char* key, faiss::MetricType metric,
        const char* parameters, const char* base_fpath, float train_ratio) {
    if (access(fpath, F_OK) == 0) {
        throw std::runtime_error(std::string("file '").append(fpath)
                .append("' already exists!"));
    }
    typedef std::shared_ptr<faiss::Index> (*func_t)(const char*,
            faiss::MetricType, const char*, util::vecs::File*, float);
    static const struct Entry {
        char type;
        func_t func;
    }
    entries[] = {
        {'b', Build<uint8_t>},
        {'i', Build<int>},
        {'f', Build<float>},
    };
    util::vecs::SuffixWrapper base(base_fpath, true);
    std::shared_ptr<faiss::Index> index;
    for (size_t i = 0; i < sizeof(entries) / sizeof(Entry); i++) {
        const Entry* entry = entries + i;
        if (base.getDataType() == entry->type) {
            index = entry->func(key, metric, parameters, base.getFile(),
                    train_ratio);
            faiss::write_index(index.get(), fpath);
            return;
        }
    }
    throw std::runtime_error("unsupported format!");
}

void Size(const char* fpath) {
    FILE* file = fopen(fpath, "r");
    if (!file) {
        throw std::runtime_error(std::string("file '").append(fpath)
                .append("' doesn't exist!"));
    }
    util::perfmon::MemorySize mem_mon;
    size_t start_size = mem_mon.getResidentSetSize();
    faiss::Index* index = faiss::read_index(file);
    size_t end_size = mem_mon.getResidentSetSize();
    delete index;
    size_t index_size = (end_size - start_size) >> 10;
    std::cout << index_size << std::endl;
}

faiss::MetricType parse_metric_type(const char* name) {
    if (strcasecmp(name, "ip") == 0) {
        return faiss::METRIC_INNER_PRODUCT;
    }
    else if (strcasecmp(name, "l2") == 0) {
        return faiss::METRIC_L2;
    }
    int value;
    if (sscanf(name, "raw:%d", &value) == 1) {
        return (faiss::MetricType)value;
    }
    throw std::runtime_error (std::string("unsupported metric: '")
            .append(name).append("'"));
}

int main(int argc, char** argv) {
    try {
        if (argc == 3 && strcmp(argv[1], "size") == 0) {
            const char* fpath = argv[2];
            Size(fpath);
            return 0;
        }
        float train_ratio;
        if (argc == 8 && strcmp(argv[1], "build") == 0 &&
                sscanf(argv[7], "%f", &train_ratio) == 1) {
            const char* fpath = argv[2];
            const char* key = argv[3];
            const char* metric = argv[4];
            const char* parameters = argv[5];
            const char* base_fpath = argv[6];
            Build(fpath, key, parse_metric_type(metric), parameters,
                    base_fpath, train_ratio);
            return 0;
        }
    }
    catch (const std::exception& e) {
        fprintf(stderr, "ERROR: %s\n", e.what());
        return 1;
    }
    fprintf(stderr, "%s size <fpath>\n"
            "Load index from <fpath>, and estimate the memory size it "
            "occupies, in MB.\n\n", argv[0]);
    fprintf(stderr, "%s build <fpath> <key> <metric> <parameters> <base> "
            "<train_ratio>\n"
            "If <fpath> doesn't exist, build a new index of <key> "
            "(e.g. 'IVF8192,PQ64') in <metric> (e.g. 'ip', 'l2') "
            "with <parameters> (e.g. 'verbose=1'). <metric> supports 'ip'"
            " and 'l2', or in format of 'raw:%%d', where %%d is the "
            " raw value. "
            "A subset of vectors are selected randomly from <base> to "
            "train the new index. The ratio to train is <train_ratio> "
            "(e.g. 0.1). Then vectors in <base> will be added to the new "
            "index, and finally save it to <fpath>.\n",
            argv[0]);
    return 1;
}