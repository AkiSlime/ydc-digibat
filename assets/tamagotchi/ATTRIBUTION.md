# Tamagotchi Bat Sprite Attribution

The bat sprite artwork used by this firmware is adapted from:

- Title: Pixel Art Bat 1BIT
- Author: dustdfg
- Source: https://dustdfg.itch.io/pixel-art-bat-1bit
- Original download: pixel_art_bat.zip
- Original asset license: Creative Commons Attribution-ShareAlike 4.0 International
  (CC BY-SA 4.0)
- License URL: https://creativecommons.org/licenses/by-sa/4.0/
- Local license text: ../../licenses/CC-BY-SA-4.0.txt

## Covered Files

This attribution applies to the adapted bat sprite artwork and converted bitmap
data in:

- assets/tamagotchi/bat_normal.png
- assets/tamagotchi/bat_angry.png
- assets/tamagotchi/bat_sleep.png
- assets/tamagotchi/bat_eating.png
- assets/tamagotchi/bat_fighting.png
- include/tamagotchi_bat_sprites.h

## Changes

The original artwork was adapted for this firmware:

- selected and renamed sprite sheets and animation states;
- converted/adapted frames for 1-bit OLED rendering;
- split/repacked frames as C++ PROGMEM bitmap arrays for ESP32 firmware;
- mapped frames to idle, move, hurt, sleep, eat, and arena attack states.

Additional project-specific edits were made by the firmware author:

- `assets/tamagotchi/bat_sleep.png`;
- `assets/tamagotchi/bat_eating.png`;
- `assets/tamagotchi/bat_fighting.png`;
- the corresponding converted bitmap frames in `include/tamagotchi_bat_sprites.h`.

These edited files remain adaptations of the original `Pixel Art Bat 1BIT`
artwork and are distributed under CC BY-SA 4.0.

## License Note

The adapted bat sprite artwork and converted bitmap data listed above are
distributed under CC BY-SA 4.0.

The firmware source code and other project assets may be licensed separately.
This attribution file does not change the license of files outside the listed
bat sprite artwork and converted bitmap data.

No endorsement by the original author is implied.
