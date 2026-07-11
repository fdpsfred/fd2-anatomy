#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Inject growth_compact.json into page_template.html -> fd2_growth_tables.html."""
import json, os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
TMPL = os.path.join(HERE, "page_template.html")
DATA = os.path.join(ROOT, "workspace", "growth_table", "growth_compact.json")
# local preview copy (workspace/ is gitignored) + committed GitHub Pages copy
OUT = os.path.join(ROOT, "workspace", "growth_table", "fd2_growth_tables.html")
DOCS_OUT = os.path.join(ROOT, "docs", "character-stat-comparison", "fd2_growth_tables.html")

tmpl = open(TMPL, encoding="utf-8").read()
data = open(DATA, encoding="utf-8").read().strip()
# embed as raw JSON inside the <script type="application/json"> tag.
# guard against premature </script> (none expected in numeric data, but be safe)
data = data.replace("</", "<\\/")
html = tmpl.replace("/*__DATA__*/", data)
for out in (OUT, DOCS_OUT):
    os.makedirs(os.path.dirname(out), exist_ok=True)
    open(out, "w", encoding="utf-8").write(html)
    print("wrote %s (%d bytes)" % (out, len(html.encode("utf-8"))))
