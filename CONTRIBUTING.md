# Contributing to IUT-Food

First off — thanks for poking at a student project! 🎉

## Getting started

The fastest way to a working build is the CMake preset:

```bash
git clone https://github.com/Abolfazlrwm/IUT_Food.git
cd IUT-Food
cmake --preset linux-release        # windows-release on Windows
cmake --build build/linux-release -j
./build/linux-release/iut-food-server &      # terminal 1
./build/linux-release/iut-food-client        # terminal 2
python3 scripts/smoke_test.py                # 50+ end-to-end checks
```

Plain CMake (`cmake -B build -DCMAKE_BUILD_TYPE=Release`) and qmake
(`IUT-Food.pro` in Qt Creator) still work exactly as before — the
presets are only a convenience wrapper.

The demo database (`iut_food.db`) is created and seeded on first launch —
delete it whenever you want a clean slate. For a production-shaped
database start the server with `--no-demo-data` instead.

## Ground rules

1. **Keep the build green.** CI builds both targets on Ubuntu + Windows,
   runs `ctest`, runs the end-to-end smoke test on both legs (the Ubuntu
   leg runs it a second time over TLS), and builds + smoke-tests the
   Docker image on every push. If you add a protocol
   command, extend `scripts/smoke_test.py` and `docs/PROTOCOL.md` in the
   same PR.
2. **Match the code style.**
   - C++17, 4 spaces, braces on their own line for classes/functions
   - `QStringLiteral(...)` for literals, `QLatin1String` comparisons
   - Every class gets a short `/*! ... */` doc comment explaining *why*
   - Parameterised SQL only — never concatenate user input into a query
   - `.clang-format` and `.clang-tidy` encode the baseline; run
     `clang-format -i` on touched files so reflows don't churn diffs
3. **Canonical order states** come from `src/common/orderstatus.h`; never
   invent new status strings in UI code.
4. **One version, one place.** The release number lives only in
   `src/common/version.h` — CMake parses it into `project(VERSION)` and
   both binaries read the same macro. Never hardcode a version string.
5. **Two build systems** must keep working: CMake (`CMakeLists.txt`) and
   qmake (`src/client/client.pro`, `src/server/server.pro`). New source
   files go into both.

## Pull requests

- One feature or fix per PR, with a short description of the *why*.
- Screenshots welcome for UI changes — regenerate the whole gallery
  offscreen and convert it in one pass:

  ```bash
  cmake --build build/linux-release --target iut-screenshot-driver
  QT_QPA_PLATFORM=offscreen ./build/linux-release/iut-screenshot-driver ./shots
  python3 scripts/convert_screenshots.py ./shots docs/screenshots
  ```

  The gallery is plain WebP files, so a diff shows exactly which screens
  changed.
- Reference the issue it fixes (`Fixes #12`).

## Reporting bugs

Open a [bug report](https://github.com/Abolfazlrwm/IUT_Food/issues/new?template=bug_report.md)
with the build system, Qt version and OS you used, plus the server console
output. Security-sensitive issues go through
[SECURITY.md](SECURITY.md) instead.
