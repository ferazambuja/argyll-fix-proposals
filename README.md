# Two driver fixes proposed for ArgyllCMS 3.5.0

> **Unofficial review fork.** Proposed fixes for Spyder 2024 calibration-array capacity and XYZ-only conversion ordering. Prepared by Fernando Voltolini de Azambuja on 2 October 2026. This is not an official ArgyllCMS release, and neither fix has been tested on instruments.

The repository contains the complete [official ArgyllCMS 3.5.0 source](https://www.argyllcms.com/downloadsrc.html), followed by two independent source commits:

- **Spyder 2024:** allow all seven advertised calibration slots to fit in the array. [Fix commit](https://github.com/ferazambuja/argyll-fix-proposals/commit/4f0ad6b52e0c86674bd6e2c5d5dc994fcf528229).
- **i1Pro/i1Pro2, i1Pro3 and ColorMunki spectrophotometers:** apply requested XRGA conversion or custom-filter compensation before discarding an unrequested spectrum. [Fix commit](https://github.com/ferazambuja/argyll-fix-proposals/commit/994e311e094705712eb3c02a2189b3642860d779).

The [review note](review/DRIVER_FIXES.md) contains original line links, checked code excerpts, the proposed changes and software evidence with its limits. It starts with an issue/solution table and a source-only comparison link, and records the code-organization review and cache consequence. Four upstream files change. The only added files are this README and the review note. Upstream's [ReadMe.txt](ReadMe.txt) remains intact.

## Inspect the proposals

The untouched baseline is branch `upstream-3.5.0`, also tagged `argyll-3.5.0`. The proposals are on `codex/instrument-fix-proposals`.

```sh
git diff upstream-3.5.0..codex/instrument-fix-proposals -- spectro/spydX2.h spectro/i1pro_imp.c spectro/i1pro3_imp.c spectro/munki_imp.c
```

The baseline commit is `2d0acfa9a1d1e480b6ff0043fd57c83eca951dac`. All 1,838 imported file contents match the official archive, whose SHA-256 is `f8576ce5589fd15620abb73ff049ea31f55ddbd1bba6d1ffa87452658e7bc85f`. This history begins with that import.

## Validation

Retained source-function tests demonstrate the array-capacity mismatch and conversion-order defect. The proposed code was compared with the tested code; the later changes to those excerpts are comments only. The note distinguishes the scripted tests from actual device behavior.

Both fixes apply together and reverse exactly on 3.5.0. The four changed files are byte-identical in the inspected `Argyll_V3.5.2_beta1` archive, so the defects and fixes are the same there. That is a source compatibility check. This isolated two-fix checkout has not had a separate complete-tool build, Windows build or legacy compiler build. No Argyll tool was executed or device accessed in preparing this checkout.

No compiled Argyll tools are added. The official USB installer and driver files under `usb/`, and bundled ZIPs such as `doc/i1proHiRes.zip` and `jpeg/jpegsr8d.zip`, remain byte-identical to the source archive. For upstream build instructions, see [Compiling.html](doc/Compiling.html).

## Upstream and licensing

ArgyllCMS is developed by Graeme Gill. Please [support the upstream project](https://www.argyllcms.com/downloadsrc.html).

All original component notices and licenses are retained. ArgyllCMS as a whole is under the GNU Affero General Public License v3 ([License.txt](License.txt)); the instrument-driver files are under GPL-2.0-or-later ([License2.txt](License2.txt)); [License3.txt](License3.txt) is the GPL v3 text that upstream applies to `spectro/spec2cie.c` (see [ArgyllDoc.html](doc/ArgyllDoc.html)). The four modified instrument files retain their GPL-2.0-or-later notices and identify the date and author of the proposed changes. This README and the review note are provided under GNU Free Documentation License 1.3, with no invariant sections or front/back cover texts; see [doc/DocLicense.txt](doc/DocLicense.txt).
