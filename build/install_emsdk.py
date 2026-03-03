#!/usr/bin/env python3

import os
import subprocess
import sys

def run_command(cmd, cwd=None):
    """Run a command and return the output"""
    print(f"Running: {cmd}")
    result = subprocess.run(cmd, shell=True, cwd=cwd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"Error running command: {cmd}")
        print(f"Stderr: {result.stderr}")
        sys.exit(1)
    return result.stdout

def setup_emscripten():
    """Install and setup Emscripten environment"""
    # Get the current directory
    current_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    emsdk_dir = os.path.join(current_dir, "emsdk")
    
    # Remove existing emsdk directory if it exists
    if os.path.exists(emsdk_dir):
        print(f"Removing existing emsdk directory: {emsdk_dir}")
        import shutil
        shutil.rmtree(emsdk_dir)
    
    # Clone emsdk repository
    print("Cloning emsdk repository...")
    run_command("git clone https://github.com/emscripten-core/emsdk.git", cwd=current_dir)
    
    # Install latest emsdk
    print("Installing latest emsdk...")
    run_command("./emsdk install latest", cwd=emsdk_dir)
    
    # Activate emsdk
    print("Activating emsdk...")
    run_command("./emsdk activate latest", cwd=emsdk_dir)
    
    # Get Emscripten.cmake path
    emscripten_cmake_path = os.path.join(emsdk_dir, "upstream", "emscripten", "cmake", "Modules", "Platform", "Emscripten.cmake")
    
    # Verify Emscripten.cmake exists
    if not os.path.exists(emscripten_cmake_path):
        print(f"Error: Emscripten.cmake not found at {emscripten_cmake_path}")
        sys.exit(1)
    
    # Activate emsdk environment and set environment variables
    print("Setting emsdk environment variables...")
    emsdk_env_script = os.path.join(emsdk_dir, "emsdk_env.sh")
    # Run the script and capture the environment variables
    result = subprocess.run(f"source {emsdk_env_script} && env", shell=True, cwd=emsdk_dir, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"Error running emsdk_env.sh: {result.stderr}")
        sys.exit(1)
    
    # Parse the environment variables and set them in the current process
    for line in result.stdout.strip().split('\n'):
        if '=' in line:
            key, value = line.split('=', 1)
            os.environ[key] = value
    
    # Modify zlib CMakeLists.txt to build as static library
    zlib_cmake_path = os.path.join(current_dir, "src", "kiwi", "third_party", "zlib-1.3.2", "CMakeLists.txt")
    if os.path.exists(zlib_cmake_path):
        print("Modifying zlib CMakeLists.txt to build as static library...")
        with open(zlib_cmake_path, 'r') as f:
            content = f.read()
        # Replace SHARED with STATIC in add_library command
        modified_content = content.replace("add_library(zlib SHARED", "add_library(zlib STATIC")
        with open(zlib_cmake_path, 'w') as f:
            f.write(modified_content)
    
    return emscripten_cmake_path

def build_wasm_config(config, emscripten_cmake_path):
    """Build WebAssembly project with specified configuration"""
    current_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    
    # Create build directory
    cmake_build_dir = os.path.join(current_dir, f"cmake-build-emscripten-{config}")
    
    # Remove existing directory to ensure clean build
    if os.path.exists(cmake_build_dir):
        print(f"Removing existing {cmake_build_dir} directory")
        import shutil
        shutil.rmtree(cmake_build_dir)
    
    print(f"Creating {cmake_build_dir} directory")
    os.makedirs(cmake_build_dir)
    
    # Run cmake
    print(f"\nRunning cmake for {config} configuration...")
    cmake_cmd = f"cmake -G 'Unix Makefiles' -DCMAKE_TOOLCHAIN_FILE={emscripten_cmake_path} -DCMAKE_BUILD_TYPE={config.capitalize()} -DBUILD_SHARED_LIBS=OFF -DZLIB_BUILD_SHARED=OFF .."
    run_command(cmake_cmd, cwd=cmake_build_dir)
    
    # Build the project
    print(f"\nBuilding {config} configuration...")
    run_command("make", cwd=cmake_build_dir)
    
    print(f"\nWebAssembly {config} build completed successfully!")
    print(f"Build directory: {cmake_build_dir}")

def main():
    """Main function to install Emscripten and build WebAssembly projects"""
    # Setup Emscripten environment
    emscripten_cmake_path = setup_emscripten()
    
    # Build Debug configuration
    build_wasm_config("debug", emscripten_cmake_path)
    
    # Build Release configuration
    build_wasm_config("release", emscripten_cmake_path)
    
    # Print completion information
    print("\nAll WebAssembly builds completed successfully!")
    print(f"Emscripten.cmake path: {emscripten_cmake_path}")


if __name__ == "__main__":
    main()
