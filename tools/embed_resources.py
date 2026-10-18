#!/usr/bin/env python3
"""Embed local assets so the installed executable needs no adjacent files."""
from pathlib import Path
root = Path(__file__).resolve().parent.parent
resources = [('MdLogoSvg', root / 'assets/MD_PCB_Logo.svg'),
             ('SoftwareLicense', root / 'LICENSE'), ('LicenseNotice', root / 'NOTICE')]
text = '#pragma once\n// Generated from assets, LICENSE and NOTICE; MD logo has separate terms in NOTICE.\nnamespace sk150 {\n'
for name, path in resources:
    data = path.read_text(encoding='utf-8')
    assert ')resource"' not in data
    text += f'inline constexpr const char {name}[] = R"resource({data})resource";\n'
text += '}\n'
(root / 'src/resources.hpp').write_text(text, encoding='utf-8')
