// StarCam ARM7 core: input (touch, X/Y, lid), sound, RTC and system services.
// Based on the BlocksDS default ARM7 core, without Wi-Fi or Maxmod.
// SPDX-License-Identifier: Zlib

#include <nds.h>

volatile bool exit_loop = false;

static void power_button_callback(void)
{
    exit_loop = true;
}

static void vblank_handler(void)
{
    inputGetAndSend();
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    enableSound();
    readUserSettings();
    ledBlink(LED_ALWAYS_ON);
    touchInit();

    irqInit();
    fifoInit();

    installSoundFIFO();
    installSystemFIFO();

    setPowerButtonCB(power_button_callback);
    initClockIRQTimer(LIBNDS_DEFAULT_TIMER_RTC);

    irqSet(IRQ_VBLANK, vblank_handler);
    irqEnable(IRQ_VBLANK);

    while (!exit_loop)
    {
        const uint16_t key_mask = KEY_SELECT | KEY_START | KEY_L | KEY_R;
        uint16_t keys_pressed = ~REG_KEYINPUT;

        if ((keys_pressed & key_mask) == key_mask)
            exit_loop = true;

        swiWaitForVBlank();
    }

    return 0;
}
