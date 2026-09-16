#!/usr/bin/env python3
# PM-BRINGUP [PM-TOOLS] ADDED: Repository-local version of the previously supplied full-product build helper; see PM-BRINGUP.md.
"""Reject compiler/linker/make fatal diagnostics, even when legacy tools exit zero."""
import argparse,re,sys
from pathlib import Path

def failures(text: str) -> list[str]:
    patterns=[r'^error:',r'^Error ',r'Error! E\d+',r'^\*\*\* Error code',
              r'^pmake: Can.t figure out how to make .+\. Stop',
              r'^pmake: don.t know how to make',r'^.*elf32run: signal']
    return [line for line in text.splitlines() if any(re.search(p,line,re.I) for p in patterns)]

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('log',type=Path);args=ap.parse_args()
    bad=failures(args.log.read_text(errors='replace'))
    if bad:
        print('\n'.join(bad),file=sys.stderr);raise SystemExit(1)
    print('No recognized fatal build diagnostics in '+str(args.log))
