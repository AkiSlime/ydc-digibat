# MEMORY.md

## 2026-07-06 - Placement Bat et texte activite ajustes

Decision: move the Bat render Y from `13` to `10`, enlarge the activity label
font from `u8g2_font_4x6_tf` to `u8g2_font_5x8_tf`, move the activity baseline
from `50` to `47`, and place the E/H bars at `52/59`.

Why: the Bat was still visually too low, and the activity text needed to be
larger with at least 3 px of margin before the stat bars.

Rejected for now: changing sprite assets, moving the X position, changing the
XBM renderer, changing animation priorities, and running PlatformIO.

## 2026-07-06 - Priorites animations Tamagotchi V2

Decision: set the Tamagotchi V2 sprite priority to `SLEEP` using the sleep
animation, `EAT` using the eating animation, `HUNT` using the angry Bat
animation, then low hunger, low energy, and active notices using the hurt
animation. Normal idle/move stays on the base Bat animation.

Why: the user defined the action-driven visual states and asked that energy,
alerts, network, and similar status feedback stay on the hurt animation.

Rejected for now: adding more activity-specific sprite states, changing sprite
assets, changing animation timings, and running PlatformIO validation.

## 2026-07-06 - Animation eating Bat et placement dashboard

Decision: integrate the user's `bat_eating.png` as `tmg_bat_eating_frames`,
render it during `PET_STATE_EAT`, move the Bat render Y from `16` to `13`, and
move the energy/hunger bars from `54/61` to `51/58`.

Why: the user validated sleeping after the XBM bit-order fix, added an eating
spritesheet, and reported that the Bat and E/H bars were too low, with hunger
being clipped at the bottom of the 64px display.

Rejected for now: changing the XBM renderer, touching sleeping/idle/move/hurt
animations, adding hunting animation, and running PlatformIO.

## 2026-07-06 - Bit order sleeping Bat corrige

Decision: regenerate only `tmg_bat_sleep_0..3` using the XBM little-endian bit
order expected by `readXbmPixel()` (`1 << (x % 8)`).

Why: the previous PNG conversion wrote each byte MSB-first, which mirrored every
8-pixel chunk and made the sleeping sprite look split or mirrored even though
the PNG itself was centered.

Rejected for now: changing `readXbmPixel()`, modifying the renderer, touching
idle/move/hurt animations, and running PlatformIO.

## 2026-07-06 - Animation sleeping Bat recentree

Decision: replace the previously integrated sleeping Bat spritesheet with the
user's corrected, centered `bat_sleep.png`, then regenerate only
`tmg_bat_sleep_0..3` in `include/tamagotchi_bat_sprites.h`.

Why: the first user-provided spritesheet was not centered correctly, so the
firmware rendering position was wrong even though the integration path was
valid.

Rejected for now: changing `PET_STATE_SLEEP` rendering logic, editing other Bat
animations, integrating new activity animations, and running PlatformIO.

## 2026-07-06 - Animation sleeping Bat integree

Decision: integrate the user's `bat_sleep.png` as the firmware sleeping
animation. The source PNG is stored at `assets/tamagotchi/bat_sleep.png`, the
four 32x32 frames are converted into `tmg_bat_sleep_frames` in
`include/tamagotchi_bat_sprites.h`, and `PET_STATE_SLEEP` now renders that
animation from `currentPetSprite()`.

Why: the user rejected the generated prototype style and chose their own
sleeping spritesheet as the validated asset to integrate.

Rejected for now: integrating the rejected prototype PNGs, adding hunting/eating
animations, changing existing idle/move/hurt frames, moving the sleeping sprite
during the left-right idle cycle, and running PlatformIO validation.

## 2026-07-06 - Prototypes PNG nouvelles animations Bat

Decision: create PNG-only Bat animation prototypes for `sleeping`, `hunting`,
`eating`, and `idle_variant` in `assets/tamagotchi/bat_prototypes/`, with a
small standard-library generator in `tools/generate_bat_animation_prototypes.py`.

Why: the user chose visual PNG validation before firmware integration, and the
sleeping direction should show the Bat upside down with wings folded inward.

Rejected for now: editing existing Bat sprites, changing firmware headers,
running PlatformIO, and integrating angry-eye variants before visual validation.

## 2026-07-06 - Barres E/H implementees firmware

Decision: implement the validated pixel-art `H` and `E` bars in `src/main.cpp`
with a dedicated renderer. The bars use 3x5 labels, a 4 px label-to-bar gap,
10 max segments, 2x5 segment size, 1 px segment gaps, and
`floor(value / 10)` clamped from 0 to 10.

Why: the mockup was validated and the user asked to implement it.

Rejected for now: changing the `S` sleep gauge, changing the current
mode-derived stat values, running PlatformIO validation, and broader interface
redesign before the next Figma-guided element.

## 2026-07-06 - Barres E/H pixel-art

Decision: design the energy and hunger bars as 10 discrete segments where each
segment is worth 10%. Each segment is 2 px wide by 5 px high, with 1 px between
segments and exactly 4 px between the 3x5 label and the first segment. The `E`
and `H` labels are 3 px wide by 5 px high. Use `floor(value / 10)` for visible
segments, clamped from 0 to 10.

Why: the user provided a Figma pixel-art reference and wants each lost 10% to
remove exactly one segment from both energy and hunger.

Rejected for now: continuous filled bars, icon-based labels, wider segment
spacing, and firmware changes before visual validation of the mockup.

## 2026-07-06 - Hibou statique retenu, animations rejetées

Decision: keep only the static owl base sprite
`assets/tamagotchi/owl_proposals/owl_from64_32_direct_1bit.png` and its 8x
preview for now. The user will create pixel-art animations separately and will
provide a Figma design for the interface redesign.

Why: the generated owl animations were not good enough, and the next UI work
should follow the user's Figma design rather than an agent-proposed layout.

Rejected for now: generated owl animation sheets, old owl proposal images, and
redesigning the interface without the Figma reference.

## 2026-07-06 - Hibou animation base retenue

Decision: use `assets/tamagotchi/owl_proposals/owl_from64_32_direct_1bit.png`
as the owl base sprite and create a PNG-only 4x4 animation sheet with four
32x32 frames each for `idle`, `happy`, `hunger`, and `tired`.

Why: the user preferred the direct 32x32 owl conversion, and validating the
animated PNG sheet before firmware integration keeps the change easy to review.

Rejected for now: generating the C `PROGMEM` header, replacing the bat in
firmware, and keeping the discarded `owl_from64_*` exploration images.

## 2026-07-06 - Hibou approche 64px puis 32px

Decision: use a 64x64 one-bit conversion as an intermediate reference, then
derive/select a 32x32 one-bit OLED sprite from that reference before any
firmware integration.

Why: the source owl image has enough detail to study at 64x64, but the current
Tamagotchi renderer and bat pipeline are built around 32x32 frames.

Rejected for now: replacing the bat in firmware, generating a full owl
spritesheet before choosing the base 32x32 sprite, and treating a raw 32x32
resize as the final asset without visual review.

## 2026-07-06 - Hibou pixel-art one-bit

Decision: create multiple 32x32 one-bit owl sprite proposals before integrating anything into firmware.

Why: the source owl image is not true pixel art, and the current Tamagotchi character pipeline already uses 32x32 monochrome frames for the bat.

Rejected for now: direct firmware integration, overwriting bat assets, and starting with a 64x64 sprite.

## 2026-07-06 - Tamagotchi V2 autonomous engine

Decision: build Tamagotchi V2 on top of finalized V1, using a real autonomous
activity engine with `hunger`, `energy`, and `food` instead of fixed mode-derived
gauges.

Why: the desired behavior is that actions have consequences. Hunting should
spend energy and may add food, eating should consume food and restore hunger,
resting or sleeping should restore energy, and idle should be the main user
interaction window.

Rejected for now: a separate sleep/fatigue stat, a complex mood/evolution system,
and implementing future activities such as play before the core loop is stable.

## 2026-07-06 - Branch bases for Tamagotchi work

Decision: keep `before-tamagotchi` at the initial baseline commit, finalize V1 on
`codex/tamagotchi-v1`, and make `tamagotchi-v2` start from that finalized V1
commit.

Why: `before-tamagotchi` should represent the portable pre-Tamagotchi baseline,
while V2 needs the visual/layout/OTA improvements already made in V1.

Rejected for now: starting V2 from the initial baseline, because that would lose
the V1 work that V2 should build on.

## 2026-07-06 - Tamagotchi V2 joueur d'abord

Decision: rewrite the Tamagotchi V2 plan as a French mini-GDD where the pet is
mostly player-directed. The default action is `IDLE`; `HUNT`, `EAT`, and `SLEEP`
are manual actions, and `REST` is removed from the current rules.

Why: the desired behavior is that the pet waits for instructions, while stats
and blocking states guide the player. Native behavior should indicate needs and
block impossible actions, not launch actions automatically.

Rejected for now: a premature implementation document, automatic hunting,
automatic eating, automatic sleeping, and a separate rest action.

## 2026-07-06 - Règles chiffrées Tamagotchi V2

Decision: lock the first numeric rules for the player-directed V2 engine.
`HUNT` requires `energy >= 10`, lasts `1h`, costs `10` energy, costs uniform
random hunger `0..10`, and keeps weighted loot results `+0 50%`, `+1 30%`,
`+2 15%`, `+4 5%`. `EAT` lasts `10 min`, consumes one food, restores `20`
hunger, and costs uniform random energy `0..5`. `SLEEP` has no minimum duration,
restores `10` energy per hour, drains `3` hunger per hour, and auto-wakes at
`08:00` regardless of energy. Initial stats are `food = 1`, `hunger = 50`, and
`energy = 50`.

Why: these values resolve the remaining gameplay ambiguities and let integration
proceed in two passes: gameplay engine first, UI/menu refinement second.

Rejected for now: uniform loot probabilities, a minimum sleep duration, automatic
sleep based on low energy, French UI messages, and automatic activity selection.

## 2026-07-06 - Implémentation moteur Tamagotchi V2 passe 1

Decision: implement the first firmware pass of the player-directed V2 engine on
`tamagotchi-v2`. The firmware now stores real `hunger`, `energy`, and `food`
stats, supports `IDLE`, `HUNT`, `EAT`, and `SLEEP`, keeps H/E pixel-art bars,
shows food as `F:NN`, and uses English UI messages/actions.

Why: the gameplay rules were resolved enough to replace the V1 mode-derived
gauges with real persistent stats and timed player-triggered actions.

Rejected for now: PlatformIO build/upload, automatic action selection, a `REST`
action, countdown display, and replacing the current pet sprite pipeline.

## 2026-07-06 - Compteur action et steak UI

Decision: show remaining action time by alternating the speech bubble between
the action label and the remaining time for timed actions. Replace `F:NN` with
`NN` followed by a small monochrome steak icon in the same dashboard location.

Why: the user wanted a lightweight time counter without redesigning the layout,
and wanted the food inventory to read visually as steaks instead of a letter
label.

Rejected for now: a permanent dedicated countdown area, moving the food display,
and changing the broader dashboard layout before Figma-guided UI work.

## 2026-07-06 - Compteur action en libellé combiné

Decision: show the current timed action and remaining time together in the
speech bubble, for example `HUNT 42M` or `EAT 08M`, instead of alternating
between action name and time.

Why: the combined label is easier to read than a fast alternation and keeps the
UI compact without adding a separate countdown zone.

Rejected for now: alternating every five seconds and adding a permanent
dedicated countdown area.

## 2026-07-06 - Layout bas droit action et stats

Decision: move the activity/status label to the lower-right dashboard area above
the H/E bars, move the H/E bars to the bottom-right area, and remove the food
counter from the home screen while keeping food as an internal stat.

Why: the user provided a mockup direction where activity and stats are grouped
as one right-aligned HUD block, and decided food should not be displayed in that
block for now.

Rejected for now: a centered speech bubble for activity, a food count or steak
icon on the home screen, and a framed HUD panel.
