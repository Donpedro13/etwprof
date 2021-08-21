"Tests for the profile command"
from enum import IntFlag, auto
from fnmatch import fnmatch
import os
import subprocess
import shutil
from test_framework import *
from TestUtils import *
import time

_profile_suite = TestSuite("Profile tests")

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
    def __init__(self, operation):
        super().__init__(os.path.join(TestConfig._testbin_folder_path, "ProfileTestHelper.exe"),
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
    def __init__(self, target_pid, outdir_or_file, extra_args = None):
        profile_args = ["profile", f"-t={target_pid}"]
        if os.path.isdir(outdir_or_file):
            profile_args.append(f"--outdir={outdir_or_file}")
        else:
            profile_args.append(f"-o={outdir_or_file}")

        profile_args.extend(extra_args)

        super().__init__(os.path.join(TestConfig._testbin_folder_path, "etwprof.exe"),
                         profile_args,
                         timeout = TestConfig.get_process_timeout())

def _perform_profile_test(operation, outdir_or_file, extra_args = None):
    "Performs a profiling test case with PTH, and returns a file/folder list as a result"
    if not extra_args:
        extra_args = []

    # Create helper process
    pth = PTHProcess(operation)

    # Acquire event for synchronizing with the helper process
    with pth.get_event_for_sync() as e:
        minidump = any(arg in extra_args for arg in ["-m", "--mdump"])

        # Start etwprof, and make it attach to the target process
        etwprof = EtwprofProcess(pth.pid, outdir_or_file, extra_args)

        # Make "sure" that etwprof attached to the target process and started profiling (see the comment in
        # PTHProcess.event for details)
        time.sleep(0.05)

        # Tell the helper process to proceed with the operation requested
        e.signal()

        # Wait for both processes to finish
        pth.wait()
        etwprof.wait()

        # Make sure they both succeeded
        pth.check()
        etwprof.check()

        # Construct list of result files
        if os.path.isdir(outdir_or_file):
            outdir = outdir_or_file
        else:
            outfile = outdir_or_file
            outdir = os.path.dirname(outfile)
            
        return [os.path.join(outdir, f) for f in os.listdir(outdir)]

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

_ETL_MIN_SIZE = 10 * 1024
_DMP_MIN_SIZE = 1 * 1024

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

# TODO
# Multiple target processes
# Unknown PID/target process
# Output redirect (misc?)
# Debug mode
# Session kill
# Emulate mode
# CTRL+C
# 7z
# uncompressed
# cswitch
# Multiple etwprofs at once