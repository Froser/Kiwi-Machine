# Copyright (C) 2024 Yisi Yu
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
import argparse
import json
import os
import shutil
import zipfile
import subprocess
import webbrowser
from pathlib import Path

# Get project root directory
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))
# Get wasm project directory
WASM_PROJECT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), 'kiwi-machine'))
rom_id = 0


def CopyFile(src, dest):
    print(f"Copying {src} to {dest}")
    shutil.copy(src, dest)


def ExtractAllTo(src, dest):
    for f in sorted(Path(src).iterdir()):
        ExtractTo(f, dest + '/' + f.stem)


def ExtractTo(src, dest):
    if src.is_file() and src.suffix == '.zip':
        print(f'Extracting {src.absolute()} to {dest}')
        with zipfile.ZipFile(src.absolute(), 'r', zipfile.ZIP_DEFLATED) as zip_file:
            zip_file.extractall(dest)
    elif src.is_dir():
        print(f'Entering {src}')
        ExtractAllTo(src, dest)


def GenerateDatabase(dir):
    db_file = Path(dir + '/db.json')
    print(f'Generating database file: {db_file}')
    db = []
    AddManifestFromDir(db, dir, dir)

    print(f'{db_file} Generated.')
    with open(db_file.absolute(), 'w', encoding='utf-8') as db_f:
        json.dump(db, db_f, ensure_ascii=False, indent=1)


def AddManifestFromDir(db, relpath, file_or_dir):
    for f in sorted(Path(file_or_dir).iterdir()):
        AddManifestToJson(db, relpath, f)


def WriteManifestToDb(manifest_data_titles, db, manifest_file):
    global rom_id
    # Separate ROM details to database.
    for title in manifest_data_titles:
        if title == 'default':
            manifest_data_titles['default']['id'] = rom_id
            manifest_data_titles['default']['name'] = manifest_file.parent.name
            db.append(manifest_data_titles['default'])
        else:
            manifest_data_titles[title]['id'] = rom_id
            manifest_data_titles[title]['name'] = title
            manifest_data_titles[title]['dir'] = manifest_file.parent.name
            db.append(manifest_data_titles[title])
        rom_id += 1


def AddManifestToJson(db, relpath, file_or_dir):
    global rom_id
    if file_or_dir.is_file():
        manifest_data_titles = {}
        if file_or_dir.name == 'manifest.json':
            manifest_data = {}
            # If a directory has manifest.json, this is what we want.
            # Load manifest, and set it to the database json file.
            print(f'Found manifest {file_or_dir.absolute()}')

            # Read manifest.json
            with open(file_or_dir.absolute(), 'r', encoding='utf-8') as f:
                manifest_data = json.load(f)
            manifest_data_titles = manifest_data['titles']

        elif file_or_dir.suffix == '.nes':
            if not (file_or_dir.parent.joinpath('manifest.json')).exists():
                # If there's no manifest, it may be a hacked ROM, so we have to construct its manifest.json to memory.
                print(f"No manifest file ({file_or_dir.parent.joinpath('manifest.json')}) for ROM, create one: {file_or_dir}")
                manifest_data_titles = {
                    'default': {
                        'id': rom_id,
                        'name': file_or_dir.parent,
                        'dir': file_or_dir.parent.relative_to(Path(relpath)).as_posix()
                    }
                }

        # Write db
        WriteManifestToDb(manifest_data_titles, db, file_or_dir)
        return True
    elif file_or_dir.is_dir():
        return AddManifestFromDir(db, relpath, file_or_dir)


def run_npm_command(command):
    """Run npm command in the wasm project directory"""
    print(f"Running: npm {command} in {WASM_PROJECT_DIR}")
    result = subprocess.run(['npm', command], cwd=WASM_PROJECT_DIR, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"Error running npm {command}: {result.stderr}")
        return False
    print(result.stdout)
    return True


def main():
    parser = argparse.ArgumentParser(description='Build and deploy WebAssembly build of Kiwi Machine')
    parser.add_argument('--debug', action='store_true', help='Deploy from debug build directory')
    parser.add_argument('--release', action='store_true', help='Deploy from release build directory')
    parser.add_argument('build_dir', nargs='?', help='Custom build directory path')
    parser.add_argument('rom_dir', nargs='?', help='ROM directory path (optional)')
    parser.add_argument('--install', action='store_true', help='Install dependencies (npm install)')
    parser.add_argument('--start', action='store_true', help='Start development server and open browser (npm start)')
    parser.add_argument('--build', action='store_true', help='Build production version (npm run build)')
    
    args = parser.parse_args()
    
    # Handle npm commands
    if args.install:
        run_npm_command('install')
        return
    elif args.start:
        # Run npm start in the background
        print("Starting development server...")
        subprocess.Popen(['npm', 'start'], cwd=WASM_PROJECT_DIR)
        # Open browser to localhost (default port 3000)
        print("Opening browser to http://localhost:3000")
        webbrowser.open('http://localhost:3000')
        return
    elif args.build:
        run_npm_command('run build')
        return
    
    # Determine build directory
    if args.debug:
        build_dir = os.path.join(PROJECT_ROOT, 'cmake-build-emscripten-debug')
    elif args.release:
        build_dir = os.path.join(PROJECT_ROOT, 'cmake-build-emscripten-release')
    elif args.build_dir:
        build_dir = args.build_dir
    else:
        parser.error('Please specify either --debug, --release, or a custom build directory')
    
    # Check if build directory exists
    if not os.path.exists(build_dir):
        print(f"Error: Build directory {build_dir} does not exist")
        return
    
    # Define public directory
    public_dir = os.path.join(PROJECT_ROOT, 'src', 'client', 'kiwi_machine_wasm', 'kiwi-machine', 'public')
    
    # Create public directory if it doesn't exist
    if not os.path.exists(public_dir):
        print(f"Creating directory: {public_dir}")
        os.makedirs(public_dir, exist_ok=True)
    
    # Copy frontend files
    html_src = os.path.join(build_dir, 'src', 'client', 'kiwi_machine', 'kiwi_machine.html')
    js_src = os.path.join(build_dir, 'src', 'client', 'kiwi_machine', 'kiwi_machine.js')
    wasm_src = os.path.join(build_dir, 'src', 'client', 'kiwi_machine', 'kiwi_machine.wasm')
    
    # Check if source files exist
    if not os.path.exists(html_src):
        print(f"Error: HTML file not found at {html_src}")
        return
    if not os.path.exists(js_src):
        print(f"Error: JS file not found at {js_src}")
        return
    if not os.path.exists(wasm_src):
        print(f"Error: WASM file not found at {wasm_src}")
        return
    
    CopyFile(html_src, public_dir)
    CopyFile(js_src, public_dir)
    CopyFile(wasm_src, public_dir)
    print('Done copying frontend files')

    # Copy roms if provided
    if args.rom_dir:
        roms_dir = os.path.join(public_dir, 'roms')
        if os.path.exists(roms_dir):
            shutil.rmtree(roms_dir)
        ExtractAllTo(args.rom_dir, roms_dir)

        # Generate database json
        GenerateDatabase(roms_dir)


if __name__ == "__main__":
    main()
