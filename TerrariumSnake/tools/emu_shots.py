#!/usr/bin/env python3
"""Headless smoke test: boots Terrarium.nds in DeSmuME (py-desmume), plays a
scripted session and saves screenshots. Usage:
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy python3 tools/emu_shots.py Terrarium.nds outdir
"""
import os, sys
from desmume.emulator import DeSmuME
from desmume.controls import Keys, keymask

rom, out = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
os.makedirs(out, exist_ok=True)
emu = DeSmuME()
emu.open(rom)

def run(frames):
    for _ in range(frames):
        emu.cycle(with_joystick=False)

def press(key, hold=4, after=6):
    emu.input.keypad_add_key(keymask(key))
    run(hold)
    emu.input.keypad_rm_key(keymask(key))
    run(after)

def shot(name):
    emu.screenshot().save(os.path.join(out, name + ".png"))
    print("saved", name)

script = sys.argv[3] if len(sys.argv) > 3 else "tour"
run(150); shot("01_title")
press(Keys.KEY_A); run(60); shot("02_reveal")
press(Keys.KEY_A); run(240); shot("03_hiding")
# mist above the hide to coax the snake out
for _ in range(3): press(Keys.KEY_UP)
for _ in range(9): press(Keys.KEY_A, after=10)
run(10); shot("04_misting")
run(600); shot("05_roaming")
# build mode: pick the stone cave and place it bottom-left
press(Keys.KEY_Y); press(Keys.KEY_Y)
for _ in range(8): press(Keys.KEY_LEFT)
for _ in range(8): press(Keys.KEY_DOWN)
shot("06_build_preview")
press(Keys.KEY_X); run(30); shot("07_placed")
press(Keys.KEY_SELECT)
# release a rat in the middle
for _ in range(4): press(Keys.KEY_RIGHT)
for _ in range(3): press(Keys.KEY_UP)
press(Keys.KEY_R); run(90); shot("08_rat")
for i in range(12):
    run(30); shot("09_hunt_%02d" % i)
run(900); shot("10_after_meal")
press(Keys.KEY_START); run(10); shot("11_pause")
