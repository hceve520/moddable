# AGENTS.md

## Cursor Cloud specific instructions

This repository is the **Moddable SDK** — a C + JavaScript SDK for building apps
that run on microcontrollers and on a desktop simulator. There is no web server
or database. "Running the app" means building an example with `mcconfig` and
running it in the `mcsim` desktop simulator, using `xsbug` as the source-level
debugger / test runner.

### Environment
- `MODDABLE` must be an **absolute** path to the repo root (`/workspace`), and
  `$MODDABLE/build/bin/lin/release` must be on `PATH`. These are set in
  `~/.bashrc` for the default shell. Shells that don't source `.bashrc` must set
  them, or pass `MODDABLE=/workspace` on the `make`/tool command line.
- The GTK+3 desktop tools (`mcsim`, `xsbug`) need an X display. A VNC X server is
  available at `DISPLAY=:1` — export `DISPLAY=:1` before launching GUI tools.
- System packages (`gcc make flex bison gperf libncurses-dev libgtk-3-dev`) are
  already installed in the image; they are not part of the update script.

### Build the SDK tools
- `make -C build/makefiles/lin MODDABLE=$(pwd)` builds all CLI tools plus `mcsim`
  and `xsbug` into `build/bin/lin/{debug,release}` (both gitignored). It is
  incremental — re-run after pulling C-source changes. The startup update script
  runs this automatically.
- The `xst` command-line JS / test262 engine is built separately with
  `make -C xs/makefiles/lin release`. Note: the default `make` there also builds
  a debug target that links LLVM AddressSanitizer libs which are not installed,
  so it fails — always use the `release` target. Output: `build/bin/lin/release/xst`.

### Run an app on the simulator
- `cd examples/helloworld && mcconfig -d -m -p lin` builds a debug app and
  launches it in `mcsim`. Debug (`-d`) builds connect to `xsbug`, so start
  `xsbug` first (`DISPLAY=:1 xsbug &`). The app pauses at any `debugger;`
  statement until you press Run in xsbug. Non-graphical apps (helloworld) print
  via `trace()` to the xsbug CONSOLE; graphical Piu/Commodetto examples
  (e.g. `examples/piu/balls`) render in the mcsim device screen.

### Lint
- ESLint config is `eslint.config.mjs`, but the repo has no `package.json`, so
  the plugin packages are installed ad-hoc (already present in this image):
  `npm install --no-save eslint @eslint/js typescript-eslint`. Run e.g.
  `npx eslint <files>`. Most rules are warnings (e.g. `no-debugger`), so a clean
  run exits 0.

### Tests
- Engine / JS-level checks run headlessly with `xst`
  (`xst -e '<js>'` exits non-zero on an unhandled exception; `xst <test262-path>`
  runs test262 cases). `trace()` output is suppressed in `xst` (built with
  `mxNoConsole`), so rely on exit codes / thrown errors.
- The full Moddable SDK suites (`tests/`, TEST262) run through the
  `tools/testmc` / `tools/test262` apps driven interactively by the `xsbug` GUI
  (see `documentation/tools/testing.md`); they require cloning
  `github.com/tc39/test262` and are not fully headless.
