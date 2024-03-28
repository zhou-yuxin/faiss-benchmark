FAISS_DIR=/root/faiss
PCM_DIR=/home/intel/yuxin/pcm

CXX=g++ -std=gnu++11 -O3 -Wall

all: randset subset index groundtruth benchmark

clean:
	rm randset subset index groundtruth benchmark

RANDSET_DEPS+=src/util/vecs.h
RANDSET_DEPS+=src/util/random.h

randset: src/randset.cpp $(RANDSET_DEPS)		
	$(CXX) -o randset src/randset.cpp					\
	-lz

SUBSET_DEPS+=src/util/vecs.h
SUBSET_DEPS+=src/util/random.h
SUBSET_DEPS+=src/util/vector.h

subset: src/subset.cpp $(SUBSET_DEPS)		
	$(CXX) -o subset src/subset.cpp						\
	-lz

INDEX_DEPS+=src/util/vecs.h
INDEX_DEPS+=src/util/random.h
INDEX_DEPS+=src/util/vector.h
INDEX_DEPS+=src/util/perfmon.h

index: src/index.cpp $(INDEX_DEPS)
	$(CXX) -o index src/index.cpp 						\
	-I$(FAISS_DIR) -L$(FAISS_DIR)/build/faiss		    \
	-I$(PCM_DIR)								\
	-lz -lfaiss

GROUNDTRUTH_DEPS+=src/util/vecs.h
GROUNDTRUTH_DEPS+=src/util/vector.h

groundtruth: src/groundtruth.cpp $(GROUNDTRUTH_DEPS)
	$(CXX) -o groundtruth src/groundtruth.cpp 				\
	-lz -lpthread

BENCHMARK_DEPS+=src/util/vecs.h
BENCHMARK_DEPS+=src/util/string.h
BENCHMARK_DEPS+=src/util/vector.h
BENCHMARK_DEPS+=src/util/perfmon.h
BENCHMARK_DEPS+=src/util/statistics.h

benchmark: src/benchmark.cpp $(BENCHMARK_DEPS)
	$(CXX) -o benchmark src/benchmark.cpp					\
	-I$(FAISS_DIR) -L$(FAISS_DIR)/build/faiss 						\
	-lz -lpthread -lfaiss
