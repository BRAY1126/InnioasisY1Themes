#!/usr/bin/env python3
"""
NEON ASH -- a text-based cyberpunk adventure.

Run:   python3 neon_ash.py            (needs story.json in the same folder)
       python3 neon_ash.py --no-color

Commands during play:
  1-9   pick a choice        s  stats        e  endings found
  save  save game           load  load game  q  quit
"""
import json
import os
import re
import shutil
import sys
import textwrap

HERE = os.path.dirname(os.path.abspath(__file__))
SAVE_FILE = os.path.join(HERE, "neon_ash_save.json")
ENDINGS_FILE = os.path.join(HERE, "neon_ash_endings.json")

TERM_RE = re.compile(r"^([a-z_][a-z0-9_]*)\s*(>=|<=|==|!=|>|<)\s*(-?\d+)$")
FX_VAR_RE = re.compile(r"^([a-z_][a-z0-9_]*)([+\-=])(-?\d+)$")
FX_FLAG_RE = re.compile(r"^([+\-])([a-z_][a-z0-9_]*)$")
CLAMP = {"body": (0, 10), "reflex": (0, 10), "tech": (0, 10), "mind": (0, 10), "nerve": (0, 10),
         "humanity": (0, 10), "heat": (0, 99), "burn": (0, 99), "money": (0, 10 ** 9)}
OPS = {">=": lambda a, b: a >= b, "<=": lambda a, b: a <= b, "==": lambda a, b: a == b,
       "!=": lambda a, b: a != b, ">": lambda a, b: a > b, "<": lambda a, b: a < b}


class Game:
    """The engine. Pure logic, no I/O, so it can be tested and simulated."""

    def __init__(self, story):
        self.story = story
        self.nodes = story["nodes"]
        self.reset("")

    def reset(self, name):
        self.name = name
        self.vars = {}
        self.flags = set()
        self.node_id = None

    # -- conditions & effects -------------------------------------------
    def cond(self, s):
        if not s:
            return True
        for part in s.split("|"):
            ok = True
            for term in part.split("&"):
                term = term.strip()
                if term.startswith("!"):
                    ok = term[1:] not in self.flags
                else:
                    m = TERM_RE.match(term)
                    if m:
                        ok = OPS[m.group(2)](self.vars.get(m.group(1), 0), int(m.group(3)))
                    else:
                        ok = term in self.flags
                if not ok:
                    break
            if ok:
                return True
        return False

    def apply(self, fx):
        for tok in (fx or "").split():
            m = FX_VAR_RE.match(tok)
            if m:
                k, op, n = m.group(1), m.group(2), int(m.group(3))
                v = self.vars.get(k, 0)
                v = n if op == "=" else v + n if op == "+" else v - n
                lo, hi = CLAMP.get(k, (-10 ** 9, 10 ** 9))
                self.vars[k] = max(lo, min(hi, v))
                continue
            m = FX_FLAG_RE.match(tok)
            if m:
                (self.flags.add if m.group(1) == "+" else self.flags.discard)(m.group(2))

    def sub(self, text):
        def rep(m):
            k = m.group(1)
            if k == "name":
                return self.name
            if k in self.vars:
                v = self.vars[k]
                return format(v, ",") if k == "money" else str(v)
            return m.group(0)
        return re.sub(r"\{([a-z_]+)\}", rep, text)

    # -- navigation ------------------------------------------------------
    def goto(self, nid, depth=0):
        if depth > 50:
            raise RuntimeError("route loop at " + nid)
        node = self.nodes[nid]
        self.node_id = nid
        self.apply(node.get("fx"))
        for cond, target in node.get("route", []):
            if self.cond(cond):
                return self.goto(target, depth + 1)
        return nid

    @property
    def node(self):
        return self.nodes[self.node_id]

    def text(self):
        parts = [self.node.get("text", "")]
        parts += [t for c, t in self.node.get("extra", []) if self.cond(c)]
        return self.sub("\n\n".join(p for p in parts if p))

    def choices(self):
        """List of (index, choice, enabled) for visible choices."""
        out = []
        for i, c in enumerate(self.node.get("choices", [])):
            if c.get("hideif") and self.cond(c["hideif"]):
                continue
            ok = self.cond(c.get("need", ""))
            if c.get("hide") and not ok:
                continue
            out.append((i, c, ok))
        return out

    def choose(self, i):
        c = self.node["choices"][i]
        self.apply(c.get("fx"))
        return self.goto(c["to"])

    def ending(self):
        return self.node.get("ending")

    # -- save state ------------------------------------------------------
    def to_dict(self):
        return {"name": self.name, "vars": self.vars, "flags": sorted(self.flags), "node": self.node_id}

    def from_dict(self, d):
        self.name, self.vars, self.flags, self.node_id = d["name"], d["vars"], set(d["flags"]), d["node"]


# ======================================================================
#  Terminal front-end
# ======================================================================
class Term:
    def __init__(self, color):
        self.color = color and sys.stdout.isatty()
        self.width = min(78, max(40, shutil.get_terminal_size((80, 24)).columns - 2))

    def c(self, code, s):
        return "\033[%sm%s\033[0m" % (code, s) if self.color else s

    def para(self, text, code=None):
        for p in text.split("\n\n"):
            for raw in p.split("\n"):
                line = textwrap.fill(raw, self.width, subsequent_indent="   " if raw.startswith("-- ") else "")
                print(self.c(code, line) if code else line)
            print()

    def rule(self):
        print(self.c("35", "=" * self.width))


def load_json(path, default):
    try:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return default


def save_json(path, data):
    try:
        with open(path, "w", encoding="utf-8") as f:
            json.dump(data, f)
        return True
    except OSError:
        return False


def status(g, t):
    v = g.vars
    line = "%s | BODY %d  REF %d  TECH %d  MIND %d  NERVE %d | ¢%s" % (
        g.name, v.get("body", 0), v.get("reflex", 0), v.get("tech", 0), v.get("mind", 0),
        v.get("nerve", 0), format(v.get("money", 0), ","))
    print(t.c("36", line))
    if "chip" in g.flags:
        print(t.c("33", "BURN %d/12  HEAT %d  HUMANITY %d/10" % (
            v.get("burn", 0), v.get("heat", 0), v.get("humanity", 0))))


def show_endings(story, t):
    found = load_json(ENDINGS_FILE, [])
    t.rule()
    print(t.c("1;35", "ENDINGS DISCOVERED: %d / %d" % (
        len([e for e in story["endings"] if e["id"] in found]), len(story["endings"]))))
    for e in story["endings"]:
        print("  " + (t.c("32", e["title"]) if e["id"] in found else t.c("90", "?????")))
    t.rule()


def prompt(msg):
    try:
        return input(msg).strip()
    except (EOFError, KeyboardInterrupt):
        print()
        sys.exit(0)


def title_screen(story, g, t):
    while True:
        t.rule()
        print(t.c("1;33", r"""
   _  _ ___ ___  _  _      _   ___ _  _
  | \| | __/ _ \| \| |    /_\ / __| || |
  | .` | _| (_) | .` |   / _ \\__ \ __ |
  |_|\_|___\___/|_|\_|  /_/ \_\___/_||_|
"""))
        print(t.c("36", "  a text adventure in Port Halcyon, 2091"))
        t.rule()
        has_save = os.path.exists(SAVE_FILE)
        print("  1. New game")
        if has_save:
            print("  2. Continue")
        print("  3. Endings discovered")
        print("  4. Quit")
        a = prompt("> ")
        if a == "1":
            name = prompt("What do they call you on the street? > ") or "V0id"
            g.reset(name[:24])
            g.goto(story["start"])
            return
        if a == "2" and has_save:
            g.from_dict(load_json(SAVE_FILE, None))
            return
        if a == "3":
            show_endings(story, t)
        if a == "4":
            sys.exit(0)


def play(story, t):
    g = Game(story)
    title_screen(story, g, t)
    while True:
        print()
        t.rule()
        status(g, t)
        t.rule()
        print()
        t.para(g.text())
        end = g.ending()
        if end:
            found = load_json(ENDINGS_FILE, [])
            if g.node_id not in found:
                found.append(g.node_id)
                save_json(ENDINGS_FILE, found)
            print(t.c("1;35", "*** ENDING: %s ***" % end["title"]))
            print(t.c("90", "Endings discovered: %d / %d" % (len(found), len(story["endings"]))))
            if os.path.exists(SAVE_FILE):
                os.remove(SAVE_FILE)
            prompt("\n[Enter] to return to the title screen.")
            g = Game(story)
            title_screen(story, g, t)
            continue

        save_json(SAVE_FILE, g.to_dict())  # autosave
        vis = g.choices()
        for n, (_, c, ok) in enumerate(vis, 1):
            tag = (c.get("tag", "") + " ") if c.get("tag") else ""
            label = g.sub(c["t"])
            if ok:
                print("  %s %s%s" % (t.c("1;33", "%d." % n), t.c("36", tag), label))
            else:
                print(t.c("90", "  %d. %s%s  (locked)" % (n, tag, label)))
        while True:
            a = prompt("\n> ").lower()
            if a.isdigit() and 1 <= int(a) <= len(vis):
                i, c, ok = vis[int(a) - 1]
                if not ok:
                    print(t.c("31", "You don't have what it takes for that. Yet."))
                    continue
                g.choose(i)
                break
            if a == "s":
                print(t.c("36", "Flags: " + ", ".join(f for f in sorted(g.flags) if not f.startswith("o_"))))
                status(g, t)
            elif a == "e":
                show_endings(story, t)
            elif a == "save":
                print("Saved." if save_json(SAVE_FILE, g.to_dict()) else "Could not save.")
            elif a == "load":
                d = load_json(SAVE_FILE, None)
                if d:
                    g.from_dict(d)
                    break
                print("No save found.")
            elif a == "q":
                save_json(SAVE_FILE, g.to_dict())
                print("Progress saved. The rain keeps falling without you.")
                sys.exit(0)
            else:
                print("Type a number (1-%d), or s / e / save / load / q." % len(vis))


def main():
    path = os.path.join(HERE, "story.json")
    story = load_json(path, None)
    if not story:
        sys.exit("Couldn't find story.json next to neon_ash.py.")
    if os.name == "nt":
        os.system("")  # enable ANSI colors on Windows 10+
    play(story, Term(color="--no-color" not in sys.argv))


if __name__ == "__main__":
    main()
