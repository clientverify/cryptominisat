#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Copyright (C) 2014  Mate Soos
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; version 2
# of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.

import sys
import gzip
import re
import ntpath
import os

from optparse import OptionParser
parser = OptionParser()
parser.add_option("-f", "--file",
                  dest="outfname", type=str,
                  help="print final values to this file")
parser.add_option("-n", "--num",
                  dest="num", type=int,
                  help="Number of reconfs")
parser.add_option("--dropdown",
                  dest="dropdown", type=float, default=0.05,
                  help="From traget 1.0 this is subtracted no matter what")
parser.add_option("--cutoff",
                  dest="cutoff", type=float, default=0.45,
                  help="At least this much or higher is needed for +")
parser.add_option("--divisor",
                  dest="divisor", type=float, default=3000.0,
                  help="Time difference is divided by this much and subtracted")
parser.add_option("--ignorethresh",
                  dest="ignore_threshold", type=float, default=4000.0,
                  help="If all solved above this score, ignore")
parser.add_option("--maxscore",
                  dest="maxscore", type=int, default=5000.0,
                  help="Scores go down from here")
parser.add_option("--ignore", "-i",
                  dest="ignore", type=str,
                  help="Ignore these reconfs")

(options, args) = parser.parse_args()
# print "args:", args

ignore = {}
if options.ignore:
    for r in options.ignore.split(","):
        r = r.strip()
        r = int(r)
        ignore[r] = True

order = ["numVars", "numClauses", "var_cl_ratio", "binary", "trinary", "horn",
         "horn_mean", "horn_std", "horn_min", "horn_max", "horn_spread",
         "vcg_var_mean", "vcg_var_std", "vcg_var_min", "vcg_var_max",
         "vcg_var_spread", "vcg_cls_mean", "vcg_cls_std", "vcg_cls_min",
         "vcg_cls_max", "vcg_cls_spread", "pnr_var_mean", "pnr_var_std",
         "pnr_var_min", "pnr_var_max", "pnr_var_spread", "pnr_cls_mean",
         "pnr_cls_std", "pnr_cls_min", "pnr_cls_max", "pnr_cls_spread",
         "avg_confl_size", "confl_size_min", "confl_size_max",
         "avg_confl_glue", "confl_glue_min", "confl_glue_max",
         "avg_num_resolutions", "num_resolutions_min",
         "num_resolutions_max", "learnt_bins_per_confl",
         "learnt_tris_per_confl", "avg_branch_depth", "branch_depth_min",
         "branch_depth_max", "avg_trail_depth_delta", "trail_depth_delta_min",
         "trail_depth_delta_max", "avg_branch_depth_delta", "props_per_confl",
         "confl_per_restart", "decisions_per_conflict",
         "irred-glue_distr_mean", "irred-glue_distr_var",
         "irred-size_distr_mean", "irred-size_distr_var",
         "irred-uip_use_distr_mean", "irred-uip_use_distr_var",
         "irred-activity_distr_mean", "irred-activity_distr_var",
         "red-glue_distr_mean", "red-glue_distr_var", "red-size_distr_mean",
         "red-size_distr_var", "red-uip_use_distr_mean",
         "red-uip_use_distr_var", "red-activity_distr_mean",
         "red-activity_distr_var", "num_gates_found_last",
         "num_xors_found_last"]

f = open("outs/reconf.names", "w")
f.write("reconf.                     | the target attribute\n\n")
f.write("name:                     label.\n")
for o in order:
    f.write("%s:                     continuous.\n" % o)
f.write("\nreconf:                 +,-.\n")
f.close()

if options.num is None:
    print "ERROR: You must give the number of reconfs"
    exit(-1)


def parse_features_line(line):
    line = re.sub("c.*features. ", "", line)
    line = line.strip().split(" ")
    dat = {}

    name = ""
    for elem, i in zip(line, xrange(1000)):
        elem = elem.strip(":").strip(",")
        if i % 2 == 0:
            name = elem
            continue

        dat[name] = elem
        name = ""
    return dat


def nobody_could_solve_it(reconf_score):
    for r_s_elem in reconf_score:
        if r_s_elem[1] != 0:
            return False

    return True


def all_above_fixed_score(reconf_score, score_limit):
    for x in reconf_score:
        if x[1] < score_limit:
            return False

    return True


def print_features_and_scores(fname, features, reconfs_scores):
    r_s = sorted(reconfs_scores, key=lambda x: x[1])[::-1]
    best_reconf = r_s[0][0]
    best_reconf_score = r_s[0][1]
    print r_s

    if nobody_could_solve_it(r_s):
        print "%s Nobody could solve it" % fname
        return -1, False

    if all_above_fixed_score(r_s, options.ignore_threshold):
        print "%s All above score" % (fname)
        return -2, False

    print "Calculating +/- for %s" % fname

    #calculate final array
    final_array = [0.0]*options.num
    val = 1.0
    best_score = r_s[0][1]
    for conf_score, i in zip(r_s, xrange(100)):
        diff = abs(conf_score[1]-best_score)
        best_score = conf_score[1]
        val -= float(diff)/options.divisor
        if diff > 0:
            val -= options.dropdown

        if val < 0.0 or conf_score[1] == 0:
            val = 0.0

        if conf_score[1] > 0:
            final_array[conf_score[0]] = val

    #assemble final string
    string = ""
    string += "%s," % fname
    for name in order:
        string += "%s," % features[name]

    #print to console
    if True:
        string2 = str(string)
        string2 += "||"
        for a in final_array:
            string2 += "%.1f " % a

        print string2

    #print to files
    origstring = str(string)
    for i in range(options.num):
        #skip files we don't need to write to
        if i in ignore:
            continue

        string = str(origstring)
        if final_array[i] >= options.cutoff:
            string += "+"
        else:
            string += "-"

        outf[i].write(string + "\n")

    only_this_could_solve_it = r_s[1][1] == 0
    return best_reconf, only_this_could_solve_it


def parse_file(fname):
    f = gzip.open(fname, 'rb')
    #print "fname orig:", fname
    fname_clean = re.sub("cnf.gz-.*", "cnf.gz", fname)
    fname_clean = ntpath.split(fname_clean)[1]
    reconf = 0

    satisfiable = None
    features = None
    score = 0
    for line in f:
        line = line.strip()
        #print "parsing line:", line
        if features is None and "features" in line and "numClauses" in line:
            features = parse_features_line(line)

        if "Total time" in line:
            time_used = line.strip().split(":")[1].strip()
            score = int(round(float(time_used)))
            #score -= score % 1000
            score = options.maxscore-score

        if "reconfigured" in line:
            reconf = line.split("to config")[1].strip()
            reconf = int(reconf)

        if "s SATIS" in line:
            satisfiable = True

        if "s UNSATIS" in line:
            satisfiable = False

    #if satisfiable == True:
    #    score = 0

    if reconf in ignore:
        score = 0

    return fname_clean, reconf, features, score

all_files = set()
all_files_scores = {}
all_files_features = {}
max_num_features = 0
for x in args:
    print "# parsing infile:", x
    fname, reconf, features, score = parse_file(x)
    if fname in all_files:
        if all_files_features[fname] != features:
            print "different features extracted for fname", fname
            print "orig:", all_files_features[fname]
            print "new: ", features
            print "Keeping the longer one!"

        if all_files_features[fname] is None:
            num_features = 0
        else:
            num_features = len(all_files_features[fname])

        if features is not None and num_features < len(features):
            all_files_features[fname] = features
    else:
        all_files.add(fname)
        all_files_features[fname] = features
        all_files_scores[fname] = []

    #print "fname:", fname
    all_files_scores[fname].append([reconf, score])

    sys.stdout.write(".")
    sys.stdout.flush()

print "END--------"
print "all files:", all_files
print ""
outf = []
for i in range(options.num):
    fname = options.outfname + str(i) + ".data"
    try:
        os.unlink(fname)
    except:
        pass

    if i not in ignore:
        outf.append(open(fname, "w"))
    else:
        outf.append(None)

best_reconf = {}
only_this = {}
for fname in all_files:
    print "calculating final DATs for CNF ", fname
    if all_files_features[fname] is None:
        print "solved too early, no features, skipping"
        continue

    if "avg_confl_size" not in all_files_features[fname]:
        print "WARNING This is weird, probably not solved by one (different features than everything else), skipping"
        continue

    if all_files_features[fname] is not None:
        best, only_this_could_solve_it = print_features_and_scores(fname, all_files_features[fname], all_files_scores[fname])

        if best not in best_reconf:
            best_reconf[best] = 1
        else:
            best_reconf[best] = best_reconf[best] + 1

        if only_this_could_solve_it:
            if best not in only_this:
                only_this[best] = 1
            else:
                only_this[best] = only_this[best] + 1

        print ""

print "best reconfs: ", best_reconf
print "uniquely solved by: ", only_this

for i in range(options.num):
    if outf[i] is not None:
        outf[i].close()
