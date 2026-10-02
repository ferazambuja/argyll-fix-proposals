# Two proposed ArgyllCMS driver fixes

Prepared 2 October 2026. All original line references below point to the unmodified **ArgyllCMS 3.5.0** import, commit `2d0acfa9a1d1e480b6ff0043fd57c83eca951dac`. The official archive SHA-256 is `f8576ce5589fd15620abb73ff049ea31f55ddbd1bba6d1ffa87452658e7bc85f`.

These proposals fix Argyll's own driver behavior. They do not change acquisition commands or conversion equations.

## Quick review

[Compare only the four changed source files](https://github.com/ferazambuja/argyll-fix-proposals/compare/2d0acfa9a1d1e480b6ff0043fd57c83eca951dac...994e311e094705712eb3c02a2189b3642860d779). The two commits are independent; the README and this note are separate from that source comparison.

| Issue | Original failure | Proposed solution |
|---|---|---|
| Spyder 2024 calibration slots | OLED/Mini-LED use indices 5/6 in an array of five records. | Allocate enough records for the larger supported model count. [Source fix](https://github.com/ferazambuja/argyll-fix-proposals/commit/4f0ad6b52e0c86674bd6e2c5d5dc994fcf528229). |
| XYZ-only conversion | Clearing the spectrum makes the existing conversion helpers skip the reading. | Run both helpers first, then clear the unrequested spectrum. [Source fix](https://github.com/ferazambuja/argyll-fix-proposals/commit/994e311e094705712eb3c02a2189b3642860d779). |

The sections below show the offending code and the solution. This is a focused proposal, not a claim that these drivers have no other defects.

## 1. Spyder 2024 calibration array is too small

### Trigger and impact

The Spyder 2024 display table advertises OLED and Mini-LED at native indices 5 and 6. The object allocates only five calibration records, whose valid indices are 0 through 4. Selecting either entry writes through the out-of-range record before choosing the high- or low-level measurement path.

This affects the Spyder 2024 family handled by `spydX2`. The five-entry Spyder X2 table does not itself exceed this array. The lower-level path performs further writes to the same out-of-range record. The exact runtime consequence depends on layout and execution; it has not been observed on an instrument.

### Original code

[spydX2.h:107–108](https://github.com/ferazambuja/argyll-fix-proposals/blob/2d0acfa9a1d1e480b6ff0043fd57c83eca951dac/spectro/spydX2.h#L107-L108) defines different counts:

```c
#define SPYDX2_NOCALIBS 5
#define SPYD2024_NOCALIBS 7
```

But [spydX2.h:137](https://github.com/ferazambuja/argyll-fix-proposals/blob/2d0acfa9a1d1e480b6ff0043fd57c83eca951dac/spectro/spydX2.h#L137) uses the smaller count:

```c
	SpX2calinfo cinfo[SPYDX2_NOCALIBS];	/* cal & meas setup info indexed by native ix */
```

The [display table, spydX2.c:1357–1374](https://github.com/ferazambuja/argyll-fix-proposals/blob/2d0acfa9a1d1e480b6ff0043fd57c83eca951dac/spectro/spydX2.c#L1357-L1374) supplies indices 5 and 6. In [set_disp_type, :1430–1431](https://github.com/ferazambuja/argyll-fix-proposals/blob/2d0acfa9a1d1e480b6ff0043fd57c83eca951dac/spectro/spydX2.c#L1430-L1431), the store occurs before measurement-path selection:

```c
		p->ix = ix;
		p->cinfo[ix].ix = ix;
```

The [low-level calibration stores, :291–343](https://github.com/ferazambuja/argyll-fix-proposals/blob/2d0acfa9a1d1e480b6ff0043fd57c83eca951dac/spectro/spydX2.c#L291-L343) use the selected record too.

### Proposed fix

[Commit 4f0ad6b](https://github.com/ferazambuja/argyll-fix-proposals/commit/4f0ad6b52e0c86674bd6e2c5d5dc994fcf528229) sizes the array for the larger of the two model counts:

```c
#define SPYDX2_MAXCALIBS (SPYD2024_NOCALIBS > SPYDX2_NOCALIBS \
                          ? SPYD2024_NOCALIBS : SPYDX2_NOCALIBS)
```

The `cinfo` array then uses `SPYDX2_MAXCALIBS`. Both advertised display types remain available. Commands, calibration arithmetic and measurement-path selection are unchanged.

### Software evidence and limits

A retained probe uses the production display tables, selector and calibration-loading function with scripted calibration replies. The original header reports five slots against seven advertised selections: two capacity assertions fail among 299 checks. To avoid executing unsafe selections, the original run skips those cases. With the larger array, the probe executes all selections and repeated selection, with 474 checks and no failures.

The two totals differ because the original run skips unsafe cases. They are not directly comparable coverage totals. The probe uses a reduced common object base and substitute logging/error helpers, so its object sizes are not production ABI measurements. The out-of-range access is established from the actual declaration and selector code; this is not evidence of a physically observed crash.

The proposed header matches that tested header apart from a shortened explanatory comment and the dated modification notice.

## 2. XYZ-only output skips requested conversions

### Trigger and impact

The `i1pro_conv2XYZ`, `i1pro3_conv2XYZ` and `munki_conv2XYZ` functions first calculate XYZ and a spectrum. When spectral output is disabled, they clear `spec_n` before calling XRGA conversion or custom-filter compensation. Both helpers skip a reading without a spectrum.

Consequently:

- An XYZ-only **reflective** reading can retain native XYZ even when conversion to another calibration standard was requested.
- An XYZ-only **emissive or ambient** reading can retain unfiltered XYZ even when custom-filter compensation was enabled.

These are conditional failures: native/same-standard requests and readings without custom-filter compensation are unaffected by this ordering. XRGA already applies only to reflective readings; this proposal leaves that policy intact.

The affected implementations cover i1Pro **and i1Pro2**, i1Pro3, and ColorMunki spectrophotometers. See [constructor dispatch, inst.c:698–704](https://github.com/ferazambuja/argyll-fix-proposals/blob/2d0acfa9a1d1e480b6ff0043fd57c83eca951dac/spectro/inst.c#L698-L704). ColorMunki Display is a different colorimeter using `i1d3` and is outside this finding.

### Original code

The same ordering occurs in all three functions:

| Driver | Clear spectrum, then call converters |
|---|---|
| i1Pro/i1Pro2 | [i1pro_imp.c:10512–10528](https://github.com/ferazambuja/argyll-fix-proposals/blob/2d0acfa9a1d1e480b6ff0043fd57c83eca951dac/spectro/i1pro_imp.c#L10512-L10528) |
| i1Pro3 | [i1pro3_imp.c:4880–4896](https://github.com/ferazambuja/argyll-fix-proposals/blob/2d0acfa9a1d1e480b6ff0043fd57c83eca951dac/spectro/i1pro3_imp.c#L4880-L4896) |
| ColorMunki | [munki_imp.c:8144–8161](https://github.com/ferazambuja/argyll-fix-proposals/blob/2d0acfa9a1d1e480b6ff0043fd57c83eca951dac/spectro/munki_imp.c#L8144-L8161) |

For example, the original i1Pro code is:

```c
		/* Don't return spectral if not asked for */
		if (!m->spec_en) {
			vals[i].sp.spec_n = 0;
		}

	}

	conv->del(conv);

	/* Apply any XRGA conversion */
	ipatch_convert_xrga(vals, nvals, xcalstd_nonpol, m->target_calstd, m->native_calstd, clamp);

	/* Apply custom filter compensation */
	if (m->custfilt_en)
		ipatch_convert_custom_filter(vals, nvals, &m->custfilt, clamp);

	return I1PRO_OK;
```

The XRGA helper skips a patch with no spectrum ([xrga.c:200–203](https://github.com/ferazambuja/argyll-fix-proposals/blob/2d0acfa9a1d1e480b6ff0043fd57c83eca951dac/spectro/xrga.c#L200-L203)):

```c
		if (vals[i].mtype != inst_mrt_reflective
		 || vals[i].sp.spec_n <= 0) {
			continue;
		}
```

Custom-filter compensation has the equivalent condition ([inst.c:2197–2202](https://github.com/ferazambuja/argyll-fix-proposals/blob/2d0acfa9a1d1e480b6ff0043fd57c83eca951dac/spectro/inst.c#L2197-L2202)). It applies to emission/ambient measurement types and also requires `spec_n > 0`.

This can affect Argyll's own output: `chartread -n -A` can label native XYZ with the requested `DEVCALSTD`. See [chartread.c:250–251](https://github.com/ferazambuja/argyll-fix-proposals/blob/2d0acfa9a1d1e480b6ff0043fd57c83eca951dac/spectro/chartread.c#L250-L251) and [the output keyword, :518](https://github.com/ferazambuja/argyll-fix-proposals/blob/2d0acfa9a1d1e480b6ff0043fd57c83eca951dac/spectro/chartread.c#L518).

### Proposed fix

[Commit 994e311](https://github.com/ferazambuja/argyll-fix-proposals/commit/994e311e094705712eb3c02a2189b3642860d779) moves spectrum clearing after both conversions in each function. The i1Pro tail becomes:

```c
	/* Apply any XRGA conversion */
	ipatch_convert_xrga(vals, nvals, xcalstd_nonpol, m->target_calstd, m->native_calstd, clamp);

	/* Apply custom filter compensation */
	if (m->custfilt_en)
		ipatch_convert_custom_filter(vals, nvals, &m->custfilt, clamp);

	/* Don't return spectral if not asked for */
	if (!m->spec_en) {
		for (i = 0; i < nvals; i++)
			vals[i].sp.spec_n = 0;
	}

```

XYZ-only output still contains no returned spectrum. The helpers can finish their existing work first. Spectral output, acquisition commands, XRGA equations and the custom-filter algorithm are unchanged.

### Software evidence and limits

Retained regressions execute the actual transfer functions and upstream XRGA/custom-filter helpers over 24 upstream TLCI ColorChecker reflectances. The XYZ integrator is a substitute linear integrator, and filter lookup uses a substitute nearest-band lookup.

| Driver | Original checks / failures | Proposed checks / failures |
|---|---:|---:|
| i1Pro | 1,393 / 120 | 1,398 / 0 |
| i1Pro3 | 779 / 72 | 782 / 0 |
| ColorMunki | 779 / 72 | 782 / 0 |

The original failures are confined to converted or filtered XYZ-only results. The tests compare the complete spectra delivered to the integrator as well as XYZ, and retain native/same-standard, spectral-output and reflective-filter-exclusion controls. Counts vary with the paths executed.

The three transfer-function bodies in this proposal were compared with those used by the retained regressions. The only differences are the shortened spectrum-clearing comments. This establishes the tested conversion ordering, not optical accuracy, numerical validation of the physical integrator, transport behavior or a real measurement.

## Fit with the existing driver structure

- **Ownership:** the Spyder capacity stays in its existing private object header. The constructor already allocates `sizeof(spydX2)` ([spydX2.c:1661](https://github.com/ferazambuja/argyll-fix-proposals/blob/2d0acfa9a1d1e480b6ff0043fd57c83eca951dac/spectro/spydX2.c#L1661)), so it allocates the enlarged object without a new allocation path. The maximum uses the two existing model-count definitions rather than changing either model's count.
- **Conversion workflow:** the XYZ fix keeps the existing conversion calls in the existing `*_conv2XYZ` functions. It adds no shared helper, mode, error code, acquisition step or build dependency. The moved clearing loop reuses the functions' existing `int i` declaration. Its syntax is C89-compatible by source inspection; this is not a new legacy-compiler test.
- **Style and review size:** the changes keep the existing prefixes, tab indentation and short upstream spectrum-clearing comment. Each defect has its own commit and can be applied independently. The original notices remain, with a dated modification notice. The review Markdown is separate from the source changes.
- **Output contract:** unrequested spectra are still cleared before the function returns. XRGA remains reflective-only, and custom-filter compensation keeps the helper's existing eligible measurement types. No new conversion policy is introduced.

### Spyder cache consequence

The larger array increases `sizeof(spydX2)` for both models using this structure. The existing black-calibration cache writes that size ([spydX2.c:1718–1723](https://github.com/ferazambuja/argyll-fix-proposals/blob/2d0acfa9a1d1e480b6ff0043fd57c83eca951dac/spectro/spydX2.c#L1718-L1723)) and rejects a different size during restore ([:1776–1784](https://github.com/ferazambuja/argyll-fix-proposals/blob/2d0acfa9a1d1e480b6ff0043fd57c83eca951dac/spectro/spydX2.c#L1776-L1784)). A cache from the smaller structure is therefore rejected, rather than reinterpreted. The cache field format is unchanged; no exception to its identification check is added.

This is a source-derived compatibility consequence, not a tested cache migration. Cache restore is enabled in this source, while mandatory black calibration is disabled ([:68–70](https://github.com/ferazambuja/argyll-fix-proposals/blob/2d0acfa9a1d1e480b6ff0043fd57c83eca951dac/spectro/spydX2.c#L68-L70)); rejection does not by itself establish that the operator will receive a new calibration prompt.

These comparisons support the changes' fit with the existing code. They do not establish Graeme Gill's personal preferences or acceptance. The maintainer can review the two source commits without taking these Markdown additions.

## Source compatibility and remaining validation

The two source commits apply to the four changed files extracted from the authenticated 3.5.0 archive and reverse to their original bytes. The resulting stable source matches this checkout.

The same source patches also apply and reverse exactly on the inspected `Argyll_V3.5.2_beta1` archive (SHA-256 `faa56dbae9b5bfe219549621a561c635e4f720a138b98be70257479d8fed80b7`). This describes that archived snapshot; it does not establish the state of later unpublished development.

This isolated two-fix checkout has not had a separate complete-tool build or a Windows/legacy compiler build. No new native test or Argyll tool was run while preparing it. Neither fix has been tested on instruments.

I used AI assistance to write this note and to draft and review the changes. The excerpts were checked against the original import, and the proposed code was bound to the retained software tests with the comment differences identified above.

## Attribution and terms

The source excerpts and modified driver code derive from Graeme Gill's ArgyllCMS source and retain the GPL-2.0-or-later notices. The review text is provided under GNU Free Documentation License 1.3, with no invariant sections or front/back cover texts; see [doc/DocLicense.txt](../doc/DocLicense.txt).
