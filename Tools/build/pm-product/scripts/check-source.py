#!/usr/bin/env python3
# PM-BRINGUP [PM-TOOLS] ADDED: read-only check of the 17 consolidated runtime source paths.
"""Check source provenance without editing a checkout or invoking a compiler.

--baseline ROOT checks the original uploaded snapshot's affected paths.
--current ROOT checks the annotated handoff and its comment-only relationship to
previously supplied sources. This is deliberately not a runtime or boot test.
After making your own edits, a current-snapshot mismatch is expected; the build
helper does not require this snapshot check to pass for development builds.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

PACKAGE = Path(__file__).resolve().parents[1]


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def without_annotations(data: bytes, suffix: str) -> bytes:
    """Remove only this handoff's full-line comments, never executable text."""
    text = data.decode('latin1').replace('\r\n', '\n')
    if suffix in ('.c', '.ui'):
        # A single, standalone C comment: no nested comment terminator or code.
        pattern = re.compile(r'^/\* PM-BRINGUP (?:(?!\*/).)*\*/\n?$')
    elif suffix in ('.mk', '.filetree'):
        pattern = re.compile(r'^# PM-BRINGUP [^\n]*\n?$')
    else:
        pattern = re.compile(r'^; PM-BRINGUP [^\n]*\n?$')
    return ''.join(line for line in text.splitlines(keepends=True)
                   if not pattern.fullmatch(line)).encode('latin1')


def check(root: Path, manifest: dict, mode: str) -> list[dict]:
    if not root.is_dir():
        raise ValueError(f'Checkout does not exist: {root}')
    root = root.resolve()
    issues = []
    for entry in manifest['runtime_sources']:
        relative = Path(entry['path'])
        if relative.is_absolute() or '..' in relative.parts:
            raise ValueError('Invalid source-manifest path')
        path = root / relative
        current = root
        linked = False
        for part in relative.parts:
            current /= part
            linked |= current.is_symlink()
        if linked:
            issues.append({'path': entry['path'], 'reason': 'symlink in source path'})
            continue
        expected = entry['base_sha256'] if mode == 'baseline' else entry['annotated_sha256']
        if expected is None:
            if path.exists():
                issues.append({'path': entry['path'], 'reason': 'expected a new, absent source path'})
            continue
        if not path.is_file():
            issues.append({'path': entry['path'], 'reason': 'missing source file'})
            continue
        data = path.read_bytes()
        if digest(data) != expected:
            issues.append({'path': entry['path'], 'reason': 'source hash differs',
                           'expected': expected, 'actual': digest(data)})
        if mode == 'current':
            stripped = without_annotations(data, path.suffix)
            if digest(stripped) != entry['previous_normalized_sha256']:
                issues.append({'path': entry['path'], 'reason': 'not comment-only relative to the previous supplied source'})
    return issues


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--baseline', type=Path)
    group.add_argument('--current', type=Path)
    args = parser.parse_args()
    mode = 'baseline' if args.baseline is not None else 'current'
    try:
        manifest = json.loads((PACKAGE / 'source-manifest.json').read_text())
        issues = check(args.baseline if mode == 'baseline' else args.current, manifest, mode)
    except (OSError, ValueError, KeyError) as exc:
        print(f'ERROR: {exc}', file=sys.stderr)
        return 2
    print(json.dumps({'mode': mode, 'base_commit': manifest['base_commit'],
                      'checked_paths': len(manifest['runtime_sources']),
                      'passed': not issues, 'issues': issues}, indent=2))
    return 1 if issues else 0


if __name__ == '__main__':
    raise SystemExit(main())
