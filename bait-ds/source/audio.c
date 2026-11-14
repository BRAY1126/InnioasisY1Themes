// Chiptune music and sound effects on the DS PSG (square wave + noise) channels.
#include <nds.h>
#include <string.h>
#include <math.h>
#include "audio.h"

typedef struct { const char *mel, *bass; int step; int vol; } SongDef;

static const SongDef SONGS[SONG_COUNT] = {
	{ // title: bright and bouncy
	  "E5 G5 C6 G5 A5 G5 E5 . F5 A5 C6 A5 G5 . . . E5 G5 C6 G5 A5 G5 E5 C5 D5 F5 E5 D5 C5 . . -",
	  "C3 G3 C3 G3 A2 E3 A2 E3 F2 C3 F2 C3 G2 D3 G2 D3 C3 G3 C3 G3 A2 E3 A2 E3 G2 D3 G2 D3 C3 G3 C3 -",
	  8, 34 },
	{ // day on the water: lazy pentatonic
	  "G4 . A4 . C5 . . . D5 . C5 . A4 . G4 . E4 . G4 . A4 . . . G4 . . . - - - - "
	  "C5 . D5 . E5 . . . D5 . C5 . A4 . . . G4 . A4 . C5 . A4 . C5 . . . - - - -",
	  "C3 . . . G2 . . . F2 . . . C3 . . . A2 . . . E2 . . . G2 . . . G2 . . . "
	  "C3 . . . G2 . . . F2 . . . F2 . . . G2 . . . G2 . . . C3 . . . C3 . . .",
	  12, 26 },
	{ // night: A minor, slow
	  "A4 . C5 . E5 . . . D5 . C5 . B4 . . . A4 . E4 . G4 . . . A4 . . . - - - -",
	  "A2 . . . E2 . . . F2 . . . C3 . . . D2 . . . E2 . . . A2 . . . A2 . . .",
	  14, 22 },
};

#define MAX_STEPS 96
typedef struct { s8 mel[MAX_STEPS], bass[MAX_STEPS]; int len; } Parsed;
static Parsed parsed[SONG_COUNT];
static u16 noteHz[128];

static int parse(const char *s, s8 *out) {
	int n = 0;
	static const int semis[7] = { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
	while (*s && n < MAX_STEPS) {
		while (*s == ' ') s++;
		if (!*s) break;
		if (*s == '.') { out[n++] = -1; s++; }
		else if (*s == '-') { out[n++] = 0; s++; }
		else {
			int semi = semis[*s - 'A']; s++;
			if (*s == '#') { semi++; s++; }
			int oct = *s - '0'; s++;
			out[n++] = (s8)(12 * (oct + 1) + semi);
		}
	}
	return n;
}

static int curSong = SONG_NONE, stepIdx, stepTimer;
static int chMel = -1, chBass = -1;
static int melAge, bassAge, melNote, bassNote;
static bool enabled = true;

typedef struct { int id, frames, hz, slide, vol; bool noise; } Sfx;
static Sfx sfx[2];

void audioInit(void) {
	soundEnable();
	for (int i = 0; i < 128; i++) {
		float f = 440.0f * powf(2.0f, (i - 69) / 12.0f);
		noteHz[i] = f > 60000 ? 60000 : (u16)f;
	}
	for (int i = 0; i < SONG_COUNT; i++) {
		int a = parse(SONGS[i].mel, parsed[i].mel), b = parse(SONGS[i].bass, parsed[i].bass);
		parsed[i].len = a < b ? a : b;
	}
	memset(sfx, 0, sizeof sfx);
	sfx[0].id = sfx[1].id = -1;
}

static void stopMusicChannels(void) {
	if (chMel >= 0) soundKill(chMel);
	if (chBass >= 0) soundKill(chBass);
	chMel = chBass = -1;
}

void musicPlay(int song) {
	if (song == curSong) return;
	stopMusicChannels();
	curSong = song; stepIdx = 0; stepTimer = 0; melNote = bassNote = 0;
	if (song == SONG_NONE || !enabled) return;
	chMel = soundPlayPSG(DutyCycle_25, 440, 0, 64);
	chBass = soundPlayPSG(DutyCycle_50, 110, 0, 64);
}

void musicSetEnabled(bool on) {
	enabled = on;
	int s = curSong; curSong = SONG_NONE - 1;
	if (!on) { stopMusicChannels(); curSong = s; return; }
	musicPlay(s);
}
bool musicEnabled(void) { return enabled; }

static void sfxStart(int slot, bool noise, int hz, int frames, int slide, int vol) {
	Sfx *s = &sfx[slot];
	if (s->id >= 0) soundKill(s->id);
	s->id = noise ? soundPlayNoise(hz, vol, 64) : soundPlayPSG(DutyCycle_50, hz, vol, 64);
	s->frames = frames; s->hz = hz; s->slide = slide; s->vol = vol; s->noise = noise;
}

void sfxTone(int hz, int frames, int slide, int vol) {
	int slot = (sfx[0].id < 0 || (sfx[1].id >= 0 && sfx[0].frames <= sfx[1].frames)) ? 0 : 1;
	sfxStart(slot, false, hz, frames, slide, vol);
}
void sfxNoise(int hz, int frames, int vol) { sfxStart(1, true, hz, frames, 0, vol); }
void sfxBeep(int hz) { sfxTone(hz, 4, 0, 40); }

void audioUpdate(void) {
	for (int i = 0; i < 2; i++) {
		Sfx *s = &sfx[i];
		if (s->id < 0) continue;
		if (--s->frames <= 0) { soundKill(s->id); s->id = -1; continue; }
		if (s->slide) { s->hz += s->slide; if (s->hz < 40) s->hz = 40; soundSetFreq(s->id, s->hz); }
	}
	if (curSong < 0 || !enabled || chMel < 0) return;
	const Parsed *p = &parsed[curSong];
	const SongDef *d = &SONGS[curSong];
	if (stepTimer-- <= 0) {
		stepTimer = d->step - 1;
		s8 m = p->mel[stepIdx], b = p->bass[stepIdx];
		if (m > 0) { melNote = m; melAge = 0; soundSetFreq(chMel, noteHz[m]); }
		else if (m == 0) melNote = 0;
		if (b > 0) { bassNote = b; bassAge = 0; soundSetFreq(chBass, noteHz[b]); }
		else if (b == 0) bassNote = 0;
		stepIdx = (stepIdx + 1) % p->len;
	}
	// simple pluck envelope
	int mv = melNote ? d->vol * (64 - (melAge > 40 ? 40 : melAge)) / 64 : 0;
	int bv = bassNote ? (d->vol * 3 / 4) * (64 - (bassAge > 30 ? 30 : bassAge)) / 64 : 0;
	soundSetVolume(chMel, mv);
	soundSetVolume(chBass, bv);
	melAge++; bassAge++;
}
