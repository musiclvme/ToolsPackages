#!/usr/bin/env python3
"""Block agent writes/commits that embed local home-directory paths."""

from __future__ import annotations

import json
import re
import sys

# Real account folders, not placeholders like <you> or %USERNAME%.
WINDOWS_HOME = re.compile(
    r"(?i)\b[a-z]:[/\\]+Users[/\\]+(?!Public\b|Default\b|Default User\b|All Users\b)[^/\\<>\s%]+",
)
MAC_HOME = re.compile(
    r"(?i)(?<![A-Za-z0-9])/Users/(?!Shared\b|Guest\b)[^/<>\s%]+",
)

DENY_MESSAGE = (
    "Blocked personal home-directory path (Windows C:\\Users\\<name> or macOS /Users/<name>). "
    "Use a generic example such as input.pdf or a placeholder like C:\\Users\\<you>\\file.pdf."
)


def find_personal_path(text: str) -> str | None:
    for pattern in (WINDOWS_HOME, MAC_HOME):
        match = pattern.search(text)
        if match:
            return match.group(0)
    return None


def deny(found: str) -> None:
    payload = {
        "permission": "deny",
        "user_message": DENY_MESSAGE,
        "agent_message": f"{DENY_MESSAGE} Offending fragment: {found}",
    }
    json.dump(payload, sys.stdout)
    sys.stdout.write("\n")


def allow() -> None:
    json.dump({"permission": "allow"}, sys.stdout)
    sys.stdout.write("\n")


def main() -> int:
    raw = sys.stdin.read()
    if not raw.strip():
        allow()
        return 0

    found = find_personal_path(raw)
    if found:
        deny(found)
        return 0

    try:
        data = json.loads(raw)
    except json.JSONDecodeError:
        allow()
        return 0

    blob = json.dumps(data, ensure_ascii=False)
    found = find_personal_path(blob)
    if found:
        deny(found)
        return 0

    allow()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
