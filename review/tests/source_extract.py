"""Extract actual Argyll functions and declarations for inert regression probes."""
# Copyright 2026 Fernando Voltolini de Azambuja.
# SPDX-License-Identifier: GPL-2.0-or-later
from pathlib import Path
import re
SOURCE_PATH="spectro/spydX2.h"
DRIVER_PATH="spectro/spydX2.c"

def _between(text: str, start: str, end: str) -> str:
    """The exact text from `start` through the first `end` after it."""
    if text.count(start) != 1:
        raise ValueError(f"Missing or ambiguous pinned extract: {start.strip()}")
    begin = text.index(start)
    finish = text.index(end, begin) + len(end)
    return text[begin:finish] + "\n"

def _function(text: str, name: str) -> str:
    """One function definition, with a return type on its own preceding line."""
    matches = []
    for match in re.finditer(r"^(?:[A-Za-z_][\w \t]*[ \t*])?" + name + r"\s*\(", text, re.M):
        head = text[match.end():text.index("{", match.end())]
        if ";" not in head:
            matches.append(match)
    if len(matches) != 1:
        raise ValueError(f"Missing or ambiguous pinned function: {name}")
    start = matches[0].start()
    previous = text.rfind("\n", 0, start - 1) + 1
    if re.fullmatch(r"(?:static )?[A-Za-z_]\w*[ \t]*\n", text[previous:start]):
        start = previous
    opening = text.index("{", matches[0].end())
    depth = 0
    for token in re.finditer(
        r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', text[opening:]
    ):
        if token[0] == "{":
            depth += 1
        elif token[0] == "}":
            depth -= 1
            if depth == 0:
                return text[start:opening + token.end()] + "\n"
    raise ValueError(f"Missing pinned function end: {name}")

def _enum(text: str, name: str) -> str:
    matches = [m[0] for m in re.finditer(r"typedef enum\s*\{[^}]*\}\s*\w+\s*;", text)
               if re.search(r"\}\s*" + name + r"\s*;", m[0])]
    if len(matches) != 1:
        raise ValueError(f"Missing or ambiguous pinned enum: {name}")
    return matches[0] + "\n"

def _define(text: str, name: str) -> str:
    matches = re.findall(r"^#define\s+" + name + r"\b[^\n]*", text, re.M)
    if len(matches) != 1:
        raise ValueError(f"Missing or ambiguous pinned constant: {name}")
    return matches[0] + "\n"

def _notice(name: str, text: str) -> str:
    prelude = text[:text.index("#include")] if "#include" in text else text[:text.index("typedef")]
    return (f"\n/* Extracted from {name}; its notice follows. */\n"
            + "\n".join(re.findall(r"/\*[\s\S]*?\*/", prelude)) + "\n")

def _extract(header: str, sources: dict[str, str]) -> tuple[str, str]:
    """Declarations and functions, each copied exactly from the pinned archive."""
    inst, driver = sources["spectro/inst.h"], sources[DRIVER_PATH]
    declarations = _notice(SOURCE_PATH, header)
    for name in ("spectro/inst.h", "spectro/disptechs.h", "numlib/numsup.c", "icc/icc_util.c"):
        declarations += _notice(name, sources[name])
    for name in ("inst_code", "inst_mode", "inst_opt_type", "inst_dtflags"):
        declarations += _enum(inst, name)
    declarations += _enum(sources["spectro/disptechs.h"], "disptech")
    for name in ("INST_DTYPE_SEL_LEN", "INST_DTYPE_DESC_LEN"):
        declarations += _define(inst, name)
    declarations += _between(inst, "typedef struct _inst_disptypesel {", "} inst_disptypesel;")
    declarations += _define(sources["spectro/icoms.h"], "ICOM_OK")
    for name in ("SPYDX2_OK", "SPYDX2_CIX_MISMATCH"):
        declarations += _define(header, name)
    # The object as the variant declares it: the calibration array is the defect.
    declarations += _between(header, "/* Extra native calibration info (X2 only) */",
                             "}; typedef struct _spydX2 spydX2;")
    numsup, icc = sources["numlib/numsup.c"], sources["icc/icc_util.c"]
    functions = "".join(_function(numsup, name) for name in (
        "IEEE754todouble", "doubletoIEEE754", "read_ORD8", "read_ORD16_be", "read_ORD32_le",
        "write_ORD8", "write_ORD16_be", "write_ORD32_le"))
    functions += _function(icc, "icmSetUnity3x3") + _function(icc, "icmCpy3x3")
    for table in ("spydX2_disptypesel", "spyd2024_disptypesel"):
        functions += _between(driver, f"static inst_disptypesel {table}[", "\n};")
    functions += _function(driver, "spydX2_getCalibration") + _function(driver, "set_disp_type")
    return declarations, functions

def _block(text: str, marker: str) -> str:
    """Exact balanced statement following a unique marker, ignoring C literals."""
    if text.count(marker) != 1:
        raise ValueError(f"Ambiguous resync probe marker: {marker}")
    start = text.index(marker)
    opening = text.index("{", start)
    depth = 0
    for token in re.finditer(
        r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]',
        text[opening:],
    ):
        if token[0] == "{":
            depth += 1
        elif token[0] == "}":
            depth -= 1
            if depth == 0:
                return text[start : opening + token.end()] + "\n"
    raise ValueError(f"Unclosed resync probe block: {marker}")
