/* Copyright 2026 Fernando Voltolini de Azambuja.
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Production source extracts retain Graeme Gill's original notices. */
/* Inert control-flow proof. Argyll function/declaration extracts are generated
 * from the digest-verified archive and retain their upstream notices. This
 * harness uses minimal structural stand-ins, scripted commands and lock counters.
 * No constructor, transport, operating-system lock or measurement tail is linked.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "calibration_declarations.h"

static unsigned checks, failures;
static const char *scenario;
static char failed[4096];
#define CHECK(test) do { \
    ++checks; \
    if (!(test)) { \
        ++failures; \
        if (strlen(failed) + strlen(scenario) + 2 < sizeof(failed)) { \
            if (failed[0]) strcat(failed, ","); \
            strcat(failed, scenario); \
        } \
    } \
} while (0)
static void a1logd(void *log, int level, const char *fmt, ...) {
    (void)log; (void)level; (void)fmt;
}

#ifdef CM_SPECBOS
typedef int instClamping;
typedef struct { int spec_n; } spectrum;
typedef struct { spectrum sp; } ipatch;
typedef struct {
    int gotcoms, inited, doing_cal, trig, model, laser, refrmode, rrset, noaverage, setnav;
    inst_mode mode;
    double measto;
    spectrum trans_white;
    int *lock;
    void *log, *uic_cntx;
    inst_code (*uicallback)(void *, inst_ui_purp);
} specbos;
typedef specbos inst;
static int lock_depth, lock_errors, average_count, fail_average, abort_trigger, fail_laser;
static void amutex_lock(int *lock) { if (*lock) ++lock_errors; ++*lock; }
static void amutex_unlock(int *lock) { if (*lock != 1) ++lock_errors; --*lock; }
static void msec_sleep(int time) { (void)time; }
static inst_code specbos_command(specbos *p, char *cmd, char *out, int size, double timeout) {
    (void)p; (void)size; (void)timeout; out[0] = '\0';
    if (!strncmp(cmd, "*para:aver ", 11) || !strncmp(cmd, "*conf:aver ", 11)) {
        ++average_count;
        return average_count == fail_average ? inst_coms_fail : inst_ok;
    }
    if (!strcmp(cmd, "*contr:laser 0\r")) return fail_laser ? inst_coms_fail : inst_ok;
    if (!strcmp(cmd, "*contr:buttonled 0\r")) return inst_ok;
    CHECK(0); return inst_internal_error;
}
static inst_code specbos_get_headno(specbos *p, int *pos, int lock) {
    (void)lock; *pos = !!(p->mode & inst_mode_ambient); return inst_ok;
}
static inst_code specbos_interp_code(inst *p, int code) { (void)p; return inst_wrong_config | code; }
static inst_code specbos_imp_measure_set_refresh(specbos *p) { p->rrset = 1; return inst_ok; }
static inst_code specbos_measure_set_refresh(specbos *p) {
    inst_code rv;
    amutex_lock(p->lock); rv = specbos_imp_measure_set_refresh(p); amutex_unlock(p->lock);
    return rv;
}
static inst_code trigger(void *p, inst_ui_purp why) {
    (void)p; (void)why; return abort_trigger ? inst_user_abort : inst_user_trig;
}
#include "calibration_functions.inc"
static specbos device(int model) {
    specbos p;
    memset(&p, 0, sizeof(p));
    lock_depth = lock_errors = average_count = fail_average = abort_trigger = fail_laser = 0;
    p.gotcoms = p.inited = 1; p.model = model; p.trig = inst_opt_trig_prog;
    p.mode = inst_mode_emis_tele; p.measto = 10.0; p.setnav = 1; p.lock = &lock_depth;
    return p;
}
static void run_checks(void) {
    int models[] = {1201, 1211, 1501, 1511, 2501};
    int index, user, failure, ambient;
    for (index = 0; index < 5; ++index) {
        for (user = 0; user < 2; ++user) for (failure = 0; failure < 2; ++failure) {
            specbos p = device(models[index]);
            p.noaverage = 3;
            if (user) { p.trig = inst_opt_trig_user; p.uicallback = trigger; }
            fail_average = failure;
            scenario = failure ? "read_average_unlock" : "read_success_unchanged";
            CHECK(specbos_read_sample((inst *)&p, "synthetic", NULL, 0) == (failure ? inst_coms_fail : inst_ok));
            CHECK(lock_depth == 0 && lock_errors == 0 && average_count == 1);
            /* EXPL1211AVG is enabled in the pinned source: older models map
             * requested 3 to 1, whereas 1501/1511/2501 retain the request. */
            CHECK(p.setnav == (failure || models[index] == 1201 || models[index] == 1211 ? 1 : 3));
        }
        for (ambient = 0; ambient < 2; ++ambient) for (failure = 0; failure < 4; ++failure) {
            specbos p = device(models[index]);
            inst_mode initial_mode = (ambient ? inst_mode_trans_spot_a : inst_mode_trans_spot) | inst_mode_spectral;
            inst_cal_type calt = inst_calt_trans_vwhite;
            inst_cal_cond calc = inst_calc_none;
            inst_calc_id_type idtype;
            char id[CALIDLEN];
            p.mode = initial_mode;
            scenario = "calibration_condition_unchanged";
            CHECK(specbos_calibrate((inst *)&p, &calt, &calc, &idtype, id) == inst_cal_setup);
            CHECK(calc == inst_calc_man_trans_white && average_count == 0 && !p.doing_cal);
            fail_average = failure;
            scenario = failure == 1 ? "calibration_initial_average_cleanup"
                     : failure == 2 ? "calibration_read_average_unlock"
                     : failure == 3 ? "calibration_restore_average_cleanup" : "calibration_success_unchanged";
            CHECK(specbos_calibrate((inst *)&p, &calt, &calc, &idtype, id) == (failure ? inst_coms_fail : inst_ok));
            CHECK(p.mode == initial_mode && p.doing_cal == 0);
            CHECK(lock_depth == 0 && lock_errors == 0);
            CHECK(calt == (failure ? inst_calt_trans_vwhite : 0));
            CHECK(average_count == (failure == 1 ? 1 : 3));
            /* The original driver commits the synthetic reference before the
             * final averaging reset; this repair does not change that policy. */
            CHECK(p.trans_white.spec_n == (failure == 1 || failure == 2 ? 0 : 3));
        }
        {
            specbos p = device(models[index]);
            scenario = "abort_unlock_unchanged";
            p.uicallback = trigger; abort_trigger = 1;
            CHECK(specbos_read_sample((inst *)&p, "synthetic", NULL, 0) == inst_user_abort);
            CHECK(lock_depth == 0 && !lock_errors && !average_count);
            p = device(models[index]); fail_laser = 1;
            scenario = "laser_error_unlock_unchanged";
            CHECK(specbos_read_sample((inst *)&p, "synthetic", NULL, 0) == inst_coms_fail);
            CHECK(lock_depth == 0 && !lock_errors && !average_count);
        }
    }
}
#endif

int main(void) {
    run_checks();
    printf("{\"checks\":%u,\"failures\":%u,\"failed_scenarios\":\"%s\"}\n", checks, failures, failed);
    return failures ? 1 : 0;
}
