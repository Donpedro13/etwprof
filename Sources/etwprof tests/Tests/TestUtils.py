import os
import subprocess
import sys
import TestConfig
import tempfile

def run_etwprof(args):
    cmd = [TestConfig._etwprof_path]
    cmd.extend(args)
    return subprocess.run(cmd, stdout = subprocess.DEVNULL, stderr = subprocess.DEVNULL).returncode

def create_temp_file(content):
    fd, path = tempfile.mkstemp()
    os.close(fd)

    mode = "wb" if isinstance(content, bytes) else "w"
    with open(path, mode) as f:
        f.write(content)

    return path

def create_temp_dir():
    return tempfile.mkdtemp()

def is_win7_or_earlier():
    v = sys.getwindowsversion()

    return v.major == 6 and v.minor <= 1