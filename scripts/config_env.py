lib_dir = "/root/faiss/build/faiss"
cmd_prefix = f"LD_LIBRARY_PATH={lib_dir} numactl --cpunodebind=0"
build_omp_nthreads = 16
bench_omp_nthreads = 1

data_dir = "/root/sift/1M"
base_fname = "base.fvecs"
query_fname = "query.fvecs"
groundtruth_fname = "groundtruth.ivecs"
train_rato = 0.05
metric_type = "l2"

index_dir = "/root/index"
output_dir = "/root/output"
db_fname = "hnsw.db"
db_table = "Grace_Neoverse_V2"
percentiles = (99, 99.9)
loops = 4

tops = (10, 100, )
batch_sizes = (1, )
thread_counts = range(4, 72 + 1, 4)
cpus = range(72)
