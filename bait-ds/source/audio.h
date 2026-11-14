#pragma once
#include <nds/ndstypes.h>

enum { SONG_NONE = -1, SONG_TITLE = 0, SONG_DAY, SONG_NIGHT, SONG_COUNT };

void audioInit(void);
void audioUpdate(void);               // call once per frame
void musicPlay(int song);
void musicSetEnabled(bool on);
bool musicEnabled(void);
void sfxTone(int hz, int frames, int slide, int vol);   // square blip, slide = Hz per frame
void sfxNoise(int hz, int frames, int vol);
// shorthand
void sfxBeep(int hz);
