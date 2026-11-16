#!/usr/bin/env python3
"""Compile ../story.json into a Z-machine story file (../neon_ash.z5).

The .z5 runs in any Z-machine interpreter: CellarDoor / Frotz on Palm OS
(e.g. Treo 700p), Frotz on desktop, Lectrote, iOS/Android IF apps, etc.

Usage:  python3 build.py            (first, to refresh story.json)
        python3 build_z.py [path/to/inform6]

Needs the Inform 6 compiler: https://github.com/DavidKinder/Inform6
(build it with:  cc -O2 -o inform *.c)
"""
import json
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.dirname(HERE)
TERM_RE = re.compile(r"^([a-z_][a-z0-9_]*)\s*(>=|<=|==|!=|>|<)\s*(-?\d+)$")
FX_VAR_RE = re.compile(r"^([a-z_][a-z0-9_]*)([+\-=])(-?\d+)$")
FX_FLAG_RE = re.compile(r"^([+\-])([a-z_][a-z0-9_]*)$")
OPMAP = {">=": ">=", "<=": "<=", "==": "==", "!=": "~=", ">": ">", "<": "<"}
CLAMP = {"body": (0, 10), "reflex": (0, 10), "tech": (0, 10), "mind": (0, 10), "nerve": (0, 10),
         "humanity": (0, 10), "heat": (0, 99), "burn": (0, 99), "money": (0, 32000)}

story = json.load(open(os.path.join(OUT, "story.json"), encoding="utf-8"))
nodes = story["nodes"]
VARS, FLAGS = set(CLAMP), set()


def scan_cond(c):
    for part in (c or "").split("|"):
        for t in part.split("&"):
            t = t.strip()
            if not t:
                continue
            m = TERM_RE.match(t)
            if m:
                VARS.add(m.group(1))
            else:
                FLAGS.add(t.lstrip("!"))


def scan_fx(fx):
    for tok in (fx or "").split():
        m = FX_VAR_RE.match(tok)
        if m:
            VARS.add(m.group(1))
        else:
            FLAGS.add(FX_FLAG_RE.match(tok).group(2))


for n in nodes.values():
    scan_fx(n.get("fx"))
    for c, _ in n.get("route", []):
        scan_cond(c)
    for c, _ in n.get("extra", []):
        scan_cond(c)
    for ch in n.get("choices", []):
        scan_cond(ch.get("need"))
        scan_cond(ch.get("hideif"))
        scan_fx(ch.get("fx"))

FLAG_IDX = {f: i for i, f in enumerate(sorted(FLAGS))}


def cond(c):
    if not c:
        return "1"
    ors = []
    for part in c.split("|"):
        ands = []
        for t in part.split("&"):
            t = t.strip()
            m = TERM_RE.match(t)
            if m:
                ands.append("(v_%s %s %s)" % (m.group(1), OPMAP[m.group(2)], m.group(3)))
            elif t.startswith("!"):
                ands.append("(flags->%d == 0)" % FLAG_IDX[t[1:]])
            else:
                ands.append("(flags->%d ~= 0)" % FLAG_IDX[t])
        ors.append("(" + " && ".join(ands) + ")")
    return "(" + " || ".join(ors) + ")"


def fx(f):
    out, touched = [], False
    for tok in (f or "").split():
        m = FX_VAR_RE.match(tok)
        if m:
            k, op, v = m.groups()
            if k == "money":  # 16-bit Z-machine numbers: keep money well in range
                pass
            out.append("v_%s = %s;" % (k, v if op == "=" else "v_%s %s %s" % (k, op, v)))
            touched = True
        else:
            m = FX_FLAG_RE.match(tok)
            out.append("flags->%d = %d;" % (FLAG_IDX[m.group(2)], 1 if m.group(1) == "+" else 0))
    if touched:
        out.append("Clamp();")
    return " ".join(out)


def zstr(s):
    s = re.sub("¢([0-9,]+)", r"\1 cr", s).replace("¢", "")
    s = s.encode("ascii", "replace").decode()
    return (s.replace("@", "@@64").replace("\\", "@@92").replace("~", "@@126")
             .replace("^", "@@94").replace('"', "~").replace("\n", "^"))


def emit_text(s):
    """Inform statements printing s, with {name}/{var} substitution."""
    s = re.sub("¢\\{money\\}", "{money} cr", s)
    out = []
    for i, piece in enumerate(re.split(r"\{([a-z_]+)\}", s)):
        if i % 2 == 0:
            if piece:
                # split long strings at paragraph breaks to keep source lines sane
                for j, chunk in enumerate(piece.split("\n\n")):
                    if j:
                        out.append('print "^^";')
                    if chunk:
                        out.append('print "%s";' % zstr(chunk))
        elif piece == "name":
            out.append("PName();")
        elif piece == "money":
            out.append("PMoney(v_money);")
        elif piece in VARS:
            out.append("print v_%s;" % piece)
        else:
            out.append('print "{%s}";' % piece)
    return out


def ident(nid):
    return "N_" + nid


L = []
w = L.append
w("!% -v5")
w("!% -~S")
w('Constant Story "NEON ASH";')
w('Constant Headline "^A text adventure in Port Halcyon, 2091.^";')
w("Release 1;")
w("Constant NFLAGS = %d;" % len(FLAG_IDX))
w("Constant NENDINGS = %d;" % len(story["endings"]))
w("Constant MAX_BURN = %d;" % story["max_burn"])
for v in sorted(VARS):
    w("Global v_%s;" % v)
w("Global nvis;")
w("Array flags -> NFLAGS;")
w("Array namebuf -> 30;")
w("Global namelen;")
w("Array vis --> 12;")
w("Array ena -> 12;")
w("Array keybuf -> 4;")

w("""
[ Main n;
  @erase_window -1;
  Title();
  for (n = 0 : n < NFLAGS : n++) flags->n = 0;
  n = N_%s;
  while (n ~= 0) n = n(0);
  @quit;
];

[ Title k i;
  print "^^   N E O N   A S H^";
  print "   ---------------^";
  print "   A text adventure in Port Halcyon, 2091.^^";
  print "   ", NENDINGS, " endings. Stats unlock options. Every choice matters.^^";
  print "   Keys: 1-9 choose.  S save, R restore, T stats, Q quit.^^";
  print "   1. New game^   2. Restore a saved game^^> ";
  for (::) {
    @read_char 1 -> k;
    if (k == '1') break;
    if (k == '2') { @restore -> i; print "^Could not restore.^> "; }
  }
  @erase_window -1;
  print "IDENTIFY YOURSELF^^What do they call you on the street?^> ";
  namebuf->0 = 24; namebuf->1 = 0;
  @aread namebuf 0 -> i;
  namelen = namebuf->1;
  if (namelen == 0) {
    namebuf->2 = 'v'; namebuf->3 = '0'; namebuf->4 = 'i'; namebuf->5 = 'd'; namelen = 4;
  }
];

[ PName i c;
  for (i = 0 : i < namelen : i++) {
    c = namebuf->(i + 2);
    if (i == 0 && c >= 'a' && c <= 'z') c = c - 32;
    print (char) c;
  }
];

[ PMoney n;
  if (n >= 1000) { print n / 1000, ","; n = n %% 1000;
    if (n < 100) print "0"; if (n < 10) print "0"; }
  print n;
];

[ Clamp;""" % story["start"])
for k, (lo, hi) in sorted(CLAMP.items()):
    w("  if (v_%s < %d) v_%s = %d; if (v_%s > %d) v_%s = %d;" % (k, lo, k, lo, k, hi, k, hi))
w("];")

w("""
[ Header;
  @erase_window -1;
  print "[", (PName) 0, "] B", v_body, " R", v_reflex, " T", v_tech, " M", v_mind,
        " N", v_nerve, " | "; PMoney(v_money); print " cr^";""")
w("  if (flags->%d) print \"BURN \", v_burn, \"/\", MAX_BURN, \"  HEAT \", v_heat, \"  HUMANITY \", v_humanity, \"/10^\";"
  % FLAG_IDX["chip"])
w("""  print "--------------------------------^^";
];

[ Opt idx en;
  nvis++; vis-->nvis = idx; ena->nvis = en;
  print "^", nvis, ". ";
  if (~~en) print "(locked) ";
];

[ Stats;
  print "^^BODY ", v_body, "  REFLEX ", v_reflex, "  TECH ", v_tech, "  MIND ", v_mind,
        "  NERVE ", v_nerve, "^MONEY "; PMoney(v_money);
  print " cr  HEAT ", v_heat, "  HUMANITY ", v_humanity, "/10  BURN ", v_burn, "/", MAX_BURN, "^";
];

! Returns the chosen choice index, or -1 if a saved game was just restored.
[ GetKey k r;
  print "^^> ";
  for (::) {
    @read_char 1 -> k;
    if (k >= '1' && k <= '9') {
      k = k - '0';
      if (k <= nvis) {
        if (ena->k) { print k, "^"; return vis-->k; }
        print "^[Locked: you don't have what it takes. Yet.]^> ";
      }
    }
    if (k == 's' or 'S') {
      @save -> r;
      if (r == 2) return -1;
      if (r) print "^[Saved.]^> "; else print "^[Save failed.]^> ";
    }
    if (k == 'r' or 'R') { @restore -> r; print "^[Restore failed.]^> "; }
    if (k == 't' or 'T') { Stats(); print "> "; }
    if (k == 'q' or 'Q') { print "^The rain keeps falling without you.^"; @quit; }
  }
];

[ EndScreen title num k;
  print "^^*** ENDING ", num, " of ", NENDINGS, ": ", (string) title, " ***^^";
  print "Press any key to play again.^";
  @read_char 1 -> k;
  @restart;
];
""")

ending_num = {e["id"]: i + 1 for i, e in enumerate(story["endings"])}

for nid, n in nodes.items():
    w("[ %s redo k;" % ident(nid))
    body = []
    f = fx(n.get("fx"))
    pre = []
    if f:
        pre.append(f)
    for c, tgt in n.get("route", []):
        pre.append("if %s return %s;" % (cond(c), ident(tgt)) if c else "return %s;" % ident(tgt))
    if pre:
        body.append("if (redo == 0) { " + " ".join(pre) + " }")
    if "choices" not in n and "ending" not in n:
        body.append("return 0;")
        w("  " + "\n  ".join(body))
        w("];")
        continue
    body.append(".Redraw;")
    body.append("Header();")
    body += emit_text(n.get("text", ""))
    for c, x in n.get("extra", []):
        st = emit_text(x)
        body.append("if %s { print \"^^\"; %s }" % (cond(c), " ".join(st)))
    if "ending" in n:
        body.append('EndScreen("%s", %d);' % (zstr(n["ending"]["title"]), ending_num[nid]))
        body.append("return 0;")
    else:
        body.append("nvis = 0;")
        for i, ch in enumerate(n["choices"]):
            en = cond(ch.get("need"))
            label = ((ch["tag"] + " ") if ch.get("tag") else "") + ch["t"]
            show = "k = %s; if (k || %d) { Opt(%d, k); %s }" % (
                en, 0 if ch.get("hide") else 1, i, " ".join(emit_text(label)))
            if ch.get("hideif"):
                show = "if (~~%s) { %s }" % (cond(ch["hideif"]), show)
            body.append(show)
        body.append("k = GetKey();")
        body.append("if (k < 0) jump Redraw;")
        body.append("switch (k) {")
        for i, ch in enumerate(n["choices"]):
            body.append("  %d: %s return %s;" % (i, fx(ch.get("fx")), ident(ch["to"])))
        body.append("}")
        body.append("jump Redraw;")
    w("  " + "\n  ".join(body))
    w("];")

src = os.path.join(HERE, "neon_ash.inf")
with open(src, "w", encoding="ascii") as fh:
    fh.write("\n".join(L) + "\n")

inform = (sys.argv[1] if len(sys.argv) > 1 else None) or shutil.which("inform6") or shutil.which("inform")
if not inform:
    sys.exit("Wrote %s, but no Inform 6 compiler found. Pass its path as an argument." % src)
dst = os.path.join(OUT, "neon_ash.z5")
r = subprocess.run([inform, "-v5", "-~S", "$MAX_STATIC_DATA=60000", src, dst], capture_output=True, text=True)
print(r.stdout[-3000:], r.stderr[-3000:])
if r.returncode != 0 or not os.path.exists(dst):
    sys.exit("Inform compile failed.")
print("OK: %s (%.1f KB), %d flags, %d vars" % (dst, os.path.getsize(dst) / 1024, len(FLAG_IDX), len(VARS)))
