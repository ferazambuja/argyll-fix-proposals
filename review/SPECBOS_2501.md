# Proposed Specbos 2501 post-read serial-lock fix

Prepared 27 October 2026. This follow-up concerns [DisplayCAL #706](https://github.com/eoyilmaz/displaycal-py3/issues/706). The original two proposals remain documented separately in [DRIVER_FIXES.md](DRIVER_FIXES.md).

[Source-only commit: 04b72b5](https://github.com/ferazambuja/argyll-fix-proposals/commit/04b72b595979f07857a7498193a326d19e35a83b) changes only `spectro/specbos.c`. The regression test and explanation are separate from that commit. The source patch can be applied independently to the official ArgyllCMS 3.5.0 import, `2d0acfa9a1d1e480b6ff0043fd57c83eca951dac`.

## Failure mechanism

After a successful reading, `specbos_read_sample()` releases `p->lock` before doing the optional transmission conversion. Its final model-2501 `*contr:buttonled 1` command then uses the serial connection without reacquiring that mutex.

The background diffuser/laser thread uses the same connection under `p->lock`. That lock protects only exchanges whose callers acquire it. The command helper explicitly documents that it is not thread safe, so the unlocked LED exchange can overlap a polling exchange and consume or combine the wrong replies.

Original 3.5.0 source:

- [Command helper and thread-safety contract, lines 196–208](https://github.com/ferazambuja/argyll-fix-proposals/blob/2d0acfa9a1d1e480b6ff0043fd57c83eca951dac/spectro/specbos.c#L196-L208).
- [Diffuser/laser polling under the mutex, lines 526–546](https://github.com/ferazambuja/argyll-fix-proposals/blob/2d0acfa9a1d1e480b6ff0043fd57c83eca951dac/spectro/specbos.c#L526-L546).
- [Reading releases the mutex, lines 1458–1464](https://github.com/ferazambuja/argyll-fix-proposals/blob/2d0acfa9a1d1e480b6ff0043fd57c83eca951dac/spectro/specbos.c#L1458-L1464).
- [Final LED command and returns, lines 1571–1581](https://github.com/ferazambuja/argyll-fix-proposals/blob/2d0acfa9a1d1e480b6ff0043fd57c83eca951dac/spectro/specbos.c#L1571-L1581).

The [reporter's interaction log](https://github.com/user-attachments/files/27589780/interact.log) is consistent with this race. Lines 1963–2016 interleave `*contr:buttonled 1` and `*contr:laser?`, with the LED exchange receiving the acknowledgement and laser-state response together. Lines 2041–2052 show the polling thread timing out. Lines 3042–3089 show another LED request timing out while another read consumes its acknowledgement.

This establishes a missing transaction lock in the source and a mechanism consistent with the reported log. It does not establish that every failure reported in #706 has this cause.

## Proposed change and code fit

The final command becomes:

```c
	if (p->model == 2501) {
		amutex_lock(p->lock);
		rv = specbos_command(p, "*contr:buttonled 1\r", buf, MAX_MES_SIZE, 1.0);
		amutex_unlock(p->lock);
		if (rv != inst_ok)
			return rv;
	}
```

This follows the existing lock/call/unlock pattern in [`specbos_measure_set_refresh()`, lines 1759–1771](https://github.com/ferazambuja/argyll-fix-proposals/blob/2d0acfa9a1d1e480b6ff0043fd57c83eca951dac/spectro/specbos.c#L1759-L1771). It reuses `p->lock`, the existing `rv` declaration, tab indentation and the driver's brace style. It adds no helper, interface or declaration. The mutex is released before the result is checked, on both command success and error.

Reacquiring the lock here keeps the existing transmission conversion outside this critical section. Only the final serial transaction gains protection. The command, buffer, timeout, measurement arithmetic, error result and user-trigger return stay the same. The dated modification notice follows the convention used by the original proposals in this repository. These comparisons describe fit with the existing code; maintainer acceptance remains open.

## Reproducible software check

From the repository root:

```sh
python3 -B spectro/specbos_lock_test.py
```

The [regression test](../spectro/specbos_lock_test.py) requires Python 3 and a C compiler with POSIX threads. It extracts the final command/return boundary from the adjacent production `specbos.c` and reads the actual `inst_ok` and `inst_user_trig` values from `inst.h`. A frozen 3.5.0 predecessor fragment is retained as regression data.

During a mocked command, an inert polling thread attempts to acquire a real pthread mutex. Eight cases cover model 2501 and a different-model control, command success/error, and user/programmatic trigger returns. The harness checks polling overlap, returned values, mutex release and command count. It links no device transport and runs no ordinary Argyll executable.

Result on macOS arm64 with Apple Clang 21.0.0:

```text
predecessor: cases=8 commands=4 poller_overlap=4 wrong_result_or_lock=4 bad_lock=0
production: cases=8 commands=4 poller_overlap=0 wrong_result_or_lock=0 bad_lock=0
PASS: predecessor exposes the command; production serializes it and preserves returns
```

The predecessor's expected failure is part of the test. The harness builds with `-std=c89 -Wall -Wextra -Werror -Wno-unused-function -pthread`. The complete changed driver also compiles to an object using Apple Clang 21.0.0, `-std=gnu89 -DUNIX -DENABLE_FAST_SERIAL`, the selected macOS SDK and the official project headers. Temporary compiler products were removed.

These checks prove the tested transaction boundary and its return behavior. They do not exercise the complete reading function, serial transport, device timing or optical accuracy. No physical Specbos, Windows execution, complete-tool link or release qualification was performed.

## Version and DisplayCAL context

The same unprotected final LED exchange was found in the inspected public `Argyll_V3.5.2_beta1` development source. The archive SHA-256 was `faa56dbae9b5bfe219549621a561c635e4f720a138b98be70257479d8fed80b7`. This is a source observation about that archived snapshot; it does not describe later unpublished development.

DisplayCAL already has a retry workaround in [PR #733](https://github.com/eoyilmaz/displaycal-py3/pull/733). This proposal addresses the driver transaction that the application cannot serialize itself. It can be shared as an informational follow-up on closed #706; no new DisplayCAL source change or issue reopening is proposed here.

I used AI tools to investigate the source, prepare the patch and regression test, and draft and review this explanation. Software checks are distinguished above from instrument validation. Publishing this proposal is not an upstream acceptance claim.

## Attribution and terms

The source excerpts and modified driver derive from Graeme Gill's ArgyllCMS source and retain the GPL-2.0-or-later notices. The regression test is GPL-2.0-or-later and identifies the predecessor excerpt's origin. This review text is provided under GNU Free Documentation License 1.3, with no invariant sections or front/back cover texts; see [doc/DocLicense.txt](../doc/DocLicense.txt).
