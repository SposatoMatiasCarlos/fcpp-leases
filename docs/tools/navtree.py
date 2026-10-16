"""Reshapes the side bar of the generated documentation (navtreedata.js and navtreeindex*.js).

Doxygen puts the sections of the main page at the top level of the side bar, as if they
were pages, and adds the namespace, class and file indexes, which are already in the tabs
of the banner. This script groups the sections of the main page under one entry and drops
the three indexes; the tree paths in the index files (used to highlight the open page) are
remapped accordingly.

Usage: python tools/navtree.py build/html/it Introduzione build/html/en Introduction
"""
import glob
import json
import os
import re
import sys

DROPPED = {'namespaces.html', 'annotated.html', 'files.html'}


def reshape(folder, intro_title):
    path = os.path.join(folder, 'navtreedata.js')
    text = open(path, encoding='utf-8').read()
    m = re.search(r'(var NAVTREE =\s*)(\[.*?\])(;\s*\n\s*var NAVTREEINDEX)', text, re.S)
    tree = json.loads(m.group(2))
    children = tree[0][2]

    # old top-level index -> new path below the root (None: dropped)
    moves = {}
    intro, rest = [], []
    for i, c in enumerate(children):
        if c[1].startswith('index.html#'):
            moves[i] = [0, len(intro)]          # below the new first entry
            intro.append(c)
        elif c[1] in DROPPED:
            moves[i] = None
        else:
            rest.append((i, c))
    new_children = [[intro_title, 'index.html', intro]] if intro else []
    for i, c in rest:
        moves[i] = [len(new_children)]
        new_children.append(c)
    tree[0][2] = new_children
    text = text[:m.start(2)] + json.dumps(tree, ensure_ascii=False, indent=1) + text[m.end(2):]
    open(path, 'w', encoding='utf-8', newline='\n').write(text)

    for index in glob.glob(os.path.join(folder, 'navtreeindex*.js')):
        s = open(index, encoding='utf-8').read()
        n = re.search(r'(var NAVTREEINDEX\d+ =\s*)(\{.*\})(;?\s*)$', s, re.S)
        if not n:
            continue
        entries = json.loads(n.group(2))
        # a path starts from the top-level entry (the root is implicit)
        for url, p in list(entries.items()):
            if not p or p[0] not in moves:
                continue
            target = moves[p[0]]
            if target is None:
                del entries[url]
            else:
                entries[url] = target + p[1:]
        s = s[:n.start(2)] + json.dumps(entries, ensure_ascii=False, indent=0) + s[n.end(2):]
        open(index, 'w', encoding='utf-8', newline='\n').write(s)


if __name__ == '__main__':
    args = sys.argv[1:]
    for folder, title in zip(args[0::2], args[1::2]):
        reshape(folder, title)
