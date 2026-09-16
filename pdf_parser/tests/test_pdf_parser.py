"""Tests for page-spec parsing and lossless PDF page export."""

from __future__ import annotations

from io import BytesIO
from pathlib import Path

import pytest
from pypdf import PdfReader, PdfWriter

from core import PdfParserError, extract_pages, extract_text, inspect_pdf, parse_page_spec


def _write_sample_pdf(path: Path, page_count: int, title: str = "Sample") -> Path:
    writer = PdfWriter()
    writer.add_metadata({"/Title": title, "/Author": "pdf_parser tests"})
    for index in range(page_count):
        writer.add_blank_page(width=200, height=280)
        writer.pages[-1].compress_content_streams()
        # Attach a unique text annotation stream via a content overlay is heavy;
        # page identity is asserted by count/order instead.
        _ = index
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as handle:
        writer.write(handle)
    return path


def test_parse_page_spec_mix():
    assert parse_page_spec("1,3,5-7", 10) == [0, 2, 4, 5, 6]


def test_parse_page_spec_open_ranges():
    assert parse_page_spec("8-", 10) == [7, 8, 9]
    assert parse_page_spec("-3", 10) == [0, 1, 2]


def test_parse_page_spec_odd_even_and_dedupe():
    assert parse_page_spec("odd", 5) == [0, 2, 4]
    assert parse_page_spec("even", 5) == [1, 3]
    assert parse_page_spec("1,1,2,1-2", 5) == [0, 1]


def test_parse_page_spec_rejects_bad_values():
    with pytest.raises(PdfParserError):
        parse_page_spec("0", 5)
    with pytest.raises(PdfParserError):
        parse_page_spec("6", 5)
    with pytest.raises(PdfParserError):
        parse_page_spec("abc", 5)
    with pytest.raises(PdfParserError):
        parse_page_spec("5-1", 5)


def test_extract_pages_writes_selected_count(tmp_path: Path):
    source = _write_sample_pdf(tmp_path / "src.pdf", 6, title="P555 Datasheet")
    dest = tmp_path / "out.pdf"
    extract_pages(source, dest, "1,3-4,6")
    out = PdfReader(str(dest))
    assert len(out.pages) == 4
    info = inspect_pdf(source)
    assert info.page_count == 6
    assert info.title == "P555 Datasheet"


def test_extract_pages_accepts_zero_based_indices(tmp_path: Path):
    source = _write_sample_pdf(tmp_path / "src.pdf", 3)
    dest = tmp_path / "out.pdf"
    extract_pages(source, dest, [0, 2])
    assert len(PdfReader(str(dest)).pages) == 2


def test_extract_text_runs_on_blank_pages(tmp_path: Path):
    source = _write_sample_pdf(tmp_path / "src.pdf", 2)
    blocks = extract_text(source, "1-2")
    assert [page for page, _ in blocks] == [1, 2]


def test_cli_export(tmp_path: Path, monkeypatch):
    import cli

    source = _write_sample_pdf(tmp_path / "src.pdf", 4)
    dest = tmp_path / "picked.pdf"
    code = cli.main(["export", str(source), "-p", "2-3", "-o", str(dest)])
    assert code == 0
    assert dest.is_file()
    assert len(PdfReader(str(dest)).pages) == 2


def test_app_export_endpoint(tmp_path: Path):
    from app import create_app

    source = _write_sample_pdf(tmp_path / "src.pdf", 5, title="Web")
    client = create_app().test_client()
    with source.open("rb") as handle:
        info = client.post("/api/info", data={"pdf": (handle, "src.pdf")})
    payload = info.get_json()
    assert payload["ok"] is True
    assert payload["pageCount"] == 5
    assert payload["title"] == "Web"

    with source.open("rb") as handle:
        exported = client.post(
            "/api/export",
            data={"pdf": (handle, "src.pdf"), "pages": "1,5"},
        )
    assert exported.status_code == 200
    reader = PdfReader(BytesIO(exported.data))
    assert len(reader.pages) == 2

    health = client.get("/health")
    assert health.status_code == 200
    assert health.get_json()["ok"] is True
