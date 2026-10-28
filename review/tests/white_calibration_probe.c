/* Copyright 2026 Fernando Voltolini de Azambuja.
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Production source extracts retain Graeme Gill's original notices. */
/* Exact white-calibration dispatch prefixes from
 * ArgyllCMS 3.5.0. Helpers are scripted; no transport or cache is linked.
 * Prefixes end at the driver's first successful validity assignment. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include "codes.inc"
#define a1logd(...) ((void)0)
#define RDEAD_TIME 0.001
typedef struct {
 int cal_valid, dark_valid, gainmode, want_calib, want_dcalib, reflective, trans, emiss, scan, adaptive;
 double inttime, iinttime, wcaltime, dcaltime, targoscale, dark_int_time, reftemp;
 int dark_gain_mode; time_t cdate, ddate, iddate;
 double *white_data, *dark_data, *iwhite_data, *cal_factor1, *cal_factor2;
} state;
typedef struct {
 int mmode, filt, transwarn;
 double min_int_time, max_int_time, optsval, minsval, maxsval;
 double ledpreheattime, ledwaittime, calscantime, *white_ref1, *white_ref2;
 state s;
} meter;
typedef struct {meter *m; void *log;} device;
static int calls, fail_at, failures, checks, script;
static state *active;
static int step(void) {
 int fail = calls++ == fail_at;
 /* Helpers may overwrite live white data even when they return failure. */
 active->white_data[0] = 99.0;
 if(active->cal_valid) {failures++; puts("FAIL helper entered with stale validity");}
 return fail ? 123 : 0;
}
static int i1pro3_pol_refl_cal(device *p) {(void)p; return step();}
static int i1pro3_refl_cal(device *p) {(void)p; return step();}
static int i1pro3_trans_cal(device *p) {(void)p; return step();}
static void munki_heatLED(device *p,double t) {(void)p;(void)t;}
static void msec_sleep(int t) {(void)t;}
static int munki_comp_nummeas(device *p,double t,double it) {(void)p;(void)t;(void)it;return 2;}
static int munki_interp_dark(device *p,double *d,double it,int g) {
 (void)p;(void)d;(void)it;(void)g;return step();
}
static int munki_whitemeasure(device *p,double *w,double *scale,int n,double *it,int g,double t) {
 int ev; (void)p;(void)w;(void)n;(void)it;(void)g;(void)t;
 ev=step(); if(ev) return ev;
 *scale=script==1?100.0:1.0;
 return script==2?MUNKI_RD_SENSORSATURATED:MUNKI_OK;
}
static int munki_optimise_sensor(device *p,double *it,int *g,double old,int og,int tr,
 int clip,double *target,double scale,double dead) {
 (void)p;(void)g;(void)og;(void)tr;(void)clip;(void)target;(void)scale;(void)dead;
 *it=old; return step();
}
static int munki_dark_measure(device *p,double *d,int n,double *it,int g) {
 (void)p;(void)d;(void)n;(void)it;(void)g;return step();
}
static int munki_ledtemp_whitemeasure(device *p,double *w,double *iw,double *rt,
 int n,double it,int g) {
 (void)p;(void)w;(void)iw;(void)rt;(void)n;(void)it;(void)g;return step();
}
static int munki_compute_wav_whitemeas(device *p,double *a,double *b,double *w) {
 (void)p;(void)a;(void)b;(void)w;return step();
}
static int munki_compute_white_cal(device *p,double *a,double *b,double *c,
 double *d,double *e,double *f) {
 (void)p;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
 return 0;
}
static int reflective(device *p,inst_cal_type *calt,inst_cal_cond *calc) {
 meter *m=p->m; state *s=&m->s; int ev=0,mmode=9; time_t cdate=1000;
#include "reflective.inc"
 return ev;
}
static int transmission(device *p,inst_cal_type *calt,inst_cal_cond *calc) {
 meter *m=p->m; state *s=&m->s; int ev=0,mmode=9; time_t cdate=1000;
#include "transmission.inc"
 return ev;
}
static int munki_white(device *p,inst_cal_type *calt,inst_cal_cond *calc) {
 meter *m=p->m; state *s=&m->s; int ev=0,mmode=9,nummeas; time_t cdate=1000;
#include "munki.inc"
 return ev;
}
static void check(int ok,const char *name) {
 checks++; if(!ok) {failures++;printf("FAIL %s\n",name);}
}
static int white_case(int cfg,int fault,int which_script) {
 meter m; device p={&m,0}; state *s=&m.s; double w=7.0;
 inst_cal_type ct; inst_cal_cond cc; int ev;
 memset(&m,0,sizeof m); m.mmode=0;m.min_int_time=0.004;
 m.optsval=1;m.minsval=0.25;m.maxsval=4;
 s->cal_valid=1;s->inttime=s->iinttime=0.1;s->white_data=&w;
 s->reflective=cfg<3; s->trans=cfg>=3;s->adaptive=1;
 m.filt=cfg==1?inst_opt_filter_pol:inst_opt_filter_none;
 ct=s->reflective?inst_calt_ref_white:inst_calt_trans_vwhite;
 cc=cfg<2?inst_calc_man_ref_white:cfg==2?inst_calc_man_cal_smode:inst_calc_man_trans_white;
 active=s;calls=0;fail_at=fault;script=which_script;
 ev=cfg<2?reflective(&p,&ct,&cc):cfg==3?transmission(&p,&ct,&cc):munki_white(&p,&ct,&cc);
 if(ev) {
  check(!s->cal_valid,"refused white calibration invalid");
  check(m.mmode==9,"failure restores mode");
 } else check(s->cal_valid==1,"successful white calibration valid");
 return calls;
}
int main(void) {
 int cfg,k,n;
 for(cfg=0;cfg<5;cfg++) {
  n=white_case(cfg,-1,0);
  for(k=0;k<n;k++) white_case(cfg,k,0);
  if(cfg==2 || cfg==4) {white_case(cfg,-1,1);white_case(cfg,-1,2);}
 }
 printf("white-calibration: %d checks, %d failures\n",checks,failures);
 return failures!=0;
}
