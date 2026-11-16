#!/usr/bin/env python3
"""Compile story_src.py -> ../story.json and ../neon_ash.html, with validation.

Usage:  python3 build.py
"""
import json
import os
import re
import sys
from collections import deque

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.dirname(HERE)
sys.path.insert(0, HERE)

import story_src  # noqa: E402

STATS = ["body", "reflex", "tech", "mind", "nerve"]
TERM_RE = re.compile(r"^([a-z_][a-z0-9_]*)\s*(>=|<=|==|!=|>|<)\s*(-?\d+)$")
FX_VAR_RE = re.compile(r"^([a-z_][a-z0-9_]*)([+\-=])(-?\d+)$")
FX_FLAG_RE = re.compile(r"^([+\-])([a-z_][a-z0-9_]*)$")

errors = []


def check_cond(cond, where):
    if not cond:
        return
    for part in cond.split("|"):
        for term in part.split("&"):
            term = term.strip()
            if not term:
                errors.append("%s: empty term in %r" % (where, cond))
            elif term.startswith("!"):
                if not re.match(r"^[a-z_][a-z0-9_]*$", term[1:]):
                    errors.append("%s: bad term %r" % (where, term))
            elif not TERM_RE.match(term) and not re.match(r"^[a-z_][a-z0-9_]*$", term):
                errors.append("%s: bad term %r" % (where, term))


def check_fx(fx, where):
    for tok in fx.split():
        if not (FX_VAR_RE.match(tok) or FX_FLAG_RE.match(tok)):
            errors.append("%s: bad effect %r" % (where, tok))


def auto_tag(need):
    tags = []
    for stat, n in re.findall(r"\b(%s)\s*>=\s*(\d+)" % "|".join(STATS), need):
        tags.append("[%s %s]" % (stat.upper(), n))
    for n in re.findall(r"\bmoney\s*>=\s*(\d+)", need):
        tags.append("[¢%s]" % format(int(n), ","))
    return " ".join(dict.fromkeys(tags))


def compile_story():
    nodes = story_src.NODES
    out = {}
    for nid, n in nodes.items():
        choices = []
        for i, c in enumerate(n["choices"]):
            need, fx, hideif = c["need"], c["fx"], c["hideif"]
            if c["once"]:
                key = "o_%s_%d" % (nid, i)
                hideif = (hideif + " | " if hideif else "") + key
                fx = (fx + " " if fx else "") + "+" + key
            tag = c["tag"] if c["tag"] is not None else auto_tag(need)
            ch = {"t": c["t"], "to": c["to"]}
            if need: ch["need"] = need
            if fx: ch["fx"] = fx
            if c["hide"]: ch["hide"] = True
            if hideif: ch["hideif"] = hideif
            if tag: ch["tag"] = tag
            choices.append(ch)
        o = {}
        if n["text"]: o["text"] = n["text"]
        if n["fx"]: o["fx"] = n["fx"]
        if n["route"]: o["route"] = n["route"]
        if n["extra"]: o["extra"] = n["extra"]
        if choices: o["choices"] = choices
        if "ending" in n: o["ending"] = n["ending"]
        out[nid] = o
    return out


def validate(nodes):
    for nid, n in nodes.items():
        check_fx(n.get("fx", ""), nid)
        for cond, tgt in n.get("route", []):
            check_cond(cond, nid + " route")
            if tgt not in nodes:
                errors.append("%s: route to missing node %r" % (nid, tgt))
        for cond, _ in n.get("extra", []):
            check_cond(cond, nid + " extra")
        for c in n.get("choices", []):
            w = "%s -> %s" % (nid, c["to"])
            if c["to"] not in nodes:
                errors.append("%s: missing target" % w)
            check_cond(c.get("need", ""), w)
            check_cond(c.get("hideif", ""), w)
            check_fx(c.get("fx", ""), w)
        terminal = "ending" in n
        if not terminal and not n.get("choices") and not n.get("route"):
            errors.append("%s: dead end (no choices, route, or ending)" % nid)
        if n.get("route") and not n.get("choices") and not any(c == "" for c, _ in n["route"]):
            errors.append("%s: route-only node without a fallback route" % nid)

    # graph reachability (ignoring conditions)
    seen, q = {"start"}, deque(["start"])
    while q:
        n = nodes[q.popleft()]
        nxt = [c["to"] for c in n.get("choices", [])] + [t for _, t in n.get("route", [])]
        for t in nxt:
            if t in nodes and t not in seen:
                seen.add(t)
                q.append(t)
    for nid in nodes:
        if nid not in seen:
            errors.append("%s: unreachable from start" % nid)


def main():
    nodes = compile_story()
    validate(nodes)
    if errors:
        print("BUILD FAILED:")
        for e in errors:
            print("  -", e)
        sys.exit(1)

    story = {
        "title": "NEON ASH",
        "version": 1,
        "start": "start",
        "max_burn": 12,
        "endings": story_src.ENDINGS,
        "nodes": nodes,
    }
    js = json.dumps(story, ensure_ascii=False, separators=(",", ":"))
    with open(os.path.join(OUT, "story.json"), "w", encoding="utf-8") as f:
        f.write(js)

    with open(os.path.join(HERE, "template.html"), encoding="utf-8") as f:
        tpl = f.read()
    html = tpl.replace("/*__STORY__*/null", js.replace("</", "<\\/"))
    with open(os.path.join(OUT, "neon_ash.html"), "w", encoding="utf-8") as f:
        f.write(html)

    words = sum(len((n.get("text", "") + " ".join(x for _, x in n.get("extra", []))).split())
                for n in nodes.values())
    print("OK: %d nodes, %d endings, ~%d words" % (len(nodes), len(story_src.ENDINGS), words))
    for fn in ("story.json", "neon_ash.html"):
        print("  %-14s %7.1f KB" % (fn, os.path.getsize(os.path.join(OUT, fn)) / 1024))


if __name__ == "__main__":
    main()
