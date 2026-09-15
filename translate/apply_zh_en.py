#!/usr/bin/env python3
"""Apply paragraph-level ZH->EN translations to a copy of the Chinese survey."""
from __future__ import annotations

import json
import shutil
from pathlib import Path
import zipfile
from lxml import etree

SRC = Path("/workspace/translate/Vendor_Survey_Laboratory_Cerba_Research_Chinese.docx")
DST = Path(
    "/workspace/translate/Vendor_Survey_Laboratory_Cerba_Research_Chinese_EN.docx"
)
MAP_PATH = Path("/workspace/translate/zh_en_map.json")
W = "{http://schemas.openxmlformats.org/wordprocessingml/2006/main}"
XML_SPACE = "{http://www.w3.org/XML/1998/namespace}space"


def has_cjk(s: str) -> bool:
    return any("\u4e00" <= ch <= "\u9fff" for ch in s)


def para_text(p) -> str:
    return "".join(t.text or "" for t in p.iter(f"{W}t"))


def replace_para(p, new_text: str) -> None:
    t_els = list(p.iter(f"{W}t"))
    if not t_els:
        return
    t_els[0].text = new_text
    t_els[0].set(XML_SPACE, "preserve")
    for t in t_els[1:]:
        t.text = ""


def translate_xml(data: bytes, mapping: dict[str, str], leftover: list[str]) -> bytes:
    root = etree.fromstring(data)
    for p in root.iter(f"{W}p"):
        src = para_text(p)
        if not src.strip() or not has_cjk(src):
            continue
        if src in mapping:
            replace_para(p, mapping[src])
            continue
        stripped = src.strip()
        if stripped in mapping:
            # keep original leading/trailing whitespace pattern if possible
            lead = src[: len(src) - len(src.lstrip())]
            trail = src[len(src.rstrip()) :]
            replace_para(p, lead + mapping[stripped] + trail)
            continue
        leftover.append(src)
    return etree.tostring(root, xml_declaration=True, encoding="UTF-8", standalone=True)


def main() -> None:
    mapping = json.loads(MAP_PATH.read_text(encoding="utf-8"))
    # header/footer extras
    mapping.setdefault(
        "供应商调查——Cerba Research 实验室",
        "Vendor Survey — Cerba Research Laboratory",
    )
    mapping.setdefault(
        "VV-QUAL-00886，第4.0版，第18页（共18页）",
        "VV-QUAL-00886, Version 4.0, Page 18 of 18",
    )
    mapping.setdefault(
        "VV-QUAL-00886, version 4.0Page 1 of 1 VV-QUAL-00886，第4.0版，第18页（共18页）VV-QUAL-00886，第4.0版，第18页（共18页）",
        "VV-QUAL-00886, Version 4.0, Page 18 of 18",
    )

    leftover: list[str] = []
    shutil.copyfile(SRC, DST)
    buf = {}
    with zipfile.ZipFile(SRC) as zin:
        for name in zin.namelist():
            data = zin.read(name)
            if name.endswith(".xml") and (
                name.startswith("word/document")
                or name.startswith("word/header")
                or name.startswith("word/footer")
            ):
                data = translate_xml(data, mapping, leftover)
            buf[name] = data
    with zipfile.ZipFile(DST, "w", compression=zipfile.ZIP_DEFLATED) as zout:
        for name, data in buf.items():
            zout.writestr(name, data)

    Path("/workspace/translate/leftover_cjk.txt").write_text(
        "\n---\n".join(leftover), encoding="utf-8"
    )
    print("saved", DST)
    print("leftover CJK paragraphs:", len(leftover))
    for s in leftover[:30]:
        print(" ", repr(s[:160]))


if __name__ == "__main__":
    main()
