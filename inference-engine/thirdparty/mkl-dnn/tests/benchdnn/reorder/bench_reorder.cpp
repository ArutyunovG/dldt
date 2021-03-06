/*******************************************************************************
* Copyright 2017-2018 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#include <vector>
#include <string.h>

#include "mkldnn.h"
#include "mkldnn_common.hpp"
#include "mkldnn_memory.hpp"

#include "reorder.hpp"

namespace reorder {

/* global driver parameters */
attr_t attr;
bool allow_unimpl = false;
bool both_dir_dt = false;
bool both_dir_fmt = false;
const char *perf_template = "perf,%n,%D,%O,%-t,%-Gp,%0t,%0Gp";

std::vector<mkldnn_data_type_t> v_idt, v_odt;
std::vector<mkldnn_memory_format_t> v_ifmt, v_ofmt;
std::vector<dims_t> v_dims;
std::vector<float> v_def_scale;

void reset_parameters() {
    attr = attr_t();
    allow_unimpl = false;
    both_dir_dt = false;
    both_dir_fmt = false;

    v_def_scale = {0.125, 0.25, 0.5, 1, 2, 4, 8};
}

void check(const prb_t *p) {
    res_t res{};
    char pstr[max_prb_len];
    prb2str(p, &res, pstr);

    int status = reorder::doit(p, &res);

    prb2str(p, &res, pstr);
    bool want_perf_report = false;

    parse_result(res, want_perf_report, allow_unimpl, status, pstr);

    if (bench_mode & PERF)
        perf_report(p, &res, pstr);

    benchdnn_stat.tests++;
}

void run() {
    for (auto &idt: v_idt)
    for (auto &odt: v_odt)
    for (int swap_dt = 0; swap_dt < 1 + both_dir_dt * (idt != odt); ++swap_dt)
    for (auto &ifmt: v_ifmt)
    for (auto &ofmt: v_ofmt)
    for (int swap_fmt = 0; swap_fmt < 1 + both_dir_fmt * (ifmt != ofmt); ++swap_fmt)
    for (auto &dims: v_dims)
    {
        reorder_conf_t reorder_conf{dims,
            swap_fmt ? ofmt : ifmt,
            swap_fmt ? ifmt : ofmt};

        dt_conf_t iconf = dt2cfg(swap_dt ? odt : idt);
        dt_conf_t oconf = dt2cfg(swap_dt ? idt : odt);

        std::vector<float> v_attr_scale = {attr.oscale.scale};
        auto &v_scale = attr.oscale.scale == 0 ? v_def_scale : v_attr_scale;

        for (auto &scale: v_scale) {
            const prb_t p(reorder_conf, iconf, oconf, attr, scale);
            check(&p);
        }
    }
}

int bench(int argc, char **argv, bool main_bench) {
    if (main_bench)
        reset_parameters();

    for (int arg = 0; arg < argc; ++arg) {
        if (!strncmp("--batch=", argv[arg], 8))
            SAFE(batch(argv[arg] + 8, bench), CRIT);
        else if (!strncmp("--idt=", argv[arg], 6))
            read_csv(argv[arg] + 6, [&]() { v_idt.clear(); },
                    [&](const char *str) { v_idt.push_back(str2dt(str)); });
        else if (!strncmp("--odt=", argv[arg], 6))
            read_csv(argv[arg] + 6, [&]() { v_odt.clear(); },
                    [&](const char *str) { v_odt.push_back(str2dt(str)); });
        else if (!strncmp("--dt=", argv[arg], 5))
            read_csv(argv[arg] + 5, [&]() { v_idt.clear(); v_odt.clear(); },
                    [&](const char *str) {
                    v_idt.push_back(str2dt(str));
                    v_odt.push_back(str2dt(str));
                    });
        else if (!strncmp("--ifmt=", argv[arg], 7))
            read_csv(argv[arg] + 7, [&]() { v_ifmt.clear(); },
                    [&](const char *str) { v_ifmt.push_back(str2fmt(str)); });
        else if (!strncmp("--ofmt=", argv[arg], 7))
            read_csv(argv[arg] + 7, [&]() { v_ofmt.clear(); },
                    [&](const char *str) { v_ofmt.push_back(str2fmt(str)); });
        else if (!strncmp("--fmt=", argv[arg], 6))
            read_csv(argv[arg] + 6, [&]() { v_ifmt.clear(); v_ofmt.clear(); },
                    [&](const char *str) {
                    v_ifmt.push_back(str2fmt(str));
                    v_ofmt.push_back(str2fmt(str));
                    });
        else if (!strncmp("--def-scales=", argv[arg], 13))
            read_csv(argv[arg] + 13, [&]() { v_def_scale.clear(); },
                    [&](const char *str) { v_def_scale.push_back(atof(str)); });
        else if (!strncmp("--attr=", argv[arg], 7))
            SAFE(str2attr(&attr, argv[arg] + 7), CRIT);
        else if (!strncmp("--both-dir-dt=", argv[arg], 14))
            both_dir_dt = str2bool(argv[arg] + 14);
        else if (!strncmp("--both-dir-fmt=", argv[arg], 14))
            both_dir_fmt = str2bool(argv[arg] + 15);
        else if (!strncmp("--allow-unimpl=", argv[arg], 15))
            allow_unimpl = str2bool(argv[arg] + 15);
        else if (!strcmp("--run", argv[arg]))
            run();
        else if (!strncmp("--perf-template=", argv[arg], 16))
            perf_template = argv[arg] + 16;
        else if (!strcmp("--reset", argv[arg]))
            reset_parameters();
        else if (!strncmp("--mode=", argv[0], 7))
            bench_mode = str2bench_mode(argv[0] + 7);
        else if (!strncmp("-v", argv[arg], 2))
            verbose = atoi(argv[arg] + 2);
        else if (!strncmp("--verbose=", argv[arg], 10))
            verbose = atoi(argv[arg] + 10);
        else {
            if (!strncmp("--", argv[arg], 2)) {
                fprintf(stderr, "driver: unknown option: `%s`, exiting...\n",
                        argv[arg]);
                exit(2);
            }
            read_csv(argv[arg], [&]() { v_dims.clear(); },
                    [&](const char *str) { v_dims.push_back(str2dims(str)); });
            run();
        }
    }

    return OK;
}

}
