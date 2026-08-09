#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]

violations = []


def scan_tree(relative_root: str, forbidden: tuple[str, ...], label: str) -> None:
    root = ROOT / relative_root
    if not root.exists():
        return

    for path in root.rglob('*'):
        if path.suffix not in {'.h', '.hpp', '.cpp', '.inl'}:
            continue
        text = path.read_text(encoding='utf-8', errors='replace')
        for token in forbidden:
            if token in text:
                violations.append(
                    f"{path.relative_to(ROOT)}: {label}: forbidden dependency {token!r}"
                )


# Authoritative runtime owns gameplay facts, not client presentation policy.
scan_tree(
    'src/game/server',
    ('src/game/presentation/', 'PresentationPolicyResolver'),
    'server must not depend on presentation policy'
)

# Simulation contracts may describe authority/motion, but must stay render-free.
scan_tree(
    'src/game/simulation',
    ('src/game/presentation/', 'PresentationPolicyResolver'),
    'simulation must not depend on presentation policy'
)

# Presentation code consumes contracts/state. It must not reach through the
# transport/session boundary into the authoritative GameServer implementation.
scan_tree(
    'src/game/presentation',
    ('src/game/server/', 'GameServer.h'),
    'presentation must not depend directly on server implementation'
)

# Entity identity and motion law declarations are deliberately low-level and
# reusable on both sides of the network boundary.
scan_tree(
    'src/game/entity',
    ('src/game/server/', 'src/game/client/', 'src/game/presentation/'),
    'entity identity contract must remain transport/presentation independent'
)
scan_tree(
    'src/game/motion',
    ('src/game/server/', 'src/game/client/', 'src/game/presentation/'),
    'motion contract must remain transport/presentation independent'
)

if violations:
    print('[FAIL] runtime policy architecture boundaries')
    for violation in violations:
        print(f'  - {violation}')
    sys.exit(1)

print('[PASS] runtime policy architecture boundaries')
