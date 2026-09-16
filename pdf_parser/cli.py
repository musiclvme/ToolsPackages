"""Command-line interface for inspecting PDFs and exporting selected pages."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

try:
    from .core import (
        PdfParserError,
        extract_pages,
        extract_text,
        flatten_bookmarks,
        inspect_pdf,
    )
except ImportError:  # python cli.py from this directory
    from core import (
        PdfParserError,
        extract_pages,
        extract_text,
        flatten_bookmarks,
        inspect_pdf,
    )


def _print_info(args: argparse.Namespace) -> int:
    info = inspect_pdf(args.pdf, args.password)
    print(f"file:       {info.path}")
    print(f"pages:      {info.page_count}")
    print(f"title:      {info.title or '-'}")
    print(f"author:     {info.author or '-'}")
    print(f"subject:    {info.subject or '-'}")
    print(f"creator:    {info.creator or '-'}")
    print(f"producer:   {info.producer or '-'}")
    print(f"encrypted:  {info.encrypted}")
    if info.bookmarks:
        print("bookmarks:")
        for depth, bookmark in flatten_bookmarks(info.bookmarks):
            page = f"p.{bookmark.page}" if bookmark.page else "p.?"
            print(f"  {'  ' * depth}- {bookmark.title} ({page})")
    return 0


def _print_bookmarks(args: argparse.Namespace) -> int:
    info = inspect_pdf(args.pdf, args.password)
    if not info.bookmarks:
        print("no bookmarks")
        return 0
    for depth, bookmark in flatten_bookmarks(info.bookmarks):
        page = str(bookmark.page) if bookmark.page else "?"
        print(f"{'  ' * depth}{page}\t{bookmark.title}")
    return 0


def _export(args: argparse.Namespace) -> int:
    dest = Path(args.output) if args.output else _default_output(args.pdf)
    result = extract_pages(args.pdf, dest, args.pages, args.password)
    info = inspect_pdf(result)
    print(f"wrote {info.page_count} page(s) to {result}")
    return 0


def _print_text(args: argparse.Namespace) -> int:
    pages = args.pages if args.pages else None
    blocks = extract_text(args.pdf, pages, args.password)
    for index, (page, text) in enumerate(blocks):
        if index:
            print("\n" + "=" * 40 + "\n")
        print(f"[page {page}]")
        print(text.rstrip() or "(no extractable text)")
    return 0


def _serve(args: argparse.Namespace) -> int:
    try:
        from .app import create_app
    except ImportError:
        from app import create_app

    app = create_app()
    app.run(host=args.host, port=args.port, debug=False)
    return 0


def _default_output(source: str) -> Path:
    path = Path(source)
    return path.with_name(f"{path.stem}.pages{path.suffix}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Inspect a PDF and export selected pages into a new file.",
    )
    parser.add_argument(
        "--password",
        default=None,
        help="password for encrypted PDFs",
    )
    sub = parser.add_subparsers(dest="command", required=True)

    info = sub.add_parser("info", help="show page count, metadata, and bookmarks")
    info.add_argument("pdf", help="source PDF path")
    info.set_defaults(func=_print_info)

    bookmarks = sub.add_parser("bookmarks", help="list outline/bookmarks with page numbers")
    bookmarks.add_argument("pdf", help="source PDF path")
    bookmarks.set_defaults(func=_print_bookmarks)

    export = sub.add_parser("export", help="copy specified pages into a new PDF")
    export.add_argument("pdf", help="source PDF path")
    export.add_argument(
        "-p",
        "--pages",
        required=True,
        help='page spec, e.g. "1-5,8,10" or "odd" / "even"',
    )
    export.add_argument(
        "-o",
        "--output",
        default=None,
        help="output PDF path (default: <name>.pages.pdf)",
    )
    export.set_defaults(func=_export)

    text = sub.add_parser("text", help="extract text from selected pages")
    text.add_argument("pdf", help="source PDF path")
    text.add_argument(
        "-p",
        "--pages",
        default=None,
        help="page spec; omit to extract every page",
    )
    text.set_defaults(func=_print_text)

    serve = sub.add_parser("serve", help="start the local web UI")
    serve.add_argument("--host", default="127.0.0.1")
    serve.add_argument("--port", type=int, default=5055)
    serve.set_defaults(func=_serve)
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return int(args.func(args))
    except PdfParserError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    sys.exit(main())
