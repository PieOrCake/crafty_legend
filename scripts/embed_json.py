#!/usr/bin/env python3
"""Convert JSON data files into C++ source with embedded byte arrays."""

import os
import sys

DATA_DIR = os.path.join(os.path.dirname(__file__), '..', 'data', 'CraftyLegend')
OUT_DIR = os.path.join(os.path.dirname(__file__), '..', 'src')

FILES = [
    ('legendaries.json', 'LEGENDARIES'),
    ('items.json',       'ITEMS'),
    ('recipes.json',     'RECIPES'),
    ('currencies.json',  'CURRENCIES'),
]

def to_c_array(data, per_line=16):
    lines = []
    for i in range(0, len(data), per_line):
        chunk = data[i:i+per_line]
        line = ', '.join(f'0x{b:02x}' for b in chunk)
        lines.append('    ' + line + ',')
    return '\n'.join(lines)

def main():
    # Generate header
    header_path = os.path.join(OUT_DIR, 'embedded_data.h')
    cpp_path = os.path.join(OUT_DIR, 'embedded_data.cpp')

    header_lines = [
        '#ifndef EMBEDDED_DATA_H',
        '#define EMBEDDED_DATA_H',
        '',
        '#include <cstddef>',
        '',
        'namespace EmbeddedData {',
        '',
    ]

    cpp_lines = [
        '#include "embedded_data.h"',
        '',
        'namespace EmbeddedData {',
        '',
    ]

    for filename, varname in FILES:
        filepath = os.path.join(DATA_DIR, filename)
        with open(filepath, 'rb') as f:
            data = f.read()

        size = len(data)
        print(f'{filename}: {size} bytes -> {varname}')

        header_lines.append(f'extern const unsigned char {varname}_DATA[];')
        header_lines.append(f'extern const size_t {varname}_SIZE;')
        header_lines.append('')

        cpp_lines.append(f'const unsigned char {varname}_DATA[] = {{')
        cpp_lines.append(to_c_array(data))
        cpp_lines.append('};')
        cpp_lines.append(f'const size_t {varname}_SIZE = {size};')
        cpp_lines.append('')

    header_lines += [
        '} // namespace EmbeddedData',
        '',
        '#endif // EMBEDDED_DATA_H',
        '',
    ]

    cpp_lines += [
        '} // namespace EmbeddedData',
        '',
    ]

    with open(header_path, 'w') as f:
        f.write('\n'.join(header_lines))

    with open(cpp_path, 'w') as f:
        f.write('\n'.join(cpp_lines))

    print(f'Generated {header_path}')
    print(f'Generated {cpp_path}')

if __name__ == '__main__':
    main()
