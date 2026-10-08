#!/usr/bin/env python3

# Copyright (C) 2026 Yisi Yu
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

import argparse
import os
import shutil
import subprocess
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(
        description="Package the Kiwi Machine NRO forwarder as an NSP.")
    parser.add_argument("--hacbrewpack", type=Path, required=True)
    parser.add_argument("--nacptool", type=Path, required=True)
    parser.add_argument("--keys", type=Path, required=True)
    parser.add_argument("--nro", type=Path, required=True)
    parser.add_argument("--nro-path", required=True)
    parser.add_argument("--icon", type=Path, required=True)
    parser.add_argument("--forwarder-exefs", type=Path, required=True)
    parser.add_argument("--title-id", required=True)
    parser.add_argument("--name", required=True)
    parser.add_argument("--publisher", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--staging-dir", type=Path, required=True)
    return parser.parse_args()


def require_file(path, description):
    if not path.is_file():
        raise RuntimeError(f"{description} not found: {path}")


def run(command, cwd=None):
    print("+", " ".join(str(part) for part in command), flush=True)
    subprocess.run([str(part) for part in command], cwd=cwd, check=True)


def main():
    args = parse_args()
    require_file(args.hacbrewpack, "hacBrewPack executable")
    require_file(args.nacptool, "nacptool executable")
    require_file(args.keys, "prod.keys")
    require_file(args.nro, "Kiwi Machine NRO")
    require_file(args.icon, "Switch icon")
    require_file(args.forwarder_exefs / "main", "Forwarder NSO")
    require_file(args.forwarder_exefs / "main.npdm", "Forwarder NPDM")

    title_id = args.title_id.lower()
    staging = args.staging_dir.resolve()
    exefs = staging / "exefs"
    control = staging / "control"
    romfs = staging / "romfs"
    nsp_output = staging / "nsp"

    if staging.exists():
        shutil.rmtree(staging)
    exefs.mkdir(parents=True)
    control.mkdir()
    romfs.mkdir()
    nsp_output.mkdir()

    shutil.copy2(args.forwarder_exefs / "main", exefs / "main")
    shutil.copy2(args.forwarder_exefs / "main.npdm", exefs / "main.npdm")
    shutil.copy2(args.icon, control / "icon_AmericanEnglish.dat")
    (romfs / "nextNroPath").write_text(args.nro_path, encoding="utf-8")
    (romfs / "nextArgv").write_text(args.nro_path, encoding="utf-8")

    run([
        args.nacptool,
        "--create",
        args.name,
        args.publisher,
        args.version,
        control / "control.nacp",
        f"--titleid={title_id}",
    ])

    run([
        args.hacbrewpack,
        "--titleid",
        title_id,
        "--titlename",
        args.name,
        "--titlepublisher",
        args.publisher,
        "--nspdir",
        nsp_output,
        "--tempdir",
        staging / "temp",
        "--backupdir",
        staging / "backup",
        "--exefsdir",
        exefs,
        "--romfsdir",
        romfs,
        "--controldir",
        control,
        "--nologo",
        "--keyset",
        args.keys,
    ], cwd=staging)

    generated = nsp_output / f"{title_id}.nsp"
    require_file(generated, "Generated NSP")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary_output = args.output.with_suffix(args.output.suffix + ".tmp")
    shutil.copy2(generated, temporary_output)
    os.replace(temporary_output, args.output)
    print(f"Created {args.output}", flush=True)


if __name__ == "__main__":
    main()
