#!/usr/bin/env python3

import os
import sys
import subprocess
import platform

# Project root directory
PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))


def run_command(cmd, cwd=None):
    """Run a command and return the output"""
    print(f"Running: {cmd}")
    result = subprocess.run(cmd, shell=True, cwd=cwd, capture_output=True, text=True)
    print(f"{result.stdout}")
    if result.returncode != 0:
        print(f"Error running command: {cmd}")
        print(f"Stderr: {result.stderr}")
        return False
    return True


def is_apple_silicon():
    """Check if the machine has Apple Silicon chip"""
    return platform.machine() == 'arm64' and platform.system() == 'Darwin'


def get_cmake_generator():
    """Get appropriate CMake generator based on platform"""
    system = platform.system()
    
    if system == 'Windows':
        # On Windows, use Visual Studio generator
        return 'Visual Studio 17 2022'
    elif system == 'Darwin':
        # On macOS, use Xcode if installed, else Unix Makefiles
        if check_xcode_installed():
            return 'Xcode'
        else:
            return 'Unix Makefiles'
    else:
        # On other platforms (Linux), use Unix Makefiles
        return 'Unix Makefiles'


def build_pc(build_project=False):
    """Build PC platform projects"""
    print("\n=== Building PC platform projects ===")
    
    success = True
    
    # Get appropriate CMake generator
    generator = get_cmake_generator()
    print(f"Using CMake generator: {generator}")
    
    # Build Debug configuration
    debug_dir = os.path.join(PROJECT_ROOT, "cmake-build-debug")
    print(f"Building Debug configuration in {debug_dir}")
    if not os.path.exists(debug_dir):
        os.makedirs(debug_dir)
    
    # Construct cmake command
    cmake_cmd = f"cmake -G \"{generator}\" -DCMAKE_BUILD_TYPE=Debug .."
    if not run_command(cmake_cmd, cwd=debug_dir):
        success = False
    
    # Build the Debug project if requested
    if build_project and success:
        print(f"\nBuilding Debug project (kiwi_machine)...")
        if generator == 'Visual Studio 17 2022':
            build_cmd = 'cmake --build . --config Debug --target kiwi_machine'
        else:
            build_cmd = 'cmake --build . --target kiwi_machine'
        if not run_command(build_cmd, cwd=debug_dir):
            success = False
    
    # Build Release configuration
    release_dir = os.path.join(PROJECT_ROOT, "cmake-build-release")
    print(f"Building Release configuration in {release_dir}")
    if not os.path.exists(release_dir):
        os.makedirs(release_dir)
    
    # Construct cmake command
    cmake_cmd = f"cmake -G \"{generator}\" -DCMAKE_BUILD_TYPE=Release .."
    if success and not run_command(cmake_cmd, cwd=release_dir):
        success = False
    
    # Build the Release project if requested
    if build_project and success:
        print(f"\nBuilding Release project (kiwi_machine)...")
        if generator == 'Visual Studio 17 2022':
            build_cmd = 'cmake --build . --config Release --target kiwi_machine'
        else:
            build_cmd = 'cmake --build . --target kiwi_machine'
        if not run_command(build_cmd, cwd=release_dir):
            success = False
    
    # For Apple Silicon machines, also build Intel architecture projects
    if is_apple_silicon() and success:
        print("\n=== Building Intel architecture projects for Apple Silicon ===")
        
        # Build Intel Debug configuration
        intel_debug_dir = os.path.join(PROJECT_ROOT, "cmake-build-intel-debug")
        print(f"Building Intel Debug configuration in {intel_debug_dir}")
        if not os.path.exists(intel_debug_dir):
            os.makedirs(intel_debug_dir)
        
        # Construct cmake command
        cmake_cmd = f"cmake -G \"{generator}\" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=x86_64 .."
        if not run_command(cmake_cmd, cwd=intel_debug_dir):
            success = False
        
        # Build the Intel Debug project if requested
        if build_project and success:
            print(f"\nBuilding Intel Debug project (kiwi_machine)...")
            if generator == 'Visual Studio 17 2022':
                build_cmd = 'cmake --build . --config Debug --target kiwi_machine'
            else:
                build_cmd = 'cmake --build . --target kiwi_machine'
            if not run_command(build_cmd, cwd=intel_debug_dir):
                success = False
        
        # Build Intel Release configuration
        intel_release_dir = os.path.join(PROJECT_ROOT, "cmake-build-intel-release")
        print(f"Building Intel Release configuration in {intel_release_dir}")
        if not os.path.exists(intel_release_dir):
            os.makedirs(intel_release_dir)
        
        # Construct cmake command
        cmake_cmd = f"cmake -G \"{generator}\" -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 .."
        if success and not run_command(cmake_cmd, cwd=intel_release_dir):
            success = False
        
        # Build the Intel Release project if requested
        if build_project and success:
            print(f"\nBuilding Intel Release project (kiwi_machine)...")
            if generator == 'Visual Studio 17 2022':
                build_cmd = 'cmake --build . --config Release --target kiwi_machine'
            else:
                build_cmd = 'cmake --build . --target kiwi_machine'
            if not run_command(build_cmd, cwd=intel_release_dir):
                success = False
    
    return success

def check_xcode_installed():
    """Check if XCode is installed"""
    try:
        # Run xcodebuild to check if it's installed
        result = subprocess.run(['xcodebuild', '-version'], capture_output=True, text=True, check=True)
        return True
    except Exception as e:
        print(f"Error checking XCode installation: {e}")
        return False

def build_mobile(build_project=False):
    """Build mobile platform projects"""
    print("\n=== Building Mobile platform projects ===")
    
    # Check if XCode is installed
    if not check_xcode_installed():
        print("Warning: XCode is not installed")
        print("iOS build will be skipped")
        return False
    
    success = True
    
    # Use iOS toolchain
    ios_toolchain = os.path.join(PROJECT_ROOT, "build", "cmake", "ios.toolchain.cmake")
    
    # Always build iOS Debug configuration for simulator (SIMULATOR64)
    ios_debug_dir = os.path.join(PROJECT_ROOT, "cmake-build-ios-debug")
    print(f"Building iOS Debug configuration (Simulator) in {ios_debug_dir}")
    if not os.path.exists(ios_debug_dir):
        os.makedirs(ios_debug_dir)
    if not run_command(f"cmake -G \"Xcode\" -DCMAKE_TOOLCHAIN_FILE={ios_toolchain} -DCMAKE_BUILD_TYPE=Debug -DPLATFORM=SIMULATOR64 -DENABLE_ARC=OFF ..", cwd=ios_debug_dir):
        success = False
    
    # Build the Debug project if requested
    if build_project and success:
        print(f"\nBuilding iOS Debug project (kiwi_machine)...")
        build_cmd = 'xcodebuild -configuration Debug -scheme kiwi_machine -sdk iphonesimulator -destination "generic/platform=iOS Simulator" CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO CODE_SIGN_IDENTITY="" DEVELOPMENT_TEAM=""'
        if not run_command(build_cmd, cwd=ios_debug_dir):
            success = False
    
    # Always build iOS Release configuration for simulator (SIMULATOR64)
    ios_release_dir = os.path.join(PROJECT_ROOT, "cmake-build-ios-release")
    print(f"Building iOS Release configuration (Simulator) in {ios_release_dir}")
    if not os.path.exists(ios_release_dir):
        os.makedirs(ios_release_dir)
    if success and not run_command(f"cmake -G \"Xcode\" -DCMAKE_TOOLCHAIN_FILE={ios_toolchain} -DCMAKE_BUILD_TYPE=Release -DPLATFORM=SIMULATOR64 -DENABLE_ARC=OFF ..", cwd=ios_release_dir):
        success = False
    
    # Build the Release project if requested
    if build_project and success:
        print(f"\nBuilding iOS Release project (kiwi_machine)...")
        build_cmd = 'xcodebuild -configuration Release -scheme kiwi_machine -sdk iphonesimulator -destination "generic/platform=iOS Simulator" CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO CODE_SIGN_IDENTITY="" DEVELOPMENT_TEAM=""'
        if not run_command(build_cmd, cwd=ios_release_dir):
            success = False
    
    return success

def install_emsdk():
    """Install Emscripten environment"""
    print("\n=== Installing Emscripten ===")
    
    # Call install_emsdk.py script
    emsdk_script = os.path.join(PROJECT_ROOT, "build", "install_emsdk.py")
    return run_command(f"python3 {emsdk_script}")

def find_python310():
    """Find Python 3.10 or higher executable"""
    # Check common Python 3.10 paths
    python_candidates = [
        "/opt/homebrew/bin/python3.10",
        "/usr/local/bin/python3.10",
        "/usr/bin/python3.10",
        "python3.10",
        "python3"
    ]
    
    for python_exec in python_candidates:
        try:
            result = subprocess.run([python_exec, "--version"], capture_output=True, text=True)
            if result.returncode == 0:
                version_str = result.stdout.strip().split()[-1]
                major, minor = map(int, version_str.split(".")[:2])
                if major >= 3 and minor >= 10:
                    return python_exec
        except (subprocess.CalledProcessError, FileNotFoundError):
            continue
    
    return None

def build_wasm_config(config):
    """Build WebAssembly project with specified configuration"""
    current_dir = PROJECT_ROOT
    
    # Create build directory
    cmake_build_dir = os.path.join(current_dir, f"cmake-build-emscripten-{config}")
    
    # Remove existing directory to ensure clean build
    if os.path.exists(cmake_build_dir):
        print(f"Removing existing {cmake_build_dir} directory")
        import shutil
        shutil.rmtree(cmake_build_dir)
    
    print(f"Creating {cmake_build_dir} directory")
    os.makedirs(cmake_build_dir)
    
    # Get Emscripten.cmake path
    emsdk_dir = os.path.join(current_dir, "emsdk")
    emscripten_cmake_path = os.path.join(emsdk_dir, "upstream", "emscripten", "cmake", "Modules", "Platform", "Emscripten.cmake")
    
    # Verify Emscripten.cmake exists
    if not os.path.exists(emscripten_cmake_path):
        print(f"Error: Emscripten.cmake not found at {emscripten_cmake_path}")
        print("Please run install_emsdk.py first")
        sys.exit(1)
    
    # Find Python 3.10 or higher
    python_exec = find_python310()
    if not python_exec:
        print("Error: Python 3.10 or higher not found")
        print("Emscripten requires Python 3.10 or above")
        sys.exit(1)
    print(f"Using Python: {python_exec}")
    
    # Use Ninja generator for WebAssembly builds
    generator = 'Ninja'
    print(f"Using CMake generator: {generator}")
    
    # Set environment variables to use the correct Python
    env = os.environ.copy()
    # Add Python directory to PATH
    python_dir = os.path.dirname(python_exec)
    env["PATH"] = python_dir + os.pathsep + env["PATH"]
    # Set PYTHON environment variable
    env["PYTHON"] = python_exec
    # Set EMSDK_PYTHON environment variable (used by emcc shell script)
    env["EMSDK_PYTHON"] = python_exec
    
    # Run cmake with the correct environment
    print(f"\nRunning cmake for {config} configuration...")
    # Add KIWI_WASM_RELEASE=ON for Release configuration
    wasm_release_flag = "-DKIWI_WASM_RELEASE=ON" if config == "release" else ""
    cmake_cmd = f"cmake -G \"{generator}\" -DCMAKE_TOOLCHAIN_FILE={emscripten_cmake_path} -DCMAKE_BUILD_TYPE={config.capitalize()} {wasm_release_flag} -DBUILD_SHARED_LIBS=OFF -DZLIB_BUILD_SHARED=OFF -DGFLAGS_INTTYPES_FORMAT=C99 -DINTTYPES_FORMAT=C99 -DSDL2MIXER_CMD=OFF -DSDL_TEST=OFF -DMINIZIP_BUILD_SHARED=OFF -DMINIZIP_BUILD_STATIC=ON .."
    
    # Run command with modified environment
    print(f"Running: {cmake_cmd}")
    result = subprocess.run(cmake_cmd, shell=True, cwd=cmake_build_dir, capture_output=True, text=True, env=env)
    print(f"{result.stdout}")
    if result.returncode != 0:
        print(f"Error running command: {cmake_cmd}")
        print(f"Stderr: {result.stderr}")
        return False
    
    # Build the project with the correct environment
    print(f"\nBuilding {config} configuration...")
    # Use appropriate build command based on generator
    if generator == 'Xcode':
        build_cmd = 'xcodebuild -configuration Debug' if config == 'debug' else 'xcodebuild -configuration Release'
    elif generator == 'Visual Studio 17 2022':
        build_cmd = 'cmake --build . --config Debug' if config == 'debug' else 'cmake --build . --config Release'
    else:
        build_cmd = 'ninja'
    print(f"Running: {build_cmd}")
    result = subprocess.run(build_cmd, shell=True, cwd=cmake_build_dir, capture_output=True, text=True, env=env)
    print(f"{result.stdout}")
    if result.returncode != 0:
        print(f"Error running command: {build_cmd}")
        print(f"Stderr: {result.stderr}")
        return False
    
    print(f"\nWebAssembly {config} build completed successfully!")
    print(f"Build directory: {cmake_build_dir}")
    return True

def build_wasm():
    """Build WebAssembly platform projects"""
    print("\n=== Building WebAssembly platform projects ===")
    
    # Install Emscripten if not already installed
    emsdk_dir = os.path.join(PROJECT_ROOT, "emsdk")
    if not os.path.exists(emsdk_dir):
        if not install_emsdk():
            print("Emscripten installation failed!")
            return False
    
    # Build Debug configuration
    debug_success = build_wasm_config("debug")
    
    # Build Release configuration
    release_success = build_wasm_config("release")
    
    # Print completion information
    if debug_success and release_success:
        print("\nAll WebAssembly builds completed successfully!")
        return True
    else:
        print("\nSome WebAssembly builds failed!")
        return False

def sync_workspace():
    """Sync workspace dependencies"""
    print("\n=== Syncing workspace dependencies ===")
    
    # Call existing workspace-sync.py script
    workspace_script = os.path.join(PROJECT_ROOT, "build", "workspace-sync.py")
    return run_command(f"python3 {workspace_script}")


def print_help():
    """Print help information"""
    print("Usage: build.py [options] [--clion] [--cleanup-clion] [--cleanup] [--build]")
    print("Options:")
    print("  pc          Build PC platform projects (Debug and Release)")
    print("  ios         Build iOS platform projects (Debug and Release)")
    print("  wasm        Build WebAssembly platform projects (Debug and Release)")
    print("  workspace   Sync workspace dependencies")
    print("  all         Build all platforms and sync dependencies")
    print("  help        Print this help message")
    print("  --clion     Generate CLion configuration files instead of building")
    print("  --cleanup-clion     Remove CLion configuration directory")
    print("  --cleanup   Remove all binary output directories")
    print("  --build     Actually build the projects after CMake configuration")
    print("")
    print("For Apple Silicon machines, additional Intel architecture builds are created.")

def generate_clion_config(platform="all"):
    """Generate CLion configuration files for specified platform"""
    print(f"\n=== Generating CLion configuration files for {platform} platform ===")
    
    # Get appropriate generator for CLion
    generator = "Ninja"
    
    # Base presets
    presets = {
        "version": 6,
        "cmakeMinimumRequired": {
            "major": 3,
            "minor": 21,
            "patch": 0
        },
        "configurePresets": []
    }
    
    # PC configurations
    if platform == "all" or platform == "pc":
        # Debug preset
        presets["configurePresets"].append({
            "name": "debug",
            "displayName": "Debug",
            "description": "Debug build for PC",
            "generator": generator,
            "binaryDir": "${sourceDir}/cmake-build-debug",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Debug"
            }
        })
        
        # Release preset
        presets["configurePresets"].append({
            "name": "release",
            "displayName": "Release",
            "description": "Release build for PC",
            "generator": generator,
            "binaryDir": "${sourceDir}/cmake-build-release",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Release"
            }
        })
        
        # For Apple Silicon machines, also generate Intel architecture configurations
        if is_apple_silicon():
            # Intel Debug preset
            presets["configurePresets"].append({
                "name": "intel-debug",
                "displayName": "Intel Debug",
                "description": "Debug build for Intel architecture",
                "generator": generator,
                "binaryDir": "${sourceDir}/cmake-build-intel-debug",
                "cacheVariables": {
                    "CMAKE_BUILD_TYPE": "Debug",
                    "CMAKE_OSX_ARCHITECTURES": "x86_64"
                }
            })
            
            # Intel Release preset
            presets["configurePresets"].append({
                "name": "intel-release",
                "displayName": "Intel Release",
                "description": "Release build for Intel architecture",
                "generator": generator,
                "binaryDir": "${sourceDir}/cmake-build-intel-release",
                "cacheVariables": {
                    "CMAKE_BUILD_TYPE": "Release",
                    "CMAKE_OSX_ARCHITECTURES": "x86_64"
                }
            })
    
    # WebAssembly configurations
    if (platform == "all" or platform == "wasm"):
        # Use Ninja generator for WASM builds
        wasm_generator = "Ninja"
        emsdk_dir = os.path.join(PROJECT_ROOT, "emsdk")
        
        # Check if Emscripten is installed
        if not os.path.exists(emsdk_dir):
            print("Emscripten not found, installing...")
            install_success = install_emsdk()
            if not install_success:
                print("Emscripten installation failed, skipping WASM configurations")
                return
        
        # Check if Emscripten is properly installed
        emscripten_cmake_path = os.path.join(PROJECT_ROOT, "emsdk", "upstream", "emscripten", "cmake", "Modules", "Platform", "Emscripten.cmake")
        if not os.path.exists(emscripten_cmake_path):
            print("Emscripten directory exists but is not properly installed, skipping WASM configurations")
            return
        
        # Common cache variables for WASM builds
        wasm_cache_vars = {
            "BUILD_SHARED_LIBS": "OFF",
            "ZLIB_BUILD_SHARED": "OFF",
            "GFLAGS_INTTYPES_FORMAT": "C99",
            "INTTYPES_FORMAT": "C99",
            "SDL2MIXER_CMD": "OFF",
            "SDL_TEST": "OFF",
            "MINIZIP_BUILD_SHARED": "OFF",
            "MINIZIP_BUILD_STATIC": "ON",
        }
        
        # WASM Debug preset
        debug_cache_vars = wasm_cache_vars.copy()
        debug_cache_vars["CMAKE_BUILD_TYPE"] = "Debug"
        
        presets["configurePresets"].append({
            "name": "wasm-debug",
            "displayName": "WASM Debug",
            "description": "Debug build for WebAssembly",
            "generator": wasm_generator,
            "binaryDir": "${sourceDir}/cmake-build-emscripten-debug",
            "cacheVariables": debug_cache_vars,
            "toolchainFile": emscripten_cmake_path
        })
        
        # WASM Release preset
        release_cache_vars = wasm_cache_vars.copy()
        release_cache_vars["CMAKE_BUILD_TYPE"] = "Release"
        release_cache_vars["KIWI_WASM_RELEASE"] = "ON"
        
        presets["configurePresets"].append({
            "name": "wasm-release",
            "displayName": "WASM Release",
            "description": "Release build for WebAssembly",
            "generator": wasm_generator,
            "binaryDir": "${sourceDir}/cmake-build-emscripten-release",
            "cacheVariables": release_cache_vars,
            "toolchainFile": emscripten_cmake_path
        })
    
    # iOS configurations (only on macOS)
    if (platform == "all" or platform == "ios") and sys.platform == 'darwin':
        ios_toolchain = os.path.join(PROJECT_ROOT, "build", "cmake", "ios.toolchain.cmake")
        if os.path.exists(ios_toolchain):
            # iOS Debug preset
            presets["configurePresets"].append({
                "name": "ios-debug",
                "displayName": "iOS Debug",
                "description": "Debug build for iOS",
                "generator": "Xcode",
                "binaryDir": "${sourceDir}/cmake-build-ios-debug",
                "cacheVariables": {
                    "CMAKE_BUILD_TYPE": "Debug",
                    "PLATFORM": "OS64",
                    "ENABLE_ARC": "OFF"
                },
                "toolchainFile": ios_toolchain
            })
            
            # iOS Release preset
            presets["configurePresets"].append({
                "name": "ios-release",
                "displayName": "iOS Release",
                "description": "Release build for iOS",
                "generator": "Xcode",
                "binaryDir": "${sourceDir}/cmake-build-ios-release",
                "cacheVariables": {
                    "CMAKE_BUILD_TYPE": "Release",
                    "PLATFORM": "OS64",
                    "ENABLE_ARC": "OFF"
                },
                "toolchainFile": ios_toolchain
            })
        else:
            print("iOS toolchain not found, skipping iOS configurations")

    # Create CMakePresets.json file
    cmake_presets_path = os.path.join(PROJECT_ROOT, "CMakePresets.json")
    
    # Remove old CMakePresets.json if it exists
    if os.path.exists(cmake_presets_path):
        print(f"Removing old CMakePresets.json at {cmake_presets_path}")
        os.remove(cmake_presets_path)
    
    print("Creating CMakePresets.json for CLion with configurations")
    
    import json
    with open(cmake_presets_path, 'w') as f:
        json.dump(presets, f, indent=2)
    
    # Track which configurations were actually generated
    generated_configs = {
        "pc": False,
        "intel": False,
        "wasm": False,
        "ios": False
    }
    
    # Check which configurations were generated
    for preset in presets["configurePresets"]:
        if preset["name"] in ["debug", "release"]:
            generated_configs["pc"] = True
        elif preset["name"] in ["intel-debug", "intel-release"]:
            generated_configs["intel"] = True
        elif preset["name"] in ["wasm-debug", "wasm-release"]:
            generated_configs["wasm"] = True
        elif preset["name"] in ["ios-debug", "ios-release"]:
            generated_configs["ios"] = True
    
    print("\nCLion configuration files generated successfully!")
    print("Open the project in CLion to build it")
    print("Available configurations:")
    if generated_configs["pc"]:
        print("  - Debug/Release: PC platform")
    if generated_configs["intel"]:
        print("  - Intel Debug/Release: Intel architecture")
    if generated_configs["wasm"]:
        print("  - WASM Debug/Release: WebAssembly platform")
    if generated_configs["ios"]:
        print("  - iOS Debug/Release: iOS platform")

def cleanup_clion():
    """Remove CLion configuration directory"""
    print("\n=== Cleaning up CLion configuration ===")
    
    # Check if .idea directory exists
    idea_dir = os.path.join(PROJECT_ROOT, ".idea")
    if os.path.exists(idea_dir):
        print(f"Removing .idea directory at {idea_dir}")
        import shutil
        shutil.rmtree(idea_dir)
        print("CLion configuration directory removed successfully!")
    else:
        print(".idea directory not found, nothing to clean up.")


def cleanup_build():
    """Remove all binary output directories"""
    print("\n=== Cleaning up binary output directories ===")
    
    # Get all directories starting with cmake-build-
    import glob
    build_dirs = glob.glob(os.path.join(PROJECT_ROOT, "cmake-build-*"))
    
    if build_dirs:
        for build_dir in build_dirs:
            print(f"Removing directory: {build_dir}")
            import shutil
            shutil.rmtree(build_dir)
        print(f"Successfully removed {len(build_dirs)} binary output directories!")
    else:
        print("No binary output directories found, nothing to clean up.")


def main():
    """Main function"""
    # Check for --clion flag
    clion_mode = "--clion" in sys.argv
    
    # Check for --cleanup-clion flag
    cleanup_clion_mode = "--cleanup-clion" in sys.argv
    
    # Check for --cleanup flag
    cleanup_build_mode = "--cleanup" in sys.argv
    
    # Check for --build flag
    build_mode = "--build" in sys.argv
    
    # Remove flags from arguments if present
    if clion_mode:
        sys.argv.remove("--clion")
    if cleanup_clion_mode:
        sys.argv.remove("--cleanup-clion")
    if cleanup_build_mode:
        sys.argv.remove("--cleanup")
    if build_mode:
        sys.argv.remove("--build")
    
    # Handle cleanup modes
    if cleanup_clion_mode:
        cleanup_clion()
        return
    if cleanup_build_mode:
        cleanup_build()
        cleanup_clion()
        return
    
    overall_success = True
    
    # Parse command line arguments
    if len(sys.argv) == 1:
        if clion_mode:
            generate_clion_config("all")
        else:
            # No arguments, execute all steps
            if not sync_workspace():
                overall_success = False
            if not build_pc(build_mode):
                overall_success = False
            if not build_wasm():
                overall_success = False
            # Build iOS only on macOS
            if platform.system() == 'Darwin':
                if not build_mobile(build_mode):
                    overall_success = False
    else:
        # With arguments, execute corresponding steps
        for arg in sys.argv[1:]:
            if arg == "pc":
                if clion_mode:
                    generate_clion_config("pc")
                else:
                    if not build_pc(build_mode):
                        overall_success = False
            elif arg == "ios":
                if clion_mode:
                    generate_clion_config("ios")
                else:
                    # Build iOS only on macOS
                    if platform.system() == 'Darwin':
                        if not build_mobile(build_mode):
                            overall_success = False
                    else:
                        print("iOS build is only supported on macOS")
                        overall_success = False
            elif arg == "wasm":
                if clion_mode:
                    generate_clion_config("wasm")
                else:
                    if not build_wasm():
                        overall_success = False
            elif arg == "workspace":
                if not sync_workspace():
                    overall_success = False
            elif arg == "all":
                if clion_mode:
                    generate_clion_config("all")
                else:
                    if not sync_workspace():
                        overall_success = False
                    if not build_pc(build_mode):
                        overall_success = False
                    if not build_wasm():
                        overall_success = False
                    # Build iOS only on macOS
                    if platform.system() == 'Darwin':
                        if not build_mobile(build_mode):
                            overall_success = False
            elif arg == "help":
                print_help()
            else:
                print(f"Unknown option: {arg}")
                print_help()
                overall_success = False
    
    if not overall_success:
        print("\nSome builds failed!")
        sys.exit(1)


if __name__ == "__main__":
    main()