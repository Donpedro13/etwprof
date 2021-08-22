"Tests for the profile command"
from enum import Flag, auto
from fnmatch import fnmatch
import os
import subprocess
import shutil
from test_framework import *
from TestUtils import *
import time

_profile_suite = TestSuite("Profile tests")

_ETL_MIN_SIZE = 10 * 1024
_DMP_MIN_SIZE = 1 * 1024
_PTH_EXE_NAME = "ProfileTestHelper.exe"

def generate_ctrl_c_for_children():
    import signal

    def _ctrl_c_handler(signal, frame):
        pass    # Ignore CTRL+C in this process

    previous_handler = signal.signal(signal.SIGINT, _ctrl_c_handler)

    os.kill(signal.CTRL_C_EVENT, 0)
    # Without this sleep, a spurious KeyboardInterrupt exception will be raised sometime after this function is called ¯\_(ツ)_/¯
    time.sleep(.1)

    # Restore the previous (most likely the default) handler
    signal.signal(signal.SIGINT, previous_handler)

class ProfileTestsFixture(ETWProfFixture):
    def setup(self):
        super().setup()

        self._outdir = create_temp_dir()
        self._outfile = os.path.join(self._outdir, "test.etl")

    def teardown(self):
        shutil.rmtree(self._outdir)

        super().teardown()

    @property
    def outdir(self):
        return self._outdir

    @property
    def outfile(self):
        return self._outfile

class PTHProcess(AsyncTimeoutedProcess):
    def __init__(self, operation = "DoNothing"):
        super().__init__(os.path.join(TestConfig._testbin_folder_path, _PTH_EXE_NAME),
                         [operation],
                         timeout = TestConfig.get_process_timeout())

        # Native Win32 event used for synchronizing with PTH
        # The test case or operation should start only when etwprof has already started profiling to avoid race
        # conditions. There is still a small window for a race condition this way, but it's negligible. Using another
        # event would solve this problem completely, but that would require etwprof to also signal an event after it has
        # started profiling.
        self._event = Win32NamedEvent(f"PTH_event_{os.getpid()}_{self._process.pid}", create_or_open = True)

    def get_event_for_sync(self):
        return self._event

class EtwprofProcess(AsyncTimeoutedProcess):
    def __init__(self, target_pid_or_name, outdir_or_file, extra_args = None):
        if not extra_args:
            extra_args = []

        profile_args = ["profile", f"-t={target_pid_or_name}"]
        if os.path.isdir(outdir_or_file):
            profile_args.append(f"--outdir={outdir_or_file}")
        else:
            profile_args.append(f"-o={outdir_or_file}")

        profile_args.extend(extra_args)

        super().__init__(os.path.join(TestConfig._testbin_folder_path, "etwprof.exe"),
                         profile_args,
                         timeout = TestConfig.get_process_timeout())

def _list_files_in_dir(dir):
    return [os.path.join(dir, f) for f in os.listdir(dir)]

class ProfileTestOptions(Flag):
    DEFAULT = 0
    TARGET_ID_NAME = auto()

def _perform_profile_test(operation, outdir_or_file, extra_args = None, options = ProfileTestOptions.DEFAULT):
    "Performs a profiling test case with PTH, and returns a file/folder list as a result"
    if not extra_args:
        extra_args = []

    # Create helper process, acquire event for synchronizing with the helper process
    with PTHProcess(operation) as pth, pth.get_event_for_sync() as e:
        # Start etwprof, and make it attach to the target process
        with EtwprofProcess(_PTH_EXE_NAME if options & ProfileTestOptions.TARGET_ID_NAME else pth.pid,
                            outdir_or_file, extra_args) as etwprof:

            # Make "sure" that etwprof attached to the target process and started profiling (see the comment in
            # PTHProcess.event for details)
            time.sleep(.1)

            # Tell the helper process to proceed with the operation requested
            e.signal()

            # Wait for both processes to finish, and make sure they both succeeded
            pth.check()
            etwprof.check()

            # Construct list of result files
            if os.path.isdir(outdir_or_file):
                outdir = outdir_or_file
            else:
                outfile = outdir_or_file
                outdir = os.path.dirname(outfile)
                
            return _list_files_in_dir(outdir)

class _ProfileTestFileExpectation():
    def __init__(self, file_pattern, expected_n, min_filesize):
        self._file_pattern = file_pattern
        self._expected_n = expected_n
        self._min_filesize = min_filesize

    def evaluate(self, filelist):
        matching = []
        for file in filelist:
            if fnmatch(file, self._file_pattern):
                matching.append(file)
                assert_gt(os.stat(file).st_size, self._min_filesize)

        assert_eq(len(matching), self._expected_n)

def _evaluate_profile_test(filelist, expectation_list):
    for expectation in expectation_list:
        expectation.evaluate(filelist)

def _evaluate_simple_profile_test(filelist, expected_etl):
    _evaluate_profile_test(filelist, [_ProfileTestFileExpectation(expected_etl, 1, _ETL_MIN_SIZE)])

@testcase(suite = _profile_suite, name = "Short lived process", fixture = ProfileTestsFixture())
def test_short_lived_process():
    _evaluate_simple_profile_test(_perform_profile_test("DoNothing", fixture.outfile), fixture.outfile)

@testcase(suite = _profile_suite, name = "Output directory", fixture = ProfileTestsFixture())
def test_output_directory():
    _evaluate_profile_test(_perform_profile_test("DoNothing", fixture.outdir),
                           [_ProfileTestFileExpectation("*.etl", 1, _ETL_MIN_SIZE)])

@testcase(suite = _profile_suite, name = "Minidump", fixture = ProfileTestsFixture())
def test_minidump():
    _evaluate_profile_test(_perform_profile_test("DoNothing", fixture.outdir, ["-m"]),
                           [_ProfileTestFileExpectation("*.etl", 1, _ETL_MIN_SIZE),
                            _ProfileTestFileExpectation("*.dmp", 1, _DMP_MIN_SIZE)])

@testcase(suite = _profile_suite, name = "5 sec CPU burn", fixture = ProfileTestsFixture())
def test_5s_cpu_burn():
    _evaluate_simple_profile_test(_perform_profile_test("BurnCPU5s", fixture.outfile), fixture.outfile)

@testcase(suite = _profile_suite, name = "Target specified by name", fixture = ProfileTestsFixture())
def test_target_name():
    _evaluate_profile_test(_perform_profile_test("DoNothing", fixture.outdir, [], ProfileTestOptions.TARGET_ID_NAME),
                           [_ProfileTestFileExpectation("*.etl", 1, _ETL_MIN_SIZE)])

@testcase(suite = _profile_suite, name = "Multiple target processes with same name", fixture = ProfileTestsFixture())
def test_multiple_target_processes_same_name():
    with PTHProcess(), PTHProcess(), EtwprofProcess(_PTH_EXE_NAME, fixture.outfile) as etwprof:
        etwprof.wait()

        assert_neq(etwprof.exitcode, 0)

@testcase(suite = _profile_suite, name = "Unknown process (name)", fixture = ProfileTestsFixture())
def test_unknown_process_name():
    with EtwprofProcess("ThisSurelyDoesNotExist.exe", fixture.outfile) as etwprof:
        etwprof.wait()

        assert_neq(etwprof.exitcode, 0)

@testcase(suite = _profile_suite, name = "Unknown process (PID)", fixture = ProfileTestsFixture())
def test_unknown_process_pid():
    PID_MAX = 0xFFFFFFFF
    with EtwprofProcess(PID_MAX, fixture.outfile) as etwprof:
        etwprof.wait()

        assert_neq(etwprof.exitcode, 0)

@testcase(suite = _profile_suite, name = "Debug mode", fixture = ProfileTestsFixture())
def test_debug_mode():
    _evaluate_profile_test(_perform_profile_test("DoNothing", fixture.outdir, ["--debug"]),
                           [_ProfileTestFileExpectation("*.etl", 2, _ETL_MIN_SIZE),
                            _ProfileTestFileExpectation("*.raw.etl", 1, _ETL_MIN_SIZE)])

@testcase(suite = _profile_suite, name = "ETW session killed", fixture = ProfileTestsFixture())
def test_etw_session_killed():
    # Start PTH (since we never signal its event, it will wait forever), then we pull the rug from under etwprof
    with PTHProcess() as pth, EtwprofProcess(_PTH_EXE_NAME, fixture.outfile) as etwprof:
        # It's possible that we will try to stop etwprof's session *before* it even started, so let's retry a few
        # times, if that happens
        for i in range(0, 3):
            if stop_etwprof_sessions_if_any():
                break

        etwprof.check()

        assert_true(pth.running)   # Make sure that etwprof exited not because the target process timed out

@testcase(suite = _profile_suite, name = "Emulate mode", fixture = ProfileTestsFixture())
def test_emulate_mode():
    # Create an ETL file that will serve as an input for testing emulate mode
    with PTHProcess() as pth, pth.get_event_for_sync() as e, EtwprofProcess(_PTH_EXE_NAME, fixture.outfile) as etwprof:
        target_pid = pth.pid

        time.sleep(.1)
        e.signal()
        
        pth.check()
        etwprof.check()

    emulate_input_etl = "".join([fixture.outfile, ".emulate_input.etl"])
    os.rename(fixture.outfile, emulate_input_etl)
    assert_false(os.path.exists(fixture.outfile))   # Check if the rename succeeded

    with EtwprofProcess(target_pid, fixture.outfile, [f"--emulate={emulate_input_etl}"]) as etwprof:
        etwprof.check()

    assert_true(os.path.exists(fixture.outfile))
    assert_gt(os.stat(fixture.outfile).st_size, _ETL_MIN_SIZE)

@testcase(suite = _profile_suite, name = "Compression with 7z", fixture = ProfileTestsFixture())
def test_compression_7z():
    _evaluate_profile_test(_perform_profile_test("DoNothing", fixture.outdir, ["--compress=7z"]),
                           [_ProfileTestFileExpectation("*.7z", 1, _ETL_MIN_SIZE)])

@testcase(suite = _profile_suite, name = "Uncompressed", fixture = ProfileTestsFixture())
def test_compression_uncompressed():
    _evaluate_simple_profile_test(_perform_profile_test("DoNothing", fixture.outfile, ["--compress=off"]), fixture.outfile)

@testcase(suite = _profile_suite, name = "Context switches", fixture = ProfileTestsFixture())
def test_context_switches():
    _evaluate_simple_profile_test(_perform_profile_test("DoNothing", fixture.outfile, ["--cswitch"]), fixture.outfile)

@testcase(suite = _profile_suite, name = "Stack caching", fixture = ProfileTestsFixture())
def test_stack_caching():
    if is_win7_or_earlier():
        return  # Not supported on Win 7

    _evaluate_simple_profile_test(_perform_profile_test("DoNothing", fixture.outfile, ["--scache"]), fixture.outfile)

@testcase(suite = _profile_suite, name = "Multiple etwprofs at once", fixture = ProfileTestsFixture())
def test_multiple_etwprofs_at_once():
    if is_win7_or_earlier():
        return  # Only one kernel session at once is supported on Win 7

    with PTHProcess() as pth,    \
         pth.get_event_for_sync() as e,          \
         EtwprofProcess(_PTH_EXE_NAME, fixture.outfile) as etwprof1, \
         EtwprofProcess(_PTH_EXE_NAME, fixture.outdir) as etwprof2:
            time.sleep(.1)  # Let etwprofs start profiling
            e.signal()

            pth.check()
            etwprof1.check()
            etwprof2.check()

            _evaluate_profile_test(_list_files_in_dir(fixture.outdir),
                                   [_ProfileTestFileExpectation("*.etl", 2, _ETL_MIN_SIZE)])

# TODO
# User providers...