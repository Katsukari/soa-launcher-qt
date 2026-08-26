# Launcher Configuration Reference

The Story of Alicia launcher stores persistent settings in `config.json`. The file is created automatically and rewritten whenever launcher settings change.

This reference describes the configuration used by the current Linux and macOS launcher.

## File locations

### Linux

The launcher uses Qt's application data directory.

When `XDG_DATA_HOME` is set:

```text
$XDG_DATA_HOME/Story Of Alicia/Story Of Alicia Launcher/config.json
```

The usual default is:

```text
~/.local/share/Story Of Alicia/Story Of Alicia Launcher/config.json
```

### macOS

```text
~/Library/Application Support/Story of Alicia/state/config.json
```

The launcher also keeps a recovery copy beside the main file:

```text
config.json.bak
```

## Editing the file

The launcher watches `config.json` while it is running. Valid external changes are reloaded automatically and normalized values may be written back to disk.

For predictable manual editing:

1. Close the launcher.
2. Back up `config.json` if you want to keep the current state.
3. Change only keys documented here.
4. Keep the file valid JSON.
5. Start the launcher again.

Changing a stored path does not move an existing Wine prefix, Proton compatibility directory, or game installation.

If `config.json` disappears while the launcher is running, the launcher recreates it from the configuration already held in memory. If the file is missing or unreadable at startup, the launcher tries `config.json.bak` before falling back to defaults.

## Runtime settings

| Key | Type | Default | Purpose |
|---|---|---|---|
| `wine_binary` | string | detected when possible | Selected Wine or Proton runtime |
| `winetricks_binary` | string | detected on Linux | Winetricks executable used with Wine |
| `umu_binary` | string | detected on Linux | UMU Runner used for Proton |
| `wine_prefix` | string | platform default | Wine prefix root |
| `proton_compat_data_root` | string | `~/soa-launcher` on Linux | Proton compatibility data root |
| `wine_arch` | string | `win64` | Wine prefix architecture |
| `runtime_selected` | boolean | `false` | Records whether runtime selection is complete |
| `use_dxvk` | boolean | `false` | Enables launcher managed DXVK for supported Linux Wine setups |
| `wine_args` | string | empty | Additional arguments passed to the selected runtime |
| `setup_runtime_preference` | string | `recommended` on Linux | Runtime family selected during setup |

### `wine_binary`

On Linux this may point to Wine, GE Proton, UMU Proton, or another runtime recognized by the launcher.

On macOS the launcher uses Wine based runtimes. Proton selections are not used there.

### `winetricks_binary`

Linux tries to find `winetricks` automatically. This setting is used for normal Wine prerequisite installation.

GE Proton and UMU Proton can use their own Winetricks support through UMU, so this path is not required for those runtimes.

### `umu_binary`

Linux tries `umu-run` from `PATH` first, followed by `~/.local/bin/umu-run`.

The value is unused on macOS.

### `wine_prefix`

Linux default:

```text
~/soa-launcher
```

macOS default:

```text
~/Library/Application Support/Story of Alicia/prefixes/shared
```

When Wine is selected, this is the active prefix root.

### `proton_compat_data_root`

This is the Proton compatibility data directory, not the `pfx` directory itself.

The active Proton prefix is:

```text
<proton_compat_data_root>/pfx
```

If a path ending in `pfx` is selected, the launcher stores its parent as the compatibility data root.

### `wine_arch`

Linux accepts:

```text
win32
win64
```

macOS always uses `win64`.

### `use_dxvk`

This option applies to supported Linux Wine configurations. It is forced off on macOS.

Proton uses the graphics stack supplied by the selected Proton runtime rather than a separate launcher installed DXVK copy.

### `wine_args`

The launcher removes null characters and limits the value to 8192 characters.

### `setup_runtime_preference`

Linux accepts:

```text
recommended
wine
proton
```

macOS uses Wine.

## Game settings

| Key | Type | Default | Purpose |
|---|---|---|---|
| `game_version` | string | `1.0` | Selected game version |
| `game_install_path_1_0` | string | derived automatically | Story of Alicia 1.0 install directory |
| `game_install_path_2_0` | string | derived automatically | Story of Alicia 2.0 install directory |
| `game_args` | string | empty | Additional game launch arguments |

### `game_version`

Accepted values are:

```text
1.0
2.0
```

The launch game ID is selected from the active game profile and is not stored in `config.json`.

### Game install paths

For Wine, the default paths are derived inside the active Wine prefix using the current Wine user.

For Proton, the launcher uses the `steamuser` directory inside the Proton prefix.

The launcher keeps both game paths inside the active prefix. Paths that point outside it are replaced with the derived default for that game version.

When the prefix or runtime family changes, paths that belonged to the previous prefix are rebased where possible.

### `game_args`

The launcher removes null characters and limits the value to 8192 characters.

Authentication arguments are generated by the launcher and are not stored in this field.

## Launcher settings

| Key | Type | Default | Accepted values |
|---|---|---|---|
| `launch_on_startup` | boolean | `false` | `true`, `false` |
| `after_game_start` | string | `keep` | `keep`, `minimize` |
| `launcher_size` | string | `1400x846` | supported launcher sizes |
| `language` | string | `en` | `en`, `nb`, `nl` |

### `launcher_size`

Supported values are:

```text
1120x677
1400x846
1600x967
1920x1160
```

The old `900x544` value is migrated to `1120x677`. Unsupported values are replaced with `1400x846`.

### `language`

The launcher supports English, Norwegian Bokmål, and Dutch.

Locale style values are normalized. Examples include `en-US` to `en`, `nb-NO` to `nb`, and `nl-NL` to `nl`. Older Norwegian values beginning with `no` are also normalized to `nb`.

## Setup and sign in state

| Key | Type | Default | Purpose |
|---|---|---|---|
| `prerequisites_confirmed` | boolean | `false` | Records completion of the current prerequisite step |
| `setup_assistant_version` | integer | `0` | Version of the setup flow already completed |
| `rules_accepted` | boolean | `false` | Records completion of the one time rules acknowledgement |
| `keep_signed_in` | boolean | `false` | Controls whether credentials persist between sessions |

### `prerequisites_confirmed`

The value only counts as complete when `setup_assistant_version` is at least `1`.

Completing the prerequisite step through the launcher sets the current setup assistant version automatically.

### `rules_accepted`

The value becomes `true` after the user completes the rules flow and confirms both acknowledgements before entering the playtest.

Once stored, the launcher does not ask for those acknowledgements again unless the launcher configuration is reset.

### `keep_signed_in`

Account credentials are not stored directly in `config.json`.

When this option is enabled, the launcher first uses the platform credential store. If that is unavailable, it can use a protected `.env` fallback beside `config.json`.

Disabling the option removes persisted credentials while leaving the current session available until it ends or is cleared.

## Diagnostics and macOS compatibility

| Key | Type | Default | Purpose |
|---|---|---|---|
| `diagnostics_enabled` | boolean | `false` | Enables the launch diagnostic mode |
| `macos_compatibility_profile` | string | `default` | Selects a macOS Wine compatibility profile |
| `rosetta_x87_path` | string | empty | Stores the optional Rosetta x87 compatibility path |

### `diagnostics_enabled`

Diagnostic Mode is available through Advanced Settings. It controls the additional per launch diagnostic collection used when troubleshooting game startup or runtime problems.

The old `macos_deep_diagnostics` key is migrated to `diagnostics_enabled` and then removed.

### `macos_compatibility_profile`

Persisted profile values currently normalized by the configuration loader are:

```text
default
safe-display
low-graphics
gl-behind
```

The macOS Advanced Settings UI also exposes the diagnostic `audio-isolation` profile. The current configuration loader does not preserve that value across a full reload, so it should not be relied on as a permanent manual config value yet.

### `rosetta_x87_path`

This value is only relevant to macOS compatibility setup. It has no effect on Linux.

## Credentials and sensitive data

The following values are deliberately kept out of `config.json`:

1. Account user ID or login name
2. Authentication token
3. Display name
4. Generated game authentication arguments

When persistent sign in is enabled, the launcher uses the platform credential store when available.

The fallback credential file is stored beside `config.json` as `.env` and is restricted to owner read and write access.

Do not commit or share the `.env` file.

## Automatic normalization

When configuration is loaded, the launcher can perform the following cleanup:

1. Add missing keys using current defaults.
2. Normalize launcher size, language, launch behavior, Wine architecture, and runtime preference.
3. Force macOS restrictions such as `win64` and disabled launcher managed DXVK.
4. Derive missing game installation paths.
5. Replace game paths that are outside the active prefix.
6. Rebase game paths when the active prefix changes.
7. Migrate older configuration keys.
8. Rewrite the normalized configuration using an atomic save.
9. Refresh `config.json.bak` after a successful save.

## Resetting launcher settings

The launcher's Reset Launcher Settings action removes the stored launcher state and recreates it from current defaults.

It clears `config.json`, `config.json.bak`, saved credentials, and the current authenticated session.

It does not delete the Wine prefix, Proton compatibility data, installed game files, downloaded runtimes, or launcher logs.

Existing game files can be detected again when setup runs.
