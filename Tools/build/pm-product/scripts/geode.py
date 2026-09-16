# PM-BRINGUP [PM-TOOLS] ADDED: Repository-local version of the previously supplied full-product build helper; see PM-BRINGUP.md.
"""Read GEOS 2.x executable headers; no guest execution or code emulation.

Offsets derive from Tools/include/os90File.h and Tools/include/geode.h
in the supplied PC/GEOS source. All values are fixed-width little-endian.
"""
from __future__ import annotations
import hashlib
import struct
from pathlib import Path

MAGIC = b'\xc7E\xc1S'
HEADER_SIZE = 344

def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()

def _text(data: bytes) -> str:
    return data.split(b'\0', 1)[0].decode('cp1252', errors='replace').rstrip(' ')

def read_geode(path: Path) -> dict:
    data = path.read_bytes()
    if len(data) < HEADER_SIZE or data[:4] != MAGIC:
        raise ValueError(f'Not a GEOS 2.x executable: {path}')
    u16 = lambda offset: struct.unpack_from('<H', data, offset)[0]
    if u16(40) != 1:
        raise ValueError(f'GEOS file is not executable: {path}')
    nlib, nexport, nres = u16(332), u16(330), u16(336)
    if (nlib, nexport, nres) != (u16(266), u16(268), u16(264)):
        raise ValueError(f'Inconsistent executable counts: {path}')
    table_end = HEADER_SIZE + nlib * 14 + nexport * 4 + nres * 10
    if table_end > len(data):
        raise ValueError(f'Truncated executable tables: {path}')
    imports = []
    for i in range(nlib):
        name, kind, major, minor = struct.unpack_from('<8sHHH', data, HEADER_SIZE + i * 14)
        imports.append({'name': _text(name), 'kind': kind, 'major': major, 'minor': minor})
    return {
        'longname': _text(data[4:40]),
        'name': _text(data[300:308]),
        'extension': _text(data[308:312]),
        'file_type': u16(258),
        'attributes': u16(256),
        'major': u16(52), 'minor': u16(54),
        'resources': nres, 'exports': nexport, 'imports': imports,
        'size': len(data), 'sha256': hashlib.sha256(data).hexdigest(),
    }

def import_issues(geodes: list[dict]) -> list[dict]:
    """Check at least one compatible provider, allowing alternate hardware drivers."""
    by_name: dict[str, list[dict]] = {}
    for item in geodes:
        by_name.setdefault(item['name'], []).append(item)
    problems = []
    for item in geodes:
        for imp in item['imports']:
            providers = by_name.get(imp['name'], [])
            compatible = [g for g in providers if
                          g['major'] == imp['major'] and g['minor'] >= imp['minor'] and
                          (g['attributes'] & imp['kind']) == imp['kind']]
            if not compatible:
                problems.append({'consumer': item.get('path', item['name']),
                                 'import': imp,
                                 'providers': [{'path': g.get('path'), 'major': g['major'],
                                                'minor': g['minor']} for g in providers]})
    return problems
