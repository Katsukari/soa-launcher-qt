# First macOS hardware test

This is the release checklist for the launcher-owned macOS runtime. It does not
claim gameplay compatibility until the full sequence has passed on clean
hardware.

## 1. Build and package the sibling runtime

The launcher requires a complete runtime package containing `runtime.json` and
`payload/StoryOfAliciaRuntime.app`.

```sh
cd ../soa_wine_runtime
./scripts/build-macos.sh
./scripts/package-macos.sh
```

The runtime build is x86_64. On Apple Silicon, run the runtime project's scripts
from the x86_64 shell they request and install Rosetta through macOS if needed.

## 2. Build the launcher

Install Xcode command-line tools, CMake, Ninja, Qt 6, and Swift:

```sh
xcode-select --install
brew install cmake ninja qt
```

Then, from the launcher source directory:

```sh
./packaging/macos/build-local.sh
```

The script selects the newest sibling runtime package automatically. To use an
exact package:

```sh
SOA_MACOS_RUNTIME_PACKAGE=/absolute/path/to/package \
  ./packaging/macos/build-local.sh
```

The build refuses an incomplete package, verifies its immutable payload hash,
runs the launcher tests, deploys Qt, verifies the copied payload again, and
optionally ad-hoc signs the outer launcher when `SOA_LOCAL_SIGN=1`.

## 3. Runtime selection

The bundled Story of Alicia runtime is selected by default. Settings →
**Runtime** → **Custom Runtime Override** should be blank.

Use an override only to bisect a compatibility regression. An explicit override
is authoritative: if it disappears or becomes unusable, the launcher reports
the failure instead of silently selecting something else.

On Apple Silicon, an Intel-only runtime requires Rosetta. The launcher may ask
macOS to present its normal Rosetta installation flow, but never accepts a
license or installs it silently.

## 4. Prefix and component policy

The default prefix is:

```text
~/Library/Application Support/Story of Alicia/prefixes/shared
```

The prefix is always 64-bit/new-WoW64 on macOS. DXVK is visible but disabled;
`config.json` also normalizes `use_dxvk` to `false`.

Prefix readiness is structural. Immediately before every game launch, the
launcher separately verifies the exact game-local DirectX, Visual C++ 2010, and
PhysX libraries shipped by the current CDN manifest. A missing component
blocks launch and directs the user to Verify and Repair.

## 5. Compatibility profiles

- **Normal (recommended)** — runtime defaults, no experimental renderer value.
- **Safe display** — a 1280×720 virtual desktop.
- **Low graphics** — safe display plus a conservative, backed-up `alice.cfg`
  update.
- **Mac GL fallback** — virtual desktop plus the targeted alternate macOS
  surface-placement value.

Start with Normal. Use one fallback at a time so the result remains
diagnosable.

**Developer deep diagnostics** is independent from those profiles. It adds
first-fault trace channels and, after the game host PID is resolved, a host
sample. It is intentionally off by default because heavy tracing changes game
startup timing.

## 6. Launch lifecycle checks

The launcher must:

1. Detect an already-running `Alicia.exe` after launcher restart.
2. Block a second launch.
3. Show **Starting** while only the outer runtime process exists.
4. Show **Running** only after `Alicia.exe` is observed.
5. Continue tracking the game if the outer wrapper exits.
6. Return to ready state after the game process disappears.
7. Enter a conservative uncertain state, without killing the game or enabling
   another launch, if both process probes repeatedly fail.

The prefix-scoped process query is primary. A host process scan provides a
second signal on Linux and macOS and also supplies the PID used for optional
host diagnostics.

## 7. First useful gameplay sequence

1. Create a fresh prefix.
2. Download or repair both game profiles.
3. Launch with Normal.
4. Test login, lobby, one race, input, audio, fullscreen/window transitions,
   PhysX-dependent movement, exit, and relaunch.
5. Restart the launcher while the game is open and verify it reattaches.
6. Repeat with Safe display only if display creation fails.
7. Enable Developer deep diagnostics only for a short unexplained failure.

## 8. Logs and reports

Normal diagnostic data is stored below:

```text
~/Library/Application Support/Story of Alicia/logs/
```

After a failed or incomplete run:

```sh
./packaging/macos/collect-diagnostics.sh
```

Review the archive before sharing it. Reports can contain local paths and
process command lines even though credentials and known secrets are redacted.

Record:

- Mac model/chip and macOS version/build.
- Launcher, runtime, and immutable build IDs.
- Rosetta state.
- Fresh or reused prefix.
- Compatibility profile and deep-diagnostic state.
- Whether the window appeared and how long the process survived.
- Exit/crash result and the generated timeline/diagnostic filenames.

Release testing must also verify Gatekeeper, Developer ID signatures,
notarization, and launch from a browser-downloaded DMG on a clean Mac.
For a public release, build the runtime with the same Developer ID Application
team used for the launcher; the launcher signing script preserves the hashed
runtime payload and rejects mismatched Team IDs.
