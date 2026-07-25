#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import json
import os
import pathlib
import re
import sys


def tree_hash(root: pathlib.Path) -> str:
    canonical_root = root.resolve(strict=True)
    digest = hashlib.sha256()
    paths = sorted(root.rglob("*"), key=lambda item: item.as_posix().casefold())
    seen_casefolded: dict[str, pathlib.Path] = {}

    for path in paths:
        relative = path.relative_to(root).as_posix()
        folded = relative.casefold()
        previous = seen_casefolded.get(folded)
        if previous is not None and previous != path:
            raise ValueError(f"case-colliding payload paths: {previous} and {path}")
        seen_casefolded[folded] = path

        digest.update(relative.encode("utf-8"))
        digest.update(b"\0")
        if path.is_symlink():
            resolved = path.resolve(strict=True)
            try:
                resolved.relative_to(canonical_root)
            except ValueError as exc:
                raise ValueError(f"payload symlink escapes package: {path}") from exc
            digest.update(b"L")
            digest.update(os.readlink(path).encode("utf-8"))
        elif path.is_file():
            digest.update(b"F")
            digest.update(path.stat().st_mode.to_bytes(4, "big"))
            with path.open("rb") as handle:
                for chunk in iter(lambda: handle.read(1024 * 1024), b""):
                    digest.update(chunk)
        elif path.is_dir():
            digest.update(b"D")
        digest.update(b"\0")

    return digest.hexdigest()


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {pathlib.Path(sys.argv[0]).name} RUNTIME_PACKAGE", file=sys.stderr)
        return 2

    package = pathlib.Path(sys.argv[1]).resolve(strict=True)
    manifest_path = package / "runtime.json"
    payload = package / "payload"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    expected = str(manifest.get("payload_tree_sha256", "")).lower()
    if not re.fullmatch(r"[0-9a-f]{64}", expected):
        raise ValueError("runtime.json has no valid payload_tree_sha256")
    if manifest.get("platform") != "macos":
        raise ValueError("runtime.json does not declare platform=macos")

    actual = tree_hash(payload)
    if actual != expected:
        raise ValueError(
            f"runtime payload hash mismatch: expected {expected}, calculated {actual}"
        )

    print(f"Runtime payload verified: {manifest.get('runtime_id')}/{manifest.get('build_id')}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
