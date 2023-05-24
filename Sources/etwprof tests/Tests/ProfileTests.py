"Tests for the profile command"
from dataclasses import dataclass
from enum import Enum, Flag, auto
from fnmatch import fnmatch
import os
import subprocess
import shutil
from test_framework import *
from TestUtils import *
from typing import *
import time
import json

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

def _list_files_in_dir(dir) -> List[str]:
    return [os.path.join(dir, f) for f in os.listdir(dir)]

class ProfileTestOptions(Flag):
    DEFAULT = 0
    TARGET_ID_NAME = auto()

@dataclass()
class ProcessInfo():
    image_name: str
    pid: int

    def __eq__(self, other) -> bool:
        return self.image_name.lower() == other.image_name.lower() and self.pid == other.pid
    
    def __hash__(self) -> int:
        return hash((self.image_name.lower(), self.pid))

@dataclass
class ImageInfo():
    name: str

    def __eq__(self, other) -> bool:
        return self.name.lower() == other.name.lower()
    
    def __hash__(self) -> int:
        return hash(self.name.lower())

@dataclass
class ThreadInfo():
    tid: int

@dataclass
class TraceData():
    etl_path: str
    processes_by_pid: dict[int, ProcessInfo]
    images_by_process: dict[ProcessInfo, List[ImageInfo]]
    threads_by_process: dict[ProcessInfo, List[ThreadInfo]]
    sampled_profile_counts_by_process: dict[ProcessInfo, int]
    context_switch_counts_by_process: dict[ProcessInfo, int]
    ready_thread_counts_by_process: dict[ProcessInfo, int]

    @classmethod
    def from_traceinfodumper_file(cls, json_path:str):
        with open(json_path, "r") as f:
            trace_data = json.load(f)

        etl_path: str = trace_data["etlPath"]
        data = trace_data["data"]

        processes_by_pid: dict[int, ProcessInfo] = {}
        for process_dict in data["processList"]:
            pid = int(process_dict["pid"])
            processes_by_pid[pid] = ProcessInfo(process_dict["imageName"], pid)

        images_by_process: dict[ProcessInfo, List[ImageInfo]] = {}
        for image_list in data["imageLists"]:
            process_info = processes_by_pid[image_list["process"]["pid"]]
            images_by_process[process_info] = []

            for image_name in image_list["imageList"]:
                images_by_process[process_info].append(ImageInfo(image_name))

        threads_by_process: dict[ProcessInfo, List[ThreadInfo]] = {}
        for thread_list in data["threadLists"]:
            process_info = processes_by_pid[thread_list["process"]["pid"]]
            threads_by_process[process_info] = []

            for tid in thread_list["threadList"]:
                threads_by_process[process_info].append(tid)

        sampled_profile_counts_by_process: dict[ProcessInfo, int] = {}
        for sampled_profile_count in data["sampledProfileCounts"]:
            process_info = processes_by_pid[sampled_profile_count["process"]["pid"]]
            sampled_profile_counts_by_process[process_info] = sampled_profile_count["count"]

        context_switch_counts_by_process: dict[ProcessInfo, int] = {}
        for context_switch_count in data["contextSwitchCounts"]:
            process_info = processes_by_pid[context_switch_count["process"]["pid"]]
            context_switch_counts_by_process[process_info] = context_switch_count["count"]

        ready_thread_counts_by_process: dict[ProcessInfo, int] = {}
        for ready_thread_count in data["contextSwitchCounts"]:
            process_info = processes_by_pid[ready_thread_count["process"]["pid"]]
            ready_thread_counts_by_process[process_info] = ready_thread_count["count"]

        return cls(etl_path,
                   processes_by_pid,
                   images_by_process,
                   threads_by_process,
                   sampled_profile_counts_by_process,
                   context_switch_counts_by_process,
                   ready_thread_counts_by_process)

def _perform_profile_test(operation, outdir_or_file, extra_args = None, options = ProfileTestOptions.DEFAULT) -> Tuple[List[str], List[ProcessInfo]]:
    "Performs a profiling test case with PTH, and returns a file/folder list, and ProcessInfo list as a result"
    if not extra_args:
        extra_args = []

    # Create helper process, acquire event for synchronizing with the helper process
    with PTHProcess(operation) as pth, pth.get_event_for_sync() as e:
        processinfo = ProcessInfo(_PTH_EXE_NAME, pth.pid)
        # Start etwprof, and make it attach to the target process
        with EtwprofProcess(_PTH_EXE_NAME if options & ProfileTestOptions.TARGET_ID_NAME else pth.pid,
                            outdir_or_file, extra_args) as etwprof:

            wait_for_etwprof_session()

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
                
            return (_list_files_in_dir(outdir), [processinfo])

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
                assert_gt(os.stat(file).st_size, 0)

        assert_eq(len(matching), self._expected_n)

class Predicate:
    def __init__(self):
        self._explanation: str

    def evaluate(self, trace_data: TraceData) -> bool:
        raise NotImplementedError
    
    def explain(self) -> str:
        return self._explanation

class ProcessExactMatchPredicate(Predicate):
    def __init__(self, processes: List[ProcessInfo]):
        self._processes = processes

    def evaluate(self, trace_data: TraceData) -> bool:
        if set(self._processes) == set(trace_data.processes_by_pid.values()):
            self._explanation = "The given process list is equivalent with the one in the trace data."
            return True
        else:
            self._explanation = f"""The given process list is not equivalent with the one in the trace data.
            \tExpected: {trace_data.processes_by_pid}
            \tActual: {self._processes}"""

            return False
        
class ImageSubsetPredicate(Predicate):
    def __init__(self, process: ProcessInfo, images: List[ImageInfo]):
        self._process = process
        self._images = images

    def evaluate(self, trace_data: TraceData) -> bool:
        if self._process not in trace_data.images_by_process:
            self._explanation = "No images are associated with the given process"
            return False

        if all(i in trace_data.images_by_process[self._process] for i in self._images):
            self._explanation = "The given image list is a subset of the one in the trace data for the given process."

            return True
        else:
            self._explanation = f"""The given image list is not a subset of the one in the trace data for the given process.
            \tIn trace data: {trace_data.images_by_process[self._process]}
            \tGiven: {self._images}"""

            return False
        
class ThreadCountGTEPredicate(Predicate):
    def __init__(self, process: ProcessInfo, expectedCountAtLeast: int):
        self._process = process
        self._expectedCountAtLeast = expectedCountAtLeast

    def evaluate(self, trace_data: TraceData) -> bool:
        if self._process not in trace_data.threads_by_process:
            self._explanation = "No threads are associated with the given process"

            return False

        if len(trace_data.threads_by_process[self._process]) >= self._expectedCountAtLeast:
            self._explanation = "The thread count for the given process was >= than the given threshold."

            return True
        else:
            self._explanation = f"""The thread count for the given process was < than the given threshold.
            \tIn trace data: {len(trace_data.threads_by_process[self._process])}
            \tThreshold: {self._expectedCountAtLeast}"""

            return False

class ComparisonOperator(Enum):
    EQUAL = auto()
    NOT_EQUAL = auto()
    GREATER_THEN = auto()
    LESS_THAN = auto()
    GREATER_THEN_EQUAL = auto()
    LESS_THEN_EQUAL = auto()

    def perform(self, left, right):
        match self:
            case ComparisonOperator.EQUAL:
                return left == right
            case ComparisonOperator.NOT_EQUAL:
                return left != right
            case ComparisonOperator.GREATER_THEN:
                return left > right
            case ComparisonOperator.LESS_THAN:
                return left < right
            case ComparisonOperator.GREATER_THEN_EQUAL:
                return left >= right
            case ComparisonOperator.LESS_THEN_EQUAL:
                return left <= right
            
    def __str__(self) -> str:
        match self:
            case ComparisonOperator.EQUAL:
                return "="
            case ComparisonOperator.NOT_EQUAL:
                return "!="
            case ComparisonOperator.GREATER_THEN:
                return ">"
            case ComparisonOperator.LESS_THAN:
                return "<"
            case ComparisonOperator.GREATER_THEN_EQUAL:
                return ">="
            case ComparisonOperator.LESS_THEN_EQUAL:
                return "<="
            
    def get_inverse(self) -> "ComparisonOperator":
        match self:
            case ComparisonOperator.EQUAL:
                return ComparisonOperator.NOT_EQUAL
            case ComparisonOperator.NOT_EQUAL:
                return ComparisonOperator.EQUAL
            case ComparisonOperator.GREATER_THEN:
                return ComparisonOperator.LESS_THEN_EQUAL
            case ComparisonOperator.LESS_THAN:
                return ComparisonOperator.GREATER_THEN_EQUAL
            case ComparisonOperator.GREATER_THEN_EQUAL:
                return ComparisonOperator.LESS_THAN
            case ComparisonOperator.LESS_THEN_EQUAL:
                return ComparisonOperator.GREATER_THEN

class CountForProcessPredicateBase(Predicate):
    def __init__(self, count_description: str, trace_data_count_container_name: str, process: ProcessInfo, operator: ComparisonOperator, expectedCountOperand: int):
        self._count_description = count_description
        self._trace_data_count_container_name = trace_data_count_container_name
        self._process = process

        self._operator = operator
        self._expectedCountOperand = expectedCountOperand

    def evaluate(self, trace_data: TraceData) -> bool:
        container = getattr(trace_data, self._trace_data_count_container_name)
        if self._process not in container:
            if (self._operator == ComparisonOperator.EQUAL or
                self._operator == ComparisonOperator.GREATER_THEN_EQUAL or
                self._operator == ComparisonOperator.LESS_THEN_EQUAL) and self._expectedCountOperand == 0:
                    self._explanation = f"Even though no {self._count_description} count is associated with the given process, the given reference was zero"

                    return True
            else:
                self._explanation = f"No {self._count_description} count is associated with the given process"

                return False
    
        if self._operator.perform(container[self._process], self._expectedCountOperand):
            self._explanation = f"The {self._count_description} count for the given process was {self._operator} to/than the given reference."

            return True
        else:
            self._explanation = f"""The {self._count_description} count for the given process was {self._operator.get_inverse()} to/than the given reference.
            \tIn trace data: {container[self._process]}
            \tReference: {self._expectedCountOperand}"""

            return False
        
class SampledProfileCountGTEPredicate(CountForProcessPredicateBase):
    def __init__(self, process: ProcessInfo, expectedCountAtLeast: int):
        super().__init__("sampled profile", "sampled_profile_counts_by_process", process, ComparisonOperator.GREATER_THEN_EQUAL, expectedCountAtLeast)
        
class ContextSwitchCountGTEPredicate(CountForProcessPredicateBase):
    def __init__(self, process: ProcessInfo, expectedCountAtLeast: int):
        super().__init__("context switch", "context_switch_counts_by_process", process, ComparisonOperator.GREATER_THEN_EQUAL, expectedCountAtLeast)

class ReadyThreadCountGTEPredicate(CountForProcessPredicateBase):
    def __init__(self, process: ProcessInfo, expectedCountAtLeast: int):
        super().__init__("ready thread", "ready_thread_counts_by_process", process, ComparisonOperator.GREATER_THEN_EQUAL, expectedCountAtLeast)

class ZeroContextSwitchCountPredicate(CountForProcessPredicateBase):
    def __init__(self, process: ProcessInfo):
        super().__init__("context switch", "context_switch_counts_by_process", process, ComparisonOperator.EQUAL, 0)

class ZeroReadyThreadCountPredicate(CountForProcessPredicateBase):
    def __init__(self, process: ProcessInfo):
        super().__init__("ready thread", "ready_thread_counts_by_process", process, ComparisonOperator.EQUAL, 0)

class _EtlContentExpectation():
    def __init__(self, file_pattern: str, predicates: List[Predicate]):
        self._file_pattern = file_pattern
        self._predicates = predicates
    
    @staticmethod
    def _run_tracedumper(etl_path: str) -> str:
        traceinfodumper_path = os.path.join(TestConfig._testbin_folder_path, "TraceInfoDumper.exe")
        basename = os.path.basename(etl_path)
        json_filename = os.path.splitext(basename)[0]
        result_json_path = os.path.join(tempfile.gettempdir(), json_filename + ".json")
        
        # First try to delete the json file, it might be present from a previous run
        try:
            os.remove(result_json_path)
        except FileNotFoundError:
            pass

        subprocess.run([traceinfodumper_path, etl_path, result_json_path], stdout = subprocess.DEVNULL, stderr = subprocess.DEVNULL).check_returncode()

        return result_json_path
    
    @staticmethod
    def _trace_data_from_etl(etl_path: str):
        result_json_path = _EtlContentExpectation._run_tracedumper(etl_path)
        return TraceData.from_traceinfodumper_file(result_json_path)

    def evaluate(self, filelist):
        for filepath in filelist:
            if not fnmatch(filepath, self._file_pattern):
                continue
            trace_data = _EtlContentExpectation._trace_data_from_etl(filepath)
            for p in self._predicates:
                if not p.evaluate(trace_data):
                    fail(f"ETL content predicate ({type(p)}) is not satisfied: {p.explain()}")

def _get_basic_etl_content_predicates(target_processes: List[ProcessInfo], thread_count_min: int = 1, sampled_profile_min: int = 1):
    '''Returns a list of predicates that most ETL content checks need. Default parameters are "empiric",
    good enough values for checking most cases'''
    predicates = []
    unknown_process = ProcessInfo("", 0) # "Imply" the presence of the "Unknown" process, containing driver images
    driver_images = [ImageInfo("afd.sys"), ImageInfo("beep.sys"), ImageInfo("ntfs.sys")]
    predicates.append(ImageSubsetPredicate(unknown_process, driver_images))

    all_processes = [unknown_process]
    all_processes.extend(target_processes)    
    
    predicates.append(ProcessExactMatchPredicate(all_processes))

    for p in target_processes:
        images = [ImageInfo("ntdll.dll"), ImageInfo("kernel32.dll"), ImageInfo("kernelbase.dll")]
        images.append(ImageInfo(p.image_name))
        
        predicates.append(ImageSubsetPredicate(p, images))
        predicates.append(ThreadCountGTEPredicate(p, thread_count_min))
        predicates.append(SampledProfileCountGTEPredicate(p, sampled_profile_min))
        
    return predicates

def _evaluate_profile_test(filelist: List[str], expectation_list):
    for expectation in expectation_list:
        expectation.evaluate(filelist)

def _evaluate_simple_profile_test(filelist: List[str], etl_file_pattern: str, processes: List[ProcessInfo], additional_expectations: Optional[List] = None):
    """Simple helper function for evaluating profile test cases where we have the usual expectations (only .etl files
    are produced, without context switches, etc.)"""
    etl_file_expectation = _ProfileTestFileExpectation(etl_file_pattern, 1, _ETL_MIN_SIZE)

    # Create some basic predicates for ETL content checking
    predicates = _get_basic_etl_content_predicates(processes)
    for p in processes:
        predicates.append(ZeroContextSwitchCountPredicate(p))
        predicates.append(ZeroReadyThreadCountPredicate(p))
    
    etl_content_expectation = _EtlContentExpectation(etl_file_pattern, predicates)
    expectations = [etl_file_expectation, etl_content_expectation]
    if additional_expectations:
        expectations.extend(additional_expectations)
    
    _evaluate_profile_test(filelist, expectations)

@testcase(suite = _profile_suite, name = "Short lived process", fixture = ProfileTestsFixture())
def test_short_lived_process():
    filelist, processes = _perform_profile_test("DoNothing", fixture.outfile)
    _evaluate_simple_profile_test(filelist, fixture.outfile, processes)

@testcase(suite = _profile_suite, name = "Output directory", fixture = ProfileTestsFixture())
def test_output_directory():
    filelist, processes = _perform_profile_test("DoNothing", fixture.outdir)
    _evaluate_simple_profile_test(filelist, "*.etl", processes)

@testcase(suite = _profile_suite, name = "Minidump", fixture = ProfileTestsFixture())
def test_minidump():
    filelist, processes = _perform_profile_test("DoNothing", fixture.outdir, ["-m"])
    _evaluate_simple_profile_test(filelist, "*.etl", processes, [_ProfileTestFileExpectation("*.dmp", 1, _DMP_MIN_SIZE)])

@testcase(suite = _profile_suite, name = "5 sec CPU burn with context switches", fixture = ProfileTestsFixture())
def test_5s_cpu_burn():
    filelist, processes = _perform_profile_test("BurnCPU5s", fixture.outfile, ["--cswitch"])
    # With the default rate of 1kHz, ideally, we should get 5000 samples, give or take
    etl_content_predicates = _get_basic_etl_content_predicates(processes, sampled_profile_min=4000)
    for p in processes:
        etl_content_predicates.append(ContextSwitchCountGTEPredicate(p, 250))
        etl_content_predicates.append(ReadyThreadCountGTEPredicate(p, 250))

    expectations = [_ProfileTestFileExpectation("*.etl", 1, _ETL_MIN_SIZE), _EtlContentExpectation("*.etl", etl_content_predicates)]
    _evaluate_profile_test(filelist, expectations)

@testcase(suite = _profile_suite, name = "Target specified by name", fixture = ProfileTestsFixture())
def test_target_name():
    filelist, processes = _perform_profile_test("DoNothing", fixture.outdir, options= ProfileTestOptions.TARGET_ID_NAME)
    _evaluate_simple_profile_test(filelist, "*.etl", processes)

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
    filelist, processes =  _perform_profile_test("DoNothing", fixture.outfile, ["--debug"])
    file_expectations = [_ProfileTestFileExpectation(fixture.outfile, 1, _ETL_MIN_SIZE),
                            _ProfileTestFileExpectation("*.raw.etl", 1, _ETL_MIN_SIZE)]
    _evaluate_simple_profile_test(filelist, fixture.outfile, processes, file_expectations)
    
@testcase(suite = _profile_suite, name = "Profiling stopped with CTRL+C", fixture = ProfileTestsFixture())
def test_ctrl_c_stop():
    with PTHProcess() as pth, pth.get_event_for_sync() as e, EtwprofProcess(_PTH_EXE_NAME, fixture.outfile) as etwprof:
        wait_for_etwprof_session()

        # Stop etwprof by "pressing" CTRL+C
        generate_ctrl_c_for_children()

        # Due to CTRL+C, etwprof should finish *before* the profiled process
        etwprof.check()

        # Now it's time for the PTH process finish its thing
        e.signal()
        pth.check()

        # Since PTH is blocked most of the time while etwprof is running, it's very likely that we won't have any
        # sampled profile events, at all
        target_process = ProcessInfo(_PTH_EXE_NAME, pth.pid)
        etl_content_predicates = _get_basic_etl_content_predicates([target_process], sampled_profile_min=0)

        etl_content_predicates.append(ZeroContextSwitchCountPredicate(target_process))
        etl_content_predicates.append(ZeroReadyThreadCountPredicate(target_process))

        expectations = [_ProfileTestFileExpectation("*.etl", 1, _ETL_MIN_SIZE), _EtlContentExpectation("*.etl", etl_content_predicates)]
        _evaluate_profile_test(_list_files_in_dir(fixture.outdir), expectations)

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
    # TODO: modify this test case, so actual filtering is tested
    # Create an ETL file that will serve as an input for testing emulate mode
    with PTHProcess() as pth, pth.get_event_for_sync() as e, EtwprofProcess(_PTH_EXE_NAME, fixture.outfile) as etwprof:
        target_pid = pth.pid

        wait_for_etwprof_session()
        e.signal()
        
        pth.check()
        etwprof.check()

    emulate_input_etl = "".join([fixture.outfile, ".emulate_input.etl"])
    os.rename(fixture.outfile, emulate_input_etl)
    assert_false(os.path.exists(fixture.outfile))   # Check if the rename succeeded

    with EtwprofProcess(target_pid, fixture.outfile, [f"--emulate={emulate_input_etl}"]) as etwprof:
        etwprof.check()

    _evaluate_simple_profile_test([fixture.outfile], fixture.outfile, [ProcessInfo(_PTH_EXE_NAME, target_pid)])

@testcase(suite = _profile_suite, name = "Compression with 7z", fixture = ProfileTestsFixture())
def test_compression_7z():
    filelist, processes = _perform_profile_test("DoNothing", fixture.outdir, ["--compress=7z"])
    _evaluate_profile_test(filelist, [_ProfileTestFileExpectation("*.7z", 1, _ETL_MIN_SIZE)])

@testcase(suite = _profile_suite, name = "Uncompressed", fixture = ProfileTestsFixture())
def test_compression_uncompressed():
    filelist, processes = _perform_profile_test("DoNothing", fixture.outfile, ["--compress=off"])
    _evaluate_simple_profile_test(filelist, fixture.outfile, processes)

@testcase(suite = _profile_suite, name = "Stack caching", fixture = ProfileTestsFixture())
def test_stack_caching():
    if is_win7_or_earlier():
        return  # Not supported on Win 7

    filelist, processes = _perform_profile_test("DoNothing", fixture.outfile, ["--scache"])
    _evaluate_simple_profile_test(filelist, fixture.outfile, processes)

@testcase(suite = _profile_suite, name = "Multiple etwprofs at once", fixture = ProfileTestsFixture())
def test_multiple_etwprofs_at_once():
    if is_win7_or_earlier():
        return  # Only one kernel session at once is supported on Win 7

    with PTHProcess() as pth,    \
         pth.get_event_for_sync() as e,          \
         EtwprofProcess(_PTH_EXE_NAME, fixture.outfile) as etwprof1, \
         EtwprofProcess(_PTH_EXE_NAME, fixture.outdir) as etwprof2:
            wait_for_etwprof_session()
            e.signal()

            pth.check()
            etwprof1.check()
            etwprof2.check()

            target_process = ProcessInfo(_PTH_EXE_NAME, pth.pid)

            # Create some basic predicates for ETL content checking
            predicates = _get_basic_etl_content_predicates([target_process])
            predicates.append(ZeroContextSwitchCountPredicate(target_process))
            predicates.append(ZeroReadyThreadCountPredicate(target_process))
    
            etl_content_expectation = _EtlContentExpectation("*.etl", predicates)
            file_expectation = _ProfileTestFileExpectation("*.etl", 2, _ETL_MIN_SIZE)
            expectations = [file_expectation, etl_content_expectation]
    
            _evaluate_profile_test(_list_files_in_dir(fixture.outdir), expectations)

# TODO
# User providers...