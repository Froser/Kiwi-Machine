#!/usr/bin/env python3

# Copyright (C) 2026 Yisi Yu
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
import os
import platform
import shlex
import shutil
import subprocess
import sys
import time
from pathlib import Path


PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SWITCH_DOCKER_IMAGE = os.environ.get(
    "KIWI_SWITCH_DOCKER_IMAGE", "devkitpro/devkita64:20260219")


class SetupError(RuntimeError):
    pass


def command_text(args):
    if platform.system() == "Windows":
        return subprocess.list2cmdline(args)
    return shlex.join(args)


def run(args, check=True, env=None):
    print(f"+ {command_text(args)}", flush=True)
    result = subprocess.run(args, env=env)
    if check and result.returncode != 0:
        raise SetupError(
            f"Command failed with exit code {result.returncode}: "
            f"{command_text(args)}")
    return result.returncode


def command_succeeds(args):
    return subprocess.run(
        args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
    ).returncode == 0


def require_command(name, install_hint):
    path = shutil.which(name)
    if not path:
        raise SetupError(f"{name} was not found. {install_hint}")
    return path


def docker_is_ready():
    docker = shutil.which("docker")
    return bool(docker) and command_succeeds([docker, "info"])


def wait_for_docker(timeout_seconds=180):
    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline:
        if docker_is_ready():
            return
        time.sleep(2)
    raise SetupError(
        "Docker did not become ready. Check the container runtime logs.")


def setup_macos(args):
    brew = require_command(
        "brew", "Install Homebrew from https://brew.sh and rerun this script.")

    packages = []
    if not shutil.which("docker"):
        packages.append("docker")
    if not shutil.which("colima"):
        packages.append("colima")
    if packages:
        run([brew, "install", *packages])

    if docker_is_ready():
        return

    colima = require_command(
        "colima", "Run: brew install docker colima")
    colima_args = [colima, "start"]
    if not (Path.home() / ".colima" / "default" / "colima.yaml").exists():
        colima_args.extend([
            "--cpu", str(args.cpu),
            "--memory", str(args.memory),
            "--disk", str(args.disk),
        ])
    try:
        run(colima_args)
        wait_for_docker(timeout_seconds=20)
    except SetupError:
        print("Colima did not expose Docker; restarting its VM.", flush=True)
        run([colima, "stop", "--force"], check=False)
        run(colima_args)
        wait_for_docker()


def sudo_prefix():
    if hasattr(os, "geteuid") and os.geteuid() == 0:
        return []
    require_command("sudo", "Install sudo or run this script as root.")
    return ["sudo"]


def setup_linux(_args):
    if not shutil.which("docker"):
        sudo = sudo_prefix()
        if shutil.which("apt-get"):
            run([*sudo, "apt-get", "update"])
            run([*sudo, "apt-get", "install", "-y", "docker.io"])
        elif shutil.which("dnf"):
            run([*sudo, "dnf", "install", "-y", "docker"])
        elif shutil.which("pacman"):
            run([*sudo, "pacman", "-Sy", "--needed", "--noconfirm", "docker"])
        elif shutil.which("zypper"):
            run([*sudo, "zypper", "--non-interactive", "install", "docker"])
        else:
            raise SetupError(
                "No supported package manager was found. Install Docker "
                "Engine and rerun this script.")

    if not docker_is_ready():
        sudo = sudo_prefix()
        systemctl = require_command(
            "systemctl", "Start the Docker daemon manually.")
        run([*sudo, systemctl, "enable", "--now", "docker"])

    if docker_is_ready():
        return

    user = os.environ.get("USER")
    if user and shutil.which("usermod"):
        sudo = sudo_prefix()
        run([*sudo, "usermod", "-aG", "docker", user])
        raise SetupError(
            "Added the current user to the docker group. Log out and back in, "
            "then rerun this script.")
    raise SetupError(
        "Docker is running, but the current user cannot access its socket.")


def setup_windows(_args):
    if not shutil.which("docker"):
        winget = require_command(
            "winget", "Install Docker Desktop manually and rerun this script.")
        run([
            winget,
            "install",
            "--id", "Docker.DockerDesktop",
            "--exact",
            "--accept-package-agreements",
            "--accept-source-agreements",
        ])

    if docker_is_ready():
        return

    desktop = os.path.join(
        os.environ.get("ProgramFiles", r"C:\Program Files"),
        "Docker", "Docker", "Docker Desktop.exe")
    if not os.path.isfile(desktop):
        raise SetupError(
            "Docker Desktop is installed but its executable was not found.")
    print(f"+ starting {desktop}", flush=True)
    subprocess.Popen([desktop])
    wait_for_docker()


def setup_container_runtime(args):
    if docker_is_ready():
        return

    system = platform.system()
    if system == "Darwin":
        setup_macos(args)
    elif system == "Linux":
        setup_linux(args)
    elif system == "Windows":
        setup_windows(args)
    else:
        raise SetupError(f"Unsupported host platform: {system}")


def image_is_available(docker):
    return command_succeeds([
        docker, "image", "inspect", SWITCH_DOCKER_IMAGE
    ])


def verify_switch_image(docker):
    check = (
        "test -f /opt/devkitpro/cmake/Switch.cmake && "
        "test -f /opt/devkitpro/libnx/lib/libnx.a && "
        "command -v elf2nro >/dev/null && "
        "command -v nacptool >/dev/null && "
        "test -f /opt/devkitpro/portlibs/switch/lib/pkgconfig/sdl2.pc && "
        "test -f /opt/devkitpro/portlibs/switch/lib/pkgconfig/SDL2_image.pc && "
        "test -f /opt/devkitpro/portlibs/switch/lib/pkgconfig/SDL2_mixer.pc"
    )
    run([docker, "run", "--rm", SWITCH_DOCKER_IMAGE, "sh", "-lc", check])


def setup_switch(args):
    if args.check_only:
        if not docker_is_ready():
            raise SetupError(
                "Docker is not ready. Run this script without --check-only.")
    else:
        setup_container_runtime(args)

    docker = require_command("docker", "Install Docker and rerun this script.")
    if args.check_only:
        if not image_is_available(docker):
            raise SetupError(
                f"Switch image is not available locally: {SWITCH_DOCKER_IMAGE}")
    else:
        run([docker, "pull", SWITCH_DOCKER_IMAGE])

    verify_switch_image(docker)
    print("Nintendo Switch Homebrew build environment is ready.", flush=True)

    if args.build:
        build_env = os.environ.copy()
        build_env["KIWI_SWITCH_SKIP_PULL"] = "1"
        run([
            sys.executable,
            os.path.join(PROJECT_ROOT, "build.py"),
            "switch",
            "--build",
        ], env=build_env)


def parse_args():
    parser = argparse.ArgumentParser(
        description="Install and verify the Switch Homebrew build environment.")
    parser.add_argument(
        "--check-only",
        action="store_true",
        help="Validate the runtime and cached image without installing anything.")
    parser.add_argument(
        "--build",
        action="store_true",
        help="Build KiwiMachine.nro after setup succeeds.")
    parser.add_argument("--cpu", type=int, default=4, help="Colima CPU count.")
    parser.add_argument(
        "--memory", type=int, default=8, help="Colima memory size in GiB.")
    parser.add_argument(
        "--disk", type=int, default=30, help="Colima disk size in GiB.")
    return parser.parse_args()


def main():
    try:
        setup_switch(parse_args())
        return 0
    except SetupError as error:
        print(f"Switch setup failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
