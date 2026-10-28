/* Copyright 2026 Fernando Voltolini de Azambuja.
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Actual interpolation, custom-filter division, standard-resolution white-factor and
 * scaling functions. Inputs are constructed in memory; file loading, CIE
 * integration, acquisition and instrument reachability are not exercised.
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

static xsp2cie *unexpected_converter(void) {
    fputs("Unexpected CIE integration in producer probe\n", stderr);
    abort();
}
#define new_xsp2cie(...) unexpected_converter()
#define a1logd(...) ((void)0)
#include "functions.inc"

static int filter_case(const char *name, int zero, int between_bands) {
    xspect filter = {0};
    ipatch value = {0};
    filter.spec_n = 4;
    filter.spec_wl_short = 400;
    filter.spec_wl_long = 700;
    filter.norm = 1;
    for (int i = 0; i < 4; ++i) filter.spec[i] = zero ? 0 : 2;
    if (between_bands) filter.spec[1] = 0;
    value.mtype = inst_mrt_emission;
    value.sp.spec_n = 2;
    value.sp.spec_wl_short = 450;
    value.sp.spec_wl_long = 550;
    value.sp.norm = 1;
    value.sp.spec[0] = 1;
    value.sp.spec[1] = 0;
    ipatch_convert_custom_filter(&value, 1, &filter, 0);
    int ok = zero ? isinf(value.sp.spec[0]) && isnan(value.sp.spec[1])
                            && !munki_patch_finite(&value)
                  : munki_patch_finite(&value) && value.sp.spec[1] == 0;
    if (!zero && !between_bands) ok &= value.sp.spec[0] == 0.5;
    if (!ok) printf("FAIL %s\n", name);
    return !ok;
}
static int white_case(const char *name, int zero) {
    munkiimp m = {0};
    munki p = {0};
    double white[2] = {10, 20}, factors[2], bands[2] = {1, 0};
    double *rows[] = {bands};
    p.m = &m;
    m.nwav1 = m.nwav = 2;
    m.ms[0].cal_factor = factors;
    if (zero) white[0] = white[1] = 0;
    int rc = munki_compute_white_cal(&p, factors, NULL, white, NULL, NULL, NULL);
    munki_scale_specrd(&p, rows, 1, rows);
    int ok = rc == MUNKI_OK;
    if (zero)
        ok &= isinf(factors[0]) && isinf(factors[1]) && isinf(bands[0])
              && isnan(bands[1]) && !munki_scaled_finite(&p, rows, 1);
    else
        ok &= factors[0] == 0.1 && factors[1] == 0.05 && bands[0] == 0.1
              && bands[1] == 0 && munki_scaled_finite(&p, rows, 1);
    if (!ok) printf("FAIL %s\n", name);
    return !ok;
}
int main(void) {
    int failures = filter_case("zero_filter", 1, 0);
    failures += filter_case("positive_filter", 0, 0);
    failures += filter_case("zero_band_between_sample_wavelengths", 0, 1);
    failures += white_case("zero_transmission_white", 1);
    failures += white_case("positive_transmission_white", 0);
    printf("Finite producers: %d failures\n", failures);
    return failures != 0;
}
