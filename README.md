# Driver fixes proposed for ArgyllCMS 3.5.0

> **Unofficial review fork.** Proposed fixes for Spyder 2024 calibration-array capacity, XYZ-only conversion ordering and Specbos 2501 serial locking. Prepared by Fernando Voltolini de Azambuja: the original two proposals on 2 October 2026, followed by the Specbos proposal on 27 October 2026. This is not an official ArgyllCMS release. None of these proposals has been tested on instruments by the contributor.

The repository contains the complete [official ArgyllCMS 3.5.0 source](https://www.argyllcms.com/downloadsrc.html), followed by three independent source commits:

- **Spyder 2024:** allow all seven advertised calibration slots to fit in the array. [Fix commit](https://github.com/ferazambuja/argyll-fix-proposals/commit/4f0ad6b52e0c86674bd6e2c5d5dc994fcf528229).
- **i1Pro/i1Pro2, i1Pro3 and ColorMunki spectrophotometers:** apply requested XRGA conversion or custom-filter compensation before discarding an unrequested spectrum. [Fix commit](https://github.com/ferazambuja/argyll-fix-proposals/commit/994e311e094705712eb3c02a2189b3642860d779).
- **Specbos 2501:** serialize the final button LED command with the background diffuser/laser poller, prompted by [DisplayCAL #706](https://github.com/eoyilmaz/displaycal-py3/issues/706). [Fix commit](https://github.com/ferazambuja/argyll-fix-proposals/commit/04b72b595979f07857a7498193a326d19e35a83b).

The [original review note](review/DRIVER_FIXES.md) covers the first two proposals, including their source links, software evidence, code organization and cache consequence. The separate [Specbos review note](review/SPECBOS_2501.md) explains #706, the mutex invariant, the fit with existing code and the reproducible regression. Five upstream files change. The added files are this README, the two review notes and `spectro/specbos_lock_test.py`. Upstream's [ReadMe.txt](ReadMe.txt) remains intact.

## Inspect the proposals

The untouched baseline is branch `upstream-3.5.0`, also tagged `argyll-3.5.0`. The proposals are on `codex/instrument-fix-proposals`.

```sh
git diff upstream-3.5.0..codex/instrument-fix-proposals -- spectro/spydX2.h spectro/i1pro_imp.c spectro/i1pro3_imp.c spectro/munki_imp.c spectro/specbos.c
```

The baseline commit is `2d0acfa9a1d1e480b6ff0043fd57c83eca951dac`. All 1,838 imported file contents match the official archive, whose SHA-256 is `f8576ce5589fd15620abb73ff049ea31f55ddbd1bba6d1ffa87452658e7bc85f`. This history begins with that import.

## Validation

Retained source-function tests demonstrate the array-capacity mismatch and conversion-order defect. The proposed code was compared with the tested code; the later changes to those excerpts are comments only. The original note distinguishes the scripted tests from actual device behavior.

All three source fixes apply together and reverse exactly on 3.5.0. For the original two fixes, the four affected files are byte-identical in the inspected `Argyll_V3.5.2_beta1` archive. The inspected development source also contains the unlocked Specbos LED exchange; see its review note for the bounded source check.

The Specbos test extracts the current production command/return boundary and uses a real mutex with an inert polling thread and mocked command results. All eight cases pass; the frozen predecessor exposes all four 2501 transactions. To run it with Python 3 and a C compiler supporting POSIX threads:

```sh
python3 -B spectro/specbos_lock_test.py
```

The complete changed `specbos.c` also compiles to an object on macOS arm64 with Apple Clang 21.0.0. This checkout has not had a complete-tool build, Windows build or legacy compiler build. No ordinary Argyll tool was executed or device accessed in preparing these proposals. AI assisted the investigation, patch preparation, regression testing and documentation; the notes describe the evidence and its limits.

No compiled Argyll tools are added. The official USB installer and driver files under `usb/`, and bundled ZIPs such as `doc/i1proHiRes.zip` and `jpeg/jpegsr8d.zip`, remain byte-identical to the source archive. For upstream build instructions, see [Compiling.html](doc/Compiling.html).

## Upstream and licensing

ArgyllCMS is developed by Graeme Gill. Please [support the upstream project](https://www.argyllcms.com/downloadsrc.html).

All original component notices and licenses are retained. ArgyllCMS as a whole is under the GNU Affero General Public License v3 ([License.txt](License.txt)); the instrument-driver files are under GPL-2.0-or-later ([License2.txt](License2.txt)); [License3.txt](License3.txt) is the GPL v3 text that upstream applies to `spectro/spec2cie.c` (see [ArgyllDoc.html](doc/ArgyllDoc.html)). The five modified instrument files retain their GPL-2.0-or-later notices and identify the date and author of the proposed changes. The Specbos regression test is GPL-2.0-or-later. This README and the review notes are provided under GNU Free Documentation License 1.3, with no invariant sections or front/back cover texts; see [doc/DocLicense.txt](doc/DocLicense.txt).
