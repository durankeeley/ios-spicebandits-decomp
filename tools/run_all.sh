#!/usr/bin/env bash
# Rebuild analysis output + docs for the decrypted build.   usage: tools/run_all.sh
set -euo pipefail
cd "$(dirname "$0")/.."
[ -d .venv ] || { python3 -m venv .venv && .venv/bin/pip -q install capstone lief unicorn; }
tools/extract.sh
tag=std; bin="work/$tag/Payload/Tower.app/Tower"
.venv/bin/python tools/find_functions.py "$bin" work/$tag/functions.json
.venv/bin/python tools/exports.py        "$bin" work/$tag/exports.json
.venv/bin/python tools/objc_methods.py "$bin" work/$tag/objc_methods.json
.venv/bin/python tools/rtti.py           "$bin" work/$tag/classes.json
.venv/bin/python tools/gen_docs.py       $tag "$bin"
.venv/bin/python tools/gen_imports.py    $tag
