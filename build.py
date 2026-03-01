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


def build_pc():
    """Build PC platform projects"""
    print("\n=== Building PC platform projects ===")
    
    # Get appropriate CMake generator
    generator = get_cmake_generator()
    print(f"Using CMake generator: {generator}")
    
    # Build Debug configuration
    debug_dir = os.path.join(PROJECT_ROOT, "cmake-build-debug")
    print(f"Building Debug configuration in {debug_dir}")
    if not os.path.exists(debug_dir):
        os.makedirs(debug_dir)
    
    # Construct cmake command
    cmake_cmd = f"cmake -G '{generator}' -DCMAKE_BUILD_TYPE=Debug .."
    run_command(cmake_cmd, cwd=debug_dir)
    
    # Build Release configuration
    release_dir = os.path.join(PROJECT_ROOT, "cmake-build-release")
    print(f"Building Release configuration in {release_dir}")
    if not os.path.exists(release_dir):
        os.makedirs(release_dir)
    
    # Construct cmake command
    cmake_cmd = f"cmake -G '{generator}' -DCMAKE_BUILD_TYPE=Release .."
    run_command(cmake_cmd, cwd=release_dir)
    
    # For Apple Silicon machines, also build Intel architecture projects
    if is_apple_silicon():
        print("\n=== Building Intel architecture projects for Apple Silicon ===")
        
        # Build Intel Debug configuration
        intel_debug_dir = os.path.join(PROJECT_ROOT, "cmake-build-intel-debug")
        print(f"Building Intel Debug configuration in {intel_debug_dir}")
        if not os.path.exists(intel_debug_dir):
            os.makedirs(intel_debug_dir)
        
        # Construct cmake command
        cmake_cmd = f"cmake -G '{generator}' -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=x86_64 .."
        run_command(cmake_cmd, cwd=intel_debug_dir)
        
        # Build Intel Release configuration
        intel_release_dir = os.path.join(PROJECT_ROOT, "cmake-build-intel-release")
        print(f"Building Intel Release configuration in {intel_release_dir}")
        if not os.path.exists(intel_release_dir):
            os.makedirs(intel_release_dir)
        
        # Construct cmake command
        cmake_cmd = f"cmake -G '{generator}' -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 .."
        run_command(cmake_cmd, cwd=intel_release_dir)

def check_xcode_installed():
    """Check if XCode is installed"""
    try:
        # Run xcodebuild to check if it's installed
        result = subprocess.run(['xcodebuild', '-version'], capture_output=True, text=True, check=True)
        return True
    except Exception as e:
        print(f"Error checking XCode installation: {e}")
        return False

def build_mobile():
    """Build mobile platform projects"""
    print("\n=== Building Mobile platform projects ===")
    
    # Check if XCode is installed
    if not check_xcode_installed():
        print("Warning: XCode is not installed")
        print("iOS build will be skipped")
        return
    
    # Use iOS toolchain
    ios_toolchain = os.path.join(PROJECT_ROOT, "build", "cmake", "ios.toolchain.cmake")
    
    # Always build iOS Debug configuration for real device (OS64)
    ios_debug_dir = os.path.join(PROJECT_ROOT, "cmake-build-ios-debug")
    print(f"Building iOS Debug configuration (Device) in {ios_debug_dir}")
    if not os.path.exists(ios_debug_dir):
        os.makedirs(ios_debug_dir)
    run_command(f"cmake -G 'Xcode' -DCMAKE_TOOLCHAIN_FILE={ios_toolchain} -DCMAKE_BUILD_TYPE=Debug -DPLATFORM=OS64 ..", cwd=ios_debug_dir)
    
    # Always build iOS Release configuration for real device (OS64)
    ios_release_dir = os.path.join(PROJECT_ROOT, "cmake-build-ios-release")
    print(f"Building iOS Release configuration (Device) in {ios_release_dir}")
    if not os.path.exists(ios_release_dir):
        os.makedirs(ios_release_dir)
    run_command(f"cmake -G 'Xcode' -DCMAKE_TOOLCHAIN_FILE={ios_toolchain} -DCMAKE_BUILD_TYPE=Release -DPLATFORM=OS64 ..", cwd=ios_release_dir)

def install_emsdk():
    """Install Emscripten environment"""
    print("\n=== Installing Emscripten ===")
    
    # Call install_emsdk.py script
    emsdk_script = os.path.join(PROJECT_ROOT, "build", "install_emsdk.py")
    run_command(f"python3 {emsdk_script}")

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
    
    # Get appropriate CMake generator (same as PC build)
    generator = get_cmake_generator()
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
    cmake_cmd = f"cmake -G '{generator}' -DCMAKE_TOOLCHAIN_FILE={emscripten_cmake_path} -DCMAKE_BUILD_TYPE={config.capitalize()} -DBUILD_SHARED_LIBS=OFF -DZLIB_BUILD_SHARED=OFF .."
    
    # Run command with modified environment
    print(f"Running: {cmake_cmd}")
    result = subprocess.run(cmake_cmd, shell=True, cwd=cmake_build_dir, capture_output=True, text=True, env=env)
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
        build_cmd = 'make'
    print(f"Running: {build_cmd}")
    result = subprocess.run(build_cmd, shell=True, cwd=cmake_build_dir, capture_output=True, text=True, env=env)
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
        install_emsdk()
    
    # Build Debug configuration
    debug_success = build_wasm_config("debug")
    
    # Build Release configuration
    release_success = build_wasm_config("release")
    
    # Print completion information
    if debug_success and release_success:
        print("\nAll WebAssembly builds completed successfully!")
    else:
        print("\nSome WebAssembly builds failed!")
        sys.exit(1)

def sync_workspace():
    """Sync workspace dependencies"""
    print("\n=== Syncing workspace dependencies ===")
    
    # Call existing workspace-sync.py script
    workspace_script = os.path.join(PROJECT_ROOT, "build", "workspace-sync.py")
    run_command(f"python3 {workspace_script}")


def print_help():
    """Print help information"""
    print("Usage: build.py [options] [--clion]")
    print("Options:")
    print("  pc          Build PC platform projects (Debug and Release)")
    print("  ios         Build iOS platform projects (Debug and Release)")
    print("  wasm        Build WebAssembly platform projects (Debug and Release)")
    print("  workspace   Sync workspace dependencies")
    print("  all         Build all platforms and sync dependencies")
    print("  help        Print this help message")
    print("  --clion     Generate CLion configuration files instead of building")
    print("")
    print("For Apple Silicon machines, additional Intel architecture builds are created.")

def generate_clion_config(platform="all"):
    """Generate CLion configuration files for specified platform"""
    print(f"\n=== Generating CLion configuration files for {platform} platform ===")
    
    # Check if .idea directory exists
    idea_dir = os.path.join(PROJECT_ROOT, ".idea")
    if not os.path.exists(idea_dir):
        print(f"Creating .idea directory at {idea_dir}")
        os.makedirs(idea_dir)
    
    # Get appropriate generator for CLion
    generator = "Ninja"
    
    # Base configurations
    configurations = []
    
    # PC configurations
    if platform == "all" or platform == "pc":
        configurations.append('''
      <configuration
          name="Debug"
          type="Debug"
          buildType="Debug"
          generator="{generator}"
          path="$PROJECT_DIR$/cmake-build-debug"
      />
      <configuration
          name="Release"
          type="Release"
          buildType="Release"
          generator="{generator}"
          path="$PROJECT_DIR$/cmake-build-release"
      />'''.format(generator=generator))
    
    # WebAssembly configurations
    if (platform == "all" or platform == "wasm"):
        emscripten_cmake_path = os.path.join(PROJECT_ROOT, "emsdk", "upstream", "emscripten", "cmake", "Modules", "Platform", "Emscripten.cmake")
        if os.path.exists(emscripten_cmake_path):
            configurations.append('''
      <configuration
          name="WASM Debug"
          type="Debug"
          buildType="Debug"
          generator="{generator}"
          path="$PROJECT_DIR$/cmake-build-emscripten-debug"
          toolchainFile="{emscripten_cmake_path}"
      />
      <configuration
          name="WASM Release"
          type="Release"
          buildType="Release"
          generator="{generator}"
          path="$PROJECT_DIR$/cmake-build-emscripten-release"
          toolchainFile="{emscripten_cmake_path}"
      />'''.format(generator=generator, emscripten_cmake_path=emscripten_cmake_path))
        else:
            print("Emscripten not found, skipping WASM configurations")
    
    # iOS configurations (only on macOS)
    if (platform == "all" or platform == "ios") and sys.platform == 'darwin':
        ios_toolchain = os.path.join(PROJECT_ROOT, "build", "cmake", "ios.toolchain.cmake")
        if os.path.exists(ios_toolchain):
            configurations.append('''
      <configuration
          name="iOS Debug"
          type="Debug"
          buildType="Debug"
          generator="Xcode"
          path="$PROJECT_DIR$/cmake-build-ios-debug"
          toolchainFile="{ios_toolchain}"
          variables="{{\"PLATFORM\":\"OS64\"}}"
      />
      <configuration
          name="iOS Release"
          type="Release"
          buildType="Release"
          generator="Xcode"
          path="$PROJECT_DIR$/cmake-build-ios-release"
          toolchainFile="{ios_toolchain}"
          variables="{{\"PLATFORM\":\"OS64\"}}"
      />'''.format(ios_toolchain=ios_toolchain))
        else:
            print("iOS toolchain not found, skipping iOS configurations")
    
    # Create CLion workspace configuration
    workspace_xml_path = os.path.join(idea_dir, "workspace.xml")
    print("Creating workspace.xml for CLion with configurations")
    
    # Combine all configurations
    workspace_content = '''<?xml version="1.0" encoding="UTF-8"?>
<project version="4">
  <component name="CMakePresetLoader"><![CDATA[{
  "useNewFormat": true
}]]></component>
  <component name="CMakeProjectFlavorService">
    <option name="flavorId" value="CMakePlainProjectFlavor" />
  </component>
  <component name="CMakeReloadState">
    <option name="reloaded" value="true" />
  </component>
  <component name="CMakeSettings">
    <configurations>'''
    
    for config in configurations:
        workspace_content += config
    
    workspace_content += '''
    </configurations>
  </component>
  <component name="ProjectId" id="3ALvpy2W3U6bNN8NeQnRERQBxTs" />
  <component name="ProjectViewState">
    <option name="hideEmptyMiddlePackages" value="true" />
    <option name="showLibraryContents" value="true" />
  </component>
  <component name="PropertiesComponent"><![CDATA[{
  "keyToString": {
    "ModuleVcsDetector.initialDetectionPerformed": "true",
    "RunOnceActivity.RadMigrateCodeStyle": "true",
    "RunOnceActivity.ShowReadmeOnStart": "true",
    "RunOnceActivity.cidr.known.project.marker": "true",
    "RunOnceActivity.git.unshallow": "true",
    "RunOnceActivity.readMode.enableVisualFormatting": "true",
    "RunOnceActivity.typescript.service.memoryLimit.init": "true",
    "cf.first.check.clang-format": "false",
    "cidr.known.project.marker": "true",
    "git-widget-placeholder": "main"
  }
}]]></component>
</project>'''
    
    with open(workspace_xml_path, 'w') as f:
        f.write(workspace_content)
    
    print("\nCLion configuration files generated successfully!")
    print("Open the project in CLion to build it")
    print("Available configurations:")
    if platform == "all" or platform == "pc":
        print("  - Debug/Release: PC platform")
    if (platform == "all" or platform == "wasm") and os.path.exists(os.path.join(PROJECT_ROOT, "emsdk", "upstream", "emscripten", "cmake", "Modules", "Platform", "Emscripten.cmake")):
        print("  - WASM Debug/Release: WebAssembly platform")
    if (platform == "all" or platform == "ios") and sys.platform == 'darwin' and os.path.exists(os.path.join(PROJECT_ROOT, "build", "cmake", "ios.toolchain.cmake")):
        print("  - iOS Debug/Release: iOS platform")

def main():
    """Main function"""
    # Check for --clion flag
    clion_mode = "--clion" in sys.argv
    
    # Remove --clion from arguments if present
    if clion_mode:
        sys.argv.remove("--clion")
    
    # Parse command line arguments
    if len(sys.argv) == 1:
        if clion_mode:
            generate_clion_config("all")
        else:
            # No arguments, execute all steps
            sync_workspace()
            build_pc()
            build_wasm()
            # Build iOS only on macOS
            if platform.system() == 'Darwin':
                build_mobile()
    else:
        # With arguments, execute corresponding steps
        for arg in sys.argv[1:]:
            if arg == "pc":
                if clion_mode:
                    generate_clion_config("pc")
                else:
                    build_pc()
            elif arg == "ios":
                if clion_mode:
                    generate_clion_config("ios")
                else:
                    # Build iOS only on macOS
                    if platform.system() == 'Darwin':
                        build_mobile()
                    else:
                        print("iOS build is only supported on macOS")
            elif arg == "wasm":
                if clion_mode:
                    generate_clion_config("wasm")
                else:
                    build_wasm()
            elif arg == "workspace":
                sync_workspace()
            elif arg == "all":
                if clion_mode:
                    generate_clion_config("all")
                else:
                    sync_workspace()
                    build_pc()
                    build_wasm()
                    # Build iOS only on macOS
                    if platform.system() == 'Darwin':
                        build_mobile()
            elif arg == "help":
                print_help()
            else:
                print(f"Unknown option: {arg}")
                print_help()


if __name__ == "__main__":
    main()