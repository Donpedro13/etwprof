import ctypes
import itertools
import os
import re
import subprocess
import sys
import TestConfig
import test_framework
import tempfile
import time

class Win32NamedEvent:
    def __init__(self, name, create_or_open = False, auto_reset = False):
        from ctypes.wintypes import BOOL, HANDLE, LPCWSTR, LPVOID

        # Constants and function prototypes
        INVALID_HANDLE_VALUE = -1
        ERROR_ALREADY_EXISTS = 183

        CreateEventW = ctypes.windll.kernel32.CreateEventW
        CreateEventW.argtypes = [LPVOID, BOOL, BOOL, LPCWSTR]
        CreateEventW.restype = HANDLE

        self._handle = CreateEventW(0, not auto_reset, False, name)

        if ctypes.windll.kernel32.GetLastError() == ERROR_ALREADY_EXISTS and not create_or_open:
            self._cleanup()

            raise RuntimeError(f"Win32 event with name \"{name}\" already exists!")

        if self._handle == HANDLE(INVALID_HANDLE_VALUE):
            raise RuntimeError("CreateEventW returned INVALID_HANDLE_VALUE")

    def wait(self):
        from ctypes.wintypes import DWORD, HANDLE

        # Constants and function prototypes
        WAIT_OBJECT_0 = 0
        INFINITE = 0xFFFFFFFF

        WaitForSingleObject = ctypes.windll.kernel32.WaitForSingleObject
        WaitForSingleObject.argtypes = [HANDLE, DWORD]
        WaitForSingleObject.restype = DWORD

        result = WaitForSingleObject(self._handle, INFINITE)
        if result != WAIT_OBJECT_0:
            raise RuntimeError(f"Value other than WAIT_OBJECT_0 is returned from WaitForSingleObject: {result}!")

    def signal(self):
        from ctypes.wintypes import BOOL, HANDLE

        # Constants and function prototypes
        SetEvent = ctypes.windll.kernel32.SetEvent
        SetEvent.argtypes = [HANDLE]
        SetEvent.restype = BOOL

        if not SetEvent(self._handle):
            raise RuntimeError(f"SetEvent failed with error {ctypes.windll.kernel32.GetLastError()}!")

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self._cleanup()

    def __del__(self):
        self._cleanup()

    def _cleanup(self):
        if self._handle:
            ctypes.windll.kernel32.CloseHandle(self._handle)
            self._handle = None

class AsyncTimeoutedProcess:
    def __init__(self, exe, args = [], timeout = None, stdout = subprocess.DEVNULL):
        merged_cmdline = " ".join(itertools.chain([exe], args))
        self._process = subprocess.Popen(merged_cmdline, stdout = stdout)
        self._exe = exe
        
        self._timeout = timeout
        self._timeout_reached = False

        if self._timeout:
            import threading
            self._timeout_killer = threading.Timer(self._timeout, self._kill_due_to_timeout)
            self._timeout_killer.start()

    @property
    def pid(self):
        return self._process.pid

    @property
    def exitcode(self):
        return self._process.returncode

    @property
    def running(self):
        return self._process.poll() is None

    @property
    def timeout_reached(self):
        return self._timeout_reached

    def wait(self):
        self._process.wait()
        self._timeout_killer.cancel()

    def check(self):
        self.wait()

        if self._timeout_reached:
            raise RuntimeError(f"Process \"{self._exe}\" was killed after the timeout of {self._timeout} seconds expired!")

        if self.exitcode != 0:
            raise RuntimeError(f"Process \"{self._exe}\" failed with exit code {self.exitcode}!")

    def kill(self):
        self._process.kill()

        self._timeout_killer.cancel()

    def _kill_due_to_timeout(self):
        self._timeout_reached = True
        self.kill()

    def __del__(self):
        self.kill()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.kill()

def run_etwprof(args):
    cmd = [os.path.join(TestConfig._testbin_folder_path, "etwprof.exe")]
    cmd.extend(args)

    try:
        return subprocess.run(cmd, stdout = subprocess.DEVNULL, stderr = subprocess.DEVNULL, timeout = TestConfig.get_process_timeout()).returncode
    except subprocess.TimeoutExpired as e:
        raise RuntimeError(f"Timeout of {TestConfig.get_process_timeout()} seconds expired for etwprof!")

def create_temp_file(content):
    fd, path = tempfile.mkstemp()
    os.close(fd)

    mode = "wb" if isinstance(content, bytes) else "w"
    with open(path, mode) as f:
        f.write(content)

    return path

def create_temp_dir():
    return tempfile.mkdtemp()

def is_win7_or_earlier() -> bool:
    v = sys.getwindowsversion()

    return v.major == 6 and v.minor <= 1

_XPERF_ETW_SESSIONS_RE = re.compile(r"Logger Name\s*:\s*(?P<session_name>.*)")

def _get_etw_session_list() -> list[str]:
    result = subprocess.run(["xperf", "-loggers"], capture_output = True)
    
    result_str = result.stdout.decode("ascii")  # We would need ACP, but that's not easy; hope for the best...
    session_names = []
    for line in result_str.splitlines():
        match = re.match(_XPERF_ETW_SESSIONS_RE, line)
        if match:
            session_names.append(match.group("session_name"))

    return session_names

def _is_relevant_session(name: str) -> bool:
    if is_win7_or_earlier():
        return name == "NT Kernel Logger"
    else:
        return name.lower().startswith("etwprof")

def _get_all_running_etwprof_sessions() -> list[str]:
    sessions = _get_etw_session_list()
    etwprof_sessions = []
    for session in sessions:
        if _is_relevant_session(session):
            etwprof_sessions.append(session)

    return etwprof_sessions

def _stop_etw_session(name) -> bool:
    return subprocess.run(["xperf", "-stop", name], stdout = subprocess.DEVNULL, stderr = subprocess.DEVNULL).returncode == 0

def stop_etwprof_sessions_if_any() -> list[str]:
    etwprof_sessions = _get_all_running_etwprof_sessions()
    for session_name in etwprof_sessions:
        _stop_etw_session(session_name)

    return etwprof_sessions

def are_any_etwprof_sessions() -> bool:
    return bool(_get_all_running_etwprof_sessions())

def wait_for_etwprof_session(timeout = 10):
    """Waits for an etwprof ETW session to appear.
       Make sure that the session cannot stop before this function returns,
       or detection will be prone to a race condition"""
    start_time = time.time()
    
    etwprof_session_found = False
    while not etwprof_session_found:
        if time.time() - start_time >= timeout:
            raise RuntimeError("etwprof session did not start in given time")
        
        etwprof_session_found = are_any_etwprof_sessions()
        
        time.sleep(0.1)

    return etwprof_session_found


class ETWProfFixture:
    def setup(self):
        stopped_sessions = stop_etwprof_sessions_if_any()
        if stopped_sessions:
            print("Note: the following (probably leaked) sessions were cleaned up:")
            for session in stopped_sessions:
                print(f"\t{session}")

    def teardown(self):
        stopped_sessions = stop_etwprof_sessions_if_any()
        if stopped_sessions:
            test_framework.fail (f'Testcase leaked the following ETW session(s): {", ".join(stopped_sessions)}')

def get_cmd_path() -> str:
    return r"C:\Windows\System32\cmd.exe"