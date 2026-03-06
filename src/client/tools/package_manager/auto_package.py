#!/usr/bin/env python3
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

import os
import sys
import subprocess
import shutil
from pathlib import Path

def main():
    # Check if we have package_manager path and kiwi_machine output directory as arguments
    if len(sys.argv) < 3:
        print("Usage: auto_package.py <package_manager_executable_path> <kiwi_machine_output_dir>")
        return 1
    
    package_manager_exe = Path(sys.argv[1])
    if not package_manager_exe.exists():
        print(f"package_manager executable not found at: {package_manager_exe}")
        return 1
    
    kiwi_machine_output_dir = Path(sys.argv[2])
    if not kiwi_machine_output_dir.exists():
        print(f"kiwi_machine output directory not found at: {kiwi_machine_output_dir}")
        return 1
    
    # Get the directory of this script
    script_dir = Path(__file__).parent
    
    # Determine the workspace path
    workspace_path = script_dir / "../../../third_party/Kiwi-Machine-Workspace"
    workspace_path = workspace_path.resolve()
    
    if not workspace_path.exists():
        print("Workspace not found. Please use build.py to fetch the workspace.")
        return 1
    
    # Check if zipped folder exists
    zipped_path = workspace_path / "zipped" / "nes"
    
    if not zipped_path.exists():
        print("Workspace is incomplete. zipped folder not found.")
        return 1
    
    # Create output directory if it doesn't exist, and clean it if it does
    output_path = workspace_path / "out"
    if output_path.exists():
        shutil.rmtree(output_path)
    output_path.mkdir(parents=True, exist_ok=True)
    
    # Run package_manager with the command line arguments
    print(f"Running package_manager...")
    print(f"  Zipped path: {zipped_path}")
    print(f"  Output path: {output_path}")
    
    cmd = [
        str(package_manager_exe),
        f"--zipped_path={zipped_path}",
        f"--output_path={output_path}"
    ]
    
    try:
        result = subprocess.run(cmd, check=True, capture_output=True, text=True)
        
        # Copy generated files to kiwi_machine output directory
        print(f"\nCopying files to kiwi_machine output directory: {kiwi_machine_output_dir}")
        
        # Find all .pak files in output directory
        pak_files = list(output_path.glob("*.pak"))
        if pak_files:
            for pak_file in pak_files:
                dest_file = kiwi_machine_output_dir / pak_file.name
                shutil.copy2(pak_file, dest_file)
                print(f"  Copied: {pak_file.name}")
            print(f"Successfully copied {len(pak_files)} file(s) to kiwi_machine output directory.")
        else:
            print("No .pak files found to copy.")
        
        return result.returncode
    except subprocess.CalledProcessError as e:
        print(f"Error: package_manager failed with exit code {e.returncode}")
        return e.returncode

if __name__ == "__main__":
    sys.exit(main())
