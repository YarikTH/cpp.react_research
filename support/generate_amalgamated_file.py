#!/usr/bin/env python3

# Script to amalgamate all react headers into single header
# Inspired by https://github.com/catchorg/Catch2/blob/v3.1.0/tools/scripts/generateAmalgamatedFiles.py

import os
import re
import datetime
from pathlib import Path
from typing import List, Set

# Path to support folder
support_folder_path = Path(__file__).parent.absolute()
assert support_folder_path.name == 'support', f'Assuming {Path(__file__).name} is placed in support folder'

# Path to ureact repository
root_path = support_folder_path.parent

# Path to ureact headers
ureact_include_path = root_path.joinpath('include')

# Path to result header
output_header = root_path.joinpath(root_path, 'single_include', 'react', 'react_amalgamated.h')

# Compiled regular expression to detect ureact self includes
internal_include_parser = re.compile(r'\s*#include "(react/.*)".*')

# These are the copyright comments in each file, we want to ignore them
ignored_lines = []

# The header of the amalgamated file: copyright information + explanation
# what this file is.
file_header = '''\
// =============================================================
// == DO NOT MODIFY THIS FILE BY HAND - IT IS AUTO GENERATED! ==
// =============================================================
'''


def formatted_file_header() -> str:
    # Returns file header with proper version string and generation time
    return file_header


def discover_hpp(where: Path) -> List[Path]:
    result: List[Path] = []

    for root, _, filenames in os.walk(where):
        for filename in [Path(f) for f in filenames]:
            if filename.suffix == '.h':
                result.append(Path(root, filename).relative_to(where))

    result.sort()

    return result


def amalgamate_source_file(out, filename: Path, /, processed_headers: Set[Path], expand_headers: bool) -> int:
    # Gathers statistics on how many headers were expanded
    concatenated = 1

    with open(filename, mode='r', encoding='utf-8') as input:
        for line in input:
            # ignore copyright and description lines
            if line in ignored_lines:
                continue

            m = internal_include_parser.match(line)

            # anything that isn't a ureact header can just be copied to
            # the resulting file
            if not m:
                out.write(line)
                continue

            # We do not want to expand headers for the cpp file amalgamation
            # but neither do we want to copy them to output
            if not expand_headers:
                continue

            next_header = Path(m.group(1))

            # We have to avoid re-expanding the same header over and over again
            if next_header in processed_headers:
                continue

            processed_headers.add(next_header)
            concatenated += amalgamate_source_file(out, Path(ureact_include_path, next_header),
                                                   processed_headers=processed_headers,  #
                                                   expand_headers=expand_headers)

    return concatenated


def amalgamate_sources(out, files: List[Path], /, processed_headers: Set[Path], expand_headers: bool) -> int:
    # Gathers statistics on how many headers were expanded
    concatenated = 0

    for next_header in files:
        if next_header not in processed_headers:
            processed_headers.add(next_header)
            concatenated += amalgamate_source_file(out, Path(ureact_include_path, next_header),
                                                   processed_headers=processed_headers,  #
                                                   expand_headers=expand_headers)

    return concatenated


def generate_header():
    output_header.parent.mkdir(exist_ok=True, parents=True)

    with open(output_header, mode='w', encoding='utf-8') as header:
        header.write(formatted_file_header())
        header.write('#ifndef REACT_REACT_AMALGAMATED_H\n')
        header.write('#define REACT_REACT_AMALGAMATED_H\n')
        concatenated = amalgamate_sources(header, discover_hpp(ureact_include_path), processed_headers=set(),
                                          expand_headers=True)
        # print(f'Concatenated {concatenated} headers')
        header.write('\n')
        header.write('#endif // REACT_REACT_AMALGAMATED_H\n')


if __name__ == "__main__":
    generate_header()
