#!/usr/bin/env python3
"""Render the concise report source to PDF (pip install reportlab)."""

from html import escape
from pathlib import Path
import os
import re

from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, PageBreak, Table, TableStyle,
)

HERE = Path(__file__).resolve().parent
windows_fonts = Path(os.environ.get("WINDIR", "C:/Windows")) / "Fonts"
linux_fonts = Path("/usr/share/fonts/truetype/dejavu")
font = windows_fonts / "arial.ttf"
bold = windows_fonts / "arialbd.ttf"
if not font.exists():
    font = linux_fonts / "DejaVuSans.ttf"
    bold = linux_fonts / "DejaVuSans-Bold.ttf"
pdfmetrics.registerFont(TTFont("Report", str(font)))
pdfmetrics.registerFont(TTFont("ReportBold", str(bold)))
styles = {
    "body": ParagraphStyle("body", fontName="Report", fontSize=9.4, leading=13.4,
                           spaceAfter=7, alignment=TA_LEFT),
    "h1": ParagraphStyle("h1", fontName="ReportBold", fontSize=20, leading=25,
                         spaceAfter=12, textColor=colors.HexColor("#173651")),
    "h2": ParagraphStyle("h2", fontName="ReportBold", fontSize=11.4, leading=16,
                         spaceBefore=9, spaceAfter=6, keepWithNext=True),
    "cell": ParagraphStyle("cell", fontName="Report", fontSize=8.6, leading=12),
}


def para(text, style="body"):
    return Paragraph(escape(text), styles[style])


story = []
lines = (HERE / "REPORT.md").read_text(encoding="utf-8").splitlines()
i = 0
while i < len(lines):
    line = lines[i].strip()
    if not line:
        i += 1
        continue
    if line == "---":
        story.append(PageBreak())
        i += 1
    elif line.startswith("# "):
        story.append(para(line[2:], "h1"))
        i += 1
    elif line.startswith("## "):
        story.append(para(line[3:], "h2"))
        i += 1
    elif line.startswith("|"):
        rows = []
        while i < len(lines) and lines[i].strip().startswith("|"):
            cells = [c.strip() for c in lines[i].strip().strip("|").split("|")]
            if not all(re.fullmatch(r":?-+:?", c) for c in cells):
                rows.append([para(c, "cell") for c in cells])
            i += 1
        table = Table(rows, colWidths=[170, 105, 236], repeatRows=1, hAlign="LEFT")
        table.setStyle(TableStyle([
            ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#EAF0F5")),
            ("VALIGN", (0, 0), (-1, -1), "TOP"),
            ("LINEBELOW", (0, 0), (-1, 0), 0.5, colors.HexColor("#A0AFBA")),
            ("LINEBELOW", (0, 1), (-1, -1), 0.3, colors.HexColor("#D4DDE3")),
            ("TOPPADDING", (0, 0), (-1, -1), 6),
            ("BOTTOMPADDING", (0, 0), (-1, -1), 6),
        ]))
        story.extend([table, Spacer(1, 9)])
    else:
        block = [line]
        i += 1
        while i < len(lines) and lines[i].strip() and not lines[i].startswith(("#", "|")) and lines[i].strip() != "---":
            block.append(lines[i].strip())
            i += 1
        story.append(para(" ".join(block)))


def footer(canvas, document):
    canvas.setFont("Report", 8)
    canvas.setFillColor(colors.HexColor("#526674"))
    canvas.drawString(42, 25, "xv6 Utilities | Thành viên 1 | 19/10/2026")
    canvas.drawRightString(A4[0] - 42, 25, str(document.page))


output = HERE / "Report_Member1.pdf"
document = SimpleDocTemplate(
    str(output), pagesize=A4, leftMargin=42, rightMargin=42,
    topMargin=37, bottomMargin=42, title="Báo cáo Memdump và Shell - Thành viên 1",
    author="Thành viên 1", allowSplitting=True,
)
document.build(story, onFirstPage=footer, onLaterPages=footer)
print(output)
