#include "util/vecs.h"
#include "util/random.h"

template <typename T, typename TDist>
void Generate(util::vecs::File* file, size_t dim, size_t count,
        float min, float max) {
    util::vecs::Formater<T> writer(file);
    TDist distribution(static_cast<T>(min), static_cast<T>(max));
    std::default_random_engine engine;
    engine.seed(time(nullptr));
    std::vector<T> vector;
    vector.resize(dim);
    for (size_t i = 0; i < count; i++) {
        for (size_t j = 0; j < dim; j++) {
            vector[j] = distribution(engine);
        }
        writer.write(vector);
    }
}

void Generate(const char* fpath, size_t dim, size_t count,
        float min, float max) {
    util::vecs::SuffixWrapper dst(fpath, false);
    typedef void (*func_t)(util::vecs::File*, size_t, size_t, float, float);
    static const struct Entry {
        char type;
        func_t func;
    }
    entries[] = {
        {'b', Generate<uint8_t, std::uniform_int_distribution<uint8_t>>},
        {'i', Generate<int32_t, std::uniform_int_distribution<int32_t>>},
        {'f', Generate<float, std::uniform_real_distribution<float>>},
    };
    for (size_t i = 0; i < sizeof(entries) / sizeof(Entry); i++) {
        const Entry* entry = entries + i;
        if (dst.getDataType() == entry->type) {
            entry->func(dst.getFile(), dim, count, min, max);
            return;
        }
    }
    throw std::runtime_error("unsupported format!");
}

int main(int argc, char** argv) {
    size_t dim, count;
    float min, max;
    if (argc != 6 || sscanf(argv[2], "%lu", &dim) != 1 ||
            sscanf(argv[3], "%lu", &count) != 1 ||
            sscanf(argv[4], "%f", &min) != 1 ||
            sscanf(argv[5], "%f", &max) != 1) {
        fprintf(stderr, "%s <dst> <dim> <n> <min> <max>\n"
                "Generate a random dataset with <n> vectors and save to "
                "<dst>. Each vector is <dim>-dimension, and each dimension "
                "is in type of uint8_t, int32_t or float, up to <dst>. "
                "For example, if <dst> is 'base.fvecs', then the type is "
                "float. The formats of <dst> can be any combination of "
                ".[b/i/f]vecs.(gz). The value of each dimension is in "
                "[min, max].\n",
                argv[0]);
        return 1;
    }
    const char* dst = argv[1];
    try {
        Generate(dst, dim, count, min, max);
    }
    catch (const std::exception& e) {
        fprintf(stderr, "ERROR: %s\n", e.what());
        return 1;
    }
    return 0;
}