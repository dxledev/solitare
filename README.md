# Solitare

A quiet, standalone solitaire collection written in C++20 with Qt Widgets.
Play Klondike, Spider, FreeCell, Pyramid, and TriPeaks with flat live theme colors,
original botanical vector cards, a resizable native window, and no web runtime. Play with
a mouse or keyboard. Help and confirmations stay inside the window, over a soft
blur of the table.

Choose a game in **New deal**: Klondike draws one or three; Spider uses one, two,
or four suits; Pyramid allows zero, one, or two redeals; TriPeaks has standard
or ace/king wraparound rules. FreeCell provides four free cells. Move legal runs,
undo your last 200 moves, and ask for a hint. The selected game, rules, deal and
elapsed time save automatically and restore when you return. Klondike recycling
is unlimited; Spider and TriPeaks never recycle. Some deals cannot be won.

The floral artwork takes its palette and botanical character from
[rohan-patnaik/Solitaire](https://github.com/rohan-patnaik/Solitaire), while
traditional suit shapes and rank labels stay prominent. See
[art provenance](assets/PROVENANCE.md).

## Build and run

Requires CMake 3.21+, a C++20 compiler, and Qt 6.7+ Core, Gui, Widgets, OpenGLWidgets, and Test
(for tests). On Linux, install Qt's Wayland platform plugin for native Wayland.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/solitare
```

Install under your own account, including desktop launchers:

```bash
bash scripts/install-desktop.sh --dry-run
bash scripts/install-desktop.sh
```

The installer accepts `--prefix`, `--config-dir`, and `--apps-list` absolute paths.
For a conventional system package, use CMake's `--install` command with your
preferred prefix. The binary needs Qt libraries and its platform plugin; it is
not a statically linked single-file distribution.

## Your colors

Solitare works independently and can be themed entirely from its CLI. The saved
palette lives at `$XDG_CONFIG_HOME/solitare/theme.json`, or
`~/.config/solitare/theme.json`. Every successful CLI update writes JSON atomically;
an open window immediately watches and reloads it. Invalid updates retain the
last valid colors. `--config-dir /absolute/path` selects a separate configuration.

```bash
solitare theme show
solitare theme set 'accent=#a9c7b0' 'card=#253238'
solitare theme set 'background=#e00e1e36'
solitare theme set 'background=Qt.rgba(14 / 255, 30 / 255, 54 / 255, 0.88)'
solitare theme import /absolute/path/palette.json
solitare theme follow file /absolute/path/palette.json
solitare theme sync
solitare theme preset lofi --dry-run
solitare theme preset lofi
```

`import` takes a snapshot; `follow file` keeps watching the source. QML color
property files and terminal palettes are supported too. Followed directories
and symlinks track replacement files and target changes. Manual `set` and `preset`
commands stop following the previous source. Add `--dry-run` to a theme mutation
to preview the result without writing anything.

Theme tokens: `background`, `surface`, `surface_alt`, `text`, `muted`, `accent`,
`accent_text`, `border`, `red_suit`, `black_suit`, `card`, `card_back`, `selection`.
Suit identity is always encoded by shape as well as color. Optional botanical
colors are `floral_red`, `floral_green`, `floral_blue`, `floral_gold`, and
`floral_cream`; old palettes receive the reference-inspired defaults. The table
uses one composed botanical frame, with a golden sun above and connected flower,
leaf, and berry branches around the open playing area. Card faces stay warm
off-white with large botanical illustrations and traditional rank and suit marks
at both ends. Card backs weave cream foliage around red flower medallions on
navy. Floral colors on the table and cards are tinted 22% toward your accent
while retaining their original hues. For example:

```bash
solitare theme set 'floral_red=#d94528' 'floral_green=#53623a'
```

Eight-digit colors use Qt's **#AARRGGBB**, with alpha first. `#e00e1e36` retains
RGB `#0e1e36` and alpha `0xe0`. `Qt.rgba()` accepts normalized decimals and simple
fractions. Transparency is preserved in the saved palette and painting; its
appearance depends on compositor support.

## Controls

- Drag cards, or click a card and its destination.
- Double-click a top card to move it to a foundation.
- Click the stock to draw or recycle according to the selected game.
- Pyramid: click two exposed cards totaling 13, or click a king alone.
- TriPeaks: click an exposed card one rank above or below the waste.
- Arrow keys choose piles; Enter selects or moves. Up selects more of a run.
- Space draws, A sends a card home, H shows a hint, Ctrl+Z undoes.
- Ctrl+N opens a new deal; F1 opens help; Escape closes a panel or clears selection.

Panels fade and scale from 98% over 120 ms with an OutCubic curve; closing uses
the same motion, paced to the display refresh rate. Set
`SOLITARE_REDUCED_MOTION=1` to make these transitions instant. Native windows
blend the frozen modal backdrop on the GPU; `SOLITARE_MODAL_RENDERER=software`
forces the software fallback. Offscreen sessions and systems without an OpenGL
context use that fallback automatically. Wayland keeps Qt’s default buffer integration
and honors explicit `QT_WAYLAND_CLIENT_BUFFER_INTEGRATION` overrides; an explicit
`shm` override uses the software backdrop. Card artwork is cached at the current
display resolution and refreshed when the deal, palette, size, or scaling changes.
Dropdowns show complete themed rows and keep their popups within the screen.

The timer pauses while the window is inactive or a panel is open. Only one
instance may write a configuration's saved deal at a time.

## Safety checks

```bash
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSOLITARE_SANITIZE=ON
cmake --build build-asan
QT_QPA_PLATFORM=offscreen ctest --test-dir build-asan --output-on-failure
```

Tests exercise card conservation, move legality, malformed saves, undo, palette
parsing and changes, input blocking, mode selection, layouts, resizing, and restoring saved deals
for every game. Set `SOLITARE_SCREENSHOT_DIR` to an existing directory when running
`ui_tests` to capture mode, minimum-size, modal, light-theme and card-art previews.
