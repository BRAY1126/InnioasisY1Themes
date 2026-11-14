#pragma once
#include <nds/ndstypes.h>

#define NUM_TIERS 6
#define NUM_BAITS 9
#define NUM_LAKES 4
#define FISH_PER_LAKE 10
#define NUM_SPECIES (NUM_LAKES * FISH_PER_LAKE)
#define MAXD 4

enum { F_NIGHT = 1, F_DAY = 2, F_RAIN = 4 };

typedef struct { const char *name; u8 tier; u8 flags; u16 avg; /* average weight, 10 g units */ } Species;
typedef struct { const char *name; u32 price; u16 odds[5]; /* uncommon..legendary, 1/100 % */ } Bait;
typedef struct { const char *name; u32 cost; u16 mult; /* price multiplier, percent */ } Lake;
typedef struct { const char *name; u16 base; u8 col; } Tier;

enum { MT_CATCH, MT_SPECIES, MT_TIER, MT_EARN, MT_NIGHT, MT_RAIN, MT_FAR, MT_HEAVY,
       MT_LOGLAKE, MT_LOGALL, MT_LAKE, MT_BAIT };
typedef struct { u8 type; s16 arg; u32 n; u32 reward; const char *text; } Mission;

extern const Tier TIERS[NUM_TIERS];
extern const Species SPECIES[NUM_SPECIES];
extern const Bait BAITS[NUM_BAITS];
extern const Lake LAKES[NUM_LAKES];
extern const Mission MISSIONS[];
extern const int NUM_MISSIONS;
