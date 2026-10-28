#!/usr/bin/env python3
"""Check the Specbos post-read transaction with an inert polling thread.

Copyright 2026 Fernando Voltolini de Azambuja.
SPDX-License-Identifier: GPL-2.0-or-later

The frozen predecessor excerpt is from ArgyllCMS 3.5.0 specbos_read_sample,
copyright Graeme W. Gill under GPL-2.0-or-later. The current excerpt always
comes from the adjacent production specbos.c. No device transport is linked.
"""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile


# The original final transaction and returns, retained only as regression data.
PREDECESSOR = r'''
    if (p->model == 2501) {
        if((rv = specbos_command(p, "*contr:buttonled 1\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
            return rv;
        }
    }
    if (user_trig)
        return inst_user_trig;
    return rv;
}
'''

PREFIX = r'''
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
typedef int inst_code;
typedef struct { int model; pthread_mutex_t *lock; } specbos;
#define MAX_MES_SIZE 500
static int fault, exposed, bad_lock, command_calls;
static void amutex_lock(pthread_mutex_t *lock) {
    if (pthread_mutex_lock(lock) != 0) bad_lock++;
}
static void amutex_unlock(pthread_mutex_t *lock) {
    if (pthread_mutex_unlock(lock) != 0) bad_lock++;
}
static void *poller(void *arg) {
    pthread_mutex_t *lock = (pthread_mutex_t *)arg;
    int result = pthread_mutex_trylock(lock);
    if (result == 0) {
        exposed++;
        if (pthread_mutex_unlock(lock) != 0) bad_lock++;
    } else if (result != EBUSY) {
        bad_lock++;
    }
    return NULL;
}
static int specbos_command(specbos *p, char *cmd, char *buf, int size, double timeout) {
    pthread_t thread;
    int previous = exposed;
    (void)cmd; (void)buf; (void)size; (void)timeout;
    command_calls++;
    if (pthread_create(&thread, NULL, poller, p->lock) != 0) return 777;
    if (pthread_join(thread, NULL) != 0) return 777;
    return exposed != previous ? 777 : fault;
}
static int run_tail(specbos *p, int user_trig) {
    char buf[MAX_MES_SIZE];
    inst_code rv = inst_ok;
'''

SUFFIX = r'''
int main(void) {
    int model, fail, trigger, got, expected, bad = 0, cases = 0, state;
    pthread_mutex_t lock;
    specbos p;
    if (pthread_mutex_init(&lock, NULL) != 0) return 2;
    p.lock = &lock;
    for (model = 0; model < 2; model++)
        for (fail = 0; fail < 2; fail++)
            for (trigger = 0; trigger < 2; trigger++) {
                p.model = model ? 2501 : 1511;
                fault = fail ? 42 : inst_ok;
                got = run_tail(&p, trigger);
                expected = model && fault ? fault : trigger ? inst_user_trig : inst_ok;
                if (got != expected) bad++;
                state = pthread_mutex_trylock(&lock);
                if (state != 0) bad++;
                else if (pthread_mutex_unlock(&lock) != 0) bad++;
                cases++;
            }
    if (pthread_mutex_destroy(&lock) != 0) bad_lock++;
    printf("cases=%d commands=%d poller_overlap=%d wrong_result_or_lock=%d bad_lock=%d\n",
           cases, command_calls, exposed, bad, bad_lock);
    return exposed || bad || bad_lock || command_calls != 4 ? 1 : 0;
}
'''


def main():
    root = Path(__file__).resolve().parent
    source = (root / "specbos.c").read_text()
    start = source.index("static inst_code\nspecbos_read_sample(")
    end = source.index("/* Set the instrument to match the current refresh settings */", start)
    routine = source[start:end]
    current = routine[routine.rindex("\tif (p->model == 2501) {"):]
    header = (root / "inst.h").read_text()
    codes = "".join(
        "#define " + name + " " + re.search(r"\b" + name + r"\s*=\s*(0x[0-9a-fA-F]+)", header)[1] + "\n"
        for name in ("inst_ok", "inst_user_trig")
    )
    compiler = shlex.split(os.environ.get("CC", "cc"))
    with tempfile.TemporaryDirectory(prefix="specbos-lock-test-") as tmp:
        for label, tail in (("predecessor", PREDECESSOR), ("production", current)):
            cfile = Path(tmp) / (label + ".c")
            program = Path(tmp) / label
            cfile.write_text(codes + PREFIX + tail + SUFFIX)
            subprocess.run(compiler + ["-std=c89", "-Wall", "-Wextra", "-Werror",
                           "-Wno-unused-function", "-pthread", str(cfile), "-o", str(program)], check=True)
            result = subprocess.run([str(program)], capture_output=True, text=True, timeout=10)
            print(label + ": " + result.stdout.strip())
            expected = 1 if label == "predecessor" else 0
            if result.returncode != expected:
                raise SystemExit(label + " returned " + str(result.returncode) + ": " + result.stderr)
    print("PASS: predecessor exposes the command; production serializes it and preserves returns")


if __name__ == "__main__":
    main()
