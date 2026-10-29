# Driver fixes proposed for ArgyllCMS 3.5.0

> **Unofficial review fork.** Selected driver repair proposals prepared by Fernando Voltolini de Azambuja on 2, 27 and 28 October 2026. This is not an official ArgyllCMS release. Software build and regression evidence is described below; none of these proposals has been tested on instruments by the contributor.

The repository contains the complete [official ArgyllCMS 3.5.0 source](https://www.argyllcms.com/downloadsrc.html), followed by nine focused source commits:

- **Spyder 2024:** allow all seven advertised calibration slots to fit in the array. [Fix commit](https://github.com/ferazambuja/argyll-fix-proposals/commit/4f0ad6b52e0c86674bd6e2c5d5dc994fcf528229).
- **i1Pro/i1Pro2, i1Pro3 and ColorMunki spectrophotometers:** apply requested XRGA conversion or custom-filter compensation before discarding an unrequested spectrum. [Fix commit](https://github.com/ferazambuja/argyll-fix-proposals/commit/994e311e094705712eb3c02a2189b3642860d779).
- **Specbos 2501:** serialize the final button LED command with the background diffuser/laser poller, prompted by [DisplayCAL #706](https://github.com/eoyilmaz/displaycal-py3/issues/706). [Fix commit](https://github.com/ferazambuja/argyll-fix-proposals/commit/04b72b595979f07857a7498193a326d19e35a83b).
- **Spyder 2024 selection:** recompute the measurement path when reselecting cached calibrations or applying a CCMX entry. [Fix commit](https://github.com/ferazambuja/argyll-fix-proposals/commit/e586c67ac1c98ec19e577691b5a61b6a9dc5a98e).
- **Specbos error recovery:** release the mutex after failed averaging setup and restore temporary calibration mode/state on averaging errors. [Fix commit](https://github.com/ferazambuja/argyll-fix-proposals/commit/de85e7aad046a43cdee3504a37b1eda8eb77fa8f).
- **Spectral calibration state:** invalidate live dark/white data before replacement, preserve ColorMunki inner-dark completion metadata and restore the original i1Pro3 mode on the selected calibration failures. [Fix commit](https://github.com/ferazambuja/argyll-fix-proposals/commit/1c35dc47786bf4ef3fc8576b88788f126db0339d).
- **Finite spectral results:** refuse nonfinite calibrated spectra and converted readings in i1Pro/i1Pro2, i1Pro3 and ColorMunki paths, preserving signed and zero finite values. [Fix commit](https://github.com/ferazambuja/argyll-fix-proposals/commit/e46f88eaa9b41a1314e16a23f091f7aa170558fe).
- **i1Pro3 calibration follow-up:** complete mode/error cleanup, release failed wavelength captures, and repair integration-time selection and saturated emission-buffer release. [Fix commit](https://github.com/ferazambuja/argyll-fix-proposals/commit/70c6f9b38d74d9697020a594b260e7b42c38fa74).
- **ColorMunki white-calibration retry:** repeat an invalid dark capture before reflective-white exposure selection, including retries retaining only the white request. [Fix commit](https://github.com/ferazambuja/argyll-fix-proposals/commit/834f8d43cc4002849526103567a2235ade373c51).

The [original review note](review/DRIVER_FIXES.md) covers the first two proposals. The [Specbos note](review/SPECBOS_2501.md) explains #706. The [four-group follow-up note](review/PRIORITY_FIXES.md) explains the added failures, source changes, code fit, dependencies and validation limits. Six upstream files change; README, review documentation and regression files are added. Upstream's [ReadMe.txt](ReadMe.txt) and build recipes remain intact.

## Inspect the proposals

The untouched baseline is branch `upstream-3.5.0`, also tagged `argyll-3.5.0`. The proposals are on `codex/instrument-fix-proposals`.

```sh
git diff upstream-3.5.0..codex/instrument-fix-proposals -- spectro/spydX2.h spectro/spydX2.c spectro/i1pro_imp.c spectro/i1pro3_imp.c spectro/munki_imp.c spectro/specbos.c
```

The baseline commit is `2d0acfa9a1d1e480b6ff0043fd57c83eca951dac`. All 1,838 imported file contents match the official archive, whose SHA-256 is `f8576ce5589fd15620abb73ff049ea31f55ddbd1bba6d1ffa87452658e7bc85f`. This history begins with that import.

## Validation

The original review note retains its source-function evidence for the array-capacity mismatch and conversion-order defect. Later changes have their own combined-source regressions below. Scripted tests remain distinct from actual device behavior.

The complete source diff applies and reverses exactly on official 3.5.0. Each of the original four follow-up source commits also applies independently after the published proposal tip `e924058`. That tip includes the capacity repair, XYZ-only conversion ordering and #706 mutex fix. The i1Pro3 cleanup follow-up applies after the reviewed `a3a1292` tip. In particular, the finite-result proposal retains the earlier conversion order. When taking that proposal alone, use the [standalone finite-results patch](review/finite-results.patch), which includes its own dated modification notices. Earlier development-snapshot comparisons retain their original scope in the two older notes; the new exports were validated against stable 3.5.0.

For a quick review, [download the source ZIP](https://github.com/ferazambuja/argyll-fix-proposals/archive/refs/heads/codex/instrument-fix-proposals.zip), extract it, and run this from the extracted folder:

```sh
python3 -B review/tests/driver_regressions.py
```

This checks the current source, including the ColorMunki retry and actual nonfinite-producer cases. It needs only Python 3 and a C99 compiler (`cc`, or set `CC`). No Git checkout/history, Python packages, Argyll build or instruments are needed. Temporary compiler products are removed automatically.

The separate #706 real-mutex test needs POSIX threads and also works from the ZIP:

```sh
python3 -B spectro/specbos_lock_test.py
```

Optional developer comparison: from a full Git clone, `python3 -B review/tests/driver_regressions.py --compare` also reproduces the documented predecessor failures and compares finite-control outputs across revisions. These historical comparisons are additional to the default current-source checks. Both modes pass. The probes execute actual functions or bounded source fragments with scripted transport/calibration helpers and reduced structs; converter tests use numerical stand-ins. Their C99 scaffolding is separate from the C89 syntax used by the production changes.

The 28 October additions use real headers for seven complete ColorMunki calibration scenarios and five constructed-input producer/control cases. The producer probe executes actual interpolation/custom-filter division and ColorMunki transmission white-factor/scaling arithmetic. Zero filter divisors and an all-zero white produce nonfinite spectral results in these bounded checks. This does not establish a physical-instrument trigger, complete file-to-reading behavior, or CIE correctness. See the [updated review note](review/PRIORITY_FIXES.md#nonfinite-spectralxyz-results) for the exact causes and limits.

The ordinary unmodified Jam build (`jam -q -fJambase -j3`) passed on production commit `70c6f9b` on macOS arm64 with Apple Clang 21.0.0, including linked `spotread`, `chartread`, `dispread`, `dispcal`, `ccxxmake` and `illumread`. The later ColorMunki retry change passed a fresh complete GNU89 object compile with implicit-function and return-type errors enabled and no diagnostics; the other four changed C files reuse their unchanged-source object evidence. The earlier full build emitted 63 warning lines in unchanged upstream files, with none located in these changed drivers. That full build has not been repeated after the retry follow-up. See the [validation receipt](review/VALIDATION.json).

No ordinary Argyll executable was run and no device was accessed. Windows, Linux, legacy-compiler execution and physical/optical validation remain unverified. AI assisted the investigation, patch preparation, regression testing and documentation; the notes distinguish this software evidence from instrument validation and upstream acceptance.

No compiled Argyll tools are added. The official USB installer and driver files under `usb/`, and bundled ZIPs such as `doc/i1proHiRes.zip` and `jpeg/jpegsr8d.zip`, remain byte-identical to the source archive. For upstream build instructions, see [Compiling.html](doc/Compiling.html).

## Upstream and licensing

ArgyllCMS is developed by Graeme Gill. Please [support the upstream project](https://www.argyllcms.com/downloadsrc.html).

All original component notices and licenses are retained. ArgyllCMS as a whole is under the GNU Affero General Public License v3 ([License.txt](License.txt)); the instrument-driver files are under GPL-2.0-or-later ([License2.txt](License2.txt)); [License3.txt](License3.txt) is the GPL v3 text that upstream applies to `spectro/spec2cie.c` (see [ArgyllDoc.html](doc/ArgyllDoc.html)). The six modified instrument files retain their GPL-2.0-or-later notices and identify the dates and author of the proposed changes. Regression code is GPL-2.0-or-later. This README, review text and validation receipt are provided under GNU Free Documentation License 1.3, with no invariant sections or front/back cover texts; see [doc/DocLicense.txt](doc/DocLicense.txt).
