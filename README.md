# Driver fixes proposed for ArgyllCMS 3.5.0

> **Unofficial review fork.** Selected driver repair proposals prepared by Fernando Voltolini de Azambuja on 2 and 27 October 2026. This is not an official ArgyllCMS release. Software build and regression evidence is described below; none of these proposals has been tested on instruments by the contributor.

The repository contains the complete [official ArgyllCMS 3.5.0 source](https://www.argyllcms.com/downloadsrc.html), followed by seven focused source commits:

- **Spyder 2024:** allow all seven advertised calibration slots to fit in the array. [Fix commit](https://github.com/ferazambuja/argyll-fix-proposals/commit/4f0ad6b52e0c86674bd6e2c5d5dc994fcf528229).
- **i1Pro/i1Pro2, i1Pro3 and ColorMunki spectrophotometers:** apply requested XRGA conversion or custom-filter compensation before discarding an unrequested spectrum. [Fix commit](https://github.com/ferazambuja/argyll-fix-proposals/commit/994e311e094705712eb3c02a2189b3642860d779).
- **Specbos 2501:** serialize the final button LED command with the background diffuser/laser poller, prompted by [DisplayCAL #706](https://github.com/eoyilmaz/displaycal-py3/issues/706). [Fix commit](https://github.com/ferazambuja/argyll-fix-proposals/commit/04b72b595979f07857a7498193a326d19e35a83b).
- **Spyder 2024 selection:** recompute the measurement path when reselecting cached calibrations or applying a CCMX entry. [Fix commit](https://github.com/ferazambuja/argyll-fix-proposals/commit/e586c67ac1c98ec19e577691b5a61b6a9dc5a98e).
- **Specbos error recovery:** release the mutex after failed averaging setup and restore temporary calibration mode/state on averaging errors. [Fix commit](https://github.com/ferazambuja/argyll-fix-proposals/commit/de85e7aad046a43cdee3504a37b1eda8eb77fa8f).
- **Spectral calibration state:** invalidate live dark/white data before replacement, preserve ColorMunki inner-dark completion metadata and restore the original i1Pro3 mode on the selected calibration failures. [Fix commit](https://github.com/ferazambuja/argyll-fix-proposals/commit/1c35dc47786bf4ef3fc8576b88788f126db0339d).
- **Finite spectral results:** refuse nonfinite calibrated spectra and converted readings in i1Pro/i1Pro2, i1Pro3 and ColorMunki paths, preserving signed and zero finite values. [Fix commit](https://github.com/ferazambuja/argyll-fix-proposals/commit/e46f88eaa9b41a1314e16a23f091f7aa170558fe).

The [original review note](review/DRIVER_FIXES.md) covers the first two proposals. The [Specbos note](review/SPECBOS_2501.md) explains #706. The [four-group follow-up note](review/PRIORITY_FIXES.md) explains the added failures, source changes, code fit, dependencies and validation limits. Six upstream files change; README, review documentation and regression files are added. Upstream's [ReadMe.txt](ReadMe.txt) and build recipes remain intact.

## Inspect the proposals

The untouched baseline is branch `upstream-3.5.0`, also tagged `argyll-3.5.0`. The proposals are on `codex/instrument-fix-proposals`.

```sh
git diff upstream-3.5.0..codex/instrument-fix-proposals -- spectro/spydX2.h spectro/spydX2.c spectro/i1pro_imp.c spectro/i1pro3_imp.c spectro/munki_imp.c spectro/specbos.c
```

The baseline commit is `2d0acfa9a1d1e480b6ff0043fd57c83eca951dac`. All 1,838 imported file contents match the official archive, whose SHA-256 is `f8576ce5589fd15620abb73ff049ea31f55ddbd1bba6d1ffa87452658e7bc85f`. This history begins with that import.

## Validation

Retained source-function tests demonstrate the array-capacity mismatch and conversion-order defect. The proposed code was compared with the tested code; the later changes to those excerpts are comments only. The original note distinguishes the scripted tests from actual device behavior.

The complete source diff applies and reverses exactly on official 3.5.0. Each of the four new source commits also applies independently after the previously published proposal tip, `e924058`. That existing tip includes the capacity repair, XYZ-only conversion ordering and #706 mutex fix. In particular, the finite-result proposal retains that conversion order. Earlier development-snapshot comparisons retain their original scope in the two older notes; the four new exports were validated against stable 3.5.0.

From a Git clone, run the four-group source regressions with Python 3 and a C99 compiler, followed by the #706 real-mutex test with POSIX threads:

```sh
python3 -B review/tests/driver_regressions.py
python3 -B spectro/specbos_lock_test.py
```

All current regressions pass, and their predecessor failure counts are checked. The probes execute actual functions or bounded source fragments with scripted transport/calibration helpers and reduced structs; converter tests use numerical stand-ins. Their C99 scaffolding is separate from the C89 syntax used by the production changes.

The ordinary unmodified Jam build (`jam -q -fJambase -j3`) passed on macOS arm64 with Apple Clang 21.0.0, including linked `spotread`, `chartread`, `dispread`, `dispcal`, `ccxxmake` and `illumread`. The five changed C driver files also compile as complete GNU89 objects with implicit-function and return-type errors enabled. The default build emitted 63 warning lines in unchanged upstream files, with none located in these changed drivers. See the [validation receipt](review/VALIDATION.json).

No ordinary Argyll executable was run and no device was accessed. Windows, Linux, legacy-compiler execution and physical/optical validation remain unverified. AI assisted the investigation, patch preparation, regression testing and documentation; the notes distinguish this software evidence from instrument validation and upstream acceptance.

No compiled Argyll tools are added. The official USB installer and driver files under `usb/`, and bundled ZIPs such as `doc/i1proHiRes.zip` and `jpeg/jpegsr8d.zip`, remain byte-identical to the source archive. For upstream build instructions, see [Compiling.html](doc/Compiling.html).

## Upstream and licensing

ArgyllCMS is developed by Graeme Gill. Please [support the upstream project](https://www.argyllcms.com/downloadsrc.html).

All original component notices and licenses are retained. ArgyllCMS as a whole is under the GNU Affero General Public License v3 ([License.txt](License.txt)); the instrument-driver files are under GPL-2.0-or-later ([License2.txt](License2.txt)); [License3.txt](License3.txt) is the GPL v3 text that upstream applies to `spectro/spec2cie.c` (see [ArgyllDoc.html](doc/ArgyllDoc.html)). The six modified instrument files retain their GPL-2.0-or-later notices and identify the dates and author of the proposed changes. Regression code is GPL-2.0-or-later. This README, review text and validation receipt are provided under GNU Free Documentation License 1.3, with no invariant sections or front/back cover texts; see [doc/DocLicense.txt](doc/DocLicense.txt).
