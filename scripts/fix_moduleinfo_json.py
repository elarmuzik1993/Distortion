#!/usr/bin/env python3
"""
Fix invalid JSON in VST3 moduleinfo.json files.

JUCE's VST3 manifest helper generates JSON with trailing commas, which is
invalid JSON and causes some DAWs to reject the plugin. This script removes
the trailing commas to make the JSON valid.

Usage:
    python fix_moduleinfo_json.py <path_to_moduleinfo.json>
    python fix_moduleinfo_json.py <path_to_vst3_bundle>
    python fix_moduleinfo_json.py --all <build_directory>
"""

import sys
import os
import re
import json
from pathlib import Path


def fix_trailing_commas(json_string: str) -> str:
    """Remove trailing commas from JSON string."""
    # Pattern matches: comma followed by optional whitespace and closing bracket/brace
    # This handles: ,] ,} , ] , }
    pattern = r',(\s*[}\]])'
    return re.sub(pattern, r'\1', json_string)


def fix_moduleinfo_file(filepath: Path) -> bool:
    """Fix a single moduleinfo.json file. Returns True if fixed."""
    if not filepath.exists():
        print(f"  File not found: {filepath}")
        return False

    try:
        content = filepath.read_text(encoding='utf-8')

        # Try to parse as-is first
        try:
            json.loads(content)
            print(f"  Already valid: {filepath}")
            return False
        except json.JSONDecodeError:
            pass

        # Fix trailing commas
        fixed_content = fix_trailing_commas(content)

        # Verify the fix worked
        try:
            json.loads(fixed_content)
        except json.JSONDecodeError as e:
            print(f"  ERROR: Could not fix {filepath}: {e}")
            return False

        # Write fixed content
        filepath.write_text(fixed_content, encoding='utf-8')
        print(f"  Fixed: {filepath}")
        return True

    except Exception as e:
        print(f"  ERROR processing {filepath}: {e}")
        return False


def find_moduleinfo_files(search_path: Path) -> list:
    """Find all moduleinfo.json files in a directory tree."""
    files = []
    for root, dirs, filenames in os.walk(search_path):
        for filename in filenames:
            if filename == 'moduleinfo.json':
                files.append(Path(root) / filename)
    return files


def main():
    if len(sys.argv) < 2:
        print("Usage: fix_moduleinfo_json.py <path> [--all]")
        print("  <path>       Path to moduleinfo.json, .vst3 bundle, or build directory")
        print("  --all        Search recursively for all moduleinfo.json files")
        sys.exit(1)

    path = Path(sys.argv[1])
    search_all = '--all' in sys.argv

    files_to_fix = []

    if path.is_file() and path.name == 'moduleinfo.json':
        # Direct path to moduleinfo.json
        files_to_fix = [path]
    elif path.suffix == '.vst3' or (path.is_dir() and path.name.endswith('.vst3')):
        # VST3 bundle - look for moduleinfo.json inside
        moduleinfo = path / 'Contents' / 'Resources' / 'moduleinfo.json'
        if moduleinfo.exists():
            files_to_fix = [moduleinfo]
        else:
            print(f"No moduleinfo.json found in VST3 bundle: {path}")
            sys.exit(1)
    elif path.is_dir():
        if search_all:
            # Search recursively
            files_to_fix = find_moduleinfo_files(path)
        else:
            # Check common locations
            common_locations = [
                path / 'moduleinfo.json',
                path / 'Contents' / 'Resources' / 'moduleinfo.json',
            ]
            files_to_fix = [f for f in common_locations if f.exists()]

            if not files_to_fix:
                # Try to find .vst3 bundles
                for vst3 in path.glob('**/*.vst3'):
                    moduleinfo = vst3 / 'Contents' / 'Resources' / 'moduleinfo.json'
                    if moduleinfo.exists():
                        files_to_fix.append(moduleinfo)
    else:
        print(f"Path not found: {path}")
        sys.exit(1)

    if not files_to_fix:
        print(f"No moduleinfo.json files found in: {path}")
        sys.exit(1)

    print(f"Fixing {len(files_to_fix)} moduleinfo.json file(s)...")

    fixed_count = 0
    for filepath in files_to_fix:
        if fix_moduleinfo_file(filepath):
            fixed_count += 1

    print(f"Done. Fixed {fixed_count}/{len(files_to_fix)} file(s).")
    sys.exit(0)


if __name__ == '__main__':
    main()
