# Copyright 2024 N-GINN LLC. All rights reserved.
# Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

import sys
import subprocess
import pathlib
import os

if len(sys.argv) < 8:
    print('Usage: installer.py platform(win64) '
        'EXE_DIR(where files are taken from) OUT_FILENAME(installer file name) SOURCE(path to Engine source) CONFIG(Debug or Release) VERSION(product version) TOOLCHAIN(path to toolchain)')
    exit(1)

def validate_path(path) :
    return os.path.abspath(os.path.expanduser(path))

if __name__ == "__main__":
    platform = sys.argv[1]
    exe_dir  = validate_path(sys.argv[2])
    out_filename = validate_path(sys.argv[3])
    nausource = validate_path(sys.argv[4])
    config = sys.argv[5]
    version = sys.argv[6]
    toolchain = validate_path(sys.argv[7])

    print('installer.py platform({0}) EXE_DIR({1}) OUT_FILENAME({2}) SOURCE({3}) CONFIG({4}) VERSION({5}) TOOLCHAIN({6})'.format(platform, exe_dir, out_filename, nausource, config, version, toolchain), flush=True)

    platform_build_script = validate_path(os.path.join(os.getcwd(),
        platform, 'build.py'))

    if pathlib.Path(platform_build_script).exists():
        print('Building SDK for platform "{0}"'.format(platform), flush=True)
        ret = subprocess.call(["python", platform_build_script, nausource, exe_dir, config, toolchain])
        if ret != 0:
            print('Failed to build SDK')
            exit(ret)
    else:
        print('For platform "{0}" SDK build tool was not implemented, skipping SDK build'.format(platform), flush=True)

    platform_specified_script = validate_path(os.path.join(os.getcwd(),
        platform, 'make_installer.py'))

    if not pathlib.Path(platform_specified_script).exists():
        print('For platform "{0}" installer tool was not implemented'
            .format(platform), flush=True)
        exit(1)

    ret = subprocess.call(
        ["python", platform_specified_script, exe_dir, out_filename, version],
        cwd = os.path.abspath(platform))

    if ret == 0 :
        print('Installer generation completed successefully.', flush=True)
    else :
        print('Failed to create the installer.', flush=True)
    exit(ret)
