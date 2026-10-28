/* Copyright 2026 Fernando Voltolini de Azambuja.
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Execute the complete calibration and need-assessment functions with real
 * headers. Acquisition is scripted; unexpected paths abort. The measurement
 * function stops after its actual readiness gate. No device code is linked.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "sa_config.h"
#include "numsup.h"
#include "rspl1.h"
#include "cgats.h"
#include "xspect.h"
#include "insttypes.h"
#include "conv.h"
#include "icoms.h"
#include "inst.h"
#include "munki.h"
struct _usb_cancelt { int inert; };
#include "munki_imp.h"
#include "constants.inc"

static int dark_calls, trial_calls, stale_trials, saves;
static int fail_dark_at, fail_white, saturate_first;
static double dark[3], white[3], factors[2], reference[2];
static void *unexpected(void) {
    fputs("Unexpected non-substituted calibration path\n", stderr);
    abort();
}
static munki_code capture_dark(munki *p, double *data, int count,
                               double *inttime, int gain) {
    munkiimp *m = (munkiimp *)p->m;
    (void)count; (void)inttime; (void)gain;
    ++dark_calls;
    /* A dark-level failure is detected after averaging into the live buffer. */
    for (int j = -1; j < m->nraw; ++j)
        data[j] = dark_calls == fail_dark_at ? 999.0 : 10.0;
    return dark_calls == fail_dark_at ? MUNKI_RD_DARKNOTVALID : MUNKI_OK;
}
static munki_code capture_trial(munki *p, double *data, double *scale, int count,
                                double *inttime, int gain, double target) {
    munkiimp *m = (munkiimp *)p->m;
    munki_state *s = &m->ms[m->mmode];
    (void)data; (void)count; (void)inttime; (void)gain; (void)target;
    ++trial_calls;
    /* The real white helper subtracts s->dark_data before choosing exposure. */
    if (s->dark_data[0] == 999.0) ++stale_trials;
    *scale = 1.0;
    return saturate_first && trial_calls == 1 ? MUNKI_RD_SENSORSATURATED : MUNKI_OK;
}

#define a1logd(...) ((void)0)
#define time(...) ((time_t)5000)
#define msec_time() 0
#define msec_sleep(...) ((void)0)
#define ENABLE_NONVCAL
#define munki_getstatus(p, spos, button) (*(spos) = mk_spos_calib, MUNKI_OK)
#define munki_heatLED(...) MUNKI_OK
#define munki_comp_nummeas(...) 1
#define munki_dark_measure capture_dark
#define munki_whitemeasure capture_trial
#define munki_ledtemp_whitemeasure(...) (fail_white ? MUNKI_COMS_FAIL : MUNKI_OK)
#define munki_compute_wav_whitemeas(...) MUNKI_OK
#define munki_compute_white_cal(...) 0
#define munki_save_calibration(...) (++saves, MUNKI_OK)
#define munki_optimise_sensor(p, it, ...) (*(it) = 0.0, unexpected(), MUNKI_INTERNAL_ERROR)
#define munki_interp_dark(...) (unexpected(), MUNKI_INTERNAL_ERROR)
#define munki_prepare_idark(...) ((void)unexpected())
#define dvectorz(...) ((double *)unexpected())
#define free_dvector(...) ((void)unexpected())
#include "functions.inc"

static void setup(munki *p, munkiimp *m) {
    munki_state *s;
    memset(p, 0, sizeof *p);
    memset(m, 0, sizeof *m);
    p->m = m;
    m->mmode = mk_refl_spot;
    m->nraw = m->nwav1 = m->nwav = 2;
    m->min_int_time = 0.004;
    m->optsval = 1.0; m->minsval = 0.25; m->maxsval = 4.0;
    m->white_ref1 = reference;
    s = &m->ms[m->mmode];
    s->mode = mk_refl_spot;
    s->reflective = 1;
    s->dark_valid = s->cal_valid = 1;
    s->ddate = s->cfdate = 4900;
    s->inttime = s->dark_int_time = 0.5;
    s->dark_data = dark + 1;
    s->white_data = white + 1;
    s->cal_factor1 = factors;
    for (int j = 0; j < 3; ++j) dark[j] = 10.0;
    dark_calls = trial_calls = stale_trials = saves = 0;
    fail_dark_at = fail_white = saturate_first = 0;
}
static munki_code calibrate(munki *p, inst_cal_type *calt) {
    inst_cal_cond calc = inst_calc_man_cal_smode;
    inst_calc_id_type idtype = inst_calc_id_none;
    char id[CALIDLEN] = {0};
    return munki_imp_calibrate(p, calt, &calc, &idtype, id);
}
static int one(const char *name) {
    munki p;
    munkiimp m;
    munki_state *s;
    inst_cal_type calt = inst_calt_ref_white, needed, available;
    munki_code rc;
    int ok = 1;
    setup(&p, &m);
    s = &m.ms[m.mmode];
    if (!strcmp(name, "invalid_white")) {
        s->dark_valid = 0;
        s->dark_data[0] = 999.0;
    } else if (!strncmp(name, "retry_", 6)) {
        fail_dark_at = 1;
        rc = calibrate(&p, &calt);
        ok &= rc == MUNKI_RD_DARKNOTVALID && !s->dark_valid && !s->cal_valid;
        ok &= calt == inst_calt_ref_white && saves == 0 && m.mmode == mk_refl_spot;
        fail_dark_at = !strcmp(name, "retry_dark_failure") ? 2 : 0;
    } else if (!strcmp(name, "saturation_recovery")) {
        saturate_first = 1;
    } else if (!strcmp(name, "white_stage_retry")) {
        fail_white = 1;
        rc = calibrate(&p, &calt);
        ok &= rc == MUNKI_COMS_FAIL && s->dark_valid && !s->cal_valid && saves == 0;
        fail_white = 0;
    } else if (!strcmp(name, "white_only_metadata")) {
        s->want_dcalib = 1;
        s->ddate = 1000;
        s->dark_int_time = 0.75;
        s->dark_gain_mode = 1;
    }
    rc = calibrate(&p, &calt);
    if (!strcmp(name, "retry_dark_failure")) {
        ok &= rc == MUNKI_RD_DARKNOTVALID && !s->dark_valid && !s->cal_valid;
        ok &= trial_calls == 1 && dark_calls == 2 && stale_trials == 0 && saves == 0;
        ok &= calt == (inst_calt_ref_dark | inst_calt_ref_white);
    } else {
        int expected_dark = !strcmp(name, "retry_failed_dark") ? 3 :
                            !strcmp(name, "invalid_white") || !strcmp(name, "white_stage_retry") ? 2 : 1;
        int expected_trials = !strcmp(name, "retry_failed_dark") ||
                              !strcmp(name, "saturation_recovery") ||
                              !strcmp(name, "white_stage_retry") ? 2 : 1;
        ok &= rc == MUNKI_OK && calt == inst_calt_none && s->dark_valid && s->cal_valid;
        ok &= dark_calls == expected_dark && trial_calls == expected_trials;
        ok &= stale_trials == 0 && saves == 1;
        ok &= munki_imp_measure(&p, NULL, 1, 0) == MUNKI_OK;
        munki_imp_get_n_a_cals(&p, &needed, &available);
        ok &= !(needed & (inst_calt_ref_dark | inst_calt_ref_white));
        ok &= s->ddate == 5000 && s->dark_int_time == s->inttime;
        ok &= s->dark_gain_mode == s->gainmode && !s->want_dcalib;
    }
    ok &= m.mmode == mk_refl_spot;
    if (!ok) printf("FAIL %s\n", name);
    return !ok;
}
int main(void) {
    static const char *cases[] = {"valid_white", "invalid_white", "retry_failed_dark",
        "retry_dark_failure", "saturation_recovery", "white_stage_retry", "white_only_metadata"};
    int failures = 0;
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) failures += one(cases[i]);
    printf("ColorMunki calibration: %d failures\n", failures);
    return failures != 0;
}
