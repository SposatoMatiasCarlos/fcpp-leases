#!/usr/bin/env bash
# Builds the documentation in Italian and English.
#   build/html/it/   Italian      (Doxyfile)
#   build/html/en/   English      (Doxyfile.en)
#   build/html/index.html opens the language chosen last (English by default).
# Override the tools with DOXYGEN=... and PYTHON=... if needed.
set -e
cd "$(dirname "$0")"
DOXYGEN=${DOXYGEN:-doxygen}
PYTHON=${PYTHON:-$(command -v python3 || command -v python)}

"$PYTHON" tools/api_index.py
"$PYTHON" tools/figures.py
rm -rf build/html/it build/html/en build/html/index.html
mkdir -p build/html
"$DOXYGEN" Doxyfile
"$DOXYGEN" Doxyfile.en
"$PYTHON" tools/navtree.py build/html/it Introduzione build/html/en Introduction
cp index.html build/html/index.html
cat doxygen-warnings-it.log doxygen-warnings-en.log
echo "open build/html/index.html"
