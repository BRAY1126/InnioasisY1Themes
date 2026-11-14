# Bait! for Nintendo DS

A cozy first-person fishing game for the Nintendo DS / DS Lite, written in C with devkitARM and libnds.

## Put it on an R4

1. Copy `bait.nds` anywhere on the R4's microSD card.
2. Start it from the R4 menu.
3. Progress saves to `bait.sav` on the card. Most R4 menus patch DLDI automatically. If the title screen says "No SD card", your R4 menu didn't patch DLDI. Try another R4 kernel (for example Wood R4) or TWiLight Menu++.

## Controls

| Input | Action |
| --- | --- |
| D-pad left / right | Look around the lake |
| D-pad up / down | Cast distance (5 steps) |
| A | Cast, then press again when the bobber dips |
| Scratch the touch screen | Reel. Faster reels faster, too fast snaps the line |
| L / R | Switch bottom-screen page |
| Tap | Buy or equip bait, travel to lakes, open log entries |
| SELECT | Music on / off |
| START | Start from the title screen |

## Features

- 4 lakes: Willow Pond (free), Rushing River (800c), Ocean Pier (4,000c), Misty Lake (15,000c), each with 10 fish.
- 6 tiers: common, uncommon, rare, exotic, special, legendary. Legendaries need the Mythic bait.
- 9 baits in a 3x3 tackle grid; locked baits double as the shop.
- Price = tier base x weight x distance bonus (rare and up, up to 1.5x at max distance) x lake multiplier.
- Day/night cycle (a full day takes 6 minutes) and rain. Some fish only bite at night, by day, or in the rain. Rain slightly improves the odds of rare-tier fish and up.
- Catch log (logs each species the first time), stats screen (count and heaviest per fish), 22 goals with coin rewards.
- Chiptune music on the DS sound channels.

## Build

```
docker run --rm -v "$PWD":/src -w /src devkitpro/devkitarm make
```

`tools/makefont.py` rebuilds `source/font8.h` from the Press Start 2P font (SIL Open Font License). `tools/makeicon.py` rebuilds `icon.bmp`.
