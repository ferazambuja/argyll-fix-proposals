# Four driver repair groups for ArgyllCMS 3.5.0

Prepared 27 October 2026. These are selected repairs to Argyll's own drivers. They add no device command, acquisition mode, calibration equation, application policy or build dependency. The [original two-fix note](DRIVER_FIXES.md) and [Specbos 2501 note](SPECBOS_2501.md) remain available with their original evidence scope.

| Group | Failure | Source-only commit |
|---|---|---|
| Spyder 2024 selection | Cached reselection or CCMX selection can keep the preceding index's measurement path. | [e586c67](https://github.com/ferazambuja/argyll-fix-proposals/commit/e586c67ac1c98ec19e577691b5a61b6a9dc5a98e) |
| Specbos recovery | Failed averaging setup can retain the read mutex; calibration averaging errors retain temporary mode/state. | [de85e7a](https://github.com/ferazambuja/argyll-fix-proposals/commit/de85e7aad046a43cdee3504a37b1eda8eb77fa8f) |
| Spectral calibration state | Live replacement can overwrite dark/white data while an earlier valid flag remains set. ColorMunki inner-dark completion retains mismatched exposure metadata. | [1c35dc4](https://github.com/ferazambuja/argyll-fix-proposals/commit/1c35dc47786bf4ef3fc8576b88788f126db0339d) |
| Finite results | Calibrated spectral scaling, XYZ integration/percentage scaling or corrections can produce nonfinite values without a reading error. | [e46f88e](https://github.com/ferazambuja/argyll-fix-proposals/commit/e46f88eaa9b41a1314e16a23f091f7aa170558fe) |

Each of the original four source commits applies separately after the published `e924058` tip. That tip includes the original capacity and XYZ-order proposals and the #706 LED mutex repair. The [i1Pro3 cleanup follow-up](https://github.com/ferazambuja/argyll-fix-proposals/commit/70c6f9b38d74d9697020a594b260e7b42c38fa74) applies after the independently reviewed `a3a1292` tip. Tests and explanations remain separate from source commits. The complete six-file source diff applies to the authenticated official 3.5.0 import and reverses exactly to its original bytes.

To take only the finite-results proposal after `e924058`, use [finite-results.patch](finite-results.patch). It contains the same tested finite-result code with the dated modification notice included in each affected file. The historical `e46f88e` commit remains unchanged; its header notices originally came from the preceding calibration-state commit. The standalone export applies and reverses exactly and requires no calibration-state or cleanup follow-up.

## Spyder 2024 measurement-path selection

The original `set_disp_type()` decides `p->usehl` only while loading an inbuilt calibration for the first time. A cached entry bypasses that branch. A CCMX list entry also bypasses it while selecting its base index. If firmware capabilities permit high-level XYZ for some indices and require low-level acquisition for others, reselection can leave `usehl` describing the previous index.

The wrong-path consequence is source-traced: a high-level-only selection can later use an unloaded low-level calibration slot, or a low-level-only index can be sent through the high-level command. Whether communication succeeds and what a physical instrument returns remain untested. This is separate from filtering unsupported display types.

The change computes a local `usehl` from the existing `is2024`, `forcell`, `mxdnp1`, `dnomask` and selected index on every selection. It stores that result wherever the existing code stores the index. These capability inputs are fixed after initialization, so calibration loading can stay cached. It changes no table, command, calibration arithmetic or option.

The regression executes the actual display tables, `spydX2_getCalibration()`, `set_disp_type()` and four selector/correction functions. Both sides use the already enlarged calibration array, isolating this selection defect. Scripted USB replies exercise mixed high/low reselection, display-number limits, CCMX selection, explicit correction matrices and a failed load followed by retry. The predecessor fails seven of 151 checks; current source passes all 151. The common object base, logging, error and list-creation helpers are reduced stand-ins; these are not production ABI sizes or physical readings.

## Specbos averaging-error recovery

`specbos_read_sample()` owns `p->lock` when it calls `set_average(..., 0)`. The original error return omits the unlock. The poller and a later read can then block on the retained mutex. This is a different transaction from #706's final LED exchange.

Transmission calibration temporarily changes `p->mode` and sets `p->doing_cal`. Its initial averaging setup and final averaging-restore failures originally return without restoring those fields. The existing read-failure path already restores them.

The change adds the missing unlock and repeats the existing `cmode`/`doing_cal` cleanup on the two omitted exits. It retains every command, timeout, returned error and successful calibration step. It uses the same mutex API, braces and cleanup assignments as neighboring exits.

The regression executes the actual averaging, calibration and calibration-assessment functions plus the read prefix through averaging setup. Scripted failures and lock counters check cleanup across the covered models, modes and trigger cases. The predecessor fails 40 of 400 checks; current source passes all 400. The measurement tail is replaced by inert success; this test does not run spectral arithmetic, real mutexes or serial transport. The separate eight-case #706 regression still uses a real pthread mutex and inert polling thread and passes on the combined source.

## Spectral calibration validity and completion metadata

The i1Pro/i1Pro2 and ColorMunki calibration paths write directly into live dark buffers. Acquisition can overwrite that buffer before returning an error. An old `dark_valid` or `idark_valid` flag must therefore be cleared before replacement starts. Existing success points revalidate the completed capture.

The same principle applies to live white data and factors. This proposal clears `cal_valid` before the initial i1Pro/i1Pro2 and ColorMunki white acquisition and before i1Pro3 reflective/transmission calibration. The original proposal restored `m->mmode` on three white-calibration error exits; the cleanup follow-up now restores it on all eight early returns in the i1Pro3 per-mode loop. This follows the mode-restore pattern in the i1Pro and ColorMunki calibration routines. The i1Pro scan gain-switch paths invalidate a dark calibration whose gain has changed; this export adds no gain-switch remeasurement.

After ColorMunki's inner reflective dark capture succeeds, it now records `dark_valid`, pending-calibration state, date, integration time and gain using the same assignments as its initial-dark completion. If that capture fails, its data remain invalid. If a later white callback fails, the completed dark data retain matching metadata while white data stay invalid.

The tests execute selected actual acquisition prefixes with a helper that overwrites a live buffer before returning success or failure. Twenty i1Pro and sixteen ColorMunki cases cover initial validity and both outcomes. Each case now checks the specific validity flag owning the written buffer and requires the other flag to remain unchanged. Five temporary wrong-flag/both-flags mutations were rejected. White-dispatch prefixes run with overwrite-before-error callbacks, including both i1Pro3 reflective paths and transmission. A separate four-scenario ColorMunki reflective-white tail checks completion metadata and failure after each covered capture/conversion callback.

All current cases pass. The predecessor has 92 failures in the white-dispatch probe, ten in the selected i1Pro dark prefixes, eight in the ColorMunki prefixes and five in the inner-dark completion probe. These counts describe assertions in bounded probes, not instrument measurements. The adaptive-scan ownership and gain-switch invalidations also rely on source inspection; their full acquisition paths are not executed here. White-factor return-value validation, high-resolution coefficient transactions and other calibration changes remain outside this selected proposal.

### i1Pro3 cleanup and integration-time follow-up

Independent review found five remaining per-mode returns that left the faked mode installed. The follow-up restores the caller mode on wavelength acquisition, wavelength matching, filter construction, adaptive-dark and integration-time probe failures. The first two exits also release their allocated wavelength vector. Two callback diagnostics now name the function that actually failed.

Tracing the integration-time return exposed a separate control-flow defect: `ev != I1PRO3_RD_SENSORSATURATED` returned early on success, leaking the returned raw buffer and leaving selection incomplete; saturation fell through to a completed-selection flag. The follow-up clears prior selection validity before the probe, returns errors/saturation, and completes successful averaging, exposure selection and raw-buffer release. A nonpositive maximum uses the same floor as existing adaptive exposure selection. The simple-emission helper now releases a saturated emission buffer with `i1p3mm_em`, matching its actual `-1` allocation origin rather than the reflective `-9` origin.

Sixteen source-bound exit cases check every per-mode early return in current and non-current modes, including wavelength release. The reviewed `a3a1292` source fails seven cases; current source passes all sixteen. Another sixteen scenarios execute the complete actual calibration, need-assessment, simple-emission and raw-release functions with real headers and inert acquisition/allocation helpers. They cover successful exposure reduction, minimum/no-increase bounds, zero/negative maxima, saturation, transport errors, prior-valid refreshes, all-modes restoration, deferred requests and unchanged adaptive/scan exclusions. The reviewed source fails eleven scenarios; current source passes all sixteen. These are software state/ownership checks, not physical exposure or optical validation. The probe suppresses the unchanged upstream header's `id[100]` versus `CALIDLEN` array-parameter warning; both declarations decay to the same pointer type.

## Nonfinite spectral/XYZ results

Finite operands can overflow during calibration multiplication, reflectance percentage scaling, XYZ integration or correction. The affected original callers can still return success. Checking an input calibration factor alone does not establish that its product is finite.

The change checks:

- Both calibrated-output callers in i1Pro/i1Pro2 and ColorMunki, after their existing spectral scaling and before publication.
- The i1Pro3 flash spectrum after calibration multiplication, with existing buffer release on refusal.
- All three shared `*_conv2XYZ()` boundaries: before CIE integration, after integration/percentage scaling and after XRGA/custom-filter correction. Converter objects are released on the new early exits. The post-correction check precedes the existing clearing of unrequested spectra.

The drivers return their native `*_RD_READINCONS` errors. Signed finite values and finite zero readings stay allowed; the patch does not add an optical acceptance threshold or claim to prevent a later tool's zero-white normalization failure. It is not presented as a confirmed fix for DisplayCAL #626.

For portability, each file has a private `*_double_finite()` helper using `value >= -DBL_MAX && value <= DBL_MAX`. The existing `numsup.h` provides `DBL_MAX` in the ordinary and standalone header paths. This needs no C99 `isfinite()` or platform-specific API. The shared `isFinite` macro is unsuitable for these guards because it rejects negative and zero values as well as nonfinite values. Existing Argyll functions, structs, errors, allocation ownership and correction interfaces stay in place; this proposal does not import a separate allocation-error propagation change.

Each family's converter probe runs twenty cases. They include first/second-row nonfinite input, reflectance overflow, integration/XYZ overflow and injected nonfinite corrections, alongside finite native, XYZ-only, reflective, signed, zero and two-row controls. Each predecessor fails twelve cases; all current cases pass and the valid-control outputs match exactly. Integration and correction helpers are bounded numerical stand-ins; these results do not validate CIE calculations or real correction files.

The actual i1Pro/i1Pro2 and ColorMunki scalers plus their callers' publication tails each pass ten cases; each predecessor fails the four overflow cases. Five source-bound i1Pro3 flash-scaling cases check refusal before copying and exactly one release, with signed/zero controls; its predecessor fails two cases. Earlier processing, full flash acquisition and physical timing are outside these probes.

## Ordinary Argyll build and platform scope

The exact combined production source is commit `70c6f9b38d74d9697020a594b260e7b42c38fa74`. The [validation receipt](VALIDATION.json) binds all six modified source/header files by SHA-256.

On macOS arm64 with Apple Clang 21.0.0, the ordinary default build passed:

```sh
jam -q -fJambase -j3
```

The build used the unchanged `Jambase`, `Jamtop`, `Jamfile` and subdirectory build files. It linked ordinary `spotread`, `chartread`, `dispread`, `dispcal`, `ccxxmake` and `illumread` as arm64 executables, together with the default build's other targets. No source substitutions, application workers or custom build recipe were used. The build emitted 63 warning lines in unchanged upstream files, including deprecated macOS interfaces, upstream pragma messages and linker/configuration warnings. None was located in the five changed C driver files.

Those five drivers also separately compiled as complete objects using `-std=gnu89`, `-Werror=implicit-function-declaration` and `-Werror=return-type`, with ordinary project headers. The corrected i1Pro3 object was recompiled; the other four source files are byte-identical to the earlier checked files and reuse that object-check evidence. The ordinary Jam build was freshly repeated on the complete combined source. This supports the changed code's dialect/interface fit; it is not strict-C89 qualification of the complete upstream tree or a test with an old Visual C++ compiler.

From a Git clone containing the published predecessor, reproduce the focused checks with Python 3 and a C99 compiler, then the POSIX-thread LED check:

```sh
python3 -B review/tests/driver_regressions.py
python3 -B spectro/specbos_lock_test.py
```

The test scaffold uses C99 separately from the C89 production changes. It reads actual current source and headers and retrieves predecessor bytes from Git. Compiler products and generated extracts live in temporary directories and are removed. These scripts execute only inert probes. No ordinary Argyll tool, discovery, calibration or measurement was run. The temporary ordinary build was also removed after recording its result.

The source changes use Argyll's own cross-platform driver interfaces and contain no new OS branches. Windows/Linux compilation, execution, legacy compiler behavior, real transport, optical accuracy and hardware validation remain unverified. A macOS build cannot establish those results. Source review, ordinary linking and bounded regression evidence are the basis for offering these proposals to the maintainer.

I used AI tools for investigation, source adaptation, test preparation, execution and documentation. These proposals remain subject to maintainer review and acceptance.

## Attribution and terms

Modified source and excerpts retain Graeme Gill's GPL-2.0-or-later notices. New regression code is GPL-2.0-or-later and carries Fernando Voltolini de Azambuja's notice. This review text and its validation receipt are provided under GNU Free Documentation License 1.3, with no invariant sections or front/back cover texts; see [doc/DocLicense.txt](../doc/DocLicense.txt).
