# StarCam (Nintendo DS)

A stargazing camera game for the Nintendo DS / DSi. Aim the camera at the night
sky, find 14 targets (9 constellations, the Moon, Jupiter, Saturn, a comet and a
shooting star), and photograph them for your star log. Each photo is rated 1-3
stars depending on how well you frame the target.

## Install

Copy `StarCam.nds` to your SD card (next to your other games) and launch it from
your flashcart menu or TWiLight Menu++.

## Controls

| Button | Action |
| --- | --- |
| D-pad | Aim the camera |
| L / R | Zoom out / in |
| A | Take a photo |
| B or Select | Guide lines on/off |
| X / Y | Next / previous target |
| Start | Photo album |
| Touch screen | All of the above, plus the zoom slider |

In the album: D-pad to pick a photo, A to view it full screen, X to delete it, B to go back.

Progress is saved to `starcam.sav` and photos to `starcam_photos/` next to the
game on the SD card. Photos are normal BMP images you can open on a computer.

## Building

Built with [BlocksDS](https://blocksds.skylyrac.net): `make` in this folder.
