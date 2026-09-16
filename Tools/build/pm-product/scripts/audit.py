#!/usr/bin/env python3
# PM-BRINGUP [PM-TOOLS] ADDED: Repository-local version of the previously supplied full-product build helper; see PM-BRINGUP.md.
"""Audit product files and load-time imports; this is not a boot test."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re
import sys
from geode import read_geode, import_issues, MAGIC

REQUIRED = {
    'system/loader': None,
    'kernel': 'SYSTEM/GEOS.GEO',
    'ui': 'SYSTEM/UI.GEO',
    'spooler': 'SYSTEM/SPOOL.GEO',
    'display': 'SYSTEM/VIDEO/VGA16.GEO',
    'primary filesystem': 'SYSTEM/FS/OS2.GEO',
}

def ini_values(text: str) -> dict[tuple[str, str], str]:
    lines = text.replace('\r\n','\n').splitlines()
    result = {}; section = ''; i = 0
    while i < len(lines):
        line = lines[i].strip(); i += 1
        if not line or line.startswith((';','#')): continue
        m = re.fullmatch(r'\[([^\]]+)\]', line)
        if m: section = m[1].casefold(); continue
        if '=' not in line: continue
        key, value = line.split('=',1); key=key.strip().casefold(); value=value.strip()
        depth = value.count('{') - value.count('}')
        while depth > 0 and i < len(lines):
            value += '\n' + lines[i]
            depth += lines[i].count('{') - lines[i].count('}')
            i += 1
        if depth: raise ValueError(f'Unterminated INI value: {section}/{key}')
        if (section,key) in result: raise ValueError(f'Duplicate INI value: {section}/{key}')
        result[section,key] = value
    return result

def dir_longname(path: Path) -> str:
    names = [p for p in path.iterdir() if p.name.casefold() == '@dirname.000']
    if names:
        data = names[0].read_bytes()
        if data[:4] == MAGIC and len(data) >= 40:
            return data[4:40].split(b'\0')[0].decode('cp1252',errors='replace').rstrip(' ')
    return path.name

def app_logical_name(root: Path, relative: str, longname: str) -> str:
    parts = Path(relative).parts
    base = 1 if parts[0].casefold() == 'world' else (2 if len(parts) > 1 and
            parts[0].casefold() == 'system' and parts[1].casefold() == 'sysappl' else -1)
    if base < 0: return ''
    dirs = [dir_longname(root.joinpath(*parts[:i+1])) for i in range(base,len(parts)-1)]
    return '\\'.join(dirs + [longname])

def audit(root: Path) -> dict:
    root = root.resolve(strict=True)
    geodes=[]; pathmap={}; errors=[]
    for path in sorted(root.rglob('*')):
        if path.is_symlink(): errors.append({'symlink':str(path)}); continue
        if not path.is_file(): continue
        rel=path.relative_to(root).as_posix()
        if rel.casefold() in pathmap: errors.append({'case_collision':rel})
        pathmap[rel.casefold()] = path
        for component in path.relative_to(root).parts:
            stem, dot, suffix = component.partition('.')
            if not (1 <= len(stem) <= 8 and len(suffix) <= 3) or any(c in component for c in ' +,;=[]'):
                errors.append({'not_dos_83':rel}); break
        if path.suffix.casefold() == '.geo':
            try: geodes.append(dict(path=rel, **read_geode(path)))
            except ValueError as exc: errors.append({'invalid_geode':str(exc)})
    issues=import_issues(geodes)
    ini_path=pathmap.get('geos.ini')
    if ini_path is None: raise ValueError('Missing GEOS.INI')
    values=ini_values(ini_path.read_text(encoding='cp1252'))
    mandatory={'kernel':'geos', 'ui':'ui', 'spooler':'spool', 'desktop':'isdesk',
               'file manager':'manager', 'word processor':'write', 'spreadsheet':'geocalc',
               'drawing':'draw','database':'geofile','preferences':'prefmgr','monitor':'perf'}
    names={g['name'] for g in geodes}
    for label,name in mandatory.items():
        if name not in names: errors.append({'missing_component':label,'permanent_name':name})
    loader=pathmap.get('loader.exe')
    if loader is None or loader.read_bytes()[:2] != b'MZ': errors.append({'missing_loader':True})
    settings={('ui','nospooler'):'false',('ui','nomailbox'):'true',
              ('screen 0','driver'):'vga16.geo',
              ('screen 0','device'):'VESA Compatible SuperVGA: 640x480 64K-color'}
    for key,expected in settings.items():
        if values.get(key) != expected: errors.append({'setting':list(key),'expected':expected,'actual':values.get(key)})
    # Explicit startup loads are not represented in the geode import table.
    apps={app_logical_name(root,g['path'],g['longname']).casefold():g['path']
          for g in geodes if g['attributes'] & 0x8000}
    startup=values.get(('ui','execonstartup'),'').strip().strip('{}').strip().splitlines()
    startup_checks=[]
    for value in startup:
        value=value.strip()
        if not value: continue
        path=apps.get(value.casefold())
        startup_checks.append({'application':value,'path':path})
        if path is None: errors.append({'unresolved_startup_application':value})
    # Verify configured drivers exist by DOS name as well as the implicit spooler.
    driver_checks=[]
    filenames={p.name.casefold():rel for rel,p in pathmap.items() if p.suffix.casefold()=='.geo'}
    for key in [('system','primaryfsd'),('screen 0','driver'),('mouse','driver'),('keyboard','driver'),
                ('ui','specific'),('sound','synthdriver'),('sound','sampledriver')]:
        val=values.get(key,'')
        resolved=filenames.get(val.casefold())
        driver_checks.append({'setting':list(key),'file':val,'resolved':resolved})
        if not resolved: errors.append({'unresolved_driver':list(key),'value':val})
    for font_driver in values.get(('system','font'),'').strip().strip('{}').splitlines():
        if not font_driver.strip(): continue
        if font_driver.strip().casefold() not in filenames:
            errors.append({'unresolved_font_driver':font_driver.strip()})
    return {'root':str(root),'geodes':geodes,'geode_count':len(geodes),
            'file_count':len(pathmap),'import_issues':issues,'errors':errors,
            'startup':startup_checks,'configured_drivers':driver_checks,
            'passed':not issues and not errors,'guest_tested':False}

def main() -> int:
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('runtime',type=Path)
    ap.add_argument('--json',type=Path);args=ap.parse_args()
    try: result=audit(args.runtime)
    except (OSError,ValueError) as exc:
        print(f'ERROR: {exc}',file=sys.stderr);return 1
    if args.json: args.json.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k not in ('geodes',)},indent=2))
    return 0 if result['passed'] else 1
if __name__=='__main__':raise SystemExit(main())
