#pragma once
#include <nds/ndstypes.h>
#include "data.h"

#define SAVE_MAGIC 0x54494142u /* "BAIT" */
#define SAVE_VERSION 1

typedef struct {
	u32 magic, version;
	u32 money, earned, totalCaught;
	u16 baitsOwned; u8 equipped; u8 lakesOwned;
	u8 lake; u8 musicOn; u8 rain; u8 nextMission;
	u16 clock; u16 pad;
	u8 activeMission[3]; u8 pad2;
	u32 missionProg[3];
	u16 count[NUM_SPECIES];
	u16 heaviest[NUM_SPECIES]; // 10 g units
	u32 best[NUM_SPECIES];
	u32 checksum;
} SaveData;

bool saveInit(void);          // mounts the SD/flashcart; false if no storage
bool saveAvailable(void);
bool saveLoad(SaveData *s);   // false if no valid save
bool saveWrite(SaveData *s);
void saveDelete(void);
