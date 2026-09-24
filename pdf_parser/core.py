"""PDF inspection, text extraction, and lossless page export."""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Sequence

from pypdf import PdfReader, PdfWriter
from pypdf.generic import Destination, IndirectObject

PageIndex = int  # 0-based


class PdfParserError(ValueError):
    """Invalid PDF path, page specification, or document state."""


@dataclass(frozen=True)
class Bookmark:
    title: str
    page: int | None  # 1-based page number when resolvable
    children: tuple["Bookmark", ...] = ()


@dataclass(frozen=True)
class PdfInfo:
    path: Path
    page_count: int
    title: str | None = None
    author: str | None = None
    subject: str | None = None
    creator: str | None = None
    producer: str | None = None
    encrypted: bool = False
    bookmarks: tuple[Bookmark, ...] = field(default_factory=tuple)


_RANGE_TOKEN = re.compile(
    r"^\s*(?P<start>\d+)?\s*(?P<sep>-)\s*(?P<end>\d+)?\s*$|^\s*(?P<single>\d+)\s*$"
)


def parse_page_spec(spec: str, page_count: int) -> list[int]:
    """Parse a 1-based page spec into unique 0-based indices, preserving order.

    Supported forms (comma / whitespace separated):
      1,3,5-8     explicit pages and inclusive ranges
      10-         from page 10 through the last page
      -4          from page 1 through page 4
      odd / even  odd or even 1-based page numbers
    """
    if page_count < 1:
        raise PdfParserError("PDF has no pages")
    if spec is None or not str(spec).strip():
        raise PdfParserError("page specification is empty")

    tokens = [token for token in re.split(r"[,\s]+", spec.strip()) if token]
    selected: list[int] = []
    seen: set[int] = set()

    def add_one_based(page: int) -> None:
        if page < 1 or page > page_count:
            raise PdfParserError(
                f"page {page} is out of range (document has {page_count} page(s))"
            )
        index = page - 1
        if index not in seen:
            seen.add(index)
            selected.append(index)

    for token in tokens:
        lowered = token.lower()
        if lowered == "odd":
            for page in range(1, page_count + 1, 2):
                add_one_based(page)
            continue
        if lowered == "even":
            for page in range(2, page_count + 1, 2):
                add_one_based(page)
            continue

        match = _RANGE_TOKEN.match(token)
        if not match:
            raise PdfParserError(f"invalid page token: {token!r}")

        if match.group("single"):
            add_one_based(int(match.group("single")))
            continue

        start_raw = match.group("start")
        end_raw = match.group("end")
        start = int(start_raw) if start_raw else 1
        end = int(end_raw) if end_raw else page_count
        if start > end:
            raise PdfParserError(f"invalid page range {token!r}: start > end")
        for page in range(start, end + 1):
            add_one_based(page)

    if not selected:
        raise PdfParserError("page specification selected no pages")
    return selected


def _open_reader(pdf_path: str | Path, password: str | None = None) -> PdfReader:
    path = Path(pdf_path)
    if not path.is_file():
        raise PdfParserError(f"file not found: {path}")
    try:
        reader = PdfReader(str(path))
    except Exception as exc:  # noqa: BLE001 - surface pypdf failures cleanly
        raise PdfParserError(f"failed to open PDF: {exc}") from exc

    if reader.is_encrypted:
        if password is None:
            raise PdfParserError("PDF is encrypted; pass --password to decrypt")
        try:
            unlocked = reader.decrypt(password)
        except Exception as exc:  # noqa: BLE001
            raise PdfParserError(f"failed to decrypt PDF: {exc}") from exc
        if unlocked == 0:
            raise PdfParserError("incorrect PDF password")
    return reader


def _destination_page(reader: PdfReader, destination: object) -> int | None:
    try:
        if isinstance(destination, Destination):
            return reader.get_destination_page_number(destination) + 1
        page = reader.get_destination_page_number(destination)  # type: ignore[arg-type]
        return page + 1
    except Exception:  # noqa: BLE001
        return None


def _outline_to_bookmarks(reader: PdfReader, outline: object) -> tuple[Bookmark, ...]:
    if not outline:
        return ()

    items: list[Bookmark] = []
    if not isinstance(outline, Sequence) or isinstance(outline, (str, bytes)):
        return ()

    for node in outline:
        if isinstance(node, list):
            children = _outline_to_bookmarks(reader, node)
            if items:
                last = items[-1]
                items[-1] = Bookmark(last.title, last.page, children)
            else:
                items.extend(children)
            continue

        dest = node
        if isinstance(node, IndirectObject):
            dest = node.get_object()

        title = ""
        page: int | None = None
        if isinstance(dest, Destination):
            title = str(dest.title or "")
            page = _destination_page(reader, dest)
        elif hasattr(dest, "title"):
            title = str(getattr(dest, "title") or "")
            page = _destination_page(reader, dest)
        else:
            title = str(dest)

        items.append(Bookmark(title=title, page=page, children=()))

    return tuple(items)


def inspect_pdf(pdf_path: str | Path, password: str | None = None) -> PdfInfo:
    path = Path(pdf_path)
    reader = _open_reader(path, password)
    metadata = reader.metadata
    bookmarks = _outline_to_bookmarks(reader, reader.outline)

    def meta(name: str) -> str | None:
        if metadata is None:
            return None
        value = metadata.get(name) if hasattr(metadata, "get") else getattr(metadata, name, None)
        if value is None:
            return None
        text = str(value).strip()
        return text or None

    return PdfInfo(
        path=path,
        page_count=len(reader.pages),
        title=meta("/Title"),
        author=meta("/Author"),
        subject=meta("/Subject"),
        creator=meta("/Creator"),
        producer=meta("/Producer"),
        encrypted=reader.is_encrypted,
        bookmarks=bookmarks,
    )


def extract_pages(
    source: str | Path,
    destination: str | Path,
    pages: Sequence[int] | str,
    password: str | None = None,
) -> Path:
    """Copy the given pages (0-based indices or a page spec) into a new PDF."""
    reader = _open_reader(source, password)
    page_count = len(reader.pages)
    indices: Sequence[int]
    if isinstance(pages, str):
        indices = parse_page_spec(pages, page_count)
    else:
        indices = list(pages)
        for index in indices:
            if index < 0 or index >= page_count:
                raise PdfParserError(
                    f"page index {index} is out of range (0..{page_count - 1})"
                )
        if not indices:
            raise PdfParserError("no pages selected")

    writer = PdfWriter()
    for index in indices:
        writer.add_page(reader.pages[index])

    dest_path = Path(destination)
    dest_path.parent.mkdir(parents=True, exist_ok=True)
    with dest_path.open("wb") as handle:
        writer.write(handle)
    return dest_path


def extract_text(
    source: str | Path,
    pages: Sequence[int] | str | None = None,
    password: str | None = None,
) -> list[tuple[int, str]]:
    """Return (1-based page number, text) pairs for the selected pages."""
    reader = _open_reader(source, password)
    page_count = len(reader.pages)
    if pages is None:
        indices = list(range(page_count))
    elif isinstance(pages, str):
        indices = parse_page_spec(pages, page_count)
    else:
        indices = list(pages)

    result: list[tuple[int, str]] = []
    for index in indices:
        if index < 0 or index >= page_count:
            raise PdfParserError(
                f"page index {index} is out of range (0..{page_count - 1})"
            )
        text = reader.pages[index].extract_text() or ""
        result.append((index + 1, text))
    return result


def flatten_bookmarks(bookmarks: Iterable[Bookmark], depth: int = 0) -> list[tuple[int, Bookmark]]:
    rows: list[tuple[int, Bookmark]] = []
    for bookmark in bookmarks:
        rows.append((depth, bookmark))
        if bookmark.children:
            rows.extend(flatten_bookmarks(bookmark.children, depth + 1))
    return rows
