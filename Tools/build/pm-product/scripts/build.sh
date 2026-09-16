#!/usr/bin/env bash
# PM-BRINGUP [PM-TOOLS] ADDED: Repository-local version of the previously supplied full-product build helper; see PM-BRINGUP.md.
# Build from a fork with this source overlay ALREADY applied, using the pinned Watcom snapshot.
# Usage: bash scripts/build.sh SOURCE_DIR WATCOM_DIR OUTPUT_DIR [--bridge]
set -euo pipefail
if [[ $# -lt 3 || $# -gt 4 || (${4:-} != '' && ${4:-} != '--bridge') ]]; then
    echo 'Usage: build.sh SOURCE_DIR WATCOM_DIR OUTPUT_DIR [--bridge]' >&2; exit 2
fi
PACKAGE=$(cd -- "$(dirname -- "$0")/.." && pwd)
export ROOT_DIR=$(cd -- "$1" && pwd)
export WATCOM=$(cd -- "$2" && pwd)
OUTPUT=$(realpath -m -- "$3")
[[ ! -e "$OUTPUT" ]] || { echo "Output already exists: $OUTPUT" >&2; exit 2; }
LOGDIR="${OUTPUT}.build-logs"
mkdir -p "$LOGDIR"
exec > >(tee "$LOGDIR/build.log") 2>&1
# Legacy product tooling lowercases paths and invokes Perl shell fragments.
python3 - "$ROOT_DIR" <<'CHECKPATH'
import re,sys
s=sys.argv[1]
if s!=s.lower() or re.search(r'[^a-z0-9_./-]',s):
    raise SystemExit('Extract source to a lowercase Linux path without spaces, such as /tmp/pcgeos-pm-src')
CHECKPATH
# Match the exact compiler installation used for the supplied binaries.
python3 - "$PACKAGE" "$WATCOM" <<'PY'
import hashlib,json,sys
from pathlib import Path
p,w=map(Path,sys.argv[1:]); d=json.loads((p/'reports/inputs.json').read_text())
for name,expected in d['compiler_binaries'].items():
    f=w/name
    if not f.is_file() or hashlib.sha256(f.read_bytes()).hexdigest()!=expected:
        raise SystemExit('Wrong or missing pinned Watcom tool: '+str(f))
PY
# PM-BRINGUP [PM-TOOLS] CHANGED for source handoff: no patch application or source overwrites.
# The caller may edit this fork; use check-source.py separately to check the shipped snapshot.
[[ -f "$ROOT_DIR/Loader32/fault.asm" ]] || { echo "Apply the source overlay before building" >&2; exit 2; }
export LOCAL_ROOT="$ROOT_DIR/Local"
export PATH="$ROOT_DIR/bin:$WATCOM/binl64:$PATH"
export INCLUDE="$WATCOM/lh:$WATCOM/h"
mkdir -p "$LOCAL_ROOT" "$ROOT_DIR/bin" "$ROOT_DIR/.pm-perf-build"
BUILD="$ROOT_DIR/.pm-perf-build"
BRIDGE=''
if [[ ${4:-} == --bridge ]]; then
    [[ $(uname -m) == x86_64 ]] || { echo 'Bridge requires x86-64 Linux' >&2; exit 2; }
    gcc -O2 -Wall -o "$BUILD/elf32run" "$PACKAGE/host/elf32run.c"
    BRIDGE="$BUILD/elf32run"
fi
wrap() {
    local name=$1
    [[ -n "$BRIDGE" ]] || return 0
    python3 - "$ROOT_DIR/bin/$name" "$BRIDGE" <<'PY'
import os,shlex,sys
from pathlib import Path
f=Path(sys.argv[1]); bridge=sys.argv[2]
if not f.is_file(): raise SystemExit('Missing SDK tool: '+str(f))
if f.read_bytes()[:5]==b'\x7fELF\x01':
    elf=f.with_name(f.name+'.elf32'); os.replace(f,elf)
    f.write_text('#!/bin/sh\nexec '+shlex.quote(bridge)+' '+shlex.quote(str(elf))+' "$@"\n')
    f.chmod(0o755)
PY
}
# pmake is bootstrapped with the native 64-bit Watcom wmake binary.
(cd "$ROOT_DIR/Tools/pmake/pmake"; wmake install)
wrap pmake
for lib in utils compat; do
    (cd "$ROOT_DIR/Installed/Tools/$lib"; pmake linux)
done
for tool in esp glue uicpp uic goc grev pmake/makedpnd pmake/findlbdr loc geodump; do
    (cd "$ROOT_DIR/Installed/Tools/$tool"; pmake installlinux)
    wrap "${tool##*/}"
done
# The uploaded branch forces PROTECTED_MODE even in default/SBCS products.
flags=$(perl "$ROOT_DIR/Tools/scripts/perl/product_flags" esp)
[[ "$flags" == *'-DPROTECTED_MODE'* ]] || { echo 'Branch PM flag is missing' >&2; exit 2; }
# 'part', not 'full': the uploaded User/GEOS32 variant lacks dependencies.
# In this pmake, -n means NO_EC. It is NOT a dry-run option.
echo '===== LIBRARY Kernel ====='
(cd "$ROOT_DIR/Installed/Library/Kernel"; pmake -n part lib)
echo '===== DRIVER Stream (Sound import dependency) ====='
(cd "$ROOT_DIR/Installed/Driver/Stream"; pmake -n part lib)
for lib in Sound Net Compress Wav User Color Styles Ruler Bitmap Text SpecUI/Motif HostIf; do
    echo "===== LIBRARY $lib ====="
    (cd "$ROOT_DIR/Installed/Library/$lib"; pmake -n part lib)
done
# PM-BRINGUP [PM-DIAG2] Force the new include into an existing build, even with stale dependency metadata.
(cd "$ROOT_DIR/Installed/Loader32/Text"; rm -f loader.obj; pmake -n loader.exe)
for driver in Font/Nimbus IFS/DOS/OS2 Keyboard Mouse/KBMouse Sound/Standard Stream Video/Dumb/VidMem Task/NonTS; do
    echo "===== DRIVER $driver ====="
    (cd "$ROOT_DIR/Installed/Driver/$driver"; pmake -n part)
done
# A make-variable change alone does not invalidate old object files.
(cd "$ROOT_DIR/Installed/Driver/Video/VGAlike/VGA16"; rm -f vga16Manager.obj; pmake -n PM_PERF_MINIMAL=1 part)
(cd "$ROOT_DIR/Installed/Appl/Perf"; rm -f perf.obj perf.rdef; pmake -n PM_PERF_MINIMAL=1 part)
# PM-BRINGUP [PM-TTF] Recompile the fixed font registration source in the selected product pass.
rm -f "$ROOT_DIR/Installed/Driver/Font/TrueType/ttinit.obj"
# Root dependencies are retained, including the GDIPointer -> GDI correction.
cp "$PACKAGE/source/Product.mk" "$ROOT_DIR/Installed/PMProduct.mk"
(cd "$ROOT_DIR/Installed"; pmake -n -f PMProduct.mk PM_PERF_MINIMAL=1 fullProduct) 2>&1 | tee "$LOGDIR/selected.log"
python3 "$PACKAGE/scripts/check-build-log.py" "$LOGDIR/selected.log"
# Product-specific providers explicitly referenced in bbxensem.filetree.
(cd "$ROOT_DIR/Installed/Library/GDI/GenPC"; pmake -n win32/gdi.geo)
(cd "$ROOT_DIR/Installed/Library/Breadbox/ImpGraph"; pmake -n FJPEG/impgraph.geo)
mkdir -p "$ROOT_DIR/Installed/Tools/swat/Stub32/LowMem"
(cd "$ROOT_DIR/Installed/Tools/swat/Stub32"; pmake -n SUBDIRS=LowMem all)
printf '0\n' > "$LOGDIR/selected-build-exit.txt"
python3 "$PACKAGE/scripts/assemble.py" "$ROOT_DIR" "$OUTPUT" --logs "$LOGDIR"
python3 "$PACKAGE/scripts/audit.py" "$OUTPUT" --json "$LOGDIR/audit.json"
echo "Built and assembled: $OUTPUT (this rebuild has NOT been guest-tested)."
