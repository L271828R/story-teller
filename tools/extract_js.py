#!/usr/bin/env python3
"""Extract inline <script> blocks from an HTML file and write them to a .js
file for syntax checking with `node --check`.

Any {{PLACEHOLDER}} tokens (C++ runtime substitution points) are replaced with
`null` so the extracted JS is syntactically valid even before C++ fills them in.

Usage: extract_js.py <input.html> <output.js>
"""
import re, sys

html = open(sys.argv[1], encoding='utf-8').read()

# Grab content of every inline <script> block (skip <script src=...>).
blocks = re.findall(
    r'<script(?![^>]*\bsrc\b)[^>]*>(.*?)</script>',
    html, re.DOTALL)

combined = '\n'.join(blocks)

# Replace {{PLACEHOLDER}} tokens with null so node --check sees valid JS.
combined = re.sub(r'\{\{[A-Za-z0-9_]+\}\}', 'null', combined)

open(sys.argv[2], 'w', encoding='utf-8').write(combined)
