lib_dir = "/home/yuxin/faiss"
cmd_prefix = "LD_LIBRARY_PATH=%s OMP_NUM_THREADS=1 "        \
        "numactl --cpunodebind=0 --localalloc" %            \
        lib_dir

out_dir = "/home/yuxin/output"
db_fname = "ivfpq.db"
db_table = "benchmark_8160L"
percentiles = (99, 99.9)

data_dir = "/home/yuxin/sift/3M"
base_fname = "base.fvecs"
query_fname = "query.fvecs"
groundtruth_fname = "groundtruth.ivecs"
train_rato = 0.1

centroids = range(1024, 8192 + 1, 1024)
codes = (64, )
tops = (10000, 100000)
nprobes = range(32, 512 + 1, 32)
batch_sizes = (1, )
thread_counts = range(4, 96 + 1, 4)
cpus = range(0, 24) + range(48, 72) + range(24, 48) + range(72, 96)
