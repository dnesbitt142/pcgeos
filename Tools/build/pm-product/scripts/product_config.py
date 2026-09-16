# PM-BRINGUP [PM-TOOLS] ADDED: Repository-local version of the previously supplied full-product build helper; see PM-BRINGUP.md.
"""Apply the selected DOSBox-X/640x480 profile to the upstream product INI."""
from __future__ import annotations
import re

def set_value(text: str, section: str, key: str, value: str | None) -> str:
    """Edit exactly one key in an INI section, including braced GEOS values."""
    lines = text.replace('\r\n', '\n').splitlines()
    headers = [(i, m.group(1).strip().casefold()) for i, line in enumerate(lines)
               if (m := re.fullmatch(r'\s*\[([^\]]+)\]\s*', line))]
    positions = [i for i, name in headers if name == section.casefold()]
    if len(positions) > 1:
        raise ValueError(f'Duplicate INI section: {section}')
    if not positions:
        if value is None: return '\n'.join(lines) + '\n'
        lines += ['', f'[{section}]', f'{key} = {value}']
        return '\n'.join(lines) + '\n'
    start = positions[0]
    stop = next((i for i, _ in headers if i > start), len(lines))
    pattern = re.compile(r'^\s*' + re.escape(key) + r'\s*=', re.I)
    found = [i for i in range(start + 1, stop) if pattern.match(lines[i])]
    if len(found) > 1: raise ValueError(f'Duplicate INI key: [{section}] {key}')
    new = [] if value is None else [f'{key} = {value}']
    if not found:
        lines[stop:stop] = new
    else:
        begin = found[0]
        end = begin + 1
        rhs = lines[begin].split('=', 1)[1].strip()
        depth = rhs.count('{') - rhs.count('}')
        while depth > 0 and end < stop:
            depth += lines[end].count('{') - lines[end].count('}')
            end += 1
        if depth != 0: raise ValueError(f'Unclosed GEOS INI value: [{section}] {key}')
        lines[begin:end] = new
    return '\n'.join(lines) + '\n'

def product_profile(text: str) -> str:
    # PM-BRINGUP [PM-PRODUCT] Full product: include/enable spooler, use ISDesk,
    # taskbar and four-meter Perf; disable mailbox startup, not its source code.
    # PM_PERF_MINIMAL selects both this Perf subset and the banked VBE driver.
    updates = {
        'system': {'primaryFSD': 'os2.geo', 'continueSetup': 'false',
                   'font': '{\nnimbus.geo\ntruetype.geo\n}', 'power': None},
        'ui': {'specific': 'motif.geo', 'noSpooler': 'false', 'noMailbox': 'true',
               'haveEnvironmentApp': 'false', 'productName': 'PC/GEOS PM Product',
               'screenBlanker': 'false', 'sound': 'false',
               'execOnStartup': '{\nISDesk\nTrayApps\nSysTray Clock\nDesk Accessories\\Perf\n}'},
        'screen 0': {'driver': 'vga16.geo',
                     'device': 'VESA Compatible SuperVGA: 640x480 64K-color'},
        'mouse': {'driver': 'kbmouse.geo', 'device': 'Arrow Key Mouse (Use F4, Ins, and Del.)'},
        'keyboard': {'driver': 'kbd.geo'},
        'sound': {'driver': 'standard.geo', 'device': 'Standard PC Speaker (PC/AT)',
                  'synthDriver': 'standard.geo', 'sampleDriver': 'standard.geo', 'volume': '0'},
        'motif options': {'taskBarEnabled': 'true', 'clickSounds': 'false'},
        'uiFeatures': {'defaultLauncher': 'ISDesk'},
        'uiFeatures - advanced': {'defaultLauncher': 'ISDesk'},
    }
    for section, keys in updates.items():
        for key, value in keys.items(): text = set_value(text, section, key, value)
    # Do not configure a raw-HLT driver for the initial DPMI guest profile.
    return '; Protected-mode product profile. This rebuild has not been guest-tested.\n' + text
