#include "util/vecs.h"
#include "util/random.h"
#include "util/vector.h"

template <typename TSrc, typename TDst>
void Extract(util::vecs::File* src_file, util::vecs::File* dst_file,
        size_t count) {
    util::vecs::Formater<TSrc> reader(src_file);
    size_t icount = 0;
    while (reader.skip()) {
        icount++;
    }
    if (count > icount) {
        char buf[256];
        sprintf(buf, "argument <count = %lu> is larger than vector count!",
                count);
        throw std::runtime_error(buf);
    }
    reader.reset();
    size_t cursor = 0;
    util::random::Sequence<size_t> seq_rand(0, icount, count);
    util::vecs::Formater<TDst> writer(dst_file);
    util::vector::Converter<TSrc, TDst> converter;
    for (size_t i = 0; i < count; i++) {
        size_t index = seq_rand.next();
        while (cursor < index) {
            reader.skip();
            cursor++;
        }
        assert(cursor == index);
        std::vector<TSrc> vector = reader.read();
        cursor++;
        assert(vector.size() > 0);
        writer.write(converter(vector));
    }
    assert(cursor <= icount);
}

void Extract(const char* src_fpath, const char* dst_fpath, size_t count) {
    util::vecs::SuffixWrapper src(src_fpath, true);
    util::vecs::SuffixWrapper dst(dst_fpath, false);
    typedef void (*func_t)(util::vecs::File*, util::vecs::File*, size_t);
    static const struct Entry {
        char src_type;
        char dst_type;
        func_t func;
    }
    entries[] = {
        {'b', 'b', Extract<uint8_t, uint8_t>},
        {'b', 'i', Extract<uint8_t, int32_t>},
        {'b', 'f', Extract<uint8_t, float>},
        {'i', 'b', Extract<int32_t, uint8_t>},
        {'i', 'i', Extract<int32_t, int32_t>},
        {'i', 'f', Extract<int32_t, float>},
        {'f', 'b', Extract<float, uint8_t>},
        {'f', 'i', Extract<float, int32_t>},
        {'f', 'f', Extract<float, float>},
    };
    for (size_t i = 0; i < sizeof(entries) / sizeof(Entry); i++) {
        const Entry* entry = entries + i;
        if (src.getDataType() == entry->src_type &&
                dst.getDataType() == entry->dst_type) {
            entry->func(src.getFile(), dst.getFile(), count);
            return;
        }
    }
    throw std::runtime_error("unsupported format!");
}

int main(int argc, char** argv) {
    size_t n;
    if (argc != 4 || sscanf(argv[3], "%lu", &n) != 1) {
        fprintf(stderr, "%s <src> <dst> <n>\n"
                "Extract <n> vectors randomly from <src> to <dst>. "
                "The formats of <src> and <dst> can be any combination of"
                " .[b/i/f]vecs.(gz).\n",
                argv[0]);
        return 1;
    }
    const char* src = argv[1];
    const char* dst =argv[2];
    try {
        Extract(src, dst, n);
    }
    catch (const std::exception& e) {
        fprintf(stderr, "ERROR: %s\n", e.what());
        return 1;
    }
    return 0;
}