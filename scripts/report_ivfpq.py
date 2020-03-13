import benchmark
import config_ivfpq as ivfpq

algo_field_names = ("centroid", "code")
algo_field_types = ("INTEGER", "INTEGER")
case_field_names = ("nprobe", )
case_field_types = ("INTEGER", )

bench = benchmark.Benchmark(algo_field_names, algo_field_types,             \
        case_field_names, case_field_types)

for centroid in ivfpq.centroids:
    for code in ivfpq.codes:
        algo_fields = (centroid, code)
        key = "IVF%d,PQ%d" % algo_fields
        case_fields = []
        for nprobe in ivfpq.nprobes:
            case_fields.append((nprobe, ))
        bench.run(key, ivfpq.parameters, algo_fields, case_fields)