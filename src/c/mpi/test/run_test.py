#!/usr/bin/env python3
"""
cross-platform mpi test runner for gcn10

this script automates running a test for the gcn10 executable in the test/ directory.
it should work on linux, macos, and windows (with mpi installed and available in PATH).

steps performed by the script:
1. determine the number of mpi processes to use.
   - if a command-line argument is given (e.g., `python run_test.py 8`), use that.
   - otherwise, default to 8 processes.

2. determine file paths:
   - `here`: the absolute path to the test/ directory (the location of this script).
   - `root`: the directory above test/, which should contain either:
       a) a `gcn10` executable directly, or
       b) a `build/` subdirectory containing `gcn10`.

3. search for the executable:
   - look in `../gcn10` and `../build/gcn10`.
   - if not found, exit with an error.
   in this case, you should check your compilation and manually copy the gcn10
   executable you compiled to the test directory.

4. copy the executable into test/:
   - copy only the gcn10 binary to the test/ directory.
   - do not copy config.txt or blocks.txt — these must already exist in test/.

5. run the executable with mpi:
   - execute: `mpirun -n <np> ./gcn10 -c config.txt -l blocks.txt`
   - working directory is set to test/.
   - raises an error if the program exits non-zero.

requirements:
- mpi installed and `mpirun` available in PATH.
- config.txt and blocks.txt already in test/ directory.
- python 3.x installed.
"""

import os
import shutil
import subprocess
import sys
from pathlib import Path

def main():
    np = sys.argv[1] if len(sys.argv) > 1 else "8"

    here = Path(__file__).parent.resolve()
    root = here.parent
    exe_candidates = [root / "gcn10", root / "build" / "gcn10"]

    exe = None
    for cand in exe_candidates:
        if cand.is_file() and os.access(cand, os.X_OK):
            exe = cand
            break
    if exe is None:
        print("error: gcn10 executable not found in ../ or ../build/", file=sys.stderr)
        sys.exit(1)

    shutil.copy(exe, here / "gcn10")

    subprocess.run(
        ["mpirun", "-n", np, str(here / "gcn10"), "-c", "config.txt", "-l", "blocks.txt"],
        cwd=here,
        check=True
    )

if __name__ == "__main__":
    main()
