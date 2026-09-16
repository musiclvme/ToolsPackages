"""Local web UI for choosing PDF pages and exporting a new document."""

from __future__ import annotations

import io
import tempfile
from pathlib import Path

from flask import Flask, jsonify, render_template, request, send_file

try:
    from .core import PdfParserError, extract_pages, flatten_bookmarks, inspect_pdf
except ImportError:  # python app.py from this directory
    from core import PdfParserError, extract_pages, flatten_bookmarks, inspect_pdf

ROOT = Path(__file__).resolve().parent


def create_app() -> Flask:
    app = Flask(
        __name__,
        template_folder=str(ROOT / "templates"),
        static_folder=str(ROOT / "static"),
    )
    app.config["MAX_CONTENT_LENGTH"] = 80 * 1024 * 1024

    @app.get("/")
    def index():
        return render_template("index.html")

    @app.get("/health")
    def health():
        return jsonify({"ok": True})

    @app.post("/api/info")
    def api_info():
        upload = _require_pdf()
        with tempfile.NamedTemporaryFile(suffix=".pdf", delete=False) as tmp:
            tmp.write(upload.read())
            tmp_path = Path(tmp.name)
        try:
            info = inspect_pdf(tmp_path, request.form.get("password") or None)
        except PdfParserError as exc:
            return jsonify({"ok": False, "error": str(exc)}), 400
        finally:
            tmp_path.unlink(missing_ok=True)

        bookmarks = [
            {
                "depth": depth,
                "title": bookmark.title,
                "page": bookmark.page,
            }
            for depth, bookmark in flatten_bookmarks(info.bookmarks)
        ]
        return jsonify(
            {
                "ok": True,
                "pageCount": info.page_count,
                "title": info.title,
                "author": info.author,
                "subject": info.subject,
                "encrypted": info.encrypted,
                "bookmarks": bookmarks,
            }
        )

    @app.post("/api/export")
    def api_export():
        upload = _require_pdf()
        pages = (request.form.get("pages") or "").strip()
        if not pages:
            return jsonify({"ok": False, "error": "pages is required"}), 400

        with tempfile.NamedTemporaryFile(suffix=".pdf", delete=False) as src:
            src.write(upload.read())
            src_path = Path(src.name)
        dest_path = src_path.with_name(src_path.stem + ".export.pdf")
        try:
            extract_pages(
                src_path,
                dest_path,
                pages,
                request.form.get("password") or None,
            )
            data = dest_path.read_bytes()
        except PdfParserError as exc:
            return jsonify({"ok": False, "error": str(exc)}), 400
        finally:
            src_path.unlink(missing_ok=True)
            dest_path.unlink(missing_ok=True)

        original = Path(upload.filename or "document.pdf").stem or "document"
        download_name = f"{original}.pages.pdf"
        return send_file(
            io.BytesIO(data),
            mimetype="application/pdf",
            as_attachment=True,
            download_name=download_name,
        )

    def _require_pdf():
        upload = request.files.get("pdf")
        if upload is None or not upload.filename:
            raise PdfParserError("please choose a PDF file")
        name = upload.filename.lower()
        if not name.endswith(".pdf"):
            raise PdfParserError("only .pdf files are supported")
        return upload

    @app.errorhandler(PdfParserError)
    def handle_parser_error(exc: PdfParserError):
        return jsonify({"ok": False, "error": str(exc)}), 400

    return app


if __name__ == "__main__":
    create_app().run(host="127.0.0.1", port=5055, debug=True)
