lib_dir = "/home/yuxin/github/faiss"
cmd_prefix = "LD_LIBRARY_PATH=%s OMP_NUM_THREADS=1 "        \
        "numactl --cpunodebind=0 --localalloc" %            \
        lib_dir

data_dir = "/home/yuxin/sift/1M"
base_fname = "base.fvecs"
query_fname = "query.fvecs"
groundtruth_fname = "groundtruth.ivecs"
train_rato = 0.1

out_dir = "/home/yuxin/output"
db_fname = "hnsw.db"
db_table = "benchmark_E5_2699"
percentiles = (99, 99.9)

tops = (100, )
batch_sizes = (1, )
thread_counts = range(1, 22 + 1, 1)
cpus = range(22)
