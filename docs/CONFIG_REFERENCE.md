# Launcher Configuration Reference

The Story of Alicia launcher stores its persistent launcher settings in `config.json`.
The file is created automatically on first launch, rewritten when settings change, and normalized whenever it is loaded.

This reference describes the configuration keys implemented by the current launcher source.

## File locations

### Linux

The launcher uses Qt's application data directory:

```text
$XDG_DATA_HOME/Story Of Alicia/Story Of Alicia Launcher/config.json
```

When `XDG_DATA_HOME` is not set, the usual location is:

```text
~/.local/share/Story Of Alicia/Story Of Alicia Launcher/config.json
```

### macOS

```text
~/Library/Application Support/Story of Alicia/state/config.json
```

## Editing the file manually

The launcher watches `config.json` for external changes and reloads it automatically. Values may be normalized and written back immediately.

For predictable manual editing:

1. Close the launcher.
2. Back up `config.json`.
3. Edit only documented keys.
4. Keep valid JSON syntax.
5. Start the launcher and inspect the launcher log for rejected or replaced values.

Changing a stored path does not move an existing Wine prefix, Proton compatibility directory, game installation, or runtime.

## Complete example

This example represents a Linux Wine configuration. Paths are examples and must be replaced with paths valid for the current system.

```json
{
    "after_game_start": "keep",
    "game_args": "",
    "game_install_path_1_0": "/home/alicia/soa-launcher/drive_c/users/alicia/AppData/Roaming/Story Of Alicia/game",
    "game_install_path_2_0": "/home/alicia/soa-launcher/drive_c/users/alicia/AppData/Roaming/Story Of Alicia 2.0/game",
    "game_version": "1.0",
    "keep_signed_in": false,
    "language": "en",
    "launch_on_startup": false,
    "launcher_size": "1400x846",
    "macos_compatibility_profile": "default",
    "macos_deep_diagnostics": false,
    "prerequisites_confirmed": true,
    "proton_compat_data_root": "/home/alicia/soa-launcher",
    "rosetta_x87_path": "",
    "rules_accepted": true,
    "runtime_selected": true,
    "setup_assistant_version": 1,
    "setup_runtime_preference": "wine",
    "umu_binary": "/usr/bin/umu-run",
    "use_dxvk": false,
    "wine_arch": "win64",
    "wine_args": "",
    "wine_binary": "/usr/bin/wine",
    "wine_prefix": "/home/alicia/soa-launcher",
    "winetricks_binary": "/usr/bin/winetricks"
}
```

The order of keys is not significant.

## Runtime and prefix settings

### `wine_binary`

- **Type:** string
- **Default on Linux:** automatically detected `wine` or `wine64`, otherwise empty
- **Default on macOS:** the launcher-managed bundled runtime selector

The selected Wine or Proton runtime. On Linux this may be a Wine executable, Proton runtime entry, or another runtime understood by the launcher. On macOS Proton selections are rejected and the bundled or custom Wine runtime is used.

Changing the runtime may cause stored game paths to be rebased to the active prefix.

### `winetricks_binary`

- **Type:** string
- **Default on Linux:** automatically detected `winetricks`, otherwise empty
- **Default on macOS:** empty

Path to the Winetricks executable used for Wine prerequisite installation. The value is cleared on macOS.

### `umu_binary`

- **Type:** string
- **Default on Linux:** automatically detected `umu-run`, then `~/.local/bin/umu-run`, otherwise empty
- **Default on macOS:** empty

Optional path to the UMU Runner executable used for Proton launching on Linux. Leading and trailing whitespace is removed.

### `wine_prefix`

- **Type:** string
- **Default on Linux:** `~/soa-launcher`
- **Default on macOS:** `~/Library/Application Support/Story of Alicia/prefixes/shared`

Root of the Wine prefix used when the selected runtime is Wine.

The path is converted to an absolute, cleaned path. Changing it rebases the stored installation paths for both supported game versions where possible.

### `proton_compat_data_root`

- **Type:** string
- **Default on Linux:** `~/soa-launcher`
- **Used by:** Proton on Linux

Root of the Proton compatibility-data directory. The active Wine prefix is:

```text
<proton_compat_data_root>/pfx
```

When a selected path ends in `pfx`, the launcher stores its parent directory as the compatibility-data root.

### `wine_arch`

- **Type:** string
- **Default:** `win64`
- **Accepted on Linux:** `win32`, `win64`
- **macOS:** always forced to `win64`

Wine prefix architecture requested by the launcher.

Any Linux value other than `win32` is normalized to `win64`.

### `runtime_selected`

- **Type:** boolean
- **Default on Linux:** `false`

Records whether runtime selection has been completed. On macOS it becomes `true` when a valid bundled or custom runtime selector is available.

This flag does not prove that the runtime still exists or is executable. Runtime validation is performed separately.

### `use_dxvk`

- **Type:** boolean
- **Default:** `false`
- **macOS:** always forced to `false`

Enables DXVK for supported Linux runtime configurations. DXVK is intentionally disabled in the macOS configuration.

### `wine_args`

- **Type:** string
- **Default:** empty
- **Maximum length:** 8192 characters

Additional arguments passed to the Wine runtime. Null characters are removed and longer values are truncated.

Incorrect arguments may prevent prefix setup or game launching.

### `setup_runtime_preference`

- **Type:** string
- **Linux values:** `recommended`, `wine`, `proton`
- **macOS value:** `wine`

Stores the runtime family chosen in the setup assistant. Invalid Linux values become `recommended`. macOS normalizes this setting to `wine`.

## Game settings

### `game_version`

- **Type:** string
- **Default:** `1.0`
- **Accepted values:** `1.0`, `2.0`

The currently selected Story of Alicia version. Any value other than `2.0` is treated as `1.0`.

The launch `GameID` is derived from the selected game profile and is not stored in `config.json`.

### `game_install_path_1_0`

- **Type:** string

Installation directory for Story of Alicia 1.0.

The default Wine location is derived as:

```text
<wine_prefix>/drive_c/users/<user>/AppData/Roaming/Story Of Alicia/game
```

The default Proton location is derived as:

```text
<proton_compat_data_root>/pfx/drive_c/users/steamuser/AppData/Roaming/Story Of Alicia/game
```

### `game_install_path_2_0`

- **Type:** string

Installation directory for Story of Alicia 2.0.

The default Wine location is derived as:

```text
<wine_prefix>/drive_c/users/<user>/AppData/Roaming/Story Of Alicia 2.0/game
```

The default Proton location is derived as:

```text
<proton_compat_data_root>/pfx/drive_c/users/steamuser/AppData/Roaming/Story Of Alicia 2.0/game
```

### Game path containment

Both game installation paths must remain inside the active prefix.

The launcher rejects or replaces paths that:

- are outside the active Wine or Proton prefix;
- escape through an existing symbolic link;
- resolve through an existing ancestor outside the prefix.

An invalid stored path is replaced with the derived default for that game version.

### `game_args`

- **Type:** string
- **Default:** empty
- **Maximum length:** 8192 characters

Additional arguments appended to the game launch command. Null characters are removed and longer values are truncated.

Authentication arguments and the selected game ID are generated by the launcher and are not stored here.

## Launcher behaviour

### `launch_on_startup`

- **Type:** boolean
- **Default:** `false`

Records whether the launcher should start automatically with the desktop session. Platform integration may require a separate system registration in addition to this value.

### `after_game_start`

- **Type:** string
- **Default:** `keep`
- **Accepted values:** `keep`, `minimize`

Controls what happens to the launcher window after the game starts. Any value other than `minimize` becomes `keep`.

### `launcher_size`

- **Type:** string
- **Default:** `1400x846`
- **Accepted values:**
  - `1120x677`
  - `1400x846`
  - `1600x967`
  - `1920x1160`

The legacy value `900x544` is migrated to `1120x677`. Other unsupported values become `1400x846`.

### `language`

- **Type:** string
- **Default:** `en`
- **Accepted values:**
  - `en` — English
  - `nb` — Norwegian Bokmål
  - `nl` — Dutch

Locale-like values are normalized. Examples include `en-US` to `en`, `nb-NO` to `nb`, and `nl-NL` to `nl`. Legacy Norwegian values beginning with `no` also become `nb`.

## Setup and agreement state

### `prerequisites_confirmed`

- **Type:** boolean
- **Default:** `false`

Records whether the user completed the current prerequisites step. It is considered valid only when `setup_assistant_version` is at least `1`.

### `setup_assistant_version`

- **Type:** integer
- **Default:** `0`
- **Current completed version:** `1`

Internal schema/version marker for setup completion. Negative values become `0`.

Setting prerequisites as confirmed through the launcher also writes this value as `1`. A future launcher may increase it to require a newer setup step.

### `rules_accepted`

- **Type:** boolean
- **Default:** `false`

Records whether the launcher rules were accepted.

This is launcher state, not a cryptographic receipt or substitute for server-side account enforcement.

### `keep_signed_in`

- **Type:** boolean
- **Default:** `false`

Controls whether login credentials persist between launcher sessions.

Credentials are not stored directly in `config.json`. When persistence is enabled, the launcher first uses the platform credential store. If that is unavailable, it may use a restricted `.env` fallback beside `config.json`.

Disabling this setting removes saved credentials while leaving the current session available until it is otherwise cleared or the launcher exits.

## macOS compatibility settings

These keys remain present in the shared configuration schema but only affect macOS.

### `macos_compatibility_profile`

- **Type:** string
- **Default:** `default`
- **Accepted values:**
  - `default`
  - `safe-display`
  - `low-graphics`
  - `gl-behind`

Selects a predefined macOS Wine compatibility profile. Unsupported values become `default`.

### `macos_deep_diagnostics`

- **Type:** boolean
- **Default:** `false`

Enables additional macOS runtime and launch diagnostics. The value has no effect on Linux.

### `rosetta_x87_path`

- **Type:** string
- **Default:** empty

Optional macOS path associated with the Rosetta x87 compatibility setup. It has no effect on Linux.

## Credentials and sensitive data

The following values are intentionally not stored in `config.json`:

- account ID or login username;
- authentication token;
- display name;
- generated launch authentication arguments.

When `keep_signed_in` is enabled, credentials are stored using the platform credential store where available.

The fallback credential file is located beside `config.json`:

### Linux

```text
$XDG_DATA_HOME/Story Of Alicia/Story Of Alicia Launcher/.env
```

### macOS

```text
~/Library/Application Support/Story of Alicia/state/.env
```

The fallback file is written with owner-only read and write permissions. It must not be committed, attached to bug reports, or shared with launcher logs.
!@
## Automatic normalization

When the launcher loads the configuration, it may automatically:

- add missing keys with defaults;
- normalize language, window size, launch behaviour, architecture, and compatibility-profile values;
- force macOS-only restrictions such as `win64`, Wine-only runtime preference, and disabled DXVK;
- derive empty game installation paths;
- replace game paths outside the active prefix;
- migrate legacy keys;
- rebase game paths after a prefix or runtime change;
- save the normalized configuration back to disk.

The launcher uses an atomic save operation so an interrupted write is less likely to leave a partially written configuration.

## Resetting launcher settings

The launcher's **Reset Launcher Settings** action:

- removes and recreates `config.json` with current defaults;
- removes the `.env` fallback;
- clears credentials from the platform credential store;
- clears the current authenticated session.

It does not delete:

- the Wine prefix;
- Proton compatibility data;
- installed game files;
- downloaded runtimes;
- launcher logs.

After a reset, existing files may still be detected again when setup runs.
