def setup_importdirs():
    import os
    import sys

    scriptdir = os.path.dirname(os.path.realpath(__file__))

    sys.path.append(os.path.join(scriptdir, "Framework"))
    sys.path.append(os.path.join(scriptdir, "Tests"))

setup_importdirs()

import os
import subprocess
import sys
import test_framework
import Tests
import TestConfig

def usage():
    print("Usage:\nRunTests.py <etwprof folder> [case/suite filter pattern]")

class StylishPrinter:
    # ANSI sequences
    RESET       = '\033[0m'

    BOLD        = '\033[1m'
    UNDERLINE   = '\033[4m'
    
    GREEN       = '\033[92m'
    RED         = '\033[91m'
    YELLOW      = '\033[93m'

    def _try_enable_vt100():
        # Constants from "windows.h"
        STD_OUTPUT_HANDLE = -11
        INVALID_HANDLE_VALUE = -1
        ENABLE_VIRTUAL_TERMINAL_PROCESSING = 4

        from ctypes import POINTER, byref, windll
        from ctypes.wintypes import DWORD, HANDLE, BOOL
        
        k32 = windll.kernel32

        # Function prototypes
        GetStdHandle = k32.GetStdHandle
        GetStdHandle.argtypes = [DWORD]
        GetStdHandle.restype = HANDLE

        GetConsoleMode = k32.GetConsoleMode
        GetConsoleMode.argtypes = [HANDLE, POINTER(DWORD)]
        GetConsoleMode.restype = BOOL

        SetConsoleMode = k32.SetConsoleMode
        SetConsoleMode.argtypes = [HANDLE, DWORD]
        SetConsoleMode.restype = BOOL

        # Query the HANDLE for stdout
        h_std_out = GetStdHandle(STD_OUTPUT_HANDLE)
        if h_std_out == INVALID_HANDLE_VALUE:
            return False

        # Get the current mode value
        original_mode = DWORD()
        if not GetConsoleMode(h_std_out, byref(original_mode)):
            return False
        
        # Try to "append" ENABLE_VIRTUAL_TERMINAL_PROCESSING
        new_mode = DWORD(original_mode.value | ENABLE_VIRTUAL_TERMINAL_PROCESSING)
        if not SetConsoleMode(h_std_out, new_mode):
            return False

        return True

    stylish = _try_enable_vt100()

    @classmethod
    def print(cls, properties, str):
        if cls.stylish:
            print(f"{''.join(properties)}{str}{cls.RESET}", end = "")
        else:
            print(str, end = "")

    @classmethod
    def print_green(cls, str):
        if cls.stylish:
            print(f"{cls.GREEN}{str}{cls.RESET}", end = "")
        else:
            print(str, end = "")

    @classmethod
    def print_red(cls, str):
        if cls.stylish:
            print(f"{cls.RED}{str}{cls.RESET}", end = "")
        else:
            print(str, end = "")

    @classmethod
    def print_yellow(cls, str):
        if cls.stylish:
            print(f"{cls.YELLOW}{str}{cls.RESET}", end = "")
        else:
            print(str, end = "")

def run_tests(testbin_folder_path, filter):
    g_n_suites = 0
    g_n_cases = 0

    from time import perf_counter
    start = None
    suite_start = None
    case_start = None

    def on_start(n_suites, n_cases):
        nonlocal start
        nonlocal g_n_suites
        nonlocal g_n_cases
        start = perf_counter()

        if filter != "*":
            StylishPrinter.print_yellow(f"Note: test filter is set to: {filter}\n")    

        StylishPrinter.print_green("[==========] ")
        print(f"Running {n_cases} test(s) from {n_suites} suite(s)...")
        g_n_suites = n_suites
        g_n_cases = n_cases

    def on_end(failed_cases):
        diff = perf_counter() - start

        StylishPrinter.print_green("[==========] ")
        print(f"{g_n_cases} case(s) from {g_n_suites} suite(s) ran ({diff * 1000:.0f} ms)")

        n_failed = len(failed_cases)
        n_passed = n_failed - g_n_cases
        if n_passed > 0:
            StylishPrinter.print_green("[  PASSED  ] ")
            print(f"{n_passed} test(s)")
            
        if n_failed > 0:
            StylishPrinter.print_red("[  FAILED  ] ")
            print(f"{n_failed} test(s), listed below:")

            for case in failed_cases:
                StylishPrinter.print_red("[  FAILED  ] ")
                print(case.full_name)

    def on_suite_start(suite, cases):
        nonlocal suite_start
        suite_start = perf_counter()

        StylishPrinter.print_green("[----------] ")
        print(f"{len(cases)} test(s) from suite {suite.name}")

    def on_suite_end(suite):
        diff = perf_counter() - suite_start

        StylishPrinter.print_green("[----------] ")
        print(f"Finished running tests from suite {suite.name} ({diff * 1000:.0f} ms)")

    def on_case_start(case):
        nonlocal case_start
        case_start = perf_counter()

        StylishPrinter.print_green("[ RUN      ] ")
        print(case.full_name)

    def on_case_end(case):
        diff = perf_counter() - case_start

        if not case.has_failures():
            StylishPrinter.print_green("[       OK ] ")
            print(f"{case.full_name} ({diff * 1000:.0f} ms)")
        else:
            for f in case.failures:
                print(f)

            StylishPrinter.print_red("[  FAILED  ] ")
            print(f"{case.full_name} ({diff * 1000:.0f} ms)")

    TestConfig.set_testbin_folder_path(testbin_folder_path)

    runner = test_framework.TestRunner()
    runner.on_start = on_start
    runner.on_end = on_end
    runner.on_suite_start = on_suite_start
    runner.on_suite_end = on_suite_end
    runner.on_case_start = on_case_start
    runner.on_case_end = on_case_end

    runner.run(filter)

def fail(error_msg):
    StylishPrinter.print_red(f"{error_msg}\n")
    sys.exit(-1)

def has_admin_privileges():
    import ctypes

    # Function prototypes
    IsUserAnAdmin = ctypes.windll.shell32.IsUserAnAdmin
    IsUserAnAdmin.restype = ctypes.wintypes.BOOL

    return IsUserAnAdmin()

def check_prerequisites(testbin_folder_path):
    if sys.version_info[0] != 3:
        fail("Python 3 is required to run this script!")

    if not os.path.exists(testbin_folder_path):
        fail("Test binary folder path is invalid or does not exist!")

    if not os.path.exists(os.path.join(testbin_folder_path, "etwprof.exe")):
        fail("etwprof binary is missing from the binary folder!")

    if not os.path.exists(os.path.join(testbin_folder_path, "traceinfodumper.exe")):
        fail("traceinfodumper binary is missing from the binary folder!")

    if not has_admin_privileges():
        fail("Tests must be run with admin privileges!")

    try:
        if subprocess.run(["xperf", "/?"], stdout = subprocess.DEVNULL, stderr = subprocess.DEVNULL).returncode != 0:
            raise RuntimeError
    except (RuntimeError, OSError):
        fail("xperf must be available and in the path environment variable!")

def main():
    if len(sys.argv) not in (2, 3):
        usage()
        sys.exit(-1)

    testbin_folder_path = sys.argv[1]
    check_prerequisites(testbin_folder_path)

    filter = sys.argv[2] if len(sys.argv) == 3 else "*"
    
    run_tests(testbin_folder_path, filter)

if __name__ == "__main__":
    main()