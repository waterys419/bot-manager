# BOT MANAGER: A Developer's Life

An original homebrew game for the Super Famicom / SNES. You start as a kid typing your first program, work your way up through college and your first dev job, and by your midlife crisis you no longer write code at all. Now you manage AI bots who take every instruction very literally.

**[▶ Play in your browser](https://waterys419.github.io/bot-manager/)**, or download `bot-manager.sfc` and play it in an emulator or on real hardware.

## Chapters
1. **Hello World**: type the falling code before it hits the floor.
2. **Stack Overflow**: climb a tower of books while dodging bugs.
3. **Bug Hunt**: clear bugs from the editor, then face Off-By-One.
4. **The Deploy**: time your builds and survive the 3am pager.
5. **AI Bot Manager**: give orders to Bossy, Picky, Besty and QA Guy, and beat the Twin Tyrants before your usage meter runs out.

Then the credits and your final stats. Progress saves after every chapter (8 KB battery SRAM).

## Controls (SNES pad)
| Button | Action |
| --- | --- |
| D-pad | Move / select |
| B or A | Act |
| Start | Begin / advance / pause |
| Select | Toggle sound; from the pause menu, return to the title |
| X / Y | Answer the pager |
| L | Ask for a manager hint |

In the browser, the keys are: arrows, X = B, Z = A, A = X, S = Y, Enter = Start, Shift = Select, Q = L, E = R. A gamepad works too.

## Where it runs
- **Emulators:** Snes9x, bsnes/ares, and Mesen.
- **Hardware:** copy `bot-manager.sfc` to a flash cart (FXPAK Pro / SD2SNES, Super EverDrive) and play it on an NTSC console using controller port 1. The ROM is 256 KB, LoROM, with no enhancement chips. It hasn't been tested on a real console yet, so reports are welcome.
- **Browser:** `web/` runs the same ROM in a bundled offline emulator (EmulatorJS + Snes9x WebAssembly). Locally, run `web/START-GAME.cmd` (Windows) or `python3 web/serve.py`.

## Building from source
The game is written in C with [PVSnesLib 4.7.0](https://github.com/alekmaul/pvsneslib).
- `python3 tools/bootstrap.py` downloads the pinned SDK into `toolchain/` and checks its hash.
- `python3 tools/build.py` (or `make`) builds the ROM.
- `tools/art.py` generates the original pixel art.
- `tools/music.py` composes the original soundtrack, "Small Steps, Big Buffs".

Tests: `python3 tests/playtest.py` plays through every chapter in Snes9x using controller input only. `python3 tools/check_rom.py` validates the header and checksum.

## Credits & legal
- **Idea:** Waterys
- **Code, art and music:** made with AI assistance (Codex "Dash"), coordinated by Claude

This is an original fan-made homebrew game. It isn't affiliated with or endorsed by Nintendo, and it contains no Nintendo code, assets or trademarks. "SNES" and "Super Famicom" are used only to describe compatible hardware.

The game is MIT licensed. The bundled browser emulator keeps its own licenses (see `web/licenses/`); Snes9x is non-commercial, so the browser build is free to play and must not be sold.
