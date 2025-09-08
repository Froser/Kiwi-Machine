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
import json
import shutil
import sys
import zipfile
from pathlib import Path

# Usage:
# Copies wasm files to 'public' folder.
# update.py full-kiwi-build-path-to-the-build-dir full-path-to-nes-dir
# Example:
# python3 update.py /Users/user/Kiwi-Machine/cmake-build-emscripten_debug /Users/user/nes

build_dir = sys.argv[1]
rom_id = 0


def CopyFile(src, dest):
    print("Copying ", src, " to ", dest)
    shutil.copy(src, dest)


def ExtractAllTo(src, dest):
    for f in sorted(Path(src).iterdir()):
        ExtractTo(f, dest + '/' + f.stem)


def ExtractTo(src, dest):
    if src.is_file() and src.suffix == '.zip':
        print('Extracting ', src.absolute(), ' to ', dest)
        with zipfile.ZipFile(src.absolute(), 'r', zipfile.ZIP_DEFLATED) as zip_file:
            zip_file.extractall(dest)
    elif src.is_dir():
        print('Entering ', src)
        ExtractAllTo(src, dest)


def GenerateDatabase(dir):
    db_file = Path(dir + '/db.json')
    print('Generating database file: ', db_file)
    db = []
    AddManifestFromDir(db, dir, dir)

    print(db_file, 'Generated.')
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
            print('Found manifest', file_or_dir.absolute())

            # Read manifest.json
            with open(file_or_dir.absolute(), 'r', encoding='utf-8') as f:
                manifest_data = json.load(f)
            manifest_data_titles = manifest_data['titles']

        elif file_or_dir.suffix == '.nes':
            if not (file_or_dir.parent.joinpath('manifest.json')).exists():
                # If there's no manifest, it may be a hacked ROM, so we have to construct its manifest.json to memory.
                print("No manifest file (", file_or_dir.parent.joinpath('manifest.json'), ") for ROM, create one: ",
                      file_or_dir)
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


def main():
    # Copy frontend files
    CopyFile(build_dir + '/src/client/kiwi_machine/kiwi_machine.html', './kiwi-machine/public')
    CopyFile(build_dir + '/src/client/kiwi_machine/kiwi_machine.js', './kiwi-machine/public')
    CopyFile(build_dir + '/src/client/kiwi_machine/kiwi_machine.wasm', './kiwi-machine/public')
    print('Done')

    # Copy roms
    if len(sys.argv) > 2:
        shutil.rmtree('./kiwi-machine/public/roms')
        ExtractAllTo(sys.argv[2], './kiwi-machine/public/roms')

        # Generate database json
        GenerateDatabase('./kiwi-machine/public/roms')


if __name__ == "__main__":
    main()
