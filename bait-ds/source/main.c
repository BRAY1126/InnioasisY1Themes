// Bait! DS - a cozy first-person fishing game for the Nintendo DS Lite.
#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "gfx.h"
#include "data.h"
#include "audio.h"
#include "save.h"

// ------------------------------------------------------------------ buffers
static u8 topOv[SW * SH] ALIGN(32);
static u8 botBuf[SW * SH] ALIGN(32);
static int bgPano, bgOv, bgSub;
static int ovPage = 0, subPage = 0;
#define OV_VRAM(p)  ((u16 *)(0x06040000 + (p) * 0x10000))
#define SUB_VRAM(p) ((u16 *)(0x06200000 + (p) * 0x10000))

// ------------------------------------------------------------------ rng
static u32 rngS = 0x9E3779B9;
static u32 rnd(void) { rngS ^= rngS << 13; rngS ^= rngS >> 17; rngS ^= rngS << 5; return rngS; }
static int rndN(int n) { return n > 0 ? (int)(rnd() % (u32)n) : 0; }

// ------------------------------------------------------------------ game state
static SaveData S;
static bool hasSave = false;

enum { MODE_TITLE, MODE_PLAY };
enum { ST_AIM, ST_FLY, ST_WAIT, ST_BITE, ST_REEL, ST_RESULT };
enum { PG_BAIT, PG_LAKE, PG_LOG, PG_STAT, PG_GOAL, PG_COUNT };
static const char *PAGE_NAMES[PG_COUNT] = { "BAIT", "LAKE", "LOG", "STAT", "GOAL" };
enum { PH_DAWN, PH_DAY, PH_DUSK, PH_NIGHT };
static const char *PHASE_NAMES[4] = { "DAWN", "DAY", "DUSK", "NIGHT" };

static int mode = MODE_TITLE;
static int st = ST_AIM, stT = 0;
static int viewX = 128, dist = 2;
static int frame = 0;
static int bobWX, bobD;
static int waitFor, biteWindow;
typedef struct { int sp, weight, price, distBonus; } Catch;
static Catch fish;
typedef struct { int prog, need, sp, tension, over, idle, snap, pull, surge; } Reel; // fixed point *256 where noted
static Reel reel;
static char msgText[40], msgSub[48]; static int msgCol, msgT;
static int flashCol, flashT;
static char toastText[48]; static int toastT;
static int page = PG_BAIT, logLake = 0, detail = -1;
static int clockSub = 0;
static int panoLake = -1, panoPhase = -1, panoRain = -1;
static int titleConfirmNew = 0;
static int scratchAccum = 0, lastTX = -1, lastTY = -1;

static int phaseOf(int minute) {
	int h = minute / 60;
	if (h >= 5 && h < 7) return PH_DAWN;
	if (h >= 7 && h < 18) return PH_DAY;
	if (h >= 18 && h < 20) return PH_DUSK;
	return PH_NIGHT;
}
static bool isNight(void) { return phaseOf(S.clock) == PH_NIGHT; }

static void say(const char *t, const char *sub, int col, int dur) {
	strncpy(msgText, t, sizeof msgText - 1); msgText[sizeof msgText - 1] = 0;
	strncpy(msgSub, sub ? sub : "", sizeof msgSub - 1); msgSub[sizeof msgSub - 1] = 0;
	msgCol = col; msgT = dur;
}

static void persist(void) { if (saveAvailable()) saveWrite(&S); }

static int speciesFound(int lake) {
	int n = 0;
	for (int i = 0; i < FISH_PER_LAKE; i++) if (S.count[lake * FISH_PER_LAKE + i]) n++;
	return n;
}
static int totalFound(void) { int n = 0; for (int l = 0; l < NUM_LAKES; l++) n += speciesFound(l); return n; }

static void fmtCoins(char *out, u32 v) {
	char tmp[16]; sprintf(tmp, "%lu", (unsigned long)v);
	int len = strlen(tmp), o = 0;
	for (int i = 0; i < len; i++) { out[o++] = tmp[i]; if ((len - i - 1) % 3 == 0 && i != len - 1) out[o++] = ','; }
	out[o] = 0;
}
static void fmtKg(char *out, int w10g) { sprintf(out, "%d.%02dkg", w10g / 100, w10g % 100); }

// ------------------------------------------------------------------ missions
static void missionFill(void) {
	for (int i = 0; i < 3; i++)
		if (S.activeMission[i] == 0xFF && S.nextMission < NUM_MISSIONS) {
			S.activeMission[i] = S.nextMission++;
			S.missionProg[i] = 0;
		}
}

static u32 missionStateProgress(const Mission *m) {
	switch (m->type) {
	case MT_LAKE:    return (S.lakesOwned >> m->arg) & 1;
	case MT_BAIT:    return (S.baitsOwned >> m->arg) & 1;
	case MT_LOGLAKE: return speciesFound(m->arg);
	case MT_LOGALL:  return totalFound();
	default: return 0;
	}
}

static void missionCheck(void) {
	for (int i = 0; i < 3; i++) {
		int idx = S.activeMission[i];
		if (idx == 0xFF) continue;
		const Mission *m = &MISSIONS[idx];
		u32 prog = S.missionProg[i];
		if (m->type == MT_LAKE || m->type == MT_BAIT || m->type == MT_LOGLAKE || m->type == MT_LOGALL)
			prog = S.missionProg[i] = missionStateProgress(m);
		if (prog >= m->n) {
			S.money += m->reward;
			char c[16]; fmtCoins(c, m->reward);
			snprintf(toastText, sizeof toastText, "GOAL! +%sc", c);
			toastT = 180;
			sfxTone(784, 6, 0, 50); sfxTone(1046, 14, 0, 50);
			S.activeMission[i] = 0xFF;
		}
	}
	missionFill();
}

static void missionCatch(const Catch *c) {
	const Species *sp = &SPECIES[c->sp];
	for (int i = 0; i < 3; i++) {
		int idx = S.activeMission[i];
		if (idx == 0xFF) continue;
		const Mission *m = &MISSIONS[idx];
		switch (m->type) {
		case MT_CATCH: S.missionProg[i]++; break;
		case MT_SPECIES: if (c->sp == m->arg) S.missionProg[i]++; break;
		case MT_TIER: if (sp->tier >= m->arg) S.missionProg[i]++; break;
		case MT_EARN: S.missionProg[i] += c->price; break;
		case MT_NIGHT: if (isNight()) S.missionProg[i]++; break;
		case MT_RAIN: if (S.rain) S.missionProg[i]++; break;
		case MT_FAR: if (bobD == MAXD) S.missionProg[i]++; break;
		case MT_HEAVY: if ((u32)c->weight >= m->n) S.missionProg[i] = m->n; break;
		}
	}
	missionCheck();
}

static void missionEarnOther(u32 v) { (void)v; missionCheck(); }

// ------------------------------------------------------------------ new game
static void newGame(void) {
	memset(&S, 0, sizeof S);
	S.baitsOwned = 1; S.equipped = 0; S.lakesOwned = 1; S.lake = 0;
	S.musicOn = 1; S.clock = 8 * 60; S.rain = 0;
	S.activeMission[0] = S.activeMission[1] = S.activeMission[2] = 0xFF;
	S.nextMission = 0;
	missionFill();
}

// ------------------------------------------------------------------ panorama
static const RGB SKY[4][2] = {
	{ { 90, 90, 160 }, { 250, 170, 140 } },
	{ { 90, 160, 230 }, { 190, 227, 245 } },
	{ { 70, 50, 110 }, { 250, 130, 80 } },
	{ { 8, 10, 30 }, { 28, 36, 74 } },
};
static const int BRIGHT[4] = { 210, 256, 180, 95 };
static const int HORIZON[NUM_LAKES] = { 72, 64, 80, 70 };

static RGB lit(RGB c, int phase, int rain) {
	RGB r = rgbScale(c, BRIGHT[phase]);
	if (rain) { RGB g = { 110, 115, 125 }; r = rgbMix(r, rgbScale(g, BRIGHT[phase]), 100); }
	return r;
}

static void drawPanorama(int lake, int phase, int rain) {
	int H = HORIZON[lake];
	RGB skyT = SKY[phase][0], skyB = SKY[phase][1];
	if (rain) { RGB g = { 105, 110, 120 }; skyT = rgbMix(skyT, rgbScale(g, BRIGHT[phase]), 150); skyB = rgbMix(skyB, rgbScale(g, BRIGHT[phase]), 150); }
	p16Grad(0, H, skyT, skyB);
	// sun / moon / stars
	if (!rain) {
		if (phase == PH_NIGHT) {
			u32 s = 12345;
			for (int i = 0; i < 120; i++) {
				s = s * 1103515245 + 12345; int x = (s >> 8) % 512;
				s = s * 1103515245 + 12345; int y = (s >> 8) % (H - 6);
				RGB w = { 230, 230, 255 }; p16Pixel(x, y, w);
			}
			RGB m = { 240, 240, 220 }; p16Disc(380, 22, 8, m);
			RGB d = skyT; p16Disc(384, 19, 7, d);
		} else {
			RGB sun = phase == PH_DAY ? (RGB){ 255, 244, 176 } : (RGB){ 255, 200, 120 };
			int sy = phase == PH_DAY ? 18 : H - 14;
			int sx = phase == PH_DAWN ? 120 : phase == PH_DUSK ? 400 : 300;
			p16Disc(sx, sy, phase == PH_DAY ? 10 : 13, sun);
		}
	}
	// far scenery
	if (lake == 0) {
		RGB hill = lit((RGB){ 111, 158, 138 }, phase, rain), pine = lit((RGB){ 47, 107, 58 }, phase, rain), pine2 = lit((RGB){ 36, 85, 46 }, phase, rain);
		for (int i = 0; i < 7; i++) p16Hill(i * 80 + 30, H, 60, 20 + (i * 7) % 9, hill);
		for (int i = 0; i < 30; i++) {
			int x = i * 17 + (i * 37) % 11, h = 12 + (i * 53) % 12;
			p16Tri(x, H - 3 - h, 6, H - 2, (i % 3) ? pine : pine2);
			p16Rect(x - 1, H - 3, 2, 3, lit((RGB){ 91, 58, 30 }, phase, rain));
		}
	} else if (lake == 1) {
		RGB bank = lit((RGB){ 80, 110, 60 }, phase, rain), tree = lit((RGB){ 60, 120, 50 }, phase, rain), tree2 = lit((RGB){ 45, 100, 45 }, phase, rain);
		RGB rock = lit((RGB){ 120, 120, 125 }, phase, rain);
		p16Rect(0, H - 8, 512, 8, bank);
		for (int i = 0; i < 26; i++) {
			int x = i * 20 + (i * 13) % 9, r = 7 + (i * 31) % 6;
			p16Rect(x - 1, H - 14, 3, 8, lit((RGB){ 80, 55, 30 }, phase, rain));
			p16Disc(x, H - 14 - r / 2, r, (i & 1) ? tree : tree2);
		}
		for (int i = 0; i < 14; i++) p16Hill(i * 37 + 10, H, 6 + i % 4, 4 + i % 3, rock);
	} else if (lake == 2) {
		RGB isl = lit((RGB){ 70, 110, 80 }, phase, rain), sand = lit((RGB){ 220, 200, 150 }, phase, rain);
		p16Hill(150, H, 50, 10, sand); p16Hill(150, H, 40, 16, isl);
		p16Hill(430, H, 24, 6, isl);
		// lighthouse
		p16Rect(455, H - 30, 6, 26, lit((RGB){ 230, 230, 230 }, phase, rain));
		p16Rect(455, H - 22, 6, 4, lit((RGB){ 210, 50, 50 }, phase, rain));
		p16Rect(455, H - 12, 6, 4, lit((RGB){ 210, 50, 50 }, phase, rain));
		if (phase == PH_NIGHT || phase == PH_DUSK) p16Disc(458, H - 32, 3, (RGB){ 255, 240, 150 });
	} else {
		RGB mtn = lit((RGB){ 90, 80, 120 }, phase, rain), mtn2 = lit((RGB){ 70, 62, 98 }, phase, rain), deadT = lit((RGB){ 40, 35, 50 }, phase, rain);
		for (int i = 0; i < 8; i++) p16Tri(i * 70 + 20, H - 40 + (i * 13) % 14, 45, H, (i & 1) ? mtn : mtn2);
		for (int i = 0; i < 16; i++) {
			int x = i * 33 + (i * 7) % 13;
			p16Rect(x, H - 16, 2, 16, deadT);
			p16Rect(x - 4, H - 12, 4, 1, deadT); p16Rect(x + 2, H - 10, 5, 1, deadT);
		}
	}
	// water
	static const RGB WATER[NUM_LAKES][2] = {
		{ { 63, 127, 168 }, { 29, 74, 107 } },
		{ { 80, 150, 175 }, { 40, 95, 120 } },
		{ { 30, 100, 160 }, { 10, 45, 95 } },
		{ { 75, 75, 115 }, { 30, 30, 62 } },
	};
	RGB wt = rgbMix(lit(WATER[lake][0], phase, rain), skyB, 60), wb = lit(WATER[lake][1], phase, rain);
	p16Grad(H, 176, wt, wb);
	if (phase == PH_NIGHT && !rain) { RGB m = { 120, 120, 110 }; for (int y = H + 4; y < 170; y += 3) p16Rect(380 - (y - H) / 6, y, 4 + (y - H) / 3, 1, m); }
	if (lake == 3) { RGB fog = lit((RGB){ 200, 190, 220 }, phase, rain); p16DitherRect(0, H - 10, 512, 8, fog, 1); p16DitherRect(0, H, 512, 6, fog, 2); }
	// foreground
	if (lake == 0 || lake == 2) {
		RGB pl = lit((RGB){ 122, 82, 48 }, phase, rain), seam = lit((RGB){ 94, 62, 34 }, phase, rain), hi = lit((RGB){ 141, 96, 56 }, phase, rain);
		p16Rect(0, 176, 512, 16, pl);
		for (int x = 0; x < 512; x += 32) p16Rect(x, 176, 2, 16, seam);
		p16Rect(0, 176, 512, 2, hi);
		if (lake == 2) for (int x = 16; x < 512; x += 96) p16Rect(x, 168, 6, 10, seam);
	} else if (lake == 1) {
		RGB grass = lit((RGB){ 70, 120, 50 }, phase, rain), stone = lit((RGB){ 110, 110, 115 }, phase, rain);
		p16Rect(0, 178, 512, 14, grass);
		for (int x = 0; x < 512; x += 23) p16Hill(x, 180, 7, 4, stone);
		for (int x = 3; x < 512; x += 5) p16Rect(x, 174 + (x * 7) % 4, 1, 5, grass);
	} else {
		RGB st = lit((RGB){ 70, 68, 78 }, phase, rain), st2 = lit((RGB){ 90, 88, 100 }, phase, rain);
		p16Rect(0, 180, 512, 12, st);
		for (int x = 0; x < 512; x += 29) p16Hill(x + 8, 182, 12, 6, st2);
	}
	panoLake = lake; panoPhase = phase; panoRain = rain;
}

static void ensurePanorama(void) {
	int lake = mode == MODE_TITLE ? 0 : S.lake;
	int ph = phaseOf(S.clock);
	if (lake != panoLake || ph != panoPhase || S.rain != panoRain) drawPanorama(lake, ph, S.rain);
}

// ------------------------------------------------------------------ fishing logic
static int bobScreenY(int d) { return 168 - d * 20; }

static int rollTier(int d) {
	const Bait *b = &BAITS[S.equipped];
	int w[5], sum = 0;
	for (int i = 0; i < 5; i++) {
		int v = b->odds[i] * (1000 + 150 * d / MAXD) / 1000;        // far casts: a little better odds
		if (S.rain && i >= 1) v = v * 110 / 100;                    // rain: slightly better rare+ odds
		w[i] = v; sum += v;
	}
	if (sum > 10000) sum = 10000;
	int r = rndN(10000);
	if (r < 10000 - sum) return 0;
	r -= 10000 - sum;
	for (int i = 0; i < 5; i++) { if (r < w[i]) return i + 1; r -= w[i]; }
	return 0;
}

static bool eligible(const Species *s) {
	if ((s->flags & F_NIGHT) && !isNight()) return false;
	if ((s->flags & F_DAY) && isNight()) return false;
	if ((s->flags & F_RAIN) && !S.rain) return false;
	return true;
}

static Catch rollFish(int d) {
	Catch c;
	int tier = rollTier(d);
	int pool[FISH_PER_LAKE], n = 0;
	for (;;) {
		n = 0;
		for (int i = 0; i < FISH_PER_LAKE; i++) {
			int idx = S.lake * FISH_PER_LAKE + i;
			if (SPECIES[idx].tier == tier && eligible(&SPECIES[idx])) pool[n++] = idx;
		}
		if (n || tier == 0) break;
		tier--;                                // nothing biting at this tier right now
	}
	c.sp = pool[rndN(n)];
	int ratio = 600 + rndN(801);               // 0.6x .. 1.4x the species' average weight
	c.weight = SPECIES[c.sp].avg * ratio / 1000;
	if (c.weight < 1) c.weight = 1;
	c.distBonus = tier >= 2 ? 1000 + 500 * d / MAXD : 1000;
	u32 p = (u32)TIERS[tier].base * ratio / 1000;
	p = p * c.distBonus / 1000;
	p = p * LAKES[S.lake].mult / 100;
	c.price = p < 1 ? 1 : p;
	return c;
}

static void toResult(void) { st = ST_RESULT; stT = 0; }

static void landFish(void) {
	int i = fish.sp;
	const Species *sp = &SPECIES[i];
	bool isNew = S.count[i] == 0;
	if (S.count[i] < 65535) S.count[i]++;
	if (fish.weight > S.heaviest[i]) S.heaviest[i] = fish.weight;
	if ((u32)fish.price > S.best[i]) S.best[i] = fish.price;
	S.money += fish.price; S.earned += fish.price; S.totalCaught++;
	flashCol = TIERS[sp->tier].col; flashT = 40;
	char t[40], sub[48], kg[16], c[16];
	fmtKg(kg, fish.weight); fmtCoins(c, fish.price);
	snprintf(t, sizeof t, "%s%s", isNew ? "NEW! " : "", sp->name);
	snprintf(sub, sizeof sub, "%s %s +%sc", TIERS[sp->tier].name, kg, c);
	say(t, sub, TIERS[sp->tier].col, 170);
	sfxTone(660, 5, 0, 50); sfxTone(880, 5, 0, 50);
	sfxTone(1320, 16, 0, 50);
	missionCatch(&fish);
	persist();
	toResult(); stT = -70;
}

static void startCast(void) {
	bobWX = viewX + 128; bobD = dist;
	st = ST_FLY; stT = 0;
	sfxTone(500, 10, 40, 40);
}

static void updatePlay(int down, int held) {
	if (held & KEY_LEFT) viewX = viewX > 2 ? viewX - 2 : 0;
	if (held & KEY_RIGHT) viewX = viewX < 254 ? viewX + 2 : 256;
	if (down & KEY_L) { page = (page + PG_COUNT - 1) % PG_COUNT; detail = -1; sfxBeep(330); }
	if (down & KEY_R) { page = (page + 1) % PG_COUNT; detail = -1; sfxBeep(330); }

	stT++;
	switch (st) {
	case ST_AIM:
		if ((down & KEY_UP) && dist < MAXD) { dist++; sfxBeep(600 + dist * 60); }
		if ((down & KEY_DOWN) && dist > 0) { dist--; sfxBeep(600 + dist * 60); }
		if (down & KEY_A) startCast();
		break;
	case ST_FLY:
		if (stT >= 30) { st = ST_WAIT; stT = 0; waitFor = 120 + rndN(361); sfxNoise(3000, 10, 50); }
		break;
	case ST_WAIT:
		if (down & KEY_A) { say("Spooked it!", "Too early", C_PINK, 100); sfxTone(180, 12, -6, 40); toResult(); break; }
		if (stT >= waitFor) {
			fish = rollFish(bobD);
			biteWindow = 24 - SPECIES[fish.sp].tier * 12 / 5;
			st = ST_BITE; stT = 0;
			sfxNoise(6000, 6, 60); sfxTone(1100, 6, 0, 50);
		}
		break;
	case ST_BITE:
		if (down & KEY_A) {
			int tier = SPECIES[fish.sp].tier;
			reel.need = (300 + bobD * 100) * 256;
			reel.prog = reel.need * 15 / 100;
			reel.snap = (38 - tier * 3) * 256;            // scratch speed (px/frame) that snaps the line
			reel.pull = 102 + tier * 64;                  // 0.4 .. 1.65 px/frame, *256
			reel.sp = 0; reel.tension = 0; reel.over = 0; reel.idle = 0; reel.surge = 256;
			st = ST_REEL; stT = 0;
			say("HOOKED!", "Scratch the touch screen!", C_YELLOW, 60);
			sfxTone(990, 8, 0, 50);
		} else if (stT > biteWindow) { say("It got away", "Too late", C_PINK, 100); sfxTone(180, 12, -6, 40); toResult(); }
		break;
	case ST_REEL: {
		Reel *r = &reel;
		r->sp = (r->sp * 204 + scratchAccum * 52 * 256) / 256;   // smooth: 0.8 old + 0.2 new
		if (rndN(100) == 0) r->surge = 294 + rndN(64);                 // fish thrashes
		r->surge += (256 - r->surge) / 50;
		r->tension = (int)((long long)r->sp * r->surge / r->snap);    // 256 = snap point
		r->prog += r->sp * 31 / 256 - r->pull;                        // 0.12 * speed
		if (r->prog > r->need) r->prog = r->need;
		if (r->prog < 0) r->prog = 0;
		if (r->tension > 256) { r->over += 2; if (frame % 6 == 0) sfxTone(1400, 2, 0, 30); }
		else if (r->over > 0) r->over--;
		r->idle = r->sp < 512 ? r->idle + 1 : 0;
		if (r->over > 50) { say("SNAP!", "Line broke - too fast!", C_RED, 120); sfxNoise(800, 20, 70); toResult(); }
		else if (r->idle > 240 || (r->prog <= 0 && stT > 60)) { say("It got away", "Keep scratching!", C_PINK, 110); sfxTone(180, 12, -6, 40); toResult(); }
		else if (r->prog >= r->need) landFish();
		else if (frame % 8 == 0 && r->sp > 768) sfxTone(300 + r->sp * 10 / 256, 2, 0, 20);
		break;
	}
	case ST_RESULT:
		if (stT > 90 || (stT > 20 && (down & KEY_A))) { st = ST_AIM; stT = 0; }
		break;
	}

	// world clock: 1 in-game minute every 15 frames (a full day = 6 minutes)
	if (++clockSub >= 15) {
		clockSub = 0;
		S.clock = (S.clock + 1) % 1440;
		if (S.clock % 60 == 0) {
			int r = rndN(100);
			if (S.rain && r < 22) { S.rain = 0; say("The rain stops", "", C_LIGHT, 80); }
			else if (!S.rain && r < (S.lake == 0 ? 12 : 16)) { S.rain = 1; say("It's raining", "Rare fish are biting", C_RAIN, 100); }
		}
	}
	musicPlay(isNight() ? SONG_NIGHT : SONG_DAY);
}

// ------------------------------------------------------------------ bottom-screen taps
static bool canChangeGear(void) { return st == ST_AIM || st == ST_RESULT; }

static void tapPlay(int x, int y) {
	if (y < 26) {
		if (y >= 13) { int i = x * PG_COUNT / 256; if (i != page) { page = i; detail = -1; sfxBeep(330); } }
		return;
	}
	if (page == PG_BAIT) {
		int i = (x / 86) + ((y - 26) / 55) * 3;
		if (i < 0 || i >= NUM_BAITS) return;
		if (S.baitsOwned & (1 << i)) {
			if (canChangeGear()) { S.equipped = i; persist(); sfxBeep(520); }
			else say("Reel in first", "", C_LIGHT, 50);
		} else if (S.money >= BAITS[i].price) {
			S.money -= BAITS[i].price; S.baitsOwned |= 1 << i;
			if (canChangeGear()) S.equipped = i;
			char t[40]; snprintf(t, sizeof t, "Bought %s", BAITS[i].name);
			say(t, "", C_LGREEN, 70); sfxTone(523, 5, 0, 45); sfxTone(784, 10, 0, 45);
			missionEarnOther(0); persist();
		} else {
			char sub[32], c[16]; fmtCoins(c, BAITS[i].price - S.money); snprintf(sub, sizeof sub, "%sc more", c);
			say("Not enough coins", sub, C_PINK, 60); sfxTone(120, 8, 0, 40);
		}
	} else if (page == PG_LAKE) {
		int i = (x / 128) + ((y - 26) / 83) * 2;
		if (i < 0 || i >= NUM_LAKES) return;
		if (!canChangeGear()) { say("Reel in first", "", C_LIGHT, 50); return; }
		if (S.lakesOwned & (1 << i)) {
			if (S.lake != i) { S.lake = i; logLake = i; viewX = 128; say(LAKES[i].name, "", C_WHITE, 90); sfxTone(440, 6, 20, 40); persist(); }
		} else if (S.money >= LAKES[i].cost) {
			S.money -= LAKES[i].cost; S.lakesOwned |= 1 << i; S.lake = i; logLake = i; viewX = 128;
			say(LAKES[i].name, "Unlocked!", C_LGREEN, 110);
			sfxTone(523, 6, 0, 45); sfxTone(1046, 16, 0, 45);
			missionEarnOther(0); persist();
		} else {
			char sub[32], c[16]; fmtCoins(c, LAKES[i].cost - S.money); snprintf(sub, sizeof sub, "%sc more", c);
			say("Not enough coins", sub, C_PINK, 60); sfxTone(120, 8, 0, 40);
		}
	} else if (page == PG_LOG || page == PG_STAT) {
		if (detail >= 0) { detail = -1; sfxBeep(300); return; }
		if (y < 40) { logLake = (logLake + (x < 128 ? NUM_LAKES - 1 : 1)) % NUM_LAKES; sfxBeep(400); return; }
		if (page == PG_LOG) {
			int row = (y - 42) / 15;
			if (row >= 0 && row < FISH_PER_LAKE) {
				int idx = logLake * FISH_PER_LAKE + row;
				if (S.count[idx]) { detail = idx; sfxBeep(440); }
			}
		}
	}
}

// ------------------------------------------------------------------ drawing: top
static void drawTopPlay(u8 *b) {
	c8Clear(b, C_CLEAR);
	int H = HORIZON[S.lake];
	// water sparkle / flow
	int flow = S.lake == 1 ? 3 : S.lake == 2 ? 1 : 0;
	for (int i = 0; i < 22; i++) {
		int y = H + 4 + i * i * 100 / 470;
		if (y > 172) break;
		int span = 300;
		int x = ((i * 71 + frame * (2 + i / 4 + flow * 4) / 8 - viewX * (8 + i) / 16) % span + span) % span - 20;
		c8Rect(b, x, y, 6 + i / 2 + flow * 4, 1, S.lake == 1 && (i & 1) ? C_FOAM : C_RIPPLE);
	}
	// aim marker
	if (st == ST_AIM) {
		int y = bobScreenY(dist);
		if ((frame / 8) & 1) c8Ellipse(b, 128, y, 10 - dist, 3, C_WHITE);
		else c8Ellipse(b, 128, y, 9 - dist, 2, C_LIGHT);
	}
	// rod
	int tipX = 150, tipY = 96;
	if (st == ST_REEL) { int t = reel.tension; tipX += ((frame % 6) < 3 ? 1 : -1) * t * 3 / 256; tipY += 6 + t * 8 / 256; }
	if (st == ST_FLY) tipY -= stT < 10 ? stT : stT < 20 ? 20 - stT : 0;
	c8ThickLine(b, 232, 199, 205, 160, 5, C_ROD);
	c8Quad(b, 205, 160, 180, 120, tipX, tipY, C_RODTIP);
	c8Quad(b, 206, 160, 181, 120, tipX + 1, tipY, C_RODTIP);
	c8Rect(b, 208, 162, 8, 6, C_REEL);
	// line + bobber
	if (st != ST_AIM && st != ST_RESULT) {
		int px = bobWX - viewX, py = bobScreenY(bobD), pr = 4 - bobD / 2;
		if (st == ST_FLY) {
			int k = stT * 256 / 30;
			int arc = (k * (256 - k)) * 50 / 16384;
			px = tipX + (px - tipX) * k / 256;
			py = tipY + (py - tipY) * k / 256 - arc;
		}
		int by = py + (((frame / 15) & 1) ? 1 : 0);
		if (st == ST_BITE) by += 4 + ((frame & 3) < 2 ? 1 : -1);
		if (st == ST_REEL) {
			int k = reel.prog / (reel.need / 256);
			px += (tipX - px) * k * 6 / 2560;
			by += (176 - by) * k * 7 / 2560 + (((frame / 2) & 1) ? reel.tension * 2 / 256 : 0);
		}
		c8Quad(b, tipX, tipY, (tipX + px) / 2, (tipY > by ? tipY : by) + 8, px, by, C_LINE);
		if (st == ST_BITE || st == ST_REEL) c8Ellipse(b, px, by + 2, 6 + (frame % 10), 2, C_WHITE);
		if (st == ST_WAIT && stT < 20) c8Ellipse(b, px, by + 1, 3 + stT / 2, 1 + stT / 10, C_FOAM);
		c8Disc(b, px, by, pr < 2 ? 2 : pr, C_WHITE);
		c8HalfDiscTop(b, px, by, pr < 2 ? 2 : pr, C_RED);
	}
	// rain
	if (S.rain) {
		for (int i = 0; i < 70; i++) {
			int x = (i * 53 + frame * 3) % 270 - 10, y = (i * 97 + frame * 7) % 200 - 8;
			c8Line(b, x, y, x - 2, y + 6, C_RAIN);
			if (i % 7 == 0) { int ry = H + 10 + (i * 31) % (170 - H); c8Ellipse(b, (i * 41 + frame) % 256, ry, 2 + (frame + i) % 4, 1, C_RIPPLE); }
		}
	}
	// HUD
	c8TextS(b, 4, 4, "DIST", C_WHITE, 1);
	for (int i = 0; i <= MAXD; i++) c8Rect(b, 38 + i * 7, 4, 5, 7, i <= (st == ST_AIM ? dist : bobD) ? C_GOLD : C_SHADE);
	c8TextR(b, 252, 4, BAITS[S.equipped].name, C_WHITE, 1);
	if (st == ST_AIM) c8TextC(b, 128, 182, "A: CAST", C_WHITE, 1);
	if (st == ST_WAIT) c8TextC(b, 128, 182, "wait for the bite...", C_LIGHT, 1);
	if (st == ST_BITE) c8TextC(b, 128, 36, "!! PRESS A !!", C_YELLOW, 2);
	// catch flash
	if (flashT > 0) c8Dither(b, 0, 0, SW, SH, flashCol, flashT > 30 ? 4 : flashT > 20 ? 3 : flashT > 10 ? 2 : 1);
	// message
	if (msgT > 0) {
		int h = msgSub[0] ? 34 : 22;
		c8Dither(b, 8, 56, 240, h, C_SHADE, 3);
		c8TextC(b, 128, 61, msgText, msgCol, 1);
		if (msgSub[0]) c8TextC(b, 128, 76, msgSub, C_LIGHT, 1);
	}
	if (toastT > 0) {
		c8Rect(b, 40, 18, 176, 14, C_TILEEQ);
		c8Frame(b, 40, 18, 176, 14, C_LGREEN);
		c8TextC(b, 128, 21, toastText, C_YELLOW, 1);
	}
}

static void drawTopTitle(u8 *b) {
	c8Clear(b, C_CLEAR);
	for (int i = 0; i < 18; i++) {
		int y = 76 + i * i * 100 / 330; if (y > 172) break;
		int x = ((i * 71 + frame * (2 + i / 4) / 8) % 300) - 20;
		c8Rect(b, x, y, 6 + i / 2, 1, C_RIPPLE);
	}
	int bob = ((frame / 20) & 1);
	c8TextC(b, 128, 30 + bob, "BAIT!", C_GOLD, 5);
	c8TextC(b, 128, 80, "a DS fishing game", C_WHITE, 1);
	// bobber
	int by = 130 + (((frame / 15) & 1) ? 1 : 0);
	c8Disc(b, 128, by, 5, C_WHITE); c8HalfDiscTop(b, 128, by, 5, C_RED);
	c8Ellipse(b, 128, by + 3, 10 + (frame / 4) % 8, 2, C_FOAM);
}

// ------------------------------------------------------------------ drawing: bottom
static void drawStatusBar(u8 *b) {
	c8Rect(b, 0, 0, SW, 26, C_PANEL);
	char c[16], s[24]; fmtCoins(c, S.money);
	snprintf(s, sizeof s, "$%s", c);
	c8Text(b, 4, 3, s, C_GOLD, 1);
	char clk[24];
	snprintf(clk, sizeof clk, "%02d:%02d %s", S.clock / 60, S.clock % 60, S.rain ? "RAIN" : PHASE_NAMES[phaseOf(S.clock)]);
	c8Text(b, 252 - textW(clk, 1), 3, clk, S.rain ? C_RAIN : C_LIGHT, 1);
	for (int i = 0; i < PG_COUNT; i++) {
		int x0 = i * 256 / PG_COUNT, x1 = (i + 1) * 256 / PG_COUNT;
		c8Rect(b, x0 + 1, 13, x1 - x0 - 2, 12, i == page ? C_TAB : C_PANEL2);
		c8Text(b, (x0 + x1) / 2 - textW(PAGE_NAMES[i], 1) / 2, 15, PAGE_NAMES[i], i == page ? C_WHITE : C_GREY, 1);
	}
}

static void drawOddsBar(u8 *b, int x, int y, int w, const Bait *bt, bool own) {
	int sum = 0; for (int i = 0; i < 5; i++) sum += bt->odds[i];
	int vals[6] = { 10000 - sum, bt->odds[0], bt->odds[1], bt->odds[2], bt->odds[3], bt->odds[4] };
	int acc = 0, px = x;
	for (int t = 0; t < 6; t++) {
		acc += vals[t];
		int nx = x + acc * w / 10000;
		if (vals[t] > 0 && nx <= px) nx = px + 1;
		c8Rect(b, px, y, nx - px, 5, own ? TIERS[t].col : C_DIM);
		px = nx;
	}
}

static void drawBaitPage(u8 *b) {
	for (int i = 0; i < NUM_BAITS; i++) {
		int x = (i % 3) * 86 - (i % 3 == 2 ? 2 : 0), y = 26 + (i / 3) * 55, w = 84, h = 53;
		bool own = S.baitsOwned & (1 << i), eq = S.equipped == i;
		c8Rect(b, x + 1, y + 1, w, h, eq ? C_TILEEQ : own ? C_TILE : C_TILELOCK);
		if (eq) c8Frame(b, x + 2, y + 2, w - 2, h - 2, C_LGREEN);
		const char *nm = BAITS[i].name;
		c8Text(b, x + 1 + w / 2 - textW(nm, 1) / 2, y + 8, nm, own ? C_WHITE : C_DIM, 1);
		drawOddsBar(b, x + 8, y + 22, w - 14, &BAITS[i], own);
		if (own) {
			const char *t = eq ? "EQUIPPED" : "equip";
			c8Text(b, x + 1 + w / 2 - textW(t, 1) / 2, y + 36, t, eq ? C_LGREEN : C_GREY, 1);
		} else {
			char c[16], s[20]; fmtCoins(c, BAITS[i].price); snprintf(s, sizeof s, "%sc", c);
			c8Text(b, x + 1 + w / 2 - textW(s, 1) / 2, y + 36, s, S.money >= BAITS[i].price ? C_GOLD : C_PINK, 1);
		}
	}
}

static void drawLakePage(u8 *b) {
	static const u8 LAKECOL[NUM_LAKES] = { C_TIER1, C_BLUE, C_TIER2, C_TIER3 };
	for (int i = 0; i < NUM_LAKES; i++) {
		int x = (i % 2) * 128, y = 26 + (i / 2) * 83, w = 126, h = 81;
		bool own = S.lakesOwned & (1 << i), here = S.lake == i;
		c8Rect(b, x + 1, y + 1, w, h, here ? C_TILEEQ : own ? C_TILE : C_TILELOCK);
		if (here) c8Frame(b, x + 2, y + 2, w - 2, h - 2, C_LGREEN);
		c8Rect(b, x + 8, y + 26, w - 14, 18, own ? LAKECOL[i] : C_DIM);
		c8Fish(b, x + 30 + (frame / 3 + i * 40) % 60, y + 35, 2, C_WHITE);
		const char *nm = LAKES[i].name;
		c8Text(b, x + 1 + w / 2 - textW(nm, 1) / 2, y + 10, nm, own ? C_WHITE : C_DIM, 1);
		char s[24];
		if (own) {
			snprintf(s, sizeof s, "%d/%d fish", speciesFound(i), FISH_PER_LAKE);
			c8Text(b, x + 1 + w / 2 - textW(s, 1) / 2, y + 52, s, C_LIGHT, 1);
			const char *t = here ? "YOU ARE HERE" : "TAP TO GO";
			c8Text(b, x + 1 + w / 2 - textW(t, 1) / 2, y + 66, t, here ? C_LGREEN : C_GREY, 1);
		} else {
			char c[16]; fmtCoins(c, LAKES[i].cost); snprintf(s, sizeof s, "%sc", c);
			c8Text(b, x + 1 + w / 2 - textW(s, 1) / 2, y + 56, s, S.money >= LAKES[i].cost ? C_GOLD : C_PINK, 1);
		}
	}
}

static void drawLakeHeader(u8 *b) {
	c8Rect(b, 0, 26, SW, 14, C_PANEL2);
	char s[40]; snprintf(s, sizeof s, "%s %d/%d", LAKES[logLake].name, speciesFound(logLake), FISH_PER_LAKE);
	c8Text(b, 128 - textW(s, 1) / 2, 29, s, C_WHITE, 1);
	c8Text(b, 4, 29, "<", C_YELLOW, 1); c8Text(b, 244, 29, ">", C_YELLOW, 1);
}

static const char *condText(const Species *s) {
	if (s->flags & F_RAIN) return "Only bites in the rain";
	if (s->flags & F_NIGHT) return "Only bites at night";
	if (s->flags & F_DAY) return "Only bites by day";
	return "Bites any time";
}

static void drawLogPage(u8 *b) {
	if (detail >= 0) {
		const Species *s = &SPECIES[detail];
		u8 col = TIERS[s->tier].col;
		c8Rect(b, 0, 26, SW, SH - 26, C_BG);
		c8Rect(b, 16, 34, 224, 3, col);
		c8Text(b, 128 - textW(s->name, 1) / 2, 44, s->name, col, 1);
		c8Text(b, 128 - textW(TIERS[s->tier].name, 1) / 2, 58, TIERS[s->tier].name, C_LIGHT, 1);
		c8Fish(b, 124, 88, 8, col);
		char t[40], kg[16], c[16];
		snprintf(t, sizeof t, "Caught:    %u", S.count[detail]); c8Text(b, 20, 114, t, C_WHITE, 1);
		fmtKg(kg, S.heaviest[detail]); snprintf(t, sizeof t, "Heaviest:  %s", kg); c8Text(b, 20, 128, t, C_WHITE, 1);
		fmtCoins(c, S.best[detail]); snprintf(t, sizeof t, "Best sale: %sc", c); c8Text(b, 20, 142, t, C_WHITE, 1);
		c8Text(b, 20, 158, condText(s), C_CYAN, 1);
		c8Text(b, 128 - textW("tap to go back", 1) / 2, 178, "tap to go back", C_GREY, 1);
		return;
	}
	drawLakeHeader(b);
	for (int i = 0; i < FISH_PER_LAKE; i++) {
		int idx = logLake * FISH_PER_LAKE + i, y = 42 + i * 15;
		const Species *s = &SPECIES[idx];
		bool got = S.count[idx] > 0;
		c8Rect(b, 2, y, 252, 14, got ? C_TILE : C_TILELOCK);
		c8Rect(b, 2, y, 3, 14, got ? TIERS[s->tier].col : C_DIM);
		if (got) {
			c8Fish(b, 18, y + 7, 2, TIERS[s->tier].col);
			c8Text(b, 34, y + 3, s->name, C_WHITE, 1);
		} else c8Text(b, 34, y + 3, "???", C_DIM, 1);
	}
}

static void drawStatPage(u8 *b) {
	drawLakeHeader(b);
	c8Text(b, 6, 42, "FISH", C_GREY, 1);
	c8Text(b, 186 - textW("#", 1), 42, "#", C_GREY, 1);
	c8Text(b, 252 - textW("BEST", 1), 42, "BEST", C_GREY, 1);
	for (int i = 0; i < FISH_PER_LAKE; i++) {
		int idx = logLake * FISH_PER_LAKE + i, y = 52 + i * 12;
		const Species *s = &SPECIES[idx];
		bool got = S.count[idx] > 0;
		if (i & 1) c8Rect(b, 2, y - 2, 252, 12, C_PANEL);
		c8Rect(b, 4, y + 1, 4, 5, got ? TIERS[s->tier].col : C_DIM);
		char nm[16]; snprintf(nm, sizeof nm, "%.13s", got ? s->name : "???");
		c8Text(b, 12, y, nm, got ? C_LIGHT : C_DIM, 1);
		char t[16];
		snprintf(t, sizeof t, "%u", S.count[idx]);
		c8Text(b, 186 - textW(t, 1), y, got ? t : "-", C_WHITE, 1);
		if (got) { int w = S.heaviest[idx]; snprintf(t, sizeof t, "%d.%dkg", w / 100, (w % 100) / 10); }
		c8Text(b, 252 - textW(got ? t : "-", 1), y, got ? t : "-", C_WHITE, 1);
	}
	char c[16], s[40]; fmtCoins(c, S.earned);
	snprintf(s, sizeof s, "Total %lu  Earned %sc", (unsigned long)S.totalCaught, c);
	c8Text(b, 128 - textW(s, 1) / 2, 176, s, C_GOLD, 1);
}

static void drawGoalPage(u8 *b) {
	char s[40];
	int done = S.nextMission;
	for (int i = 0; i < 3; i++) if (S.activeMission[i] != 0xFF) done--;
	snprintf(s, sizeof s, "Goals done %d/%d", done, NUM_MISSIONS);
	c8Text(b, 128 - textW(s, 1) / 2, 30, s, C_LIGHT, 1);
	int y = 44;
	for (int i = 0; i < 3; i++) {
		int idx = S.activeMission[i];
		c8Rect(b, 4, y, 248, 44, idx == 0xFF ? C_TILELOCK : C_TILE);
		if (idx == 0xFF) { c8Text(b, 128 - textW("All goals complete!", 1) / 2, y + 18, "All goals complete!", C_LGREEN, 1); y += 48; continue; }
		const Mission *m = &MISSIONS[idx];
		c8Text(b, 10, y + 6, m->text, C_WHITE, 1);
		u32 prog = S.missionProg[i]; if (prog > m->n) prog = m->n;
		c8Rect(b, 10, y + 20, 160, 7, C_PANEL);
		c8Rect(b, 10, y + 20, 160 * prog / m->n, 7, C_BLUE);
		char p[24], c[16];
		if (m->type == MT_HEAVY) snprintf(p, sizeof p, "%s", prog ? "done" : "");
		else snprintf(p, sizeof p, "%lu/%lu", (unsigned long)prog, (unsigned long)m->n);
		c8Text(b, 176, y + 20, p, C_LIGHT, 1);
		fmtCoins(c, m->reward); snprintf(p, sizeof p, "Reward %sc", c);
		c8Text(b, 10, y + 32, p, C_GOLD, 1);
		y += 48;
	}
}

static void drawReel(u8 *b) {
	Reel *r = &reel;
	c8Clear(b, C_SHADE);
	for (int i = 0; i < 20; i++) c8Line(b, i * 16 - 40, 60, i * 16 + 20, 188, C_PANEL);
	bool hot = r->tension > 256;
	c8TextC(b, 128, 6, "SCRATCH TO REEL!", hot ? C_RED : C_YELLOW, 1);
	c8Text(b, 6, 24, "TENSION", C_GREY, 1);
	c8Rect(b, 66, 24, 184, 8, C_PANEL2);
	int tw = r->tension > 333 ? 333 : r->tension;
	c8Rect(b, 66, 24, tw * 184 / 333, 8, hot ? ((frame % 6) < 3 ? C_RED : C_WHITE) : r->tension > 192 ? C_ORANGE : C_GREEN);
	c8Rect(b, 66 + 184 * 256 / 333, 22, 1, 12, C_WHITE);
	c8Text(b, 6, 38, "LINE", C_GREY, 1);
	c8Rect(b, 66, 38, 184, 8, C_PANEL2);
	c8Rect(b, 66, 38, (int)((long long)r->prog * 184 / r->need), 8, C_BLUE);
	if (r->over > 0) c8Rect(b, 66, 48, (r->over > 50 ? 50 : r->over) * 184 / 50, 2, C_RED);
	int cx = 128, cy = 118;
	c8Ellipse(b, cx, cy, 34, 34, C_REEL); c8Ellipse(b, cx, cy, 33, 33, C_REEL);
	static const s8 SIN16[16] = { 0, 12, 22, 29, 31, 29, 22, 12, 0, -12, -22, -29, -31, -29, -22, -12 };
	static int ang = 0; ang += r->sp / 256; if (ang < 0) ang = 0;
	for (int i = 0; i < 3; i++) {
		int a = (ang / 4 + i * 5) & 15, a2 = (a + 4) & 15;
		c8ThickLine(b, cx, cy, cx + SIN16[a2], cy + SIN16[a], 2, C_LIGHT);
	}
	c8Disc(b, cx, cy, 5, C_LIGHT);
	const char *t = hot ? "TOO FAST!" : r->sp < 512 ? "faster!" : "";
	c8TextC(b, 128, 166, t, hot ? C_RED : C_GREY, 1);
}

static void drawBottomPlay(u8 *b) {
	if (st == ST_REEL) { drawReel(b); return; }
	c8Clear(b, C_BG);
	drawStatusBar(b);
	switch (page) {
	case PG_BAIT: drawBaitPage(b); break;
	case PG_LAKE: drawLakePage(b); break;
	case PG_LOG:  drawLogPage(b); break;
	case PG_STAT: drawStatPage(b); break;
	case PG_GOAL: drawGoalPage(b); break;
	}
}

static void drawBottomTitle(u8 *b) {
	c8Clear(b, C_BG);
	if (hasSave) {
		c8Rect(b, 48, 40, 160, 40, C_TILEEQ); c8Frame(b, 48, 40, 160, 40, C_LGREEN);
		c8Text(b, 128 - textW("CONTINUE", 2) / 2, 52, "CONTINUE", C_WHITE, 2);
		c8Rect(b, 48, 96, 160, 30, C_TILELOCK); c8Frame(b, 48, 96, 160, 30, titleConfirmNew ? C_RED : C_DIM);
		const char *t = titleConfirmNew ? "TAP AGAIN TO ERASE" : "NEW GAME";
		c8Text(b, 128 - textW(t, 1) / 2, 107, t, titleConfirmNew ? C_PINK : C_LIGHT, 1);
	} else if ((frame / 30) & 1) {
		c8Text(b, 128 - textW("TOUCH TO START", 2) / 2, 70, "TOUCH TO START", C_WHITE, 2);
	}
	const char *sv = saveAvailable() ? "Progress saves to the SD card" : "No SD card: progress won't save";
	c8Text(b, 128 - textW(sv, 1) / 2, 150, sv, saveAvailable() ? C_GREY : C_PINK, 1);
	const char *mu = musicEnabled() ? "SELECT: music on" : "SELECT: music off";
	c8Text(b, 128 - textW(mu, 1) / 2, 166, mu, C_GREY, 1);
}

static void startPlay(bool fresh) {
	if (fresh) { saveDelete(); newGame(); persist(); }
	mode = MODE_PLAY; st = ST_AIM; stT = 0; page = PG_BAIT; logLake = S.lake; viewX = 128;
	panoLake = -1;
	sfxTone(660, 6, 0, 45); sfxTone(990, 12, 0, 45);
	say(LAKES[S.lake].name, "A: cast  Scratch: reel", C_WHITE, 140);
	missionCheck();
}

static void updateTitle(int down, bool tapped, int tx, int ty) {
	musicPlay(SONG_TITLE);
	if (hasSave) {
		if ((down & (KEY_A | KEY_START)) || (tapped && ty >= 40 && ty < 80 && tx >= 48 && tx < 208)) { startPlay(false); return; }
		if (tapped && ty >= 96 && ty < 126 && tx >= 48 && tx < 208) {
			if (titleConfirmNew) { startPlay(true); titleConfirmNew = 0; }
			else { titleConfirmNew = 120; sfxTone(220, 8, 0, 40); }
		}
		if (titleConfirmNew > 0) titleConfirmNew--;
	} else if ((down & (KEY_A | KEY_START)) || tapped) startPlay(true);
}

// ------------------------------------------------------------------ main
int main(int argc, char **argv) {
	(void)argc; (void)argv;
	videoSetMode(MODE_5_2D);
	videoSetModeSub(MODE_5_2D);
	vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
	vramSetBankB(VRAM_B_MAIN_BG_0x06020000);
	vramSetBankD(VRAM_D_MAIN_BG_0x06040000);
	vramSetBankC(VRAM_C_SUB_BG);
	bgPano = bgInit(3, BgType_Bmp16, BgSize_B16_512x256, 0, 0);
	bgOv = bgInit(2, BgType_Bmp8, BgSize_B8_256x256, 16, 0);
	bgSub = bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
	bgSetPriority(bgOv, 0); bgSetPriority(bgPano, 1);
	gfxInitPalette();
	dmaFillWords(0, OV_VRAM(0), 0x20000);
	dmaFillWords(0, SUB_VRAM(0), 0x20000);

	audioInit();
	rngS ^= (u32)time(NULL) * 2654435761u;
	if (!rngS) rngS = 1;

	saveInit();
	newGame();
	hasSave = saveLoad(&S);
	if (!hasSave) newGame();
	if (S.musicOn == 0) musicSetEnabled(false);

	while (pmMainLoop()) {
		scanKeys();
		int down = keysDown(), held = keysHeld();
		touchPosition tp; bool touching = held & KEY_TOUCH;
		bool tapped = false; int tx = 0, ty = 0;
		if (touching) {
			touchRead(&tp); tx = tp.px; ty = tp.py;
			if (down & KEY_TOUCH) tapped = true;
			else if (lastTX >= 0) {
				int dx = abs(tx - lastTX), dy = abs(ty - lastTY);
				int d = dx > dy ? dx + dy / 2 : dy + dx / 2;
				if (d >= 2 && d < 120) scratchAccum += d;    // ignore jitter and glitchy jumps
			}
			lastTX = tx; lastTY = ty;
		} else lastTX = lastTY = -1;
		rnd();

		if (down & KEY_SELECT) { musicSetEnabled(!musicEnabled()); S.musicOn = musicEnabled(); if (mode == MODE_PLAY) persist(); }

		frame++;
		if (mode == MODE_TITLE) {
			updateTitle(down, tapped, tx, ty);
		} else {
			if (tapped && st != ST_REEL) tapPlay(tx, ty);
			updatePlay(down, held);
		}
		scratchAccum = 0;
		if (msgT > 0) msgT--;
		if (flashT > 0) flashT--;
		if (toastT > 0) toastT--;
		audioUpdate();

		ensurePanorama();
		if (mode == MODE_TITLE) { drawTopTitle(topOv); drawBottomTitle(botBuf); }
		else { drawTopPlay(topOv); drawBottomPlay(botBuf); }

		int ovBack = ovPage ^ 1, subBack = subPage ^ 1;
		DC_FlushRange(topOv, sizeof topOv);
		DC_FlushRange(botBuf, sizeof botBuf);
		dmaCopy(topOv, OV_VRAM(ovBack), sizeof topOv);
		dmaCopy(botBuf, SUB_VRAM(subBack), sizeof botBuf);

		swiWaitForVBlank();
		ovPage = ovBack; subPage = subBack;
		bgSetMapBase(bgOv, 16 + ovPage * 4);
		bgSetMapBase(bgSub, subPage * 4);
		bgSetScroll(bgPano, mode == MODE_TITLE ? 128 + ((frame / 4) % 256 < 128 ? (frame / 4) % 128 : 128 - (frame / 4) % 128) - 64 : viewX, 0);
		bgUpdate();
	}
	return 0;
}
