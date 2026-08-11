# Workflow switches

All workflows are enabled by default when the variables below are unset.
Set a variable to the literal value `false` to disable its automatic jobs.

- `SOA_ACTIONS_ENABLED` - master switch for all automatic Story of Alicia workflows.
- `SOA_LINUX_ENABLED` - Linux only.
- `SOA_LINUX_CLANG_ENABLED` - Linux Clang matrix job only; GCC/AppImage still runs.
- `SOA_MACOS_ARM64_ENABLED` - Apple Silicon only.
- `SOA_MACOS_INTEL_ENABLED` - Intel macOS only.
- `SOA_RELEASE_ENABLED` - tagged releases only.

Manual `workflow_dispatch` runs of the three platform build workflows bypass their switches, so an individual build can still be started from the Actions page while automatic CI is off.

## Fast master toggle with GitHub CLI

From the repository:

```bash
gh variable set SOA_ACTIONS_ENABLED --body false
```

Turn automatic CI back on:

```bash
gh variable set SOA_ACTIONS_ENABLED --body true
```

Disable only the automatic Linux Clang matrix job:

```bash
gh variable set SOA_LINUX_CLANG_ENABLED --body false
```

Re-enable it:

```bash
gh variable set SOA_LINUX_CLANG_ENABLED --body true
```

The master variable prevents jobs from being sent to runners, but GitHub will still create a skipped workflow run for matching push/PR events.

For absolutely no push/PR workflow run at all on a test commit, include `[skip actions]` in the commit message.

You can also fully disable/re-enable individual workflow files:

```bash
gh workflow disable build-linux.yml
gh workflow disable build-macos-arm64.yml
gh workflow disable build-macos-intel.yml
gh workflow disable release.yml

gh workflow enable build-linux.yml
gh workflow enable build-macos-arm64.yml
gh workflow enable build-macos-intel.yml
gh workflow enable release.yml
```
