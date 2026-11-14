#include "save.h"
#include <fat.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

static bool fatOk = false;
static const char *PATH = "bait.sav";

static u32 sum(const SaveData *s) {
	const u8 *p = (const u8 *)s;
	u32 h = 2166136261u;
	for (unsigned i = 0; i < offsetof(SaveData, checksum); i++) { h ^= p[i]; h *= 16777619u; }
	return h;
}

bool saveInit(void) { fatOk = fatInitDefault(); return fatOk; }
bool saveAvailable(void) { return fatOk; }

bool saveLoad(SaveData *s) {
	if (!fatOk) return false;
	FILE *f = fopen(PATH, "rb");
	if (!f) return false;
	SaveData tmp;
	size_t n = fread(&tmp, 1, sizeof tmp, f);
	fclose(f);
	if (n != sizeof tmp || tmp.magic != SAVE_MAGIC || tmp.version != SAVE_VERSION || tmp.checksum != sum(&tmp)) return false;
	*s = tmp;
	return true;
}

bool saveWrite(SaveData *s) {
	if (!fatOk) return false;
	s->magic = SAVE_MAGIC; s->version = SAVE_VERSION; s->checksum = sum(s);
	FILE *f = fopen(PATH, "wb");
	if (!f) return false;
	size_t n = fwrite(s, 1, sizeof *s, f);
	fclose(f);
	return n == sizeof *s;
}

void saveDelete(void) { if (fatOk) remove(PATH); }
