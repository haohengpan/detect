# AGENTS.md

## What this is

`iseethedead` — a DLL injected into **Warcraft III 1.27 (build 52240 only)** that provides maphack features plus detection of other players' out-of-view clicks. Windows only. All source lives in `iseethedead/`; there is no build system, CI, or tests in the repo.

## Build (Windows + Visual Studio only; this is an MSVC-only codebase)

- Solution: `iseethedead.sln` (root) → `iseethedead/iseethedead.vcxproj`. **Win32 (x86) configurations only** — do not add x64: inline `_asm`/`__asm` blocks are used throughout (`icome.cpp`, `memedit.cpp`, `tools.cpp`, `Jass.cpp`, `safeclick.cpp`, `miniMapHack.cpp`) and MSVC x64 cannot compile them. Code also stores pointers as `unsigned int`.
- Output is a **DLL**, injected into the `Warcraft III` process (`dllmain.cpp` finds it via `FindWindowW`).
- PlatformToolset is `v143` (VS2022); static CRT (`/MT`, `/MTd`) so the injected DLL has no vcruntime dependency.
- Precompiled header: every `.cpp` starts with `#include "pch.h"`; `pch.cpp` is the Create file. Do not break that ordering.
- External, non-vendored deps: Microsoft Detours (`detours.lib` + `detours.h`, linked via `#pragma` in `pch.h`; lib dir = `<DetoursRoot>\lib.X86`) and spdlog (header-only, `<SpdlogRoot>\include`; project defines `SPDLOG_HEADER_ONLY`, so no spdlog library needs building). The project resolves them from env vars `DETOURS_ROOT` / `SPDLOG_ROOT`, falling back to `external\detours` / `external\spdlog` next to the .sln.
- All sources are UTF-8 (with BOM); project compiles with `/utf-8`. Keep new files UTF-8 — Chinese comments otherwise break on non-GBK codepages.

## Architecture

- `dllmain.cpp` — entry. Refuses to load unless `WarcraftVersion() == 52240` (message box + return false otherwise). Gets `gameDll = GetModuleHandle(L"Game.dll")`, starts spdlog logger → `isee.txt`, calls `icome::icome()`.
- **All game access is via hardcoded offsets from `gameDll`** (e.g. `gameDll + 0x21080`, `0xBE6350`). These are only valid for 1.27.52240 — do not "fix" or port them without a version reference.
- `icome.cpp` — main loop: a 200 ms `SetTimer` callback (`icome::timer`) wrapped in `__try/__except`; first tick traverses the unit table, later ticks track unit creation/updates. **HOME key toggles maphack** on/off (apply/roll back patches).
- Feature modules, all initialized from `icome::icome()`:
  - `Jass.*` — Jass VM/API access and hooking
  - `memedit.*` — code patches + Detours (`applyPatch`, `applyDetour`)
  - `mhDetect.*` — detects other players' out-of-view click behavior
  - `safeclick.*` — "safe" clicks + gray healthbars for fogged units
  - `miniMapHack.*` — minimap pings/HP numbers
  - `unitTracker.*` — hooks unit create/destroy events
  - `antiExploit.*`, `player.*`, `tools.*` — shared helpers (order IDs, chat, version check)

## Conventions

- Comments are in Chinese; code style uses tabs, `unsigned int` for pointers/handles.
- Global cross-file state is declared in `pch.h` as `extern` (`logger`, `gameDll`, `localplayer`, `hIsee`, `hWnd`).
- `//#define LIMITED` in `pch.h` gates a reduced feature set.
- No test framework exists; verification means loading the DLL into the game.
