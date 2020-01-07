#ifndef UTIL_VECS_H
#define UTIL_VECS_H

#include <memory>
#include <string>
#include <vector>
#include <cassert>
#include <stdexcept>

#include <zlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

namespace util {

namespace vecs {

class File {
public:
    virtual ~File() {}

    virtual void open(const char* fpath, bool rw) = 0;

    virtual void close() = 0;

    virtual ssize_t read(void* buf, size_t len) = 0;

    virtual ssize_t write(const void* buf, size_t len) = 0;

    virtual ssize_t seek(size_t position, int whence) = 0;

    virtual bool eof() = 0;
};

class PlainFile : public File {

private:
    FILE* file;

public:
    PlainFile() : file(nullptr) {}
    
    ~PlainFile() {
        assert(!file);
    }

    void open(const char* fpath, bool rw) override {
        assert(!file);
        file = fopen(fpath, rw ? "rb" : "wb");
        if (!file) {
            throw std::runtime_error(std::string("cannot open file '")
                    .append(fpath).append("'!"));
        }
    }

    void close() override {
        int ret = fclose(file);
        if (ret) {
            throw std::runtime_error("cannot close file!");
        }
        file = nullptr;
    }

    ssize_t read(void* buf, size_t len) override {
        return fread(buf, 1, len, file);
    }

    ssize_t write(const void* buf, size_t len) override {
        return fwrite(buf, 1, len, file);
    }

    ssize_t seek(size_t position, int whence) override {
        return fseek(file, position, whence);
    }

    bool eof() override {
        return feof(file);
    }

};

class GzFile : public File {

private:
    gzFile file;

public:
    GzFile() : file(nullptr) {}

    ~GzFile() {
        assert(!file);
    }

    void open(const char* fpath, bool rw) override {
        assert(!file);
        file = gzopen(fpath, rw ? "rb" : "wb");
        if (!file) {
            throw std::runtime_error(std::string("cannot open file '")
                    .append(fpath).append("'!"));
        }
    }

    void close() override {
        int ret = gzclose(file);
        if (ret) {
            throw std::runtime_error("cannot close file!");
        }
        file = nullptr;
    }

    ssize_t read(void* buf, size_t len) override {
        return gzread(file, buf, len);
    }

    ssize_t write(const void* buf, size_t len) override {
        return gzwrite(file, buf, len);
    }

    ssize_t seek(size_t position, int whence) override {
        return gzseek(file, position, whence);
    }

    bool eof() override {
        return gzeof(file);
    }

};

template <typename T>
class Formater {

private:
    File* file;

public:
    Formater(File* _file) : file(_file) {}

    std::vector<T> read() {
        uint32_t dim;
        std::vector<T> vector;
        ssize_t ret = file->read(&dim, sizeof(dim));
        if (ret == 0) {
            assert(file->eof());
            return vector;
        }
        if (ret != sizeof(dim)) {
            throw std::runtime_error("broken file!");
        }
        vector.resize(dim);
        ret = file->read(vector.data(), sizeof(T) * dim);
        if (ret != (ssize_t)sizeof(T) * dim) {
            throw std::runtime_error("broken file!");
        }
        return vector;
    }

    bool skip() {
        uint32_t dim;
        ssize_t ret = file->read(&dim, sizeof(dim));
        if (ret == 0) {
            assert(file->eof());
            return false;
        }
        if (ret != sizeof(dim)) {
            throw std::runtime_error("broken file!");
        }
        if (file->seek(sizeof(T) * dim, SEEK_CUR) < 0) {
            throw std::runtime_error("broken file!");
        }
        return true;
    }

    void reset() {
        file->seek(0, SEEK_SET);
    }

    void write(const std::vector<T>& vector) {
        uint32_t dim = vector.size();
        ssize_t ret = file->write(&dim, sizeof(dim));
        if (ret != sizeof(dim)) {
            throw std::runtime_error("Output error!");
        }
        ret = file->write(vector.data(), sizeof(T) * dim);
        if (ret != (ssize_t)sizeof(T) * dim) {
            throw std::runtime_error("Output error!");
        }
    }

};

class SuffixWrapper {
private:
    File* file;
    char type;

public:
    SuffixWrapper(const char* fpath, bool rw) {
        std::string suffix(fpath);
        bool is_gz = EndsWith(suffix, ".gz");
        if (is_gz) {
            suffix.resize(suffix.length() - 3);
        }
        if (EndsWith(suffix, ".cvecs")) {
            type = 'c';
        }
        else if (EndsWith(suffix, ".bvecs")) {
            type = 'b';
        }
        else if (EndsWith(suffix, ".ivecs")) {
            type = 'i';
        }
        else if (EndsWith(suffix, ".fvecs")) {
            type = 'f';
        }
        else {
            throw std::runtime_error(std::string("unsupported format '")
                    .append(fpath).append("'!"));
        }
        file = is_gz ? (File*)(new GzFile) : (File*)(new PlainFile);
        std::unique_ptr<File> file_deleter(file);
        file->open(fpath, rw);
        file_deleter.release();
    }

    ~SuffixWrapper() {
        file->close();
        delete file;
    }

    File* getFile() const {
        return file;
    }

    char getDataType() const {
        return type;
    }

private:
    static bool EndsWith(const std::string& str, const std::string& suffix) {
        size_t str_len = str.length();
        size_t suffix_len = suffix.length();
        if (str_len < suffix_len) {
            return false;
        }
        return memcmp((char*)(str.data()) + str_len - suffix_len,
                suffix.data(), suffix_len) == 0;
    }

};

}

}

#endif
