import benchmark
import config_ivfflat as ivfflat

algo_field_names = ("centroid", )
algo_field_types = ("INTEGER", )
case_field_names = ("nprobe", )
case_field_types = ("INTEGER", )

bench = benchmark.Benchmark(algo_field_names, algo_field_types,             \
        case_field_names, case_field_types)

for centroid in ivfflat.centroids:
    algo_fields = (centroid, )
    key = "IVF%d,Flat" % algo_fields
    case_fields = []
    for nprobe in ivfflat.nprobes:
        case_fields.append((nprobe, ))
    bench.run(key, ivfflat.parameters, algo_fields, case_fields)