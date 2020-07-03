import os
import sqlite3
import config_env as env

class Benchmark:

    def __init__(self, algo_field_names, algo_field_types,                  \
            case_field_names, case_field_types):
        self.__algo_fields = algo_field_names
        self.__case_fields = case_field_names
        fpath = "%s/%s" % (env.out_dir, env.db_fname)
        conn = sqlite3.connect(fpath)
        cursor = conn.cursor()
        sql = "select count(*) from sqlite_master where type = 'table'"     \
                "and name = '%s'" % env.db_table
        if not Benchmark.__does_exist(cursor, sql):
            user_fields = []
            assert(len(algo_field_names) == len(algo_field_types))
            for i in range(len(algo_field_names)):
                user_fields.append("%s %s" % (algo_field_names[i],          \
                        algo_field_types[i]))
            assert(len(case_field_names) == len(case_field_types))
            for i in range(len(case_field_names)):
                user_fields.append("%s %s" % (case_field_names[i],          \
                        case_field_types[i]))
            joint_user_fields = ", ".join(user_fields)
            latency_fields = []
            recall_fields = []
            for t in ("best", "worst", "avg"):
                latency_fields.append("latency_%s REAL" % t)
                recall_fields.append("recall_%s REAL" % t)
            for p in env.percentiles:
                strp = str(p).replace(".", "")
                latency_fields.append("latency_P%s REAL" % strp)
                recall_fields.append("recall_P%s REAL" % strp)
            joint_latency_fields = ", ".join(latency_fields)
            joint_recall_fields = ", ".join(recall_fields)
            sql = "create table %s (%s, top INTEGER, batch_size INTEGER,"   \
                    " thread_count INTEGER, qps REAL, cpu_util REAL,"       \
                    " mem_r_bw REAL, mem_w_bw REAL, mem_size REAL,"         \
                    " %s, %s)" % (env.db_table, joint_user_fields,          \
                    joint_latency_fields, joint_recall_fields)
            cursor.execute(sql)
        self.__conn = conn
        self.__cursor = cursor

    def run(self, index_key, index_parameters, algo_fields,                 \
            case_fieldss):
        for top in env.tops:
            self.__run(index_key, index_parameters, top, algo_fields,       \
                    case_fieldss)
        index_fpath = "%s/%s.idx" % (env.data_dir, index_key)
        tmp_fpath = "%s/%s.log" % (env.out_dir, os.getpid())
        cmd = "%s ../index size %s > %s" % (env.cmd_prefix, index_fpath,    \
                tmp_fpath)
        return_code = os.system(cmd)
        if return_code != 0:
            exit(return_code)
        fd = open(tmp_fpath, "r")
        size = fd.read()
        fd.close()
        os.remove(tmp_fpath)
        search_fields = []
        assert(len(algo_fields) == len(self.__algo_fields))
        for i in range(len(algo_fields)):
            search_fields.append("%s = %s" % (self.__algo_fields[i],        \
                    algo_fields[i]))
        joint_search_fields = " and ".join(search_fields)
        sql = "update %s set mem_size=%s where %s" % (env.db_table, size,   \
                joint_search_fields)
        self.__cursor.execute(sql)
        self.__conn.commit()

    def __run(self, index_key, index_parameters, top,                       \
            algo_fields, case_fieldss):
        index_fpath = "%s/%s.idx" % (env.data_dir, index_key)
        if not os.access(index_fpath, os.R_OK):
            base_fpath = "%s/%s" % (env.data_dir, env.base_fname)
            cmd = "%s ../index build '%s' '%s' l2 '%s' '%s' %f" %           \
                    (env.cmd_prefix, index_fpath, index_key,                \
                    index_parameters, base_fpath, env.train_rato)
            return_code = os.system(cmd)
            if return_code != 0:
                exit(return_code)
        assert(os.access(index_fpath, os.R_OK))
        query_fpath = "%s/%s" % (env.data_dir, env.query_fname)
        groundtruth_fpath = "%s/%s" % (env.data_dir, env.groundtruth_fname)
        joint_percentiles = ",".join(map(str, env.percentiles))
        cases = []
        expresses = []
        for case_fields in case_fieldss:
            search_fields = []
            assert(len(algo_fields) == len(self.__algo_fields))
            for i in range(len(algo_fields)):
                search_fields.append("%s = %s" % (self.__algo_fields[i],    \
                        algo_fields[i]))
            assert(len(case_fields) == len(self.__case_fields))
            for i in range(len(case_fields)):
                search_fields.append("%s = %s" % (self.__case_fields[i],    \
                        case_fields[i]))
            joint_search_fields = " and ".join(search_fields)
            for batch_size in env.batch_sizes:
                for thread_count in env.thread_counts:
                    sql = "select count(*) from %s where %s and top = %d"   \
                            " and batch_size = %d and thread_count = %d" %  \
                            (env.db_table, joint_search_fields, top,        \
                            batch_size, thread_count)
                    if Benchmark.__does_exist(self.__cursor, sql):
                        continue
                    case = list(algo_fields)
                    case.extend(case_fields)
                    case.extend((top, batch_size, thread_count))
                    cases.append(case)
                    parameters = []
                    for i in range(len(case_fields)):
                        parameters.append("%s=%s" % (self.__case_fields[i], \
                                case_fields[i]))
                    joint_parameters = ",".join(parameters)
                    cpus = env.cpus[0 : thread_count]
                    joint_cpus = ",".join(map(str, cpus))
                    express = "%s/%dx%d:%s" % (joint_parameters,            \
                            batch_size, thread_count, joint_cpus)
                    expresses.append(express)
        if len(cases) == 0:
            return
        joint_expresses = ";".join(expresses)
        tmp_fpath = "%s/%s.log" % (env.out_dir, os.getpid())
        cmd = "%s ../benchmark '%s' '%s' '%s' '%d' '%s' '%s' > %s" %        \
                (env.cmd_prefix, index_fpath, query_fpath,                  \
                groundtruth_fpath, top, joint_percentiles,                  \
                joint_expresses, tmp_fpath)
        return_code = os.system(cmd)
        if return_code != 0:
            exit(return_code)
        fd = open(tmp_fpath, "r")
        lines = fd.readlines()
        fd.close()
        os.remove(tmp_fpath)
        offset = 0
        for case in cases:
            field_count = len(case) + 5 + (3 + len(env.percentiles)) * 2
            place_holders = ["?"] * field_count
            joint_place_holders = ", ".join(place_holders)
            sql = "insert into %s values (%s)" % (env.db_table,             \
                    joint_place_holders)
            qps = Benchmark.__parse_value(lines[offset + 0], "qps")
            cpu_util = Benchmark.__parse_value(lines[offset + 1], "cpu-util")
            mem_r_bw = Benchmark.__parse_value(lines[offset + 2], "mem-r-bw")
            mem_w_bw = Benchmark.__parse_value(lines[offset + 3], "mem-w-bw")
            latencies = Benchmark.__parse_statistics(lines[offset + 4], "latency")
            recalls = Benchmark.__parse_statistics(lines[offset + 5], "recall")
            offset += 6
            values = list(case)
            values.extend((qps, cpu_util, mem_r_bw, mem_w_bw, 0))
            values.extend(latencies)
            values.extend(recalls)
            assert(len(values) == field_count)
            self.__cursor.execute(sql, values)

    @staticmethod
    def __does_exist(cursor, sql):
        rows = cursor.execute(sql)
        loops = 0
        for row in rows:
            exist = row[0]
            loops += 1
        assert(loops == 1)
        assert(exist == 0 or exist == 1)
        return exist == 1

    @staticmethod
    def __parse_value(raw, key):
        items = raw.split()
        assert(len(items) == 2)
        assert(items[0] == ("%s:" % key))
        value = float(items[1])
        return value

    @staticmethod
    def __parse_statistics(raw, key):
        items = raw.split()
        assert(len(items) == 4 + len(env.percentiles))
        assert(items[0] == ("%s:" % key))
        values = []
        for item in items[1: ]:
            k, v = item.split("=")
            values.append(float(v))
        return values
