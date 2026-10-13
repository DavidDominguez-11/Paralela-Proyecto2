"""Render the report with the supplied DOCX's layout as a visual reference.

The source Markdown remains authoritative. Graphviz renders the five diagrams
from their Mermaid edge lists; no execution evidence is generated here. Print
the HTML to PDF with a browser, rerender with --toc-pdf for real index page
numbers, print again until --check-toc-pdf passes, then add footer numbers with
--number-pdf.
"""

from __future__ import annotations

import argparse
import base64
import html
import re
import subprocess
from pathlib import Path

from markdown_it import MarkdownIt


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "papers" / "INFORME_FINAL.md"
OUTPUT = ROOT / "papers" / "INFORME_FINAL.html"
PDF = ROOT / "papers" / "INFORME_FINAL.pdf"


def dot_quote(value: str) -> str:
    return '"' + value.replace('\\', '\\\\').replace('"', '\\"') + '"'


def diagram_png(source: str) -> bytes:
    lines = [line.strip() for line in source.splitlines() if line.strip()]
    if lines[0].startswith("flowchart "):
        direction = "LR" if lines[0].endswith("LR") else "TB"
        nodes: dict[str, str] = {}
        edges: list[tuple[str, str, str]] = []
        part = r"([A-Za-z0-9_]+)(?:\[([^\]]+)\])?"
        for line in lines[1:]:
            match = re.fullmatch(part + r"\s*-->\s*" + part, line)
            if not match:
                raise ValueError(f"Mermaid no compatible: {line}")
            left, left_label, right, right_label = match.groups()
            nodes.setdefault(left, left_label or left)
            nodes.setdefault(right, right_label or right)
            edges.append((left, right, ""))
    elif lines[0] == "sequenceDiagram":
        direction = "LR"
        nodes = {}
        edges = []
        for line in lines[1:]:
            participant = re.fullmatch(r"participant\s+(\w+)\s+as\s+(.+)", line)
            message = re.fullmatch(r"(\w+)->>(\w+):\s*(.+)", line)
            if participant:
                nodes[participant.group(1)] = participant.group(2)
            elif message:
                edges.append(message.groups())
            elif line.startswith(("alt ", "else ", "end")):
                continue
            else:
                raise ValueError(f"Mermaid no compatible: {line}")
    else:
        raise ValueError(f"Diagrama no compatible: {lines[0]}")

    statements = [f"rankdir={direction};", "node [shape=box,style=rounded,fontname=Arial];"]
    statements += [f"{key} [label={dot_quote(label)}];" for key, label in nodes.items()]
    statements += [
        f"{left} -> {right}" + (f" [label={dot_quote(label)}]" if label else "") + ";"
        for left, right, label in edges
    ]
    dot = "digraph G {\n" + "\n".join(statements) + "\n}"
    result = subprocess.run(
        ["dot", "-Tpng", "-Gdpi=140"], input=dot.encode("utf-8"),
        capture_output=True, check=True,
    )
    return result.stdout


def render_markdown(markdown: str) -> str:
    md = MarkdownIt("default", {"html": False}).enable("table")
    original_fence = md.renderer.rules["fence"]
    original_image = md.renderer.rules["image"]

    def render_fence(tokens, index, options, env):
        token = tokens[index]
        if token.info.strip() != "mermaid":
            return original_fence(tokens, index, options, env)
        data = base64.b64encode(diagram_png(token.content)).decode("ascii")
        return f'<figure><img src="data:image/png;base64,{data}" alt="Diagrama de flujo" /></figure>'

    md.renderer.rules["fence"] = render_fence

    def render_image(tokens, index, options, env):
        image_tag = original_image(tokens, index, options, env)
        caption = html.escape(tokens[index].content)
        return image_tag + f'<br><span class="figure-caption">{caption}</span>'

    md.renderer.rules["image"] = render_image
    return md.render(markdown)


def front_and_contents(markdown: str) -> tuple[str, list[str], str]:
    front, separator, rest = markdown.partition("\n---\n")
    if not separator:
        raise ValueError("No se encontró la separación entre portada e índice")
    headings = [line.lstrip("# ") for line in front.splitlines() if line.startswith("#")]
    if len(headings) != 3:
        raise ValueError("Se esperaban institución, curso y título en la portada")
    metadata = dict(re.findall(r"^\*\*(.+?):\*\*\s*(.+)$", front, re.MULTILINE))
    required = ("Docente", "Integrantes", "Repositorio", "Última verificación")
    if any(key not in metadata for key in required):
        raise ValueError("Faltan datos de portada en el Markdown")
    names = re.split(r",\s+|\s+y\s+", metadata["Integrantes"])
    if len(names) != 3:
        raise ValueError("La portada debe contener los tres integrantes provistos")
    esc = html.escape
    repo = metadata["Repositorio"].strip("<>")
    project_title = headings[2].removeprefix("Proyecto 2: ")
    project_title = project_title[0].upper() + project_title[1:]
    cover = (
        '<section class="cover">'
        '<div class="cover-school">'
        f'<p><strong>{esc(headings[0])}</strong></p>'
        f'<p>{esc(headings[1])}</p>'
        f'<p>Docente: {esc(metadata["Docente"])}</p>'
        '</div>'
        '<div class="cover-title">'
        '<p><strong>Proyecto 2</strong></p>'
        f'<p><strong>{esc(project_title)}</strong></p>'
        '</div>'
        '<div class="cover-team"><p><strong>Integrantes</strong></p>'
        + "".join(f'<p>{esc(name)}</p>' for name in names)
        + '</div>'
        f'<p class="cover-repo">Repositorio: <a href="{esc(repo)}">{esc(repo)}</a></p>'
        f'<p class="cover-date">Última verificación: {esc(metadata["Última verificación"])}</p>'
        '</section>'
    )
    index_source, marker, content_source = rest.partition("## 1. Resumen")
    if not marker or "## Índice" not in index_source:
        raise ValueError("No se encontró el índice o el inicio del informe")
    titles = re.findall(r"^\d+\. (.+)$", index_source, re.MULTILINE)
    if len(titles) != 16:
        raise ValueError(f"Se esperaban 16 entradas de índice, llegaron {len(titles)}")
    return cover, titles, render_markdown(marker + content_source)


def page_numbers(pdf_path: Path, titles: list[str]) -> dict[str, int]:
    import pymupdf

    pdf = pymupdf.open(pdf_path)
    numbers = {}
    for number, title in enumerate(titles, 1):
        target = f"{number}. {title}"
        for index in range(2, len(pdf)):
            if target in pdf[index].get_text():
                numbers[target] = index + 1
                break
        if target not in numbers:
            raise ValueError(f"No se encontró el título en el PDF: {target}")
    pdf.close()
    return numbers


def add_page_numbers(pdf_path: Path) -> None:
    import pymupdf

    pdf = pymupdf.open(pdf_path)
    for index, page in enumerate(pdf):
        label = str(index + 1)
        width = pymupdf.get_text_length(label, fontsize=9, fontname="helv")
        page.insert_text(
            (page.rect.width / 2 - width / 2, page.rect.height - 37),
            label, fontsize=9, fontname="helv", color=(0, 0, 0),
        )
    pdf.saveIncr()
    print(f"Numeradas {len(pdf)} páginas de {pdf_path.relative_to(ROOT)}")
    pdf.close()


def check_toc(pdf_path: Path) -> None:
    _, titles, _ = front_and_contents(SOURCE.read_text(encoding="utf-8"))
    actual = page_numbers(pdf_path, titles)
    shown = re.findall(r'class="toc-page">(\d+)</span>', OUTPUT.read_text(encoding="utf-8"))
    mismatches = []
    for index, title in enumerate(titles, 1):
        label = f"{index}. {title}"
        displayed = shown[index - 1] if index <= len(shown) else "<faltante>"
        if str(actual[label]) != displayed:
            mismatches.append((label, actual[label], displayed))
    if mismatches:
        raise ValueError(f"Índice desactualizado: {mismatches}")
    print(f"Índice correcto: {len(titles)} entradas en {pdf_path.relative_to(ROOT)}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--toc-pdf", type=Path, help="PDF preliminar para calcular el índice")
    parser.add_argument("--check-toc-pdf", type=Path, help="Comprueba el índice del HTML contra el PDF")
    parser.add_argument("--number-pdf", type=Path, help="PDF final al que se añaden folios")
    args = parser.parse_args()
    if args.check_toc_pdf:
        check_toc(args.check_toc_pdf.resolve())
        return
    if args.number_pdf:
        add_page_numbers(args.number_pdf.resolve())
        return

    cover, titles, content = front_and_contents(SOURCE.read_text(encoding="utf-8"))
    annex_heading = "<h2>16. Anexo D: capturas de ejecución</h2>"
    if content.count(annex_heading) != 1:
        raise ValueError("No se encontró el Anexo D para iniciar una página nueva")
    content = content.replace(annex_heading, '<section class="evidence-annex">' + annex_heading, 1) + '</section>'
    numbers = page_numbers(args.toc_pdf.resolve(), titles) if args.toc_pdf else {}
    toc_rows = []
    for number, title in enumerate(titles, 1):
        label = f"{number}. {title}"
        toc_rows.append(
            '<div class="toc-row">'
            f'<span>{html.escape(label)}</span><span class="toc-dots"></span>'
            f'<span class="toc-page">{numbers.get(label, "00")}</span></div>'
        )
    toc = '<section class="toc"><h2>Índice</h2><div class="toc-rule"></div>' + "".join(toc_rows) + '</section>'
    css = """
        @page { size: letter; margin: 25.4mm; }
        body { font-family: Arial, sans-serif; font-size: 10pt; line-height: 1.48;
               color: #111; text-align: justify; }
        .cover { break-after: page; text-align: center; padding-top: 55pt; }
        .cover p { margin: 0 0 3pt; line-height: 1.35; }
        .cover-title { margin-top: 38pt; font-weight: bold; }
        .cover-team { margin-top: 42pt; }
        .cover-team p:first-child { margin-bottom: 10pt; }
        .cover-repo { margin-top: 34pt !important; overflow-wrap: anywhere; }
        .cover-date { margin-top: 16pt !important; font-size: 9pt; }
        .toc { break-after: page; }
        .evidence-annex { break-before: page; }
        .toc h2 { text-align: center; margin: 0 0 6pt; }
        .toc-rule { border-top: 1px solid #6e9ac8; margin-bottom: 12pt; }
        .toc-row { display: flex; align-items: baseline; margin: 0 0 11pt; }
        .toc-dots { flex: 1; border-bottom: 1px dotted #333; margin: 0 4pt 3pt; }
        .toc-page { width: 20pt; text-align: right; }
        h2 { font-size: 11pt; color: #111; margin: 16pt 0 7pt; font-weight: bold; }
        h3 { font-size: 10pt; color: #111; margin: 13pt 0 6pt; font-weight: bold; }
        h2, h3 { break-after: avoid; }
        p, li { margin: 0 0 7pt; }
        ul, ol { margin: 0 0 9pt; padding-left: 24pt; }
        code { font-family: 'Courier New', monospace; font-size: 8pt; }
        pre { white-space: pre-wrap; overflow-wrap: anywhere; font-size: 7.5pt;
              padding: 6pt; border: 1px solid #bbb; background: #f8f8f8; }
        pre code { font-size: 7.5pt; }
        table { border-collapse: collapse; width: 100%; font-size: 7.3pt;
                margin: 7pt 0 11pt; table-layout: fixed; line-height: 1.25; }
        tr { break-inside: avoid; }
        th, td { border: 1px solid #999; padding: 3pt; vertical-align: top;
                 overflow-wrap: anywhere; }
        th { background: #f0f0f0; }
        a { color: #1d3860; }
        figure { text-align: center; margin: 8pt 0; break-inside: avoid; }
        figure img { max-width: 100%; max-height: 320pt; }
        p:has(> img) { text-align: center; break-inside: avoid; }
        p > img { max-width: 100%; max-height: 165mm; }
        .figure-caption { font-size: 8pt; }
    """
    document = (
        "<!doctype html><html lang='es'><head><meta charset='utf-8'>"
        "<title>Proyecto 2 — Informe final</title><style>" + css + "</style></head><body>"
        + cover + toc + '<main>' + content + '</main></body></html>'
    )
    OUTPUT.write_text(document, encoding="utf-8")
    print(f"Generado {OUTPUT.relative_to(ROOT)} desde {SOURCE.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
