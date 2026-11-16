# NEON ASH

A 100% text-based, choice-driven cyberpunk adventure set in **Port Halcyon, 2091**.

You pull off a heist on a megacorp tower, and the thing you steal ends up wired into your
own skull. It talks to you, and it's slowly overwriting your mind. The open city is yours
to explore: make deals with factions, take side gigs, buy chrome, talk to the voice in your
head, or walk away from all of it.

- **34 endings.** Some come early, some are secret, many depend on choices made hours apart.
- **3 origins** (Gutter-Born, Salt Rider, Glass-Blood), each with its own prologue and its own dialogue options later.
- **5 attributes** (BODY, REFLEX, TECH, MIND, NERVE). Options marked `[TECH 6]` unlock when your stats are high enough.
- **Open city hub:** 7 districts, 4 factions, side gigs, a chromedoc shop, and a companion.
- **Clocks and consequences:** NEURAL BURN (your time limit), HEAT (police attention), HUMANITY (go too far with chrome and it costs you).
- Autosave, plus a tracker for the endings you've found.

## How to play

**Browser (recommended):** open `neon_ash.html`. It's a single 115 KB file, works fully
offline, and needs no install. Keys: `1`-`9` pick a choice; arrow keys + `Enter` work too.

**Terminal:** `python3 neon_ash.py` (Python 3, no dependencies; keep `story.json` next to it).
Commands: `s` stats, `e` endings, `save`, `load`, `q`.

The whole game is about 220 KB, so it fits easily on any SD card or USB stick.

## Editing the story

The story lives in `src/story_src.py`. Rebuild after changing it:

```
python3 src/build.py
```

The build checks every link, condition and effect, flags dead ends and unreachable scenes,
then regenerates `story.json` and `neon_ash.html`.
