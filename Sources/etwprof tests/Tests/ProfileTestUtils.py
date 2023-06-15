from dataclasses import dataclass
from enum import Enum, Flag, auto
from fnmatch import fnmatch
import json
import subprocess
import shutil
from typing import *
from test_framework import *
from TestUtils import *
import time
from uuid import UUID

ETL_MIN_SIZE = 10 * 1024
DMP_MIN_SIZE = 1 * 1024
PTH_EXE_NAME = "ProfileTestHelper.exe"

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
        super().__init__(os.path.join(TestConfig._testbin_folder_path, PTH_EXE_NAME),
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

def list_files_in_dir(dir) -> List[str]:
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
    stack_counts_by_process_providerid_eventid: dict[ProcessInfo, dict[UUID, dict[int, int]]]
    event_counts_by_process_providerid_eventid: dict[ProcessInfo, dict[UUID, dict[int, int]]]

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
        for ready_thread_count in data["readyThreadCounts"]:
            process_info = processes_by_pid[ready_thread_count["process"]["pid"]]
            ready_thread_counts_by_process[process_info] = ready_thread_count["count"]

        stack_counts_by_process_providerid_eventid: dict[ProcessInfo, dict[UUID, dict[int, int]]] = {}
        for stack_count in data["stackCounts"]:
            process_info = processes_by_pid[stack_count["process"]["pid"]]
            stack_counts_by_process_providerid_eventid[process_info] = {}

            for stack_count_by_provider_and_id in stack_count["stackCountsByProviderAndId"]:
                provider_id = UUID(stack_count_by_provider_and_id["providerId"])
                if provider_id not in stack_counts_by_process_providerid_eventid[process_info]:
                    stack_counts_by_process_providerid_eventid[process_info][provider_id] = {}

                event_id = stack_count_by_provider_and_id["eventId"]
                stack_counts_by_process_providerid_eventid[process_info][provider_id][event_id] = stack_count_by_provider_and_id["count"]

        event_counts_by_process_providerid_eventid: dict[ProcessInfo, dict[UUID, dict[int, int]]] = {}
        for event_count in data["generalEventCounts"]:
            process_info = processes_by_pid[event_count["process"]["pid"]]
            event_counts_by_process_providerid_eventid[process_info] = {}

            for event_count_by_provider_and_id in event_count["generalEventCountsByProviderAndId"]:
                provider_id = UUID(event_count_by_provider_and_id["providerId"])
                if provider_id not in event_counts_by_process_providerid_eventid[process_info]:
                    event_counts_by_process_providerid_eventid[process_info][provider_id] = {}

                event_id = event_count_by_provider_and_id["eventId"]
                event_counts_by_process_providerid_eventid[process_info][provider_id][event_id] = event_count_by_provider_and_id["count"]

        return cls(etl_path,
                   processes_by_pid,
                   images_by_process,
                   threads_by_process,
                   sampled_profile_counts_by_process,
                   context_switch_counts_by_process,
                   ready_thread_counts_by_process,
                   stack_counts_by_process_providerid_eventid,
                   event_counts_by_process_providerid_eventid)
    
unknown_process = ProcessInfo("", 0)

def perform_profile_test(operation, outdir_or_file, extra_args = None, options = ProfileTestOptions.DEFAULT) -> Tuple[List[str], List[ProcessInfo]]:
    "Performs a profiling test case with PTH, and returns a file/folder list, and ProcessInfo list as a result"
    if not extra_args:
        extra_args = []

    # Create helper process, acquire event for synchronizing with the helper process
    with PTHProcess(operation) as pth, pth.get_event_for_sync() as e:
        processinfo = ProcessInfo(PTH_EXE_NAME, pth.pid)
        # Start etwprof, and make it attach to the target process
        with EtwprofProcess(PTH_EXE_NAME if options & ProfileTestOptions.TARGET_ID_NAME else pth.pid,
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
                
            return (list_files_in_dir(outdir), [processinfo])

class ProfileTestFileExpectation():
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
    GREATER_THAN = auto()
    LESS_THAN = auto()
    GREATER_THAN_EQUAL = auto()
    LESS_THAN_EQUAL = auto()

    def perform(self, left, right):
        match self:
            case ComparisonOperator.EQUAL:
                return left == right
            case ComparisonOperator.NOT_EQUAL:
                return left != right
            case ComparisonOperator.GREATER_THAN:
                return left > right
            case ComparisonOperator.LESS_THAN:
                return left < right
            case ComparisonOperator.GREATER_THAN_EQUAL:
                return left >= right
            case ComparisonOperator.LESS_THAN_EQUAL:
                return left <= right
            
    def __str__(self) -> str:
        match self:
            case ComparisonOperator.EQUAL:
                return "="
            case ComparisonOperator.NOT_EQUAL:
                return "!="
            case ComparisonOperator.GREATER_THAN:
                return ">"
            case ComparisonOperator.LESS_THAN:
                return "<"
            case ComparisonOperator.GREATER_THAN_EQUAL:
                return ">="
            case ComparisonOperator.LESS_THAN_EQUAL:
                return "<="
            
    def get_inverse(self) -> "ComparisonOperator":
        match self:
            case ComparisonOperator.EQUAL:
                return ComparisonOperator.NOT_EQUAL
            case ComparisonOperator.NOT_EQUAL:
                return ComparisonOperator.EQUAL
            case ComparisonOperator.GREATER_THAN:
                return ComparisonOperator.LESS_THAN_EQUAL
            case ComparisonOperator.LESS_THAN:
                return ComparisonOperator.GREATER_THAN_EQUAL
            case ComparisonOperator.GREATER_THAN_EQUAL:
                return ComparisonOperator.LESS_THAN
            case ComparisonOperator.LESS_THAN_EQUAL:
                return ComparisonOperator.GREATER_THAN
            
    def has_equality(self) -> bool:
        return self == ComparisonOperator.EQUAL or self == ComparisonOperator.GREATER_THAN_EQUAL or self == ComparisonOperator.LESS_THAN_EQUAL

class CountForProcessPredicateBase(Predicate):
    def __init__(self, count_description: str, trace_data_count_container_name: str, process: ProcessInfo, operator: ComparisonOperator, expected_count_operand: int):
        self._count_description = count_description
        self._trace_data_count_container_name = trace_data_count_container_name
        self._process = process

        self._operator = operator
        self._expected_count_operand = expected_count_operand

    def evaluate(self, trace_data: TraceData) -> bool:
        container = getattr(trace_data, self._trace_data_count_container_name)
        if self._process not in container:
            if self._operator.has_equality() and self._expected_count_operand == 0:
                    self._explanation = f"Even though no {self._count_description} count is associated with the given process, the given reference was zero"

                    return True
            else:
                self._explanation = f"No {self._count_description} count is associated with the given process"

                return False
    
        if self._operator.perform(container[self._process], self._expected_count_operand):
            self._explanation = f"The {self._count_description} count for the given process was {self._operator} to/than the given reference."

            return True
        else:
            self._explanation = f"""The {self._count_description} count for the given process was {self._operator.get_inverse()} to/than the given reference.
            \tIn trace data: {container[self._process]}
            \tReference: {self._expected_count_operand}"""

            return False
        
class SampledProfileCountGTEPredicate(CountForProcessPredicateBase):
    def __init__(self, process: ProcessInfo, expectedCountAtLeast: int):
        super().__init__("sampled profile", "sampled_profile_counts_by_process", process, ComparisonOperator.GREATER_THAN_EQUAL, expectedCountAtLeast)
        
class ContextSwitchCountGTEPredicate(CountForProcessPredicateBase):
    def __init__(self, process: ProcessInfo, expectedCountAtLeast: int):
        super().__init__("context switch", "context_switch_counts_by_process", process, ComparisonOperator.GREATER_THAN_EQUAL, expectedCountAtLeast)

class ReadyThreadCountGTEPredicate(CountForProcessPredicateBase):
    def __init__(self, process: ProcessInfo, expectedCountAtLeast: int):
        super().__init__("ready thread", "ready_thread_counts_by_process", process, ComparisonOperator.GREATER_THAN_EQUAL, expectedCountAtLeast)

class ZeroContextSwitchCountPredicate(CountForProcessPredicateBase):
    def __init__(self, process: ProcessInfo):
        super().__init__("context switch", "context_switch_counts_by_process", process, ComparisonOperator.EQUAL, 0)

class ZeroReadyThreadCountPredicate(CountForProcessPredicateBase):
    def __init__(self, process: ProcessInfo):
        super().__init__("ready thread", "ready_thread_counts_by_process", process, ComparisonOperator.EQUAL, 0)

class StackCountByProviderAndEventIdGTEPredicate(Predicate):
    def __init__(self, process: ProcessInfo, expected_counts_at_least: dict[tuple[UUID, int], int]):
        self._process = process
        self._expected_counts_at_least = expected_counts_at_least

    def evaluate(self, trace_data: TraceData) -> bool:
        if self._process not in trace_data.stack_counts_by_process_providerid_eventid:
            if all(count == 0 for count in self._expected_counts_at_least) == 0:
                self._explanation = "Even though no stack events are associated with the given process, all given references were zero"

                return True
            else:
                self._explanation = "No stack events are associated with the given process"

                return False
        
        stack_counts_by_providers = trace_data.stack_counts_by_process_providerid_eventid[self._process]

        # Check if we have stack events for providers and/or event ids that we do not expect
        for provider in stack_counts_by_providers:
            expected_providers = [provider for (provider, _) in self._expected_counts_at_least]
            if provider not in expected_providers:
                self._explanation = f'Unexpected stack events found for provider "{provider}"'

                return False
                
            for event_id in stack_counts_by_providers[provider]:
                if (provider, event_id) not in self._expected_counts_at_least:
                    self._explanation = f'Unexpected stack events found for event id "{event_id}" of provider "{provider}"'

                    return False

        for (provider_id, event_id), expected_count_at_least in self._expected_counts_at_least.items():
            if provider_id not in stack_counts_by_providers and expected_count_at_least == 0:
                continue

            if event_id not in stack_counts_by_providers[provider_id] and expected_count_at_least == 0:
                continue

            if provider_id not in stack_counts_by_providers:
                self._explanation = f'No stack events are associated with provider "{provider_id}"'

                return False
            
            if event_id not in stack_counts_by_providers[provider_id]:
                self._explanation = f'No stack events are associated with event id of "{event_id}" for provider "{provider_id}"'

                return False
            
            if stack_counts_by_providers[provider_id][event_id] < expected_count_at_least:
                self._explanation = f'''The stack event count for the provider "{provider_id}" and event id "{event_id}" was < than the given reference.
                                     \tIn trace data: {stack_counts_by_providers[provider_id][event_id]}
                                     \tReference: {expected_count_at_least}'''

                return False

        self._explanation = "The stack counts for each provider and event id were >= than the given references"

        return True
    
class GeneralEventCountByProviderAndEventIdSubsetPredicate(Predicate):
    def __init__(self, process: ProcessInfo, expected_counts: dict[tuple[UUID, int], Tuple[ComparisonOperator, int]]):
        self._process = process
        self._expected_counts = expected_counts

    def evaluate(self, trace_data: TraceData) -> bool:
        if self._process not in trace_data.event_counts_by_process_providerid_eventid:
            if all(operator.has_equality() and count == 0 for (operator, count) in self._expected_counts.values()) == 0:
                self._explanation = "Even though no events are associated with the given process, all given references were zero"

                return True
            else:
                self._explanation = "No events are associated with the given process"

                return False
        
        event_counts_by_providers = trace_data.event_counts_by_process_providerid_eventid[self._process]

        for (provider_id, event_id), (operator, expected_count) in self._expected_counts.items():
            if provider_id not in event_counts_by_providers:
                if operator.has_equality() and expected_count == 0:
                    continue
                else:
                    self._explanation = f'No events are associated with provider "{provider_id}" for the given process'

                    return False
            
            if event_id not in event_counts_by_providers[provider_id]:
                if operator.has_equality() and expected_count == 0:
                    continue
                else:
                    self._explanation = f'No events are associated with event id of "{event_id}" for provider "{provider_id}" for the given process'

                    return False
            
            if not operator.perform(event_counts_by_providers[provider_id][event_id], expected_count):
                self._explanation = f'''The event count for provider "{provider_id}" and event id "{event_id}" was {operator.get_inverse()} to/than the given reference.
                                     \tIn trace data: {event_counts_by_providers[provider_id][event_id]}
                                     \tReference: {expected_count}'''

                return False

        self._explanation = f"The event counts for each provider and event id were in line with the given references"

        return True
    
# Some common provider GUIDs and event IDs
PERF_INFO_GUID = UUID("ce1dbfb4-137e-4da6-87b0-3f59aa102cbc")
PERF_INFO_SAMPLED_PROFILE_ID = 46

THREAD_GUID = UUID("3d6fa8d1-fe05-11d0-9dda-00c04fd7ba7c")
THREAD_CSWITCH_ID = 36
THREAD_READY_THREAD_ID = 50

STACK_WALK_GUID = UUID("def2fe46-7bd6-4b80-bd94-f57fe20d0ce3")
STACK_WALK_EVENT = 32
STACK_WALK_RUNDOWN_DEFINITION = 36
STACK_WALK_REFERENCE_KERNEL = 37
STACK_WALK_REFERENCE_USER = 38

class EtlContentExpectation():
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
        result_json_path = EtlContentExpectation._run_tracedumper(etl_path)
        return TraceData.from_traceinfodumper_file(result_json_path)

    def evaluate(self, filelist):
        for filepath in filelist:
            if not fnmatch(filepath, self._file_pattern):
                continue
            trace_data = EtlContentExpectation._trace_data_from_etl(filepath)
            for p in self._predicates:
                if not p.evaluate(trace_data):
                    fail(f"ETL content predicate ({type(p)}) is not satisfied: {p.explain()}")

def get_basic_etl_content_predicates(target_processes: List[ProcessInfo],
                                      thread_count_min: int = 1,
                                      sampled_profile_min: int = 1,
                                      additional_predicates: Optional[List[Predicate]] = None,
                                      stack_count_predicate: Optional[StackCountByProviderAndEventIdGTEPredicate] = None):
    '''Returns a list of predicates that most ETL content checks need. Default parameters are "empiric",
    good enough values for checking most cases'''
    predicates = []
    # "Imply" the presence of the "Unknown" process, containing driver images
    global unknown_process
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

        if not stack_count_predicate:
            # We expect call stacks for sampled profiles
            expected_stack_counts = {
                (PERF_INFO_GUID, PERF_INFO_SAMPLED_PROFILE_ID): sampled_profile_min
            }
            predicates.append(StackCountByProviderAndEventIdGTEPredicate(p, expected_stack_counts))
        else:
            predicates.append(stack_count_predicate)

    if additional_predicates:
        predicates.extend(additional_predicates)
        
    return predicates

def evaluate_profile_test(filelist: List[str], expectation_list):
    for expectation in expectation_list:
        expectation.evaluate(filelist)

def evaluate_simple_profile_test(filelist: List[str],
                                 etl_file_pattern: str,
                                 processes: List[ProcessInfo],
                                 additional_etl_content_predicates: Optional[List[Predicate]] = None,
                                 additional_expectations: Optional[List] = None,
                                 sampled_profile_min: int = 1):
    """Simple helper function for evaluating profile test cases where we have the usual expectations (only .etl files
    are produced, without context switches, etc.)"""
    etl_file_expectation = ProfileTestFileExpectation(etl_file_pattern, 1, ETL_MIN_SIZE)

    # Create some basic predicates for ETL content checking
    etl_content_predicates = get_basic_etl_content_predicates(processes, sampled_profile_min=sampled_profile_min)
    for p in processes:
        etl_content_predicates.append(ZeroContextSwitchCountPredicate(p))
        etl_content_predicates.append(ZeroReadyThreadCountPredicate(p))

    if additional_etl_content_predicates:
        etl_content_predicates.extend(additional_etl_content_predicates)
    
    etl_content_expectation = EtlContentExpectation(etl_file_pattern, etl_content_predicates)
    expectations = [etl_file_expectation, etl_content_expectation]
    if additional_expectations:
        expectations.extend(additional_expectations)
    
    evaluate_profile_test(filelist, expectations)

# GUIDs and opcodes for user provider testing
MB_A_GUID = UUID("382b5c97-a095-4f52-bbb6-f3b011b33563")
MB_A_A = 0
MB_A_B = 1
MB_A_C = 2

MB_B_GUID = UUID("465646f1-41d0-4bd2-8173-dbf7ff6cc9e2")
MB_B_A = 0
MB_B_B = 1
MB_B_C = 2

TL_A_GUID = UUID("11b83188-f8a1-5301-5690-e964fd71beba")
TL_A_A = 0
TL_A_B = 1

TL_B_GUID = UUID("7ae7cc76-bdaf-5e8a-1b73-d85398dbadd3")
TL_B_A = 0
TL_B_B = 1

def _compose_user_provider_event_counts_impl(count:int):
    result = {}
    result[(MB_A_GUID, MB_A_A)] = (ComparisonOperator.EQUAL, count)
    result[(MB_A_GUID, MB_A_B)] = (ComparisonOperator.EQUAL, count)
    result[(MB_A_GUID, MB_A_C)] = (ComparisonOperator.EQUAL, count)

    result[(MB_B_GUID, MB_B_A)] = (ComparisonOperator.EQUAL, count)
    result[(MB_B_GUID, MB_B_B)] = (ComparisonOperator.EQUAL, count)
    result[(MB_B_GUID, MB_B_C)] = (ComparisonOperator.EQUAL, count)

    result[(TL_A_GUID, TL_A_A)] = (ComparisonOperator.EQUAL, count)
    result[(TL_A_GUID, TL_A_B)] = (ComparisonOperator.EQUAL, count)

    result[(TL_B_GUID, TL_B_A)] = (ComparisonOperator.EQUAL, count)
    result[(TL_B_GUID, TL_B_B)] = (ComparisonOperator.EQUAL, count)

    return result

def get_empty_user_provider_event_counts():
    return _compose_user_provider_event_counts_impl (0)

def get_all_1_user_provider_event_counts():
    return _compose_user_provider_event_counts_impl (1)

def get_enable_string_for_all_providers(kw_bitmask_string: Optional[str] = None, max_level: Optional[int] = None) -> str:
    result = "--enable="
    for p in [MB_A_GUID, MB_B_GUID, TL_A_GUID, TL_B_GUID]:
        result = "".join([result, str(p), ":"])

        if kw_bitmask_string:
            result += kw_bitmask_string

        result += ":"

        if max_level:
            result += str(max_level)

        result += "+"

    result = result[0:-1] # Strip unnecessary '+' from the last iteration

    return result