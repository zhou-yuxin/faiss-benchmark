lib_dir = "/home/yuxin/faiss"
cmd_prefix = "LD_LIBRARY_PATH=%s OMP_NUM_THREADS=1 "        \
        "numactl --cpunodebind=0 --localalloc" %            \
        lib_dir

data_dir = "/home/yuxin/sift"
base_fname = "base.fvecs"
query_fname = "query.fvecs"
groundtruth_fname = "groundtruth.ivecs"
train_rato = 0.1

out_dir = "/home/yuxin/output"
db_fname = "ivfpq.db"
db_table = "benchmark_8260L"
percentiles = (99, 99.9)

tops = (10, 100, )
batch_sizes = (1, )
thread_counts = (1, 2, )
cpus = range(4)