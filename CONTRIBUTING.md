# Contributing to IUT-Food

First off — thanks for poking at a student project! 🎉

## Getting started

```bash
git clone https://github.com/Abolfazlrwm/IUT_Food.git
cd IUT-Food
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/iut-food-server &      # terminal 1
./build/iut-food-client        # terminal 2
python3 scripts/smoke_test.py  # should print ✅ 35 checks
```

The demo database (`iut_food.db`) is created and seeded on first launch —
delete it whenever you want a clean slate.

## Ground rules

1. **Keep the build green.** CI builds both targets (CMake) and runs the
   end-to-end smoke test on every push. If you add a protocol command,
   extend `scripts/smoke_test.py` and `docs/PROTOCOL.md` in the same PR.
2. **Match the code style.**
   - C++17, 4 spaces, braces on their own line for classes/functions
   - `QStringLiteral(...)` for literals, `QLatin1String` comparisons
   - Every class gets a short `/*! ... */` doc comment explaining *why*
   - Parameterised SQL only — never concatenate user input into a query
3. **Canonical order states** come from `src/common/orderstatus.h`; never
   invent new status strings in UI code.
4. **Two build systems** must keep working: CMake (`CMakeLists.txt`) and
   qmake (`src/client/client.pro`, `src/server/server.pro`). New source
   files go into both.

## Pull requests

- One feature or fix per PR, with a short description of the *why*.
- Screenshots welcome for UI changes (the offscreen
  `scripts/screenshot_driver.cpp` target regenerates the whole gallery).
- Reference the issue it fixes (`Fixes #12`).

## Reporting bugs

Open a [bug report](https://github.com/Abolfazlrwm/IUT_Food/issues/new?template=bug_report.md)
with the build system, Qt version and OS you used, plus the server console
output. Security-sensitive issues go through
[SECURITY.md](SECURITY.md) instead.
