import benchmark
import config_hnsw as hnsw

algo_field_names = ("M", "efConstruction")
algo_field_types = ("INTEGER", "INTEGER")
case_field_names = ("efSearch", )
case_field_types = ("INTEGER", )

bench = benchmark.Benchmark(algo_field_names, algo_field_types,             \
        case_field_names, case_field_types)

for M in hnsw.Ms:
    for efConstruction in hnsw.efConstructions:
        algo_fields = (M, efConstruction)
        key = "HNSW%d-%d" % algo_fields
        parameters = "efConstruction=%d" % efConstruction
        case_fields = []
        for efSearch in hnsw.efSearchs:
            case_fields.append((efSearch, ))
        bench.run(key, parameters, algo_fields, case_fields)
