/* Copyright 2026 Fernando Voltolini de Azambuja.
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Whole Argyll calibration, need-assessment, simple emission and release
 * functions are extracted at runtime. Real headers define the native objects.
 * Acquisition, timing and allocation use inert stand-ins; unexpected paths abort.
 */
#include "constants.inc"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
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
#include "i1pro3.h"
/* Unused transport token: no USB ABI or runtime is exercised. */
struct _usb_cancelt { int inert; };
#include "i1pro3_imp.h"

static int reads, frees, live_buffers, wrong_free_origin, averages, linearized, saves;
static int probe_error, probe_saturated, probe_inconsistent, read_path, normalized;
static double sample_max = 100000.0;
static double row_storage[4][2][MX_NRAW+1], *rows[4][2];
static int allocated[4], emission_allocations;
static void *unexpected(void) {
    fputs("Unexpected non-substituted calibration path\n", stderr);
    abort();
}
static int probe_measure(i1pro3 *p, i1p3_mmode mm, double ***raw, int *n, double *t) {
    (void)p;
    ++reads;
    if (mm != i1p3mm_em || (!read_path && *t != 0.05)) unexpected();
    if (probe_error) return probe_error;
    int slot;
    for (slot = 0; slot < 4 && allocated[slot]; ++slot) {}
    if (slot == 4) unexpected();
    allocated[slot] = 1;
    ++emission_allocations;
    *n = 2;
    for (int i = 0; i < 2; ++i) {
        rows[slot][i] = row_storage[slot][i] - CM_EMISSION_ROW_ORIGIN;
        for (int j = CM_EMISSION_ROW_ORIGIN; j < 4; ++j) rows[slot][i][j] = sample_max;
    }
    *raw = rows[slot];
    ++live_buffers;
    return I1PRO3_OK;
}
static void probe_free(double **raw, int rlo, int rhi, int clo, int chi) {
    int slot;
    for (slot = 0; slot < 4 && raw != rows[slot]; ++slot) {}
    if (slot == 4 || !allocated[slot] || rlo != 0 || rhi != 1 || chi != 3)
        unexpected();
    allocated[slot] = 0;
    ++frees;
    if (clo != CM_EMISSION_ROW_ORIGIN) ++wrong_free_origin;
    --live_buffers; /* Inert ownership token; never free an invalid pointer. */
}
static void probe_average(i1pro3 *p, double *avg, double **raw, int n) {
    (void)p;
    int slot;
    for (slot = 0; slot < 4 && raw != rows[slot]; ++slot) {}
    if (slot == 4 || !allocated[slot] || n != 2) unexpected();
    ++averages;
    for (int j = 0; j < 4; ++j) avg[j] = (raw[0][j] + raw[1][j]) / 2.0;
}
static double probe_max(double *v, int n) {
    double maximum = v[0];
    for (int i = 1; i < n; ++i) if (v[i] > maximum) maximum = v[i];
    return maximum;
}
#define a1logd(...) ((void)0)
#define time(...) ((time_t)12345)
#define ENABLE_NONVCAL
#define i1pro3_check_calib(...) I1PRO3_OK
#define i1pro3_getboardtemp(p, t) (*(t) = 23.0, I1PRO3_OK)
#define i1pro3_recompute_wav_filters_for_temp(...) (unexpected(), I1PRO3_INTERNAL_ERROR)
#define i1pro3_comp_emis_black(...) ((void)0)
#define i1pro3_multimeas_check_raw_consistency(...) probe_inconsistent
#define i1pro3_vect_lin(...) ((void)0)
#define error(...) unexpected()
#define vect_sub(...) ((void)0)
#define i1pro3_save_calibration(...) (++saves)
#define i1pro3_comp_nummeas(...) 2
#define i1pro3_do_measure(p, mm, raw, n, t, a, b) probe_measure(p, mm, raw, n, t)
#define i1pro3_comp_simple_emis_black(...) ((void)0)
#define i1pro3_multimeas_sub_black(...) ((void)0)
#define i1pro3_multimeas_check_sat(...) probe_saturated
#define i1pro3_multimeas_lin(...) (++linearized)
#define i1pro3_normalize_rawmmeas(...) (read_path ? ++normalized : (unexpected(), 0))
#define i1pro3_average_rawmmeas(...) probe_average(__VA_ARGS__)
#define vect_max(...) probe_max(__VA_ARGS__)
#define free_dmatrix(...) probe_free(__VA_ARGS__)
#define dvectorz(...) ((double *)unexpected())
#define free_dvector(...) unexpected()
#undef vect_cpy
#define vect_cpy(...) unexpected()
#define i1pro3_delay_llampoff(...) unexpected()
#define i1pro3_wl_measure(...) (unexpected(), I1PRO3_INTERNAL_ERROR)
#define i1pro3_match_wl_meas(...) (unexpected(), I1PRO3_INTERNAL_ERROR)
#define i1pro3_compute_wav_filters(...) (unexpected(), I1PRO3_INTERNAL_ERROR)
#define i1pro3_adapt_emis_cal(p,t) (*(t)=0.0, unexpected(), I1PRO3_INTERNAL_ERROR)
#define i1pro3_pol_refl_cal(...) (unexpected(), I1PRO3_INTERNAL_ERROR)
#define i1pro3_refl_cal(...) (unexpected(), I1PRO3_INTERNAL_ERROR)
#define i1pro3_trans_cal(...) (unexpected(), I1PRO3_INTERNAL_ERROR)

#include "functions.inc"
static int run_case(const char *name) {
    reads = frees = live_buffers = wrong_free_origin = averages = linearized = saves = 0;
    probe_error = probe_saturated = probe_inconsistent = read_path = normalized = 0;
    emission_allocations = 0;
    memset(allocated, 0, sizeof allocated);
    sample_max = 100000.0;
    i1pro3 meter = {0};
    i1pro3imp implementation = {0};
    i1pro3imp *m = &implementation;
    meter.m = m;
    m->mmode = i1p3_emiss_spot_na;
    m->nraw = 4;
    m->sens_target = 45000.0;
    m->min_int_time = 0.01;
    i1pro3_state *s = &m->ms[i1p3_emiss_spot_na];
    s->mode = i1p3_emiss_spot_na;
    s->emiss = 1;
    s->dark_valid = 1;
    s->iinttime = 1.0;
    s->inttime = 0.5; /* Sentinel: selected probe resets to the upstream default. */
    s->targoscale = 0.9;
    inst_cal_type calt = inst_calt_emis_int_time, needed, needed_after, available;
    inst_cal_cond calc = inst_calc_emis_white;
    inst_calc_id_type idtype = inst_calc_id_none;
    char id[CALIDLEN] = {0};
    if (!strcmp(name, "zero_maximum")) sample_max = 0.0;
    if (!strcmp(name, "negative_maximum")) sample_max = -3.0;
    if (!strcmp(name, "success_noincrease")) sample_max = 100.0;
    if (!strcmp(name, "success_minimum")) sample_max = 10000000.0;
    if (!strncmp(name, "prior_valid_", 12)) {
        s->done_dintsel = 1;
        s->diseldate = 11111; /* Previously valid selection, before this probe. */
    }
    if (!strcmp(name, "saturated") || !strcmp(name, "prior_valid_saturated"))
        probe_saturated = 1;
    if (!strcmp(name, "prior_valid_error")) probe_error = I1PRO3_COMS_FAIL;
    if (!strcmp(name, "measure_error")) probe_error = I1PRO3_COMS_FAIL;
    if (!strcmp(name, "wrong_condition")) calc = inst_calc_none;
    if (!strcmp(name, "needed_request")) calt = inst_calt_needed;
    if (!strcmp(name, "pending_other")) calt |= inst_calt_em_dark;
    if (!strcmp(name, "adaptive_ignored")) { s->adaptive = 1; calt = inst_calt_ap_flag; }
    if (!strcmp(name, "scan_ignored")) { s->scan = 1; calt = inst_calt_ap_flag; }
    if (!strcmp(name, "all_modes_error")) {
        m->mmode = i1p3_emiss_spot;
        m->ms[i1p3_emiss_spot].emiss = 1;
        m->ms[i1p3_emiss_spot].adaptive = 1;
        m->ms[i1p3_emiss_spot].dark_valid = 1;
        calt = inst_calt_ap_flag;
        probe_error = I1PRO3_COMS_FAIL;
    }
    i1pro3_imp_get_n_a_cals(&meter, &needed, &available);
    int original_mode = m->mmode;
    int ev = i1pro3_imp_calibrate(&meter, &calt, &calc, &idtype, id);
    i1pro3_imp_get_n_a_cals(&meter, &needed_after, NULL);
    printf("{\"case\":\"%s\",\"return_code\":%d,\"ok_code\":%d,"
           "\"saturation_code\":%d,\"error_code\":%d,\"setup_code\":%d,"
           "\"needed_before\":%u,\"needed_after\":%u,\"available\":%u,\"remaining\":%u,"
           "\"integration_bit\":%u,\"dark_bit\":%u,\"condition\":%u,"
           "\"done\":%d,\"date\":%ld,\"inttime\":%.17g,"
           "\"original_mode\":%d,\"final_mode\":%d,"
           "\"reads\":%d,\"frees\":%d,\"live_buffers\":%d,"
           "\"wrong_free_origin\":%d,\"averages\":%d,\"linearized\":%d,\"saves\":%d}\n",
           name, ev, I1PRO3_OK, I1PRO3_RD_SENSORSATURATED, I1PRO3_COMS_FAIL,
           I1PRO3_CAL_SETUP, (unsigned)needed, (unsigned)needed_after,
           (unsigned)available, (unsigned)calt,
           (unsigned)inst_calt_emis_int_time, (unsigned)inst_calt_em_dark, (unsigned)calc,
           (int)s->done_dintsel, (long)s->diseldate, s->inttime, original_mode, m->mmode,
           reads, frees, live_buffers, wrong_free_origin, averages, linearized, saves);
    return 0;
}

int main(void) {
    static const char *cases[] = {"success_reduce","success_noincrease","success_minimum","saturated","measure_error","wrong_condition","needed_request","pending_other","all_modes_error","adaptive_ignored","scan_ignored","prior_valid_success","prior_valid_saturated","prior_valid_error","zero_maximum","negative_maximum"};
    size_t i;
    for (i = 0; i < sizeof cases / sizeof cases[0]; i++) run_case(cases[i]);
    return 0;
}
