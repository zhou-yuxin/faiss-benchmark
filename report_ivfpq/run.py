import os
import sqlite3
from config import *

db_conn = None
db_cursor = None
done_cases = set()

def open_db():
    global db_conn
    global db_cursor
    fpath = "%s/%s" % (out_dir, db_fname)
    is_new = not os.access(fpath, os.F_OK)
    db_conn = sqlite3.connect(fpath)
    db_cursor = db_conn.cursor()
    if is_new:
        latency_fields = []
        recall_fields = []
        for t in ("best", "worst", "avg"):
            latency_fields.append("latency_%s REAL" % t)
            recall_fields.append("recall_%s REAL" % t)
        for p in percentiles:
            strp = str(p).replace(".", "")
            latency_fields.append("latency_P%s REAL" % strp)
            recall_fields.append("recall_P%s REAL" % strp)
        joint_latency_fields = ", ".join(latency_fields)
        joint_recall_fields = ", ".join(recall_fields)
        sql = "create table %s (centroid INTEGER, code INTEGER,"            \
                " top INTEGER, nprobe INTEGER, batch_size INTEGER,"         \
                " thread_count INTEGER, qps REAL, cpu_util REAL,"           \
                " mem_r_bw REAL, mem_w_bw REAL, mem_size REAL,"             \
                " %s, %s)" %                                                \
                (db_table, joint_latency_fields, joint_recall_fields)
        db_cursor.execute(sql)
    else:
        sql = "select * from %s" % db_table
        rows = db_cursor.execute(sql)
        for row in rows:
            done_cases.add(row[ : 6])

def parse_value(raw, key):
    items = raw.split()
    assert(len(items) == 2)
    assert(items[0] == ("%s:" % key))
    value = float(items[1])
    return value

def parse_statistics(raw, key):
    items = raw.split()
    assert(len(items) == 4 + len(percentiles))
    assert(items[0] == ("%s:" % key))
    values = []
    for item in items[1: ]:
        k, v = item.split("=")
        values.append(v)
    return values

def handle_output(centroid, code, top, lines):
    field_count = 17 + len(percentiles) * 2
    place_holders = ["?"] * field_count
    joint_place_holders = ",".join(place_holders)
    sql = "insert into %s values (%s)" % (db_table, joint_place_holders)
    offset = 0
    for nprobe in nprobes:
        for batch_size in batch_sizes:
            for thread_count in thread_counts:
                qps = parse_value(lines[offset + 0], "qps")
                cpu_util = parse_value(lines[offset + 1], "cpu-util")
                mem_r_bw = parse_value(lines[offset + 2], "mem-r-bw")
                mem_w_bw = parse_value(lines[offset + 3], "mem-w-bw")
                latencies = parse_statistics(lines[offset + 4], "latency")
                recalls = parse_statistics(lines[offset + 5], "recall")
                offset += 6
                fields = [centroid, code, top, nprobe, batch_size,          \
                        thread_count, qps, cpu_util, mem_r_bw, mem_w_bw,    \
                        0]
                fields.extend(latencies)
                fields.extend(recalls)
                assert(len(fields) == field_count)
                db_cursor.execute(sql, fields)
    db_conn.commit()

def make_case_express(nprobe, batch_size, thread_count):
    selected_cpus = cpus[ : thread_count]
    case = "nprobe=%d/%dx%d:%s" % (nprobe, batch_size, thread_count,    \
            ",".join(map(str, selected_cpus)))
    return case

def run_benchmark(centroid, code, top):
    cases = []
    for nprobe in nprobes:
        for batch_size in batch_sizes:
            for thread_count in thread_counts:
                case = (centroid, code, top, nprobe, batch_size,        \
                        thread_count)
                if case not in done_cases:
                    cases.append(make_case_express(nprobe, batch_size,  \
                            thread_count))
    if len(cases) == 0:
        return
    key = "IVF%d,PQ%d" % (centroid, code)
    index = "%s/%s.idx" % (data_dir, key)
    if not os.access(index, os.R_OK):
        parameters = "verbose=0"
        base_fpath = "%s/%s" % (data_dir, base_fname)
        cmd = "%s ../index build %s %s %s %s %f" %                      \
                (cmd_prefix, index, key, parameters, base_fpath,        \
                train_rato)
        returncode = os.system(cmd)
        if returncode != 0:
            exit(returncode)
    assert(os.access(index, os.R_OK))
    query_fpath = "%s/%s" % (data_dir, query_fname)
    groundtruth_fpath = "%s/%s" % (data_dir, groundtruth_fname)
    joint_percentiles = ",".join(map(str, percentiles))
    joint_cases = "'%s'" % (";".join(cases))
    tmp_fpath = "%s/%s.log" % (out_dir, os.getpid())
    cmd = "%s ../benchmark %s %s %s %d %s %s > %s" %                    \
            (cmd_prefix, index, query_fpath, groundtruth_fpath,         \
            top, joint_percentiles, joint_cases, tmp_fpath)
    returncode = os.system(cmd)
    if returncode != 0:
        exit(returncode)
    fd = open(tmp_fpath, "r")
    lines = fd.readlines()
    fd.close()
    os.remove(tmp_fpath)
    handle_output(centroid, code, top, lines)

def update_size(centroid, code):
    key = "IVF%d,PQ%d" % (centroid, code)
    index = "%s/%s.idx" % (data_dir, key)
    tmp_fpath = "%s/%s.log" % (out_dir, os.getpid())
    cmd = "%s ../index size %s > %s" % (cmd_prefix, index, tmp_fpath)
    returncode = os.system(cmd)
    if returncode != 0:
        exit(returncode)
    fd = open(tmp_fpath, "r")
    size = fd.read()
    fd.close()
    os.remove(tmp_fpath)
    sql = "update %s set mem_size=%s where centroid=%s and code=%s" %   \
            (db_table, size, centroid, code)
    db_cursor.execute(sql)
    db_conn.commit()

open_db()
for centroid in centroids:
    for code in codes:
        for top in tops:
            run_benchmark(centroid, code, top)
        update_size(centroid, code)
