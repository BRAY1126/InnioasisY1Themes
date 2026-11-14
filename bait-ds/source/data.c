#include "data.h"
#include "gfx.h"

const Tier TIERS[NUM_TIERS] = {
	{ "Common",    3,    C_TIER0 },
	{ "Uncommon",  11,   C_TIER1 },
	{ "Rare",      60,   C_TIER2 },
	{ "Exotic",    225,  C_TIER3 },
	{ "Special",   750,  C_TIER4 },
	{ "Legendary", 4000, C_TIER5 },
};

// 10 fish per lake: 3 common, 2 uncommon, 2 rare, 1 exotic, 1 special, 1 legendary.
const Species SPECIES[NUM_SPECIES] = {
	// Willow Pond
	{ "Bluegill", 0, 0, 30 },        { "Perch", 0, 0, 40 },          { "Shiner", 0, 0, 10 },
	{ "Crappie", 1, 0, 60 },         { "Pumpkinseed", 1, 0, 30 },
	{ "Largemouth Bass", 2, 0, 250 },{ "Channel Catfish", 2, F_NIGHT, 400 },
	{ "Koi", 3, 0, 500 },
	{ "Ghost Carp", 4, F_NIGHT, 1000 },
	{ "Golden Koi", 5, F_RAIN, 1200 },
	// Rushing River
	{ "Dace", 0, 0, 20 },            { "Chub", 0, 0, 50 },           { "Sculpin", 0, 0, 10 },
	{ "Brook Trout", 1, 0, 80 },     { "Rainbow Trout", 1, 0, 150 },
	{ "Smallmouth Bass", 2, 0, 200 },{ "Brown Trout", 2, F_RAIN, 300 },
	{ "Steelhead", 3, 0, 500 },
	{ "King Salmon", 4, F_DAY, 1500 },
	{ "Ghost Sturgeon", 5, F_NIGHT, 6000 },
	// Ocean Pier
	{ "Sardine", 0, 0, 10 },         { "Mackerel", 0, 0, 60 },       { "Pinfish", 0, 0, 20 },
	{ "Flounder", 1, 0, 200 },       { "Red Snapper", 1, 0, 300 },
	{ "Mahi-Mahi", 2, F_DAY, 800 },  { "Grouper", 2, 0, 1200 },
	{ "Yellowfin Tuna", 3, 0, 4000 },
	{ "Sailfish", 4, 0, 4500 },
	{ "Kraken Squid", 5, F_NIGHT, 15000 },
	// Misty Lake
	{ "Fogfin", 0, 0, 30 },          { "Pale Perch", 0, 0, 50 },     { "Lantern Shiner", 0, 0, 20 },
	{ "Moon Crappie", 1, F_NIGHT, 80 },{ "Silver Eel", 1, 0, 150 },
	{ "Phantom Pike", 2, 0, 600 },   { "Arowana", 2, 0, 400 },
	{ "Coelacanth", 3, 0, 8000 },
	{ "Moonfish", 4, F_NIGHT, 800 },
	{ "Crystal Leviathan", 5, F_RAIN, 10000 },
};

const Bait BAITS[NUM_BAITS] = {
	{ "Stick&Rope", 0,     { 770, 25, 5, 0, 0 } },
	{ "Bread",      25,    { 1800, 180, 20, 0, 0 } },
	{ "Worm",       60,    { 2800, 600, 100, 0, 0 } },
	{ "Cricket",    150,   { 3300, 1300, 350, 50, 0 } },
	{ "Minnow",     400,   { 3500, 2000, 800, 200, 0 } },
	{ "Spinner",    1000,  { 3200, 2600, 1200, 500, 0 } },
	{ "Shiny Lure", 2500,  { 2700, 3000, 1800, 1000, 0 } },
	{ "Gold Fly",   6000,  { 2000, 3000, 2500, 1700, 0 } },
	{ "Mythic",     15000, { 1500, 2800, 2700, 2000, 500 } },
};

const Lake LAKES[NUM_LAKES] = {
	{ "Willow Pond",   0,     100 },
	{ "Rushing River", 800,   140 },
	{ "Ocean Pier",    4000,  190 },
	{ "Misty Lake",    15000, 260 },
};

const Mission MISSIONS[] = {
	{ MT_CATCH,   0,  5,   20,    "Catch 5 fish" },
	{ MT_SPECIES, 0,  1,   15,    "Catch a Bluegill" },
	{ MT_BAIT,    2,  1,   30,    "Buy the Worm bait" },
	{ MT_FAR,     0,  3,   50,    "Catch 3 fish at max distance" },
	{ MT_TIER,    1,  1,   40,    "Catch an uncommon fish" },
	{ MT_EARN,    0,  200, 60,    "Earn 200 coins" },
	{ MT_NIGHT,   0,  2,   80,    "Catch 2 fish at night" },
	{ MT_RAIN,    0,  1,   80,    "Catch a fish in the rain" },
	{ MT_LAKE,    1,  1,   150,   "Unlock Rushing River" },
	{ MT_TIER,    2,  1,   120,   "Catch a rare fish" },
	{ MT_SPECIES, 14, 2,   150,   "Catch 2 Rainbow Trout" },
	{ MT_LOGLAKE, 0,  6,   200,   "Log 6 species at Willow Pond" },
	{ MT_HEAVY,   0,  500, 250,   "Catch a fish over 5 kg" },
	{ MT_LAKE,    2,  1,   400,   "Unlock Ocean Pier" },
	{ MT_TIER,    3,  1,   500,   "Catch an exotic fish" },
	{ MT_CATCH,   0,  50,  600,   "Catch 50 fish" },
	{ MT_EARN,    0,  5000,800,   "Earn 5,000 coins" },
	{ MT_TIER,    4,  1,   1500,  "Catch a special fish" },
	{ MT_LAKE,    3,  1,   2000,  "Unlock Misty Lake" },
	{ MT_BAIT,    8,  1,   1000,  "Buy the Mythic bait" },
	{ MT_TIER,    5,  1,   5000,  "Catch a legendary fish" },
	{ MT_LOGALL,  0,  40,  10000, "Log all 40 species" },
};
const int NUM_MISSIONS = sizeof(MISSIONS) / sizeof(MISSIONS[0]);
