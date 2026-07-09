# MEMORY.md

## 2026-07-09 - Duree Eat reduite

Decision: reduce `EAT` duration from `10 min` to `3 min` by changing
`PET_EAT_DURATION_MS` in firmware defaults and the example config, then update
README and docs where the meal duration is documented.

Why: the user confirmed that "hit" meant `EAT` and chose the full firmware,
config example, README, and docs update path.

Rejected for now: changing Eat food cost, hunger gain, random energy cost,
queue behavior, Hunt, Arena, and running PlatformIO validation.

## 2026-07-09 - Guide utilisateur firmware simple

Decision: create `docs/firmware-user-guide.md` as a simple non-technical user
guide focused on what the firmware does, how to navigate it, the dashboard
pages, the Tamagotchi loop, actions, alerts, LED behavior, and OTA status.

Why: the user chose the simple guide format after the README/code audit and
wanted a readable `.md` for someone installing and using the firmware.

Rejected for now: a full installation guide, a longer technical guide, changing
README again, running PlatformIO validation, and documenting low-level formulas
in detail.

## 2026-07-09 - Chances HUNT et docs alignees

Decision: reduce the base chance of `HUNT` returning zero food from `50%` to
`20%`, redistributing the non-zero outcomes proportionally as `+1 48%`,
`+2 24%`, and `+4 8%`. Update README and gameplay docs to match current
firmware constants for `HUNT`, `ARENA`, activity menus, run selectors, and the
home `F` gauge.

Why: the user wanted fewer empty hunts and asked for the README/docs to be
updated after the code audit showed drift.

Rejected for now: changing Hunt duration/costs again, changing stat bonus
formulas, changing Arena mechanics, running PlatformIO validation, and touching
unrelated assets or OTA behavior.

## 2026-07-09 - Stats multipage guide et runs Arena

Decision: make `STATS` a four-page sheet with a visible `page/total` indicator:
combat stats, needs (`E`, `H`, `F`), guide (`E = ENERGY`, `H = HUNGER`,
`F = FOOD`, `MAX E/H 100`), and info/title/skill. Add an `ARENA RUNS`
selector that mirrors the numeric run selector style and displays total energy
and hunger cost before launch. Queued Arena runs start one at a time after the
previous result modal is dismissed.

Why: the user chose to keep guide content inside the stats flow and wanted
current page count visible on the stats screen, while also adding run selection
for Arena at launch.

Rejected for now: adding `GUIDE` as a main menu entry, changing Arena result
modal behavior, hiding per-run Arena result windows, changing costs/rewards,
updating README docs, and running PlatformIO validation.

## 2026-07-09 - Menu activite commun

Decision: replace the Hunt-only activity menu with a shared activity menu for
`HUNT`, `EAT`, `ARENA`, and `SLEEP`. The shared menu always exposes `STATS`,
`SCREEN OFF`, and `BACK`; `HUNT` also exposes `STOP`, and `SLEEP` also exposes
`WAKE UP`.

Why: the user wanted stats to remain visible/selectable during activities and
also wanted screen-off access from every activity menu.

Rejected for now: changing activity behavior, adding stats shortcuts outside
the menu, changing run selectors, changing Arena result behavior, and running
PlatformIO validation.

## 2026-07-09 - Cout energie Arena reduit

Decision: reduce Arena's launch energy requirements by setting
`PET_ARENA_MIN_ENERGY` and `PET_ARENA_ENERGY_COST` from `15` to `5`. Keep
`PET_ARENA_HUNGER_COST` at `5` and leave Arena end/resolution behavior
unchanged.

Why: the user wanted Arena to require and spend less energy while preserving
the existing end behavior.

Rejected for now: changing Arena hunger cost, changing Arena rewards/results,
changing combat timing, updating README docs, and running PlatformIO
validation.

## 2026-07-09 - Duree Hunt reduite

Decision: reduce `HUNT` duration from `60 min` to `20 min` by changing
`PET_HUNT_DURATION_MS` to `(20UL * 60UL * 1000UL)` in firmware defaults and the
example config.

Why: the user wanted hunting to resolve faster now that run quantity selection
exists.

Rejected for now: changing Hunt energy/faim costs again, changing Hunt rewards,
changing Arena costs, updating README docs, and running PlatformIO validation.

## 2026-07-09 - Selecteur run en input number

Decision: replace the `HUNT RUNS` and `EAT RUNS` list presentation with an
input-number style screen. Left/right now decrement/increment the selected
run value without wrapping, with `Cancel` before `1`. The selected value is
shown as `< Cancel >` or `< N >`; selecting `Cancel` closes the selector, while
selecting a number starts that many runs. Hunt shows consumed energy as
`E used/total`; Eat shows consumed food as `F used/total`.

Why: the user disliked the vertical `x1`, `x2`, `x3` list with detailed impact
rows and wanted a clearer numeric selector with direct cancellation.

Rejected for now: showing hunger impact in the selector, keeping a separate
`START` / `CANCEL` confirmation screen, changing the Eat random energy cost,
changing action durations/rewards, and running PlatformIO validation.

## 2026-07-09 - Selection quantite Eat

Decision: add an `EAT RUNS` submenu that mirrors the Hunt quantity selector.
The maximum run count is limited by both current food and useful hunger gain:
`min(food, ceil((100 - hunger) / PET_EAT_HUNGER_GAIN))`. Selected eats run
sequentially, and the completion LED pattern fires only after the final queued
eat.

Why: the user wanted the quantity selector applied to Eat as well, while
choosing to avoid wasting food beyond full hunger.

Rejected for now: allowing Eat runs up to total food regardless of hunger,
auto-stopping an oversized Eat queue, changing Eat rewards/costs, changing Hunt,
and running PlatformIO validation.

## 2026-07-08 - Chrono action en texte

Decision: replace the action countdown pie beside the activity tag with the
remaining time as text. Keep the action tag framed, then render the remaining
time to its right with a fixed margin. Keep Arena's wins display unchanged.

Why: the user wanted the countdown to be directly readable instead of a
depleting half-circle style indicator, with visible spacing between the tag and
the time.

Rejected for now: putting tag and time in the same frame, moving the timer into
a separate HUD area, changing action durations, and changing Arena display.

## 2026-07-08 - Ordre menu principal Tamagotchi

Decision: reorder the main Tamagotchi menu to open on `STATS`, then show
`SLEEP`, `EAT`, `HUNT`, `ARENA`, `SCREEN OFF`, and `BACK`. Interpret the user's
spoken `slip` / `hit` wording as the existing `SLEEP` / `EAT` actions.

Why: the user wanted the menu flow to prioritize status viewing, then core
maintenance actions, then Hunt/Arena and system controls.

Rejected for now: adding a separate display-order table, changing the sleep
submenu order, changing action behavior, and changing the red LED completion
pattern.

## 2026-07-08 - Selection quantite Hunt

Decision: add a `HUNT RUNS` submenu before launching Hunt. The menu lists run
counts from `x1` up to the current maximum allowed by energy and
`PET_HUNT_ENERGY_COST`, with each row previewing energy after the selection and
the maximum hunger cost range. Selected hunts run sequentially; the completion
LED pattern only fires after the final queued hunt, and `STOP` during Hunt
cancels the remaining queue.

Why: the user wanted to choose how many times to hunt before starting the
action, with feedback that makes the energy cost understandable.

Rejected for now: adding the same quantity selector to `EAT`, grouping all
hunt rewards into one delayed resolution, instant bulk hunting, changing hunt
loot probabilities, and running PlatformIO validation.

## 2026-07-08 - Hunt moins couteuse et jauges E/H continues

Decision: reduce `HUNT` to a lightweight repeated action by setting the minimum
energy, energy cost, and maximum hunger cost to `2`. Replace only the dashboard
`E` and `H` segmented bars with continuous framed bars for finer stat reading,
while leaving the `F` food bar segmented.

Why: the user wants Hunt runs to be cheaper before adding quantity selection,
and wants energy/hunger to show a more precise HUD than 10 coarse segments.

Rejected for now: changing Hunt rewards, changing `EAT`, replacing the food
bar, rewriting the full HUD layout, and running PlatformIO validation.

## 2026-07-07 - Animation Arena Bat fighting

Decision: use the user-provided `bat_fighting.png` spritesheet for `ARENA`.
The sheet is stored as `assets/tamagotchi/bat_fighting.png`, converted into
three 4-frame attack rows in `include/tamagotchi_bat_sprites.h`, and rendered
only during `PET_STATE_ARENA` as right-wing attack, left-wing attack, and
double-wing attack with a short pause between each row.

Why: the user chose the rhythmic sequence approach so Arena feels like a looped
combat animation instead of reusing the angry idle/move animation.

Rejected for now: changing `HUNT`, changing combat resolution, changing Arena
duration or rewards, adding enemy sprites, and running PlatformIO validation.

## 2026-07-07 - Signal LED fin de tache

Decision: use the red LED as a non-blocking completion signal for timed tasks:
when `HUNT`, `EAT`, or `ARENA` finishes normally, blink short / long / short
with `150 ms`, `600 ms`, and `150 ms` on phases separated by `150 ms` off
phases. The existing `HOT` LED behavior resumes after the pattern.

Why: the user chose the scoped implementation for task completion feedback and
did not want the signal tied to `SLEEP`.

Rejected for now: blocking `delay`-based blinking, triggering the pattern for
`SLEEP`, triggering it for cancelled `HUNT`, and replacing the `HOT` alert
semantics.

## 2026-07-07 - Durees menu idle et annulation Hunt

Decision: show the fixed durations directly in the idle Tamagotchi menu for
`HUNT` and `EAT`, and allow `SELECT` during `HUNT` to open a small `STOP` /
`BACK` menu. Stopping `HUNT` applies the normal hunt energy and hunger costs,
gives zero food reward, then returns the pet to `IDLE`.

Why: the user wanted the classic idle menu to show action durations, and wanted
activities to be cancellable without a full activity-system refactor. The first
cancel path is limited to `HUNT` because the user explicitly excluded `EAT`.

Rejected for now: cancellable `EAT`, cancellable `ARENA`, changing `SLEEP`, and
rewriting the activity completion system.

## 2026-07-07 - Mapping boutons menu corrige en profondeur

Decision: refactor the button handling model from ambiguous internal
`left/right` names to semantic `next/previous` names. `BUTTON_LEFT_PIN` remains
the next-page physical input for compatibility, but its interrupt now feeds
`nextButtonPending`; `BUTTON_RIGHT_PIN` feeds `previousButtonPending`. In the
vertical Tamagotchi menu, next moves down and previous moves up.

Why: the user reported that the intended menu inversion was not actually
reflected on-device, and the old internal names made the mapping ambiguous.

Rejected for now: renaming the public config macros, changing page navigation
semantics, and changing the physical wiring documentation.

## 2026-07-07 - Libelle idle chill

Decision: show `chill` in the dashboard activity panel when the pet is in
neutral `IDLE` with no active notice, low-stat warning, or action.

Why: the user wanted the existing activity text frame to remain populated while
the pet is waiting for an action.

Rejected for now: leaving the activity panel empty in neutral idle, using
uppercase `CHILL`, and adding a new idle sprite/state.

## 2026-07-07 - Level accueil et navigation menu

Decision: show the current `LV n` above the home E/H/F stat bars using a larger
font than the pixel labels, aligned with the stat block. Keep the vertical menu
navigation explicit as next button moves down and previous button moves up.

Why: the user wanted the level visible directly on the home screen above the
needs bars, and wanted the physical button behavior to match the vertical menu
direction observed on the device.

Rejected for now: moving level to the top activity row, adding XP to the home
screen, and changing the physical button roles outside menu navigation.

## 2026-07-07 - Menu vertical Stats et jauges EHF

Decision: replace the Tamagotchi horizontal menu with a full-screen vertical
menu navigated by the same left/right buttons, show the character sheet entry
as `STATS`, add `BACK` to the sleep menu after `WAKE UP` and `SCREEN OFF`, move
the home E/H/F stat block upward, and render food `F` as the same segmented bar
style as `E` and `H`.

Why: the user wanted the menu to read vertically even with the existing buttons,
wanted `STATS` instead of `SHEET`, wanted a non-action exit while sleeping, and
wanted the food display to be more readable and consistent with the other stats.

Rejected for now: a compact bottom menu, a right-side menu, keeping food as
dots, and adding new button semantics beyond the current left/right/select
controls.

## 2026-07-07 - Implementation Arena progression V1

Decision: implement the full first Arena/progression pass with menu order
`HUNT`, `EAT`, `SLEEP`, `ARENA`, `SHEET`, `SCREEN OFF`, `BACK`. Arena costs
`15` energy and `5` hunger, resolves one abstract combat every `2 min`, shows
wins instead of the countdown pie, and opens a `SELECT`-dismissed result modal
with wins, XP gained, and current level. XP uses
`targetWins = 3 + level * 2` and grants about `35%` of the next level at target
performance. Initial progression stats are `LV 1`, `XP 0`, `HP 10`, `ATK 2`,
`DEF 1`, and `LCK 1`.

Why: the user chose the full implementation approach, wanted `SHEET` after
`ARENA`, and wanted XP based on performance against the current level target
rather than a flat `+1` per win.

Rejected for now: build/upload validation, item rewards, perks, visible
turn-by-turn combat, enemy sprites, death/sickness systems, and putting `SHEET`
in the main page carousel.

## 2026-07-07 - Document refonte Arena progression

Decision: document the planned Arena/progression/Hunt refactor in a new
dedicated file `docs/pet-arena-progression-refactor.md` and add only a short
README pointer while keeping the current behavior documentation intact.

Why: the refactor is not yet firmware behavior, and a separate design document
keeps the V2 current-engine plan readable while capturing the V9 Arena
direction.

Rejected for now: replacing `docs/pet-autonomous-engine-plan.md`, rewriting the
README current behavior section, and locking undecided numeric constants.

## 2026-07-07 - Dashboard activite gauche temperature droite

Decision: on the dashboard home page, render the Bat activity tag on the top
left and the large temperature on the top right. Keep the food line on the
lower-right stat area, but draw food units as larger 3x3 blocks with more
spacing and shift only that row left enough to keep all 10 units visible.

Why: the user wanted activity and temperature positions inverted, and clarified
that the larger, more spaced elements were the food balls rather than lightning
icons.

Rejected for now: changing gameplay values, changing E/H bars, moving the Bat
sprite, running PlatformIO, and touching OTA/hardware validation.

## 2026-07-07 - V9 Arena affichage wins et fiche perso

Decision: for V9 Arena, use the selected hybrid internal combat resolution and
show the current number of Arena wins during the activity instead of the
countdown pie/timer. When Arena ends because HP reaches zero, show a dismissible
result window with wins, XP gained, and current level; close it only when the
user presses `SELECT`. Add a compact RPG-style character sheet: page 1 shows
core stats with shared lines such as `LV 4 | XP 18/30`, `HP 8 | ATK 5`,
`DEF 3 | LCK 2`, and `ARENA BEST 12`; page 2 can show the Bat title and
unlocked skill.

Why: the user wants Arena progress to be visible without showing turn-by-turn
combat, and wants the final result to remain on screen until acknowledged so it
cannot be missed.

Rejected for now: displaying remaining Arena time, using the countdown pie for
Arena, visual turn-by-turn combat, enemy sprites, auto-dismissing the Arena
result, showing a separate `WIN` field on the character sheet, and padding
values below 10 with leading zeroes.

## 2026-07-07 - Header dashboard temperature seule

Decision: remove the dashboard clock from the top-left header and render only
the weather temperature with the larger `u8g2_font_7x14B_tf` font.

Why: the user wanted to free the dashboard header from the clock and make the
temperature the primary readable value.

Rejected for now: changing the Network page time display, changing weather
polling, changing Tamagotchi layout below the header, and running PlatformIO.

## 2026-07-07 - Debounce boutons augmente

Decision: increase the global button lockout from `120 ms` to `300 ms`.

Why: the physical buttons were too reactive: `SELECT` could open the Tamagotchi
menu and immediately trigger the first action, while left/right could skip
multiple pages when pressed a little too long.

Rejected for now: adding a select-only menu guard, changing the default menu
entry, and refactoring the button input model.

## 2026-07-07 - Arena sans mort et resolution agregee

Decision: leave death/risk states out of the current Arena design, even though a
future `WEAK/SICK/DEAD`-style chain remains interesting. Resolve Arena runs with
aggregated automatic calculations instead of visual turn-by-turn combat.

Why: risk/death would likely require healing items or extra recovery systems,
which the user does not want to add now. Aggregated combat keeps Arena useful
without requiring enemy sprites, combat UI, or visible turn sequencing.

Rejected for now: Tamagotchi death, sickness/weakness states, healing items,
and true turn-by-turn combat presentation.

## 2026-07-07 - Progression Arena stats et skills

Decision: keep the simple progression direction based on automatic stat gains
and fixed skill unlocks by level tier. Use `LCK` instead of `SPD` in the core
combat/hunt stat set.

Why: the user wants an Arena loop inspired by LaBrute without item/perk
complexity or reward sprites, and wants the same character stats to influence
both automatic combat and hunting outcomes.

Rejected for now: item rewards, perk systems, sprite-based rewards, and `SPD`
as a core stat.

## 2026-07-06 - Camembert countdown plein et heure agrandie

Decision: render the action countdown as a filled 8-state pie mask that empties
clockwise from full to empty, and make the clock use `u8g2_font_6x12_tf` while
keeping the adjacent temperature smaller with `u8g2_font_5x8_tf`.

Why: the user clarified that the countdown should behave like a solid clock
timer, with a full circle at the start, a half-circle around half time, and an
empty circle when finished. The time also needed to be more distinguishable
than the temperature.

Rejected for now: sparse radial timer pixels, showing remaining time as text,
changing timed action durations, changing stat placement again, and running
PlatformIO.

## 2026-07-06 - Animation eating Bat remplacee

Decision: replace the previously integrated `bat_eating.png` with the corrected
user-provided eating spritesheet, pad the 128x31 source to 128x32, and
regenerate only `tmg_bat_eating_0..3` in XBM little-endian order.

Why: the earlier asset used for `PET_STATE_EAT` was the wrong animation. The
new source was one pixel short in height, so padding preserved the 32x32 frame
pipeline without changing rendering logic.

Rejected for now: changing `PET_STATE_EAT` behavior, creating a separate hitting
state, changing layout helpers, changing the XBM renderer, and running
PlatformIO.

## 2026-07-06 - Layout dashboard action et food

Decision: switch the dashboard header to compact clock plus temperature on the
left, render the current activity in a larger framed panel at the top right,
show timed action progress as a small 8x8 countdown pie under the activity, and
group E/H/F stats on the lower right with food drawn as fruit-like dots.

Why: the user wanted to free top-right space, make the action label more
prominent, represent remaining action time visually as a consuming pie timer,
and display food as discrete fruit units like the E/H pixel-art bars.

Rejected for now: showing remaining time as text, changing the Bat sprite
position again, changing action/gameplay logic, changing the XBM renderer, and
running PlatformIO.

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
