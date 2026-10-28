#!/usr/bin/env python3
"""Run inert, source-bound checks of the four follow-up repair groups.

Copyright 2026 Fernando Voltolini de Azambuja.
SPDX-License-Identifier: GPL-2.0-or-later

Requires a Git clone with the published predecessor, Python 3 and a C99 compiler.
Production changes use C89 syntax; the test scaffolding uses C99.
No ordinary Argyll executable, constructor or device transport is run.
"""
from collections import Counter
from pathlib import Path
import json
import os
import re
import shlex
import subprocess
import tempfile

from source_extract import _block, _define, _enum, _extract, _function

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
BEFORE = "e924058ee3b23e49be5053e0ab2bc73e727354c4"
CC = shlex.split(os.environ.get("CC", "cc"))


def source(path, before=False):
    if before:
        return subprocess.check_output(
            ["git", "show", BEFORE + ":" + path], cwd=ROOT, text=True
        )
    return (ROOT / path).read_text()


def run(tmp, name, harness, files, flags=()):
    folder = tmp / name
    folder.mkdir()
    for path, text in files.items():
        (folder / path).write_text(text)
    (folder / "probe.c").write_text(harness)
    exe = folder / ("probe.exe" if os.name == "nt" else "probe")
    cmd = CC + ["-std=c99", "-O1", "-Wall", "-Wextra", "-Werror",
                "-Wno-unused-variable", "-Wno-unused-but-set-variable",
                "-Wno-unused-parameter", "-Wno-unused-function",
                "-Wno-missing-field-initializers"]
    subprocess.run(cmd + list(flags) + [str(folder / "probe.c"), "-o", str(exe), "-lm"],
                   check=True, timeout=60)
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
    return result


def expect(result, failures, json_result=False):
    assert result.returncode == bool(failures), result.stdout + result.stderr
    if json_result:
        record = json.loads(result.stdout)
        assert record["failures"] == failures, record
        return record
    found = [line[5:] for line in result.stdout.splitlines() if line.startswith("FAIL ")]
    assert len(found) == failures, result.stdout + result.stderr
    return found


def spyder(tmp):
    names = ("spectro/spydX2.c", "spectro/inst.h", "spectro/disptechs.h",
             "spectro/icoms.h", "numlib/numsup.c", "icc/icc_util.c")
    for before in (True, False):
        texts = {name: source(name, before) for name in names}
        decl, funcs = _extract(source("spectro/spydX2.h", before), texts)
        funcs += "".join(_function(texts[names[0]], name) for name in (
            "spydX2_set_disptype", "set_default_disp_type", "set_base_disp_type",
            "spydX2_col_cor_mat"))
        result = run(tmp, "spyder-" + str(before),
                     (HERE / "spyder_selection_probe.c").read_text(),
                     {"declarations.h": decl, "functions.inc": funcs},
                     ("-DSPYDX2_PATH_PROBE",))
        record = expect(result, 7 if before else 0, True)
        assert record["checks"] == 151, record
    print("PASS Spyder selection: predecessor 7 failures; current 151 checks, 0 failures")


def specbos(tmp):
    inst = source("spectro/inst.h")
    decl = "".join(_enum(inst, name) for name in (
        "inst_code", "inst_mode", "inst_cal_type", "inst_cal_cond",
        "inst_calc_id_type", "inst_ui_purp"))
    decl += _define(inst, "CALIDLEN")
    decl += "enum {\n" + "\n".join(
        re.search(r"^\s*" + name + r"\s*=.*$", inst, re.M)[0]
        for name in ("inst_opt_trig_prog", "inst_opt_trig_user")) + "\n};\n"
    decl += "".join(_define(source("spectro/specbos.h"), name)
                    for name in ("SPECBOS_SPOS_AMB", "SPECBOS_SPOS_EMIS"))
    for before in (True, False):
        text = source("spectro/specbos.c", before)
        local = decl + "".join(_define(text, name) for name in (
            "MAX_MES_SIZE", "MAX_RD_SIZE", "DEFAULT_NAV", "DEFAULT_TRANS_NAV", "EXPL1211AVG"))
        read = _function(text, "specbos_read_sample")
        read = read[:read.index("\n\t/* Adjust timeout to account for averaging */")]
        read += ("\n/* TEST ONLY: measurement tail replaced by inert success. */\n"
                 "if (val) val->sp.spec_n = 3;\namutex_unlock(p->lock);\nreturn inst_ok;\n}\n")
        funcs = _function(text, "set_average") + read
        funcs += _function(text, "specbos_get_n_a_cals") + _function(text, "specbos_calibrate")
        result = run(tmp, "specbos-" + str(before),
                     (HERE / "specbos_recovery_probe.c").read_text(),
                     {"calibration_declarations.h": local, "calibration_functions.inc": funcs},
                     ("-DCM_SPECBOS",))
        record = expect(result, 40 if before else 0, True)
        assert record["checks"] == 400, record
    print("PASS Specbos recovery: predecessor 40 failures; current 400 checks, 0 failures")


def conversions(tmp):
    harness = (HERE / "finite_conversion_probe.c").read_text()
    for family, selector in (("i1pro", 1), ("i1pro3", 2), ("munki", 3)):
        controls = None
        for before in (True, False):
            text = source("spectro/" + family + "_imp.c", before)
            header = source("spectro/" + family + "_imp.h")
            codes = "".join(_define(header, family.upper() + suffix)
                            for suffix in ("_OK", "_RD_READINCONS", "_INT_CIECONVFAIL"))
            codes += _define(source("xicc/xspect.h"), "XSPECT_MAX_BANDS")
            funcs = "" if before else (_function(text, family + "_double_finite")
                                       + _function(text, family + "_patch_finite"))
            funcs += _function(text, family + "_conv2XYZ")
            result = run(tmp, family + "-converter-" + str(before), harness,
                         {"codes.inc": codes, "converter.inc": funcs,
                          "weights.inc": re.search(
                              r"static double opt_adj_weights\[21\] = \{.*?\};", text, re.S)[0]},
                         ("-DFAMILY=" + str(selector),))
            failed = expect(result, 12 if before else 0)
            if before:
                expected = Counter(name + suffix for name in (
                    "nonfinite-input", "reflectance-scale-overflow", "integration-overflow",
                    "xyz-percent-overflow", "post-xrga-overflow", "post-filter-overflow")
                    for suffix in ("", "-second"))
                assert Counter(failed) == expected, failed
            valid = tuple(line for line in result.stdout.splitlines() if line.startswith("CONTROL "))
            assert len(valid) == 13, valid
            if before:
                controls = valid
            else:
                assert valid == controls, "finite signed/zero controls changed"
        print("PASS " + family + " conversion: predecessor 12 failures; current 20 cases, 0 failures")


def scaled(tmp):
    for family in ("munki", "i1pro"):
        harness = (HERE / "scaled_output_probe.c").read_text()
        if family == "i1pro":
            harness = harness.replace("munki", "i1pro").replace("MUNKI", "I1PRO")
            harness = harness.replace("double *cal_factor;", "double *cal_factor[2];")
            harness = harness.replace("int mmode, nwav;", "int mmode, highres, nwav[2];")
            harness = harness.replace("state.nwav = 2; state.ms[0].cal_factor = factors;",
                                      "state.nwav[0] = 2; state.ms[0].cal_factor[0] = factors;")
        controls = None
        for before in (True, False):
            text = source("spectro/" + family + "_imp.c", before)
            funcs = "" if before else (_function(text, family + "_double_finite")
                                       + _function(text, family + "_scaled_finite"))
            funcs += _function(text, family + "_scale_specrd")
            for caller, wrapper in (("_read_patches_2", "source_caller2"),
                                    ("_read_patches_2a", "source_caller2a")):
                # Upstream has alternative preprocessor branches with unbalanced
                # raw braces. Extract only the post-scale publication tail.
                begin = text.index(family + "_code " + family + caller + "(")
                anchor = "\t" + family + "_scale_specrd(p, specrd, numpatches, specrd);\n"
                begin = text.index(anchor, begin)
                end = text.index("\n}\n", begin) + len("\n}\n")
                funcs += ("static " + family + "_code " + wrapper + "(" + family
                          + " *p, double **specrd, int numpatches) {\n"
                          + family + "_code ev = " + family.upper() + "_OK;\n"
                          + text[begin:end])
            codes = "".join(_define(source("spectro/" + family + "_imp.h"),
                                   family.upper() + suffix) for suffix in ("_OK", "_RD_READINCONS"))
            result = run(tmp, family + "-scaled-" + str(before), harness,
                         {"functions.inc": funcs, "codes.inc": codes})
            expect(result, 4 if before else 0)
            valid = tuple(line for line in result.stdout.splitlines() if line.startswith("CONTROL "))
            assert len(valid) == 6
            if before:
                controls = valid
            else:
                assert valid == controls
        print("PASS " + family + " calibrated output: predecessor 4 failures; current 10 cases, 0 failures")


def white(tmp):
    harness = (HERE / "white_calibration_probe.c").read_text()
    for before in (True, False):
        files = {}
        for name, family, marker in (
            ("reflective", "i1pro3", "\t\tif (s->cdate != cdate\n\t\t && (*calt & (inst_calt_ref_white"),
            ("transmission", "i1pro3", "\t\tif (s->cdate != cdate\n\t\t && (*calt & (inst_calt_trans_vwhite"),
            ("munki", "munki", "\t\tif ((*calt & (inst_calt_ref_white")):
            text = source("spectro/" + family + "_imp.c", before)
            block = _block(_function(text, family + "_imp_calibrate"), marker)
            end = block.index("s->cal_valid = 1;") + len("s->cal_valid = 1;")
            files[name + ".inc"] = block[:end] + "\n\t\t}\n"
        codes = _enum(source("spectro/inst.h"), "inst_cal_type")
        codes += _enum(source("spectro/inst.h"), "inst_cal_cond")
        codes += _enum(source("spectro/inst.h"), "inst_opt_filter")
        joined = harness + "".join(files.values())
        for family in ("i1pro3", "munki"):
            names = set(re.findall(r"\b" + family.upper() + r"_[A-Z0-9_]+", joined))
            codes += "".join(_define(source("spectro/" + family + "_imp.h"), name)
                             for name in sorted(names))
        files["codes.inc"] = codes
        result = run(tmp, "white-" + str(before), harness, files)
        found = [line[5:] for line in result.stdout.splitlines() if line.startswith("FAIL ")]
        if before:
            assert result.returncode == 1 and found
            assert set(found) <= {"helper entered with stale validity",
                                  "refused white calibration invalid", "failure restores mode"}, result.stdout
            baseline = len(found)
        else:
            expect(result, 0)
    print("PASS white calibration: predecessor " + str(baseline) + " failures; current 0 failures")


def dark(tmp):
    """Execute actual acquisition prefixes, including overwrite-before-error."""
    for family in ("i1pro", "munki"):
        for before in (True, False):
            text = source("spectro/" + family + "_imp.c", before)
            start = text.index(family + "_code " + family + "_imp_calibrate(")
            end = text.index("\n}\n", start)
            text = text[start:end]
            wrappers = []
            for match in re.finditer(r"if \(\(ev = " + family
                                    + r"_dark_measure\(p, s->(dark_data|idark_data\[0\]),", text):
                log = text.rfind("a1logd(", 0, match.start())
                # The scan branch has preparation before this log and is covered
                # by source review, rather than this bounded prefix execution.
                if any(marker in text[log:match.start()]
                       for marker in ("Doing adaptive scan", "Doing another black calibration")):
                    continue
                begin = text.rfind("\n", 0, log) + 1
                if family == "munki" and match[1] == "idark_data[0]":
                    setup = text.rfind("s->idark_int_time[0] =", 0, log)
                    begin = list(re.finditer(r"(?m)^[ \t]*\n", text[:setup]))[-1].start()
                indent = text[text.rfind("\n", 0, match.start()) + 1:match.start()]
                end = text.index("\n" + indent + "}", match.end()) + len("\n" + indent + "}")
                block = text[match.start():end]
                prefix = text[begin:match.start()] + block
                index = len(wrappers)
                wrappers.append("static int capture" + str(index) + "(device *p, state *s) {\n"
                                "imp *m=p->m; int ev=0, nummeas=1, mmode=9, stm=0;\n"
                                + prefix + "\nreturn ev;\n}\n")
            assert len(wrappers) == (5 if family == "i1pro" else 4), len(wrappers)
            upper = family.upper()
            harness = """#include <stdio.h>
#include <string.h>
#include "codes.inc"
typedef struct {int dark_valid,idark_valid,gainmode; double inttime,idark_int_time[4];
 double *dark_data,*idark_data[4];} state;
typedef struct {int mmode;double min_int_time;} imp;
typedef struct {imp *m;void *log;} device;
static int error, failures;
#define a1logd(...) ((void)0)
#define msec_time() 0
static int measure(device *p,double *data,int count,double *time,int gain) {
 (void)p;(void)count;(void)time;(void)gain; data[0]=99;return error;
}
""" + ("#define " + family + "_dark_measure measure\n#define " + family
       + "_comp_nummeas(...) 1\n") + "".join(wrappers)
            harness += "int main(void) {\nint k,valid,e,rc;imp m={0};device p={&m,0};state s;double d;\n"
            harness += "int (*captures[])(device*,state*)={" + ",".join(
                "capture" + str(i) for i in range(len(wrappers))) + "};\n"
            harness += """for(k=0;k<(int)(sizeof captures/sizeof *captures);k++) {
 for(valid=0;valid<2;valid++)for(e=0;e<2;e++) {
  memset(&s,0,sizeof s);s.dark_valid=s.idark_valid=valid;
  s.dark_data=s.idark_data[0]=&d;d=7;error=e?42:0;
  rc=captures[k](&p,&s);
  if(rc!=error || d!=99 || (s.dark_valid && s.idark_valid)) {
   printf("FAIL dark prefix %d validity=%d error=%d\\n",k,valid,e);failures++;
  }
 }
}
 printf("dark: %d failures\\n",failures);return failures!=0;
}
"""
            codes = _define(source("spectro/" + family + "_imp.h"), upper + "_OK")
            result = run(tmp, family + "-dark-" + str(before), harness, {"codes.inc": codes})
            expect(result, len(wrappers) * 2 if before else 0)
        print("PASS " + family + " dark capture: " + str(len(wrappers) * 4)
              + " current prefix cases, 0 failures")
    for before in (True, False):
        text = source("spectro/munki_imp.c", before)
        start = "\t\t\tif (s->reflective) {\t/* We read the write reference - check it */"
        end = "\t\t\t*calt &= ~(inst_calt_ref_white\n\t\t\t         | inst_calt_trans_vwhite);"
        block = text[text.index(start):text.index(end) + len(end)]
        codes = _enum(source("spectro/inst.h"), "inst_cal_type")
        codes += "".join(_define(source("spectro/munki_imp.h"), name)
                         for name in ("MUNKI_OK", "MUNKI_COMS_FAIL", "MUNKI_RD_WHITEREFERROR"))
        result = run(tmp, "munki-completion-" + str(before),
                     (HERE / "munki_dark_probe.c").read_text(),
                     {"block.inc": block, "codes.inc": codes})
        found = [line for line in result.stdout.splitlines() if line.startswith("FAIL ")]
        if before:
            assert result.returncode == 1 and found, result.stdout
            baseline = len(found)
        else:
            expect(result, 0)
    print("PASS ColorMunki inner-dark completion: predecessor " + str(baseline)
          + " failures; current 4 scenarios, 0 failures")


def flash(tmp):
    harness = """#include <stdio.h>
#include <math.h>
#include <float.h>
#include "codes.inc"
typedef struct {double *cal_factor[2];} state;
typedef struct {int nwav[2];} imp;
typedef struct {void *log;} device;
static int freed, copied;
#define a1logd(...) ((void)0)
static void vect_mul(double *v,double *f,int count) {
 int i;for(i=0;i<count;i++)v[i]*=f[i];
}
static void vect_cpy(double *v,double *in,int count) {
 int i;copied++;for(i=0;i<count;i++)v[i]=in[i];
}
static void release(double **v,int a,int b,int c,int d) {
 (void)v;(void)a;(void)b;(void)c;(void)d;freed++;
}
#define free_dmatrix release
static int capture(device *p,state *s,imp *m,double **wav_sample,double **specrd) {
 int i,hr=0;
#include "flash.inc"
 return I1PRO3_OK;
}
int main(void) {
 double inputs[]={1,-2,0,DBL_MAX,NAN},factor[]={2},row[1],out[1];
 double *rows[]={row},*outs[]={out};state s={{factor,0}};imp m={{1,0}};device p={0};
 int i,rc,failures=0;
 for(i=0;i<5;i++) {
  row[0]=inputs[i];out[0]=17;freed=copied=0;
  rc=capture(&p,&s,&m,rows,outs);
  if(i<3 ? !(rc==I1PRO3_OK && copied==1 && freed==1 && out[0]==inputs[i]*2)
         : !(rc==I1PRO3_RD_READINCONS && copied==0 && freed==1 && out[0]==17)) {
   printf("FAIL flash scaled case %d\\n",i);failures++;
  }
 }
 printf("flash: %d failures\\n",failures);return failures!=0;
}
"""
    for before in (True, False):
        text = source("spectro/i1pro3_imp.c", before)
        start = text.index("\t\tvect_mul(wav_sample[0], s->cal_factor[hr], m->nwav[hr]);")
        end = text.index("\n\t}", start)
        codes = "".join(_define(source("spectro/i1pro3_imp.h"), name)
                         for name in ("I1PRO3_OK", "I1PRO3_RD_READINCONS"))
        funcs = "" if before else _function(text, "i1pro3_double_finite")
        current = harness.replace('#include "codes.inc"', '#include "codes.inc"\n' + funcs)
        result = run(tmp, "flash-" + str(before), current,
                     {"codes.inc": codes, "flash.inc": text[start:end]})
        expect(result, 2 if before else 0)
    print("PASS i1pro3 flash scaling: predecessor 2 failures; current 5 cases, 0 failures")


def main():
    with tempfile.TemporaryDirectory(prefix="argyll-driver-regression-") as tmp:
        folder = Path(tmp)
        spyder(folder)
        specbos(folder)
        white(folder)
        dark(folder)
        conversions(folder)
        scaled(folder)
        flash(folder)
    print("PASS: temporary sources and compiler products removed; no instrument access")


if __name__ == "__main__":
    main()
