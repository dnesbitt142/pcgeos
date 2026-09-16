#!/usr/bin/env python3
# PM-BRINGUP [PM-TOOLS] ADDED: Repository-local version of the previously supplied full-product build helper; see PM-BRINGUP.md.
"""Assemble a fresh full product from compiled source and the upstream manifest.

This is a LOCAL build operation: it copies fonts and HDPMI16 from the caller's
source tree. This source handoff contains no font files or runtime binaries. Never use an
existing installation as --output.
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
from geode import sha256
from product_config import product_profile

PACKAGE = Path(__file__).resolve().parent.parent
FONT_SUFFIXES = {'.fnt','.ttf','.otf','.fon','.pfa','.pfb','.bdf','.pcf','.woff','.woff2'}
SUPPLEMENTAL = {
    'SYSTEM/MAILBOX.GEO':'Library/Mailbox/mailbox.geo',
    'SYSTEM/MBDATA/FILEDD.GEO':'Driver/Mailbox/Data/FileDD/filedd.geo',
    'SYSTEM/MBDATA/VMTREE.GEO':'Driver/Mailbox/Data/VMTree/vmtree.geo',
    'SYSTEM/MBTRANS/SPOOLTD.GEO':'Driver/Mailbox/Transport/SpoolTD/spooltd.geo',
    'SYSTEM/PRINTER/CANONBJC.GEO':'Driver/Printer/DotMatrix/CanonBJC/canonbjc.geo',
    'SYSTEM/FONT/NIMBUS.GEO':'Driver/Font/Nimbus/nimbus.geo',
}
START = '''@ECHO OFF
REM PC/GEOS protected-mode full-product candidate, not guest-certified.
IF NOT EXIST HDPMI16.EXE GOTO MISSING
IF NOT EXIST USERDATA\\FONT\\BERKELEY.FNT GOTO MISSING
IF NOT EXIST SYSTEM\\SPOOL.GEO GOTO MISSING
HDPMI16 -r
LOADER %1 %2 %3 %4 %5 %6 %7 %8 %9
GOTO END
:MISSING
REM PM-BRINGUP: local source build, not the older binary installer.
ECHO Incomplete installation. Rebuild using Tools/build/pm-product on the host.
ECHO Start from the newly assembled PMGEOS directory.
:END
'''
S = '''@ECHO OFF
REM Experimental serial SWAT entry point; debugger transport is untested.
IF NOT EXIST HDPMI16.EXE GOTO MISSING
HDPMI16 -r
SWAT /c:1 /b:3 LOADER.EXE %1 %2 %3 %4
GOTO END
:MISSING
ECHO Build and assemble using Tools/build/pm-product first.
:END
'''

def crlf(text: str) -> bytes:
    return text.replace('\r\n','\n').replace('\r','\n').replace('\n','\r\n').encode('cp1252')

def local_only(relative: str) -> bool:
    r=relative.upper()
    return r.startswith('USERDATA/FONT/') or r in ('HDPMI16.EXE','PRIVDATA/TTF_CACH.000') or Path(r).suffix.lower() in FONT_SUFFIXES

def assemble(source: Path, output: Path, logdir: Path) -> dict:
    source=source.resolve(strict=True); output=output.absolute(); logdir=logdir.absolute()
    if output.exists() or output.is_symlink(): raise ValueError(f'Output already exists: {output}')
    if source.as_posix()!=source.as_posix().lower() or re.search(r'[^a-z0-9_./-]',source.as_posix()):
        raise ValueError('The legacy product assembler requires a lowercase Linux source path without spaces/shell metacharacters')
    logdir.mkdir(parents=True,exist_ok=True)
    allowed={i['source'].casefold():i for i in json.loads((PACKAGE/'source/product-exclusions.json').read_text())}
    tree=source/'Tools/build/product/bbxensem'; script=tree/'Scripts/buildbbx.pl'
    if not script.is_file(): raise ValueError('Not the recorded PC/GEOS source tree')
    # Refuse an overlay of unrelated product binaries from an earlier build.
    overlay=source/'bbxensem/Installed'
    if overlay.is_dir() and any(overlay.rglob('*.geo')):
        raise ValueError('Remove/quarantine the bbxensem/Installed product overlay first')
    with tempfile.TemporaryDirectory(prefix='pmgeos-product-',dir='/tmp') as tempdir:
        temp=Path(tempdir); (temp/'local').mkdir()
        env=os.environ.copy()
        env.update(ROOT_DIR=str(source),LOCAL_ROOT=str(temp/'local'),PERL5LIB=str(tree/'Scripts'),
                   PATH=str(source/'bin')+os.pathsep+env.get('PATH',''))
        command=['perl',str(script),'-noprompt','-nolocal',f'desttree={temp}/image']
        proc=subprocess.run(command,cwd=tree,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        text=proc.stdout.decode(errors='replace')
        (logdir/'assembly.log').write_text(text)
        if proc.returncode: raise ValueError(f'Upstream assembler exited {proc.returncode}; see assembly.log')
        missing=sorted(set(re.findall(r'^ERROR: Could not find file (.+?) in any of the source trees\.',text,re.M)))
        other_errors=[l for l in text.splitlines() if l.startswith('ERROR:') and not l.startswith('ERROR: Could not find file ')]
        unexpected=[x for x in missing if x.casefold() not in allowed]
        (logdir/'assembly-missing.json').write_text(json.dumps({'missing':missing,'unexpected':unexpected,'other_errors':other_errors},indent=2)+'\n')
        if unexpected or other_errors:
            raise ValueError(f'Unresolved product assembly errors: {unexpected+other_errors}')
        assembled=temp/'image/localpc/ensemble'
        if not (assembled/'geos.ini').is_file(): raise ValueError('Upstream assembler did not produce a product')
        source_map={}
        for a,b in re.findall(r'^Copying (.+)\n\s+to (.+)$',text,re.M):
            dst=Path(b)
            if dst.is_relative_to(assembled): source_map[dst.relative_to(assembled).as_posix().upper()]=Path(a)
        output.parent.mkdir(parents=True,exist_ok=True)
        staging=Path(tempfile.mkdtemp(prefix='.pmgeos-assemble-',dir=output.parent))
        try:
            for f in sorted(assembled.rglob('*')):
                if f.is_symlink():raise ValueError(f'Unexpected symlink from assembler: {f}')
                rel=f.relative_to(assembled).as_posix().upper(); dest=staging/rel
                if f.is_dir():dest.mkdir(parents=True,exist_ok=True);continue
                dest.parent.mkdir(parents=True,exist_ok=True)
                if dest.exists():raise ValueError(f'DOS filename collision: {rel}')
                shutil.copy2(f,dest)
            for rel,origin in SUPPLEMENTAL.items():
                src=source/'Installed'/origin
                if not src.is_file():raise ValueError(f'Missing supplementary build: {origin}')
                dest=staging/rel;dest.parent.mkdir(parents=True,exist_ok=True)
                if dest.exists() and sha256(dest)!=sha256(src):raise ValueError(f'Supplement conflicts with product file: {rel}')
                shutil.copy2(src,dest);source_map[rel]=src
            ini=staging/'GEOS.INI'
            ini.write_bytes(crlf(product_profile(ini.read_text(encoding='cp1252'))))
            (staging/'START.BAT').write_bytes(crlf(START))
            for name in ('GO.BAT','ENSEMBLE.BAT'):
                (staging/name).write_bytes(crlf('@ECHO OFF\nCALL START.BAT %1 %2 %3 %4 %5 %6 %7 %8 %9\n'))
            (staging/'S.BAT').write_bytes(crlf(S))
            (staging/'SS.BAT').write_bytes(crlf(S.replace('/b:3 LOADER','/b:3 /s LOADER')))
            for path in staging.iterdir():
                if path.is_file() and path.suffix.upper() in ('.BAT','.INI'):
                    path.write_bytes(crlf(path.read_text(encoding='cp1252')))
            (staging/'PMBUILD.TXT').write_bytes(crlf(
                'PC/GEOS protected-mode full-product candidate\n'
                'Compiled from branch commit 30df506fc64720fd82e528acbcb192adbaa48fce.\n'
                'This new build has not been guest-tested. See PM-BRINGUP.md for reported results.\n'
                '640x480 RGB565 banked VESA; Perf retains four basic meters.\n'
                'Spooler is included and enabled. Desktop is ISDesk with a taskbar.\n'
                'Use START.BAT. Do not overlay an old minimal installation.\n'
                'F4: keyboard mouse; arrows: movement; Ins/Del: mouse buttons.\n'
                'See PM-BRINGUP.md and TechDocs/Markdown/pm-bringup for scope and exclusions.\n'))
            origins={}
            local=[]
            for f in sorted(staging.rglob('*')):
                if not f.is_file():continue
                rel=f.relative_to(staging).as_posix()
                if rel in source_map:
                    src=source_map[rel]
                    if src.is_relative_to(source):origins[rel]=src.relative_to(source).as_posix()
                if local_only(rel):
                    if rel not in origins: raise ValueError(f'No source provenance for local-only input: {rel}')
                    local.append(dict(path=rel,source=origins[rel],size=f.stat().st_size,sha256=sha256(f)))
            result={'source':str(source),'output':str(output),'upstream_assembler_exit':proc.returncode,
                    'missing':missing,'supplemental':SUPPLEMENTAL,'origins':origins,'local_inputs':local,
                    'guest_tested':False}
            (logdir/'assembly-provenance.json').write_text(json.dumps(result,indent=2)+'\n')
            if output.exists() or output.is_symlink():raise ValueError('Output appeared during assembly')
            os.rename(staging,output)
        finally:
            if staging.exists():shutil.rmtree(staging)
    return result

def main() -> int:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('source',type=Path);ap.add_argument('output',type=Path);ap.add_argument('--logs',type=Path,required=True)
    args=ap.parse_args()
    try:result=assemble(args.source,args.output,args.logs)
    except (OSError,ValueError,KeyError) as exc:print(f'ERROR: {exc}',file=sys.stderr);return 1
    print(json.dumps({k:v for k,v in result.items() if k not in ('origins','local_inputs')},indent=2))
    return 0
if __name__=='__main__':raise SystemExit(main())
