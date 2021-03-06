import ctypes
import itertools
import os
import re
import subprocess
import sys
import TestConfig
import test_framework
import tempfile

class Win32NamedEvent:
    def __init__(self, name, create_or_open = False):
        from ctypes.wintypes import BOOL, HANDLE, LPCWSTR, LPVOID

        # Constants and function prototypes
        INVALID_HANDLE_VALUE = -1
        ERROR_ALREADY_EXISTS = 183

        CreateEventW = ctypes.windll.kernel32.CreateEventW
        CreateEventW.argtypes = [LPVOID, BOOL, BOOL, LPCWSTR]
        CreateEventW.restype = HANDLE

        self.handle = CreateEventW(0, True, False, name)

        if ctypes.windll.kernel32.GetLastError() == ERROR_ALREADY_EXISTS and not create_or_open:
            self._cleanup()

            raise RuntimeError(f"Win32 event with name \"{name}\" already exists!")

        if self.handle == HANDLE(INVALID_HANDLE_VALUE):
            raise RuntimeError("CreateEventW returned INVALID_HANDLE_VALUE")

    def wait(self):
        from ctypes.wintypes import DWORD, HANDLE

        # Constants and function prototypes
        WAIT_OBJECT_0 = 0
        INFINITE = 0xFFFFFFFF

        WaitForSingleObject = ctypes.windll.kernel32.WaitForSingleObject
        WaitForSingleObject.argtypes = [HANDLE, DWORD]
        WaitForSingleObject.restype = DWORD

        result = WaitForSingleObject(self.handle, INFINITE)
        if result != WAIT_OBJECT_0:
            raise RuntimeError(f"Value other than WAIT_OBJECT_0 is returned from WaitForSingleObject: {result}!")

    def signal(self):
        from ctypes.wintypes import BOOL, HANDLE

        # Constants and function prototypes
        SetEvent = ctypes.windll.kernel32.SetEvent
        SetEvent.argtypes = [HANDLE]
        SetEvent.restype = BOOL

        if not SetEvent(self.handle):
            raise RuntimeError(f"SetEvent failed with error {ctypes.windll.kernel32.GetLastError()}!")

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self._cleanup()

    def __del__(self):
        self._cleanup()

    def _cleanup(self):
        if self.handle:
            ctypes.windll.kernel32.CloseHandle(self.handle)
            self.handle = None

class AsyncTimeoutedProcess:
    def __init__(self, exe, args = [], timeout = None, stdout = subprocess.DEVNULL):
        merged_cmdline = " ".join(itertools.chain([exe], args))
        self.process = subprocess.Popen(merged_cmdline, stdout = stdout)
        self.exe = exe

        if timeout:
            import threading
            threading.Timer(timeout, AsyncTimeoutedProcess.kill_process, self.process)

    @property
    def pid(self):
        return self.process.pid

    @property
    def exitcode(self):
        return self.process.returncode

    def wait(self):
        self.process.wait()

    def check(self):
        self.wait()

        if self.exitcode != 0:
            raise RuntimeError(f"Process \"{self.exe}\" failed with exit code {self.exitcode}!")

    @staticmethod
    def kill_process(process):
        process.kill()

    def __del__(self):
        self.process.kill()

def gracefully_end_every_etwprof_child():
    # This function assumes that every child process that is *not* etwprof will ignore CTRL+C
    _generate_ctrl_c_event()

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

def _get_etw_session_list():
    result = subprocess.run(["xperf", "-loggers"], capture_output = True)
    
    result_str = result.stdout.decode("ascii")  # We would need ACP, but that's not easy; hope for the best...
    session_names = []
    for line in result_str.splitlines():
        match = re.match(_XPERF_ETW_SESSIONS_RE, line)
        if match:
            session_names.append(match.group("session_name"))

    return session_names

def _is_relevant_session(name: str):
    if is_win7_or_earlier():
        return name == "NT Kernel Logger"
    else:
        return name.lower().startswith("etwprof")

def _get_all_running_etwprof_sessions():
    sessions = _get_etw_session_list()
    etwprof_sessions = []
    for session in sessions:
        if _is_relevant_session(session):
            etwprof_sessions.append(session)

    return etwprof_sessions

def _stop_etw_session(name) -> bool:
    return subprocess.run(["xperf", "-stop", name], stdout = subprocess.DEVNULL, stderr = subprocess.DEVNULL).returncode == 0

def _stop_etwprof_sessions_if_any():
    etwprof_sessions = _get_all_running_etwprof_sessions()
    for session_name in etwprof_sessions:
        _stop_etw_session(session_name)

    return etwprof_sessions

def _generate_ctrl_c_event():
    "This function generates a CTRL+C event, which will be sent to every child process"

    try:
        ctypes.windll.kernel32.GenerateConsoleCtrlEvent(0, 0)
    except KeyboardInterrupt:
    	pass    # Ignore

class ETWProfFixture:
    def setup(self):
        stopped_sessions = _stop_etwprof_sessions_if_any()
        if stopped_sessions:
            print("Note: the following (probably leaked) sessions were cleaned up:")
            for session in stopped_sessions:
                print(f"\t{session}")

    def teardown(self):
        stopped_sessions = _stop_etwprof_sessions_if_any()
        if stopped_sessions:
            test_framework.current_case.add_failure(test_framework.TestFailure(f'Testcase leaked the following ETW session(s): {", ".join(stopped_sessions)}'))