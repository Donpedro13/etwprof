"Tests for the profile command"
from ProfileTestUtils import *
import os
from test_framework import *
from TestUtils import *
from typing import *

_profile_suite = TestSuite("Profile tests")

@testcase(suite = _profile_suite, name = "Short lived process", fixture = ProfileTestsFixture())
def test_short_lived_process():
    filelist, processes = perform_profile_test("DoNothing", fixture.outfile)
    evaluate_simple_profile_test(filelist, fixture.outfile, processes)

@testcase(suite = _profile_suite, name = "Output directory", fixture = ProfileTestsFixture())
def test_output_directory():
    filelist, processes = perform_profile_test("DoNothing", fixture.outdir)
    evaluate_simple_profile_test(filelist, "*.etl", processes)

@testcase(suite = _profile_suite, name = "Minidump", fixture = ProfileTestsFixture())
def test_minidump():
    filelist, processes = perform_profile_test("DoNothing", fixture.outdir, ["-m"])
    evaluate_simple_profile_test(filelist, "*.etl", processes, None, [ProfileTestFileExpectation("*.dmp", 1, DMP_MIN_SIZE)])

@testcase(suite = _profile_suite, name = "5 sec CPU burn with context switches", fixture = ProfileTestsFixture())
def test_5s_cpu_burn():
    filelist, processes = perform_profile_test("BurnCPU5s", fixture.outfile, ["--cswitch"])
    assert(len(processes) == 1)
    process = processes[0]

    # With the default rate of 1kHz, ideally, we should get 5000 samples, give or take
    SAMPLED_PROFILE_MIN = 4000
    CONTEXT_SWITCH_MIN = 150
    READY_THREAD_MIN = 5
    # I wasn't able to figure out why, but even for xperf's traces, only about half ot the ready thread and cswitch
    # events have associated call stack events
    expected_stack_counts = {
            (PERF_INFO_GUID, PERF_INFO_SAMPLED_PROFILE_ID): SAMPLED_PROFILE_MIN,
            (THREAD_GUID, THREAD_READY_THREAD_ID): READY_THREAD_MIN / 3,
            (THREAD_GUID, THREAD_CSWITCH_ID): CONTEXT_SWITCH_MIN / 3
        }
    stack_count_predicate=StackCountByProviderAndEventIdGTEPredicate(process, expected_stack_counts)
    etl_content_predicates = get_basic_etl_content_predicates([process],
                                                               sampled_profile_min=SAMPLED_PROFILE_MIN,
                                                               stack_count_predicate=stack_count_predicate)
    for p in processes:
        etl_content_predicates.append(ContextSwitchCountGTEPredicate(p, CONTEXT_SWITCH_MIN))
        etl_content_predicates.append(ReadyThreadCountGTEPredicate(p, READY_THREAD_MIN))

    expectations = [ProfileTestFileExpectation("*.etl", 1, ETL_MIN_SIZE), EtlContentExpectation("*.etl", etl_content_predicates)]
    evaluate_profile_test(filelist, expectations)

@testcase(suite = _profile_suite, name = "100 threads", fixture = ProfileTestsFixture())
def test_100_threads():
    filelist, processes = perform_profile_test("Create100Threads", fixture.outfile)
    # We expect at least 101 threads: 100 "background" threads, and one main thread
    etl_content_predicates = get_basic_etl_content_predicates(processes, thread_count_min=101)

    expectations = [ProfileTestFileExpectation("*.etl", 1, ETL_MIN_SIZE), EtlContentExpectation("*.etl", etl_content_predicates)]
    evaluate_profile_test(filelist, expectations)

@testcase(suite = _profile_suite, name = "Target specified by name", fixture = ProfileTestsFixture())
def test_target_name():
    filelist, processes = perform_profile_test("DoNothing", fixture.outdir, options= ProfileTestOptions.TARGET_ID_NAME)
    evaluate_simple_profile_test(filelist, "*.etl", processes)

@testcase(suite = _profile_suite, name = "Multiple target processes (few) w/ dumps", fixture = ProfileTestsFixture())
def test_multiple_target_processes_few_w_dumps():
    N_PROCESSES = 5
    pths = launch_pth_processes(N_PROCESSES)

    with EtwprofProcess.attach_to_profilee(PTH_EXE_NAME, fixture.outfile, ["-m"]) as etwprof:
        wait_for_etwprof_session()

        for event in pths.values():
            event.signal()

        etwprof.wait()

        for p in pths:
            p.check()

    processInfos = [ProcessInfo(PTH_EXE_NAME, p.pid) for p in pths]

    etl_content_predicates = get_basic_etl_content_predicates(processInfos)

    for p in processInfos:
        etl_content_predicates.append(ZeroContextSwitchCountPredicate(p))
        etl_content_predicates.append(ZeroReadyThreadCountPredicate(p))

    expectations = [ProfileTestFileExpectation("*.etl", 1, ETL_MIN_SIZE),
                    ProfileTestFileExpectation("*.dmp", N_PROCESSES, DMP_MIN_SIZE),
                    EtlContentExpectation("*.etl", etl_content_predicates)]
    evaluate_profile_test(list_files_in_dir(fixture.outdir), expectations)

@testcase(suite = _profile_suite, name = "Multiple target processes (many)", fixture = ProfileTestsFixture())
def test_multiple_target_processes_many():
    N_PROCESSES = 128
    pths = launch_pth_processes(N_PROCESSES)

    with EtwprofProcess.attach_to_profilee(PTH_EXE_NAME, fixture.outfile) as etwprof:
        wait_for_etwprof_session()

        for event in pths.values():
            event.signal()

        etwprof.wait()

        for p in pths:
            p.check()

    processInfos = [ProcessInfo(PTH_EXE_NAME, p.pid) for p in pths]
    evaluate_simple_profile_test(list_files_in_dir(fixture.outdir), "*.etl", processInfos)

@testcase(suite = _profile_suite, name = "Multiple target processes (many), various operations", fixture = ProfileTestsFixture())
def test_multiple_target_processes_many_various_ops():
    from itertools import chain

    N_PROCESSES_DO_NOTHING = 64
    N_PROCESSES_BURN_CPU_5_SEC = 4
    N_PROCESSES_WAIT_1_SEC = 4
    N_PROCESSES_WAIT_5_SEC = 4

    pths_do_nothing = launch_pth_processes(N_PROCESSES_DO_NOTHING)
    pths_5_sec_cpu_burn = launch_pth_processes(N_PROCESSES_BURN_CPU_5_SEC, "BurnCPU5s")
    pths_wait_1_sec = launch_pth_processes(N_PROCESSES_WAIT_1_SEC, "Wait1Sec")
    pths_wait_5_sec = launch_pth_processes(N_PROCESSES_WAIT_5_SEC, "Wait5Sec")

    pths_all_processes = lambda : chain(pths_do_nothing, pths_5_sec_cpu_burn, pths_wait_1_sec, pths_wait_5_sec)
    pths_all_events = lambda : chain(pths_do_nothing.values(), pths_5_sec_cpu_burn.values(), pths_wait_1_sec.values(), pths_wait_5_sec.values())

    with EtwprofProcess.attach_to_profilee(PTH_EXE_NAME, fixture.outfile) as etwprof:
        wait_for_etwprof_session()

        for event in pths_all_events():
            event.signal()

        etwprof.wait()

        for p in pths_all_processes():
            p.check()

    # The different operations require different ETL content predicates, so let's construct them
    process_infos_do_nothing = [ProcessInfo(PTH_EXE_NAME, p.pid) for p in pths_do_nothing]
    process_infos_5_sec_cpu_burn = [ProcessInfo(PTH_EXE_NAME, p.pid) for p in pths_5_sec_cpu_burn]
    process_infos_waits = [ProcessInfo(PTH_EXE_NAME, p.pid) for p in chain(pths_wait_1_sec, pths_wait_5_sec)]

    predicates_do_nothing = get_basic_etl_content_predicates(process_infos_do_nothing, custom_process_predicates=True)
    predicates_5_sec_cpu_burn = get_basic_etl_content_predicates(process_infos_5_sec_cpu_burn, custom_process_predicates=True)
    predicates_waits = get_basic_etl_content_predicates(process_infos_waits, sampled_profile_min=0, custom_process_predicates=True)

    predicates_all = [p for p in chain(predicates_do_nothing, predicates_5_sec_cpu_burn, predicates_waits)]
    
    process_predicate = ProcessExactMatchPredicate(process_infos_do_nothing +
                                                   process_infos_5_sec_cpu_burn +
                                                   process_infos_waits + [unknown_process])
    predicates_all.extend(get_basic_predicates_for_unknown_process())
    predicates_all.append(process_predicate)

    expectations = [ProfileTestFileExpectation("*.etl", 1, ETL_MIN_SIZE),
                    EtlContentExpectation("*.etl", predicates_all)]

    evaluate_profile_test(list_files_in_dir(fixture.outdir), expectations)

def _test_children_impl(count:int, operation = None, waitforchilden = True):
    if not operation:
        operation = f"CreateChildProcess{count}"

    extra_args = ["--children"]
    if waitforchilden:
        extra_args.append("--waitchildren")

    filelist, processes = perform_profile_test(operation, fixture.outdir, extra_args)
    expected_process_counts = { unknown_process.image_name : 1, PTH_EXE_NAME: 1 + count}
    process_predicate = ProcessSubsetAndThenCountsPredicate(processes, expected_process_counts)

    # Unfortunately, we don't do the usual ETL content check for child processes, as that would require knowing their
    #   PIDs in advance (which we don't)
    etl_content_predicates = get_basic_etl_content_predicates (processes, custom_process_predicates=True)
    etl_content_predicates.append (process_predicate)

    expectations = [EtlContentExpectation ("*etl", etl_content_predicates),
                    ProfileTestFileExpectation("*.etl", 1, ETL_MIN_SIZE)]

    evaluate_profile_test(filelist, expectations)

@testcase(suite = _profile_suite, name = "Child processes (few)", fixture = ProfileTestsFixture())
def test_child_processes_few():
    _test_children_impl(1)

@testcase(suite = _profile_suite, name = "Child processes (many)", fixture = ProfileTestsFixture())
def test_child_processes_many():
    _test_children_impl(128)

@testcase(suite = _profile_suite, name = "Child processes (cascading)", fixture = ProfileTestsFixture())
def test_child_processes_cascading():
    _test_children_impl(5, "CreateChildProcessCascade5")

@testcase(suite = _profile_suite, name = "Child processes (profiling not waiting for grandchildren)", fixture = ProfileTestsFixture())
def test_child_processes_outlives_parent():
    # TODO: improve test oracle here, no real chance of detecting if a failure happens in the current state
    _test_children_impl(1, "CreateChildProcess1OutlivesParent", False)

def _test_children_process_tree_impl(tree_operations: Iterable[str], process_counts: dict[str, int], trace_children = True):
    from contextlib import ExitStack

    pths: List[PTHProcess] = []
    pth_infos = []
    for op in tree_operations:
        pths.append(PTHProcess(op))
        pth_infos.append(ProcessInfo(PTH_EXE_NAME, pths[-1].pid))

    # When only one process tree is to be constructed (1 launched PTH process), specifying the target by PID is a
    #   "nice touch", so child processes will be "picked up" by the child process tracing functionality, and not the
    #   "multiple process" support that works based on process name
    target = pths[0].pid if len(pths) == 1 else PTH_EXE_NAME

    with ExitStack() as stack, EtwprofProcess.attach_to_profilee(target, fixture.outdir, ["--children", "--waitchildren"] if trace_children else []) as etwprof:
        for p in pths:
            stack.enter_context(p)

        wait_for_etwprof_session()

        # Create process trees
        pth_events = [] # We need to keep them around until all PTHs exit, to avoid race conditions
        for p in pths:
            pth_events.append(p.get_event_for_sync())
            pth_events[-1].signal()

        # At this point, all PTH child processes should wait for their own event and the "global cancel event" (which we
        #   did not signal yet). Let's signal it, so all processes will exit.
        cancel_event = get_PTH_global_cancel_event()
        cancel_event.signal()

        for p in pths:
            p.check()

        etwprof.check()
    
    filelist = list_files_in_dir(fixture.outdir)

    process_predicate = ProcessSubsetAndThenCountsPredicate(pth_infos, process_counts)
    etl_content_predicates = get_basic_etl_content_predicates(pth_infos, custom_process_predicates=True)
    etl_content_predicates.append (process_predicate)

    expectations = [EtlContentExpectation ("*etl", etl_content_predicates),
                    ProfileTestFileExpectation("*.etl", 1, ETL_MIN_SIZE)]

    evaluate_profile_test(filelist, expectations)

@testcase(suite = _profile_suite, name = "Child processes (PTH process tree)", fixture = ProfileTestsFixture())
def test_child_processes_pth_process_tree():
    expected_process_counts = { unknown_process.image_name : 1, PTH_EXE_NAME: 6}
    _test_children_process_tree_impl(["CreateProcessTreeWith5PTHs"], expected_process_counts)

@testcase(suite = _profile_suite, name = "Child processes (multiple PTH process trees)", fixture = ProfileTestsFixture())
def test_child_processes_pth_multiple_process_trees():
    expected_process_counts = { unknown_process.image_name : 1, PTH_EXE_NAME: 12}
    _test_children_process_tree_impl(["CreateProcessTreeWith5PTHs", "CreateProcessTreeWith5PTHs"], expected_process_counts)

@testcase(suite = _profile_suite, name = "Child processes (mixed process tree)", fixture = ProfileTestsFixture())
def test_child_processes_mixed_process_tree():
    expected_process_counts = { unknown_process.image_name : 1, PTH_EXE_NAME: 6, "cmd.exe" : 1, "conhost.exe" : 2}
    _test_children_process_tree_impl(["CreateProcess5TreeMixed"], expected_process_counts)

@testcase(suite = _profile_suite, name = "Child processes (no child tracing)", fixture = ProfileTestsFixture())
def test_child_processes_no_child_tracing():
    expected_process_counts = { unknown_process.image_name : 1, PTH_EXE_NAME: 1}
    _test_children_process_tree_impl(["CreateProcess5TreeMixed"], expected_process_counts, trace_children=False)

def _test_profile_start_process_impl(process_path: str, args: Iterable[str], outdir_or_file: str, process_counts: dict[str, int], etwprof_extra_args: Optional[Iterable[str]] = None):
    with EtwprofProcess.start_profilee(outdir_or_file, process_path, args, etwprof_extra_args) as etwprof:
        etwprof.check()
    
    filelist = list_files_in_dir(fixture.outdir)

    process_predicate = ProcessSubsetAndThenCountsPredicate([], process_counts)
    etl_content_predicates = get_basic_etl_content_predicates([], custom_process_predicates=True)
    etl_content_predicates.append (process_predicate)

    expectations = [EtlContentExpectation ("*etl", etl_content_predicates),
                    ProfileTestFileExpectation("*.etl", 1, ETL_MIN_SIZE)]

    evaluate_profile_test(filelist, expectations)

@testcase(suite = _profile_suite, name = "Profilee started (no args)", fixture = ProfileTestsFixture())
def test_profilee_started_no_args():
    expected_process_counts = { unknown_process.image_name : 1, PTH_EXE_NAME: 1}
    _test_profile_start_process_impl(get_pth_path(), [], fixture.outfile, expected_process_counts)

@testcase(suite = _profile_suite, name = "Profilee started (multiple args)", fixture = ProfileTestsFixture())
def test_profilee_started_multiple_args():
    expected_process_counts = { unknown_process.image_name : 1, "cmd.exe": 1}
    _test_profile_start_process_impl(get_cmd_path(), ["/C", "exit"], fixture.outfile, expected_process_counts)

@testcase(suite = _profile_suite, name = "Profilee started (w/ child processes)", fixture = ProfileTestsFixture())
def test_profilee_started_w_child_processes():
    expected_process_counts = { unknown_process.image_name : 1, "cmd.exe": 2, "conhost.exe": 1}
    _test_profile_start_process_impl(get_cmd_path(), ["/C", "cmd", "/C", "exit"], fixture.outfile, expected_process_counts, ["--children"])

@testcase(suite = _profile_suite, name = "Profilee started (arg with space)", fixture = ProfileTestsFixture())
def test_profilee_started_arg_with_space():
    """This test case is tricky: we echo a string with cmd containing spaces. The output is piped into find.exe which
       looks for spaces. If it finds one, another cmd.exe instance is launched."""
    

    expected_process_counts = { unknown_process.image_name : 1,
                               "cmd.exe" : 3,   # It's 3 because piping seems to make the echo command run in its own cmd instance
                               "conhost.exe" : 1,
                               "find.exe" : 1}
    _test_profile_start_process_impl(get_cmd_path(),
                                     ["/C echo \"Cheeky & exit\"| find \" \" >nul & if errorlevel 1 (echo No space found) else (cmd /C exit)"],
                                     fixture.outfile,
                                     expected_process_counts,
                                     ["--children"])
    
@testcase(suite = _profile_suite, name = "Profilee started (parse error)", fixture = ProfileTestsFixture())
def test_profilee_started_parse_error():
    with EtwprofProcess(["profile", '-o="%TMP%\\out -- file.etl"', f"-- {get_cmd_path()} /C exit"]) as etwprof:
        etwprof.wait()

        assert_neq(etwprof.exitcode, 0)

@testcase(suite = _profile_suite, name = "Unknown process (name)", fixture = ProfileTestsFixture())
def test_unknown_process_name():
    with EtwprofProcess.attach_to_profilee("ThisSurelyDoesNotExist.exe", fixture.outfile) as etwprof:
        etwprof.wait()

        assert_neq(etwprof.exitcode, 0)

@testcase(suite = _profile_suite, name = "Unknown process (PID)", fixture = ProfileTestsFixture())
def test_unknown_process_pid():
    PID_MAX = 0xFFFFFFFF
    with EtwprofProcess.attach_to_profilee(PID_MAX, fixture.outfile) as etwprof:
        etwprof.wait()

        assert_neq(etwprof.exitcode, 0)

@testcase(suite = _profile_suite, name = "Debug mode", fixture = ProfileTestsFixture())
def test_debug_mode():
    filelist, processes =  perform_profile_test("DoNothing", fixture.outfile, ["--debug"])
    file_expectations = [ProfileTestFileExpectation(fixture.outfile, 1, ETL_MIN_SIZE),
                            ProfileTestFileExpectation("*.raw.etl", 1, ETL_MIN_SIZE)]
    evaluate_simple_profile_test(filelist, fixture.outfile, processes, None, file_expectations)
    
@testcase(suite = _profile_suite, name = "Profiling stopped with CTRL+C", fixture = ProfileTestsFixture())
def test_ctrl_c_stop():
    with PTHProcess() as pth, pth.get_event_for_sync() as e, EtwprofProcess.attach_to_profilee(PTH_EXE_NAME, fixture.outfile) as etwprof:
        wait_for_etwprof_session()

        # Stop etwprof by "pressing" CTRL+C
        generate_ctrl_c_for_children()

        # Due to CTRL+C, etwprof should finish *before* the profiled process
        etwprof.check()

        # Now it's time for the PTH process to finish its thing
        e.signal()
        pth.check()

        # Since PTH is blocked most of the time while etwprof is running, it's very likely that we won't have any
        # sampled profile events, at all
        target_process = ProcessInfo(PTH_EXE_NAME, pth.pid)
        etl_content_predicates = get_basic_etl_content_predicates([target_process],
                                                                  sampled_profile_min=0,
                                                                  process_lifetime_matcher = PROCESS_LIFETIME_UNKNOWN)

        etl_content_predicates.append(ZeroContextSwitchCountPredicate(target_process))
        etl_content_predicates.append(ZeroReadyThreadCountPredicate(target_process))

        expectations = [ProfileTestFileExpectation("*.etl", 1, ETL_MIN_SIZE), EtlContentExpectation("*.etl", etl_content_predicates)]
        evaluate_profile_test(list_files_in_dir(fixture.outdir), expectations)

@testcase(suite = _profile_suite, name = "ETW session killed", fixture = ProfileTestsFixture())
def test_etw_session_killed():
    # Start PTH (since we never signal its event, it will wait forever), then we pull the rug from under etwprof
    with PTHProcess() as pth, EtwprofProcess.attach_to_profilee(PTH_EXE_NAME, fixture.outfile) as etwprof:
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
    with PTHProcess() as pth, pth.get_event_for_sync() as e, EtwprofProcess.attach_to_profilee(PTH_EXE_NAME, fixture.outfile) as etwprof:
        target_pid = pth.pid

        wait_for_etwprof_session()
        e.signal()
        
        pth.check()
        etwprof.check()

    emulate_input_etl = "".join([fixture.outfile, ".emulate_input.etl"])
    os.rename(fixture.outfile, emulate_input_etl)
    assert_false(os.path.exists(fixture.outfile))   # Check if the rename succeeded

    with EtwprofProcess.attach_to_profilee(target_pid, fixture.outfile, [f"--emulate={emulate_input_etl}"]) as etwprof:
        etwprof.check()

    evaluate_simple_profile_test([fixture.outfile], fixture.outfile, [ProcessInfo(PTH_EXE_NAME, target_pid)])

@testcase(suite = _profile_suite, name = "Compression with 7z", fixture = ProfileTestsFixture())
def test_compression_7z():
    filelist, processes = perform_profile_test("DoNothing", fixture.outdir, ["--compress=7z"])
    evaluate_profile_test(filelist, [ProfileTestFileExpectation("*.7z", 1, ETL_MIN_SIZE)])

@testcase(suite = _profile_suite, name = "Uncompressed", fixture = ProfileTestsFixture())
def test_compression_uncompressed():
    filelist, processes = perform_profile_test("DoNothing", fixture.outfile, ["--compress=off"])
    evaluate_simple_profile_test(filelist, fixture.outfile, processes)

@testcase(suite = _profile_suite, name = "Stack caching", fixture = ProfileTestsFixture())
def test_stack_caching():
    filelist, processes = perform_profile_test("DoNothing", fixture.outfile, ["--scache"])
    assert(len(processes) == 1)
    process = processes[0]

    # We expect stack cache events, but not "regular" stack walk ones. Cache events are "global", so they will be
    #   attributed to the "Unknown process". Since the profiled process is short-lived, there is no guarantee that
    #   we will get each "type" of reference.
    expected_cache_event_counts = {
        (STACK_WALK_GUID, STACK_WALK_REFERENCE_USER): (ComparisonOperator.GREATER_THAN_EQUAL, 0),
        (STACK_WALK_GUID, STACK_WALK_REFERENCE_KERNEL): (ComparisonOperator.GREATER_THAN_EQUAL, 0),
        (STACK_WALK_GUID, STACK_WALK_RUNDOWN_DEFINITION): (ComparisonOperator.GREATER_THAN_EQUAL, 1),
    }
    expected_stack_walk_event_counts = {
        (STACK_WALK_GUID, STACK_WALK_EVENT): (ComparisonOperator.EQUAL, 0),
    }

    global unknown_process
    stack_etl_content_predicates = [
        GeneralEventCountByProviderAndEventIdSubsetPredicate(unknown_process, expected_cache_event_counts),
        GeneralEventCountByProviderAndEventIdSubsetPredicate(process, expected_stack_walk_event_counts)]
    evaluate_simple_profile_test(filelist,
                                 fixture.outfile,
                                 processes,
                                 additional_etl_content_predicates=stack_etl_content_predicates)

@testcase(suite = _profile_suite, name = "Multiple etwprofs at once", fixture = ProfileTestsFixture())
def test_multiple_etwprofs_at_once():
    with PTHProcess() as pth,    \
         pth.get_event_for_sync() as e,          \
         EtwprofProcess.attach_to_profilee(PTH_EXE_NAME, fixture.outfile) as etwprof1, \
         EtwprofProcess.attach_to_profilee(PTH_EXE_NAME, fixture.outdir) as etwprof2:
            wait_for_etwprof_session()
            e.signal()

            pth.check()
            etwprof1.check()
            etwprof2.check()

            target_process = ProcessInfo(PTH_EXE_NAME, pth.pid)

            # Create some basic predicates for ETL content checking
            predicates = get_basic_etl_content_predicates([target_process])
            predicates.append(ZeroContextSwitchCountPredicate(target_process))
            predicates.append(ZeroReadyThreadCountPredicate(target_process))
    
            etl_content_expectation = EtlContentExpectation("*.etl", predicates)
            file_expectation = ProfileTestFileExpectation("*.etl", 2, ETL_MIN_SIZE)
            expectations = [file_expectation, etl_content_expectation]
    
            evaluate_profile_test(list_files_in_dir(fixture.outdir), expectations)

# Test cases for user providers
# 4 providers are used for testing: two manifest-based (MB), and two TraceLogging (TL) ones. Events have various levels,
#   keywords, etc., so the "inclusion criteria" handling of etwprof can be tested.
# There's a table below that describes the properties of these providers and events, for convenience. The "source of
#   truth" for MB providers is the manifest file (MB.man), for TL ones it's the code itself (ETWCases.cpp). I hope to 
#   keep them in sync...

# +----------+--------+--------+---------+--------+
# | Provider | Opcode | Level  | Keyword | Symbol |
# +----------+--------+--------+---------+--------+
# |          | 0      | 16     | 0x1     | A      |
# |          +--------+--------+---------+--------+
# |    MBA   | 1      | 17     | 0x2     | B      |
# |          +--------+--------+---------+--------+
# |          | 2      | 0      | 0x2     | C      |
# +----------+--------+--------+---------+--------+
# |          | 0      | 16     | 0x20    | A      |
# |          +--------+--------+---------+--------+
# |    MBB   | 1      | 17     | 0x200   | B      |
# |          +--------+--------+---------+--------+
# |          | 2      | 0      | 0x220   | C      |
# +----------+--------+--------+---------+--------+
# |          | 0      | 1      | 0x1     | A      |
# |    TLA   +--------+--------+---------+--------+
# |          | 1      | 3      | 0x11    | B      |
# +----------+--------+--------+---------+--------+
# |          | 0      | 3      | 0x11    | A      |
# |    TLB   +--------+--------+---------+--------+
# |          | 1      | 1      | 0x1     | B      |
# +----------+--------+--------+---------+--------+


@testcase(suite = _profile_suite, name = "No user providers", fixture = ProfileTestsFixture())
def test_no_user_providers():
    filelist, processes = perform_profile_test("MBTLEmitAll", fixture.outfile)

    user_provider_etl_content_predicates = []
    for p in processes:
        user_provider_etl_content_predicates.append(
            GeneralEventCountByProviderAndEventIdSubsetPredicate(p, get_empty_user_provider_event_counts()))
        
    evaluate_simple_profile_test(filelist,
                                 fixture.outfile,
                                 processes,
                                 additional_etl_content_predicates=user_provider_etl_content_predicates,
                                 sampled_profile_min=0)
    
@testcase(suite = _profile_suite, name = "User providers, manifest-based", fixture = ProfileTestsFixture())
def test_user_providers_mb_a():
    filelist, processes = perform_profile_test("MBTLEmitAll", fixture.outfile, [f"--enable={MB_A_GUID}"])

    expected_user_provider_event_counts = get_empty_user_provider_event_counts()
        
    expected_user_provider_event_counts[MB_A_GUID, MB_A_A] = (ComparisonOperator.EQUAL, 1)
    expected_user_provider_event_counts[MB_A_GUID, MB_A_B] = (ComparisonOperator.EQUAL, 1)
    expected_user_provider_event_counts[MB_A_GUID, MB_A_C] = (ComparisonOperator.EQUAL, 1)

    user_provider_etl_content_predicates = []
    for p in processes:
        user_provider_etl_content_predicates.append(
            GeneralEventCountByProviderAndEventIdSubsetPredicate(p, expected_user_provider_event_counts))
        
    evaluate_simple_profile_test(filelist,
                                 fixture.outfile,
                                 processes,
                                 additional_etl_content_predicates=user_provider_etl_content_predicates,
                                 sampled_profile_min=0)

@testcase(suite = _profile_suite, name = "User providers, TraceLogging", fixture = ProfileTestsFixture())
def test_user_providers_tl_a():
    filelist, processes = perform_profile_test("MBTLEmitAll", fixture.outfile, [f"--enable={TL_A_GUID}"])

    expected_user_provider_event_counts = get_empty_user_provider_event_counts()
        
    expected_user_provider_event_counts[TL_A_GUID, TL_A_A] = (ComparisonOperator.EQUAL, 1)
    expected_user_provider_event_counts[TL_A_GUID, TL_A_B] = (ComparisonOperator.EQUAL, 1)

    user_provider_etl_content_predicates = []
    for p in processes:
        user_provider_etl_content_predicates.append(
            GeneralEventCountByProviderAndEventIdSubsetPredicate(p, expected_user_provider_event_counts))
        
    evaluate_simple_profile_test(filelist,
                                 fixture.outfile,
                                 processes,
                                 additional_etl_content_predicates=user_provider_etl_content_predicates,
                                 sampled_profile_min=0)
    
def _impl_test_user_providers_mb_b_tl_b(stacks: bool):    
    enable_string = "--enable="
    if stacks:
        enable_string += f"{TL_B_GUID}::'stack'+{MB_B_GUID}::'stack'"
    else:
        enable_string += f"{TL_B_GUID}+{MB_B_GUID}"
    filelist, processes = perform_profile_test("MBTLEmitAll", fixture.outfile, [enable_string])
    assert(len(processes) == 1)
    process = processes[0]

    expected_user_provider_event_counts = get_empty_user_provider_event_counts()

    expected_user_provider_event_counts[MB_B_GUID, MB_B_A] = (ComparisonOperator.EQUAL, 1)
    expected_user_provider_event_counts[MB_B_GUID, MB_B_B] = (ComparisonOperator.EQUAL, 1)
    expected_user_provider_event_counts[MB_B_GUID, MB_B_C] = (ComparisonOperator.EQUAL, 1)
    expected_user_provider_event_counts[TL_B_GUID, TL_B_A] = (ComparisonOperator.EQUAL, 1)
    expected_user_provider_event_counts[TL_B_GUID, TL_B_B] = (ComparisonOperator.EQUAL, 1)

    etl_content_predicates = []

    etl_content_predicates.append(
        GeneralEventCountByProviderAndEventIdSubsetPredicate(process, expected_user_provider_event_counts))
    
    stack_counts_predicate = None
    if stacks:
        expected_stack_counts = {
            (PERF_INFO_GUID, PERF_INFO_SAMPLED_PROFILE_ID): 0,
            (MB_B_GUID, MB_B_A): 1,
            (MB_B_GUID, MB_B_B): 1,
            (MB_B_GUID, MB_B_C): 1,
            (TL_B_GUID, 0): 2,  # HACK: TraceInfoDumper/TraceProcessor cannot distinguish TraceLogging events in the
                                # context of call stacks, so we have to "aggregate" all events using a dummy
                                # value of zero...
        }
        stack_counts_predicate = StackCountByProviderAndEventIdGTEPredicate(process, expected_stack_counts)

    etl_content_predicates.extend (get_basic_etl_content_predicates(processes, stack_count_predicate= stack_counts_predicate, sampled_profile_min=0))
    expectations = [ProfileTestFileExpectation("*.etl", 1, ETL_MIN_SIZE), EtlContentExpectation("*.etl", etl_content_predicates)]
    evaluate_profile_test(filelist, expectations)

@testcase(suite = _profile_suite, name = "User providers, manifest-based and TraceLogging", fixture = ProfileTestsFixture())
def test_user_providers_mb_b_tl_b():
    _impl_test_user_providers_mb_b_tl_b(False)

@testcase(suite = _profile_suite, name = "User providers, manifest-based and TraceLogging w/ stacks", fixture = ProfileTestsFixture())
def test_user_providers_mb_b_tl_b_stacks():
    _impl_test_user_providers_mb_b_tl_b(True)

@testcase(suite = _profile_suite, name = "User providers, max. level", fixture = ProfileTestsFixture())
def test_user_providers_all_max_level_1():
    filelist, processes = perform_profile_test("MBTLEmitAll", fixture.outfile, [get_enable_string_for_all_providers(max_level=1)])

    expected_user_provider_event_counts = get_empty_user_provider_event_counts()
        
    expected_user_provider_event_counts[MB_A_GUID, MB_A_C] = (ComparisonOperator.EQUAL, 1)
    expected_user_provider_event_counts[MB_B_GUID, MB_B_C] = (ComparisonOperator.EQUAL, 1)
    expected_user_provider_event_counts[TL_A_GUID, TL_A_A] = (ComparisonOperator.EQUAL, 1)
    expected_user_provider_event_counts[TL_B_GUID, TL_B_B] = (ComparisonOperator.EQUAL, 1)

    user_provider_etl_content_predicates = []
    for p in processes:
        user_provider_etl_content_predicates.append(
            GeneralEventCountByProviderAndEventIdSubsetPredicate(p, expected_user_provider_event_counts))
        
    evaluate_simple_profile_test(filelist,
                                 fixture.outfile,
                                 processes,
                                 additional_etl_content_predicates=user_provider_etl_content_predicates,
                                 sampled_profile_min=0)
    
def _impl_every_user_providers_case(enable_string: str, expected_user_provider_event_counts):
    filelist, processes = perform_profile_test("MBTLEmitAll", fixture.outfile, [enable_string])

    user_provider_etl_content_predicates = []
    for p in processes:
        user_provider_etl_content_predicates.append(
            GeneralEventCountByProviderAndEventIdSubsetPredicate(p, expected_user_provider_event_counts))
        
    evaluate_simple_profile_test(filelist,
                                 fixture.outfile,
                                 processes,
                                 additional_etl_content_predicates=user_provider_etl_content_predicates,
                                 sampled_profile_min=0)
    
@testcase(suite = _profile_suite, name = "User providers, keyword bitmask simple", fixture = ProfileTestsFixture())
def test_user_providers_all_kw_1():
    expected_user_provider_event_counts = get_empty_user_provider_event_counts()
        
    expected_user_provider_event_counts[MB_A_GUID, MB_A_A] = (ComparisonOperator.EQUAL, 1)
    expected_user_provider_event_counts[TL_A_GUID, TL_A_A] = (ComparisonOperator.EQUAL, 1)
    expected_user_provider_event_counts[TL_B_GUID, TL_B_B] = (ComparisonOperator.EQUAL, 1)

    _impl_every_user_providers_case(get_enable_string_for_all_providers(kw_bitmask_string="0x1"), expected_user_provider_event_counts)
    
@testcase(suite = _profile_suite, name = "User providers, keyword bitmask negated", fixture = ProfileTestsFixture())
def test_user_providers_all_kw_not_1():
    expected_user_provider_event_counts = get_all_1_user_provider_event_counts()
        
    expected_user_provider_event_counts[MB_A_GUID, MB_A_A] = (ComparisonOperator.EQUAL, 0)
    expected_user_provider_event_counts[TL_A_GUID, TL_A_A] = (ComparisonOperator.EQUAL, 0)
    expected_user_provider_event_counts[TL_B_GUID, TL_B_B] = (ComparisonOperator.EQUAL, 0)

    _impl_every_user_providers_case(get_enable_string_for_all_providers(kw_bitmask_string="~0x1"), expected_user_provider_event_counts)
    
@testcase(suite = _profile_suite, name = "User providers, keyword bitmask complex", fixture = ProfileTestsFixture())
def test_user_providers_all_kw_220():
    expected_user_provider_event_counts = get_empty_user_provider_event_counts()
        
    expected_user_provider_event_counts[MB_B_GUID, MB_B_A] = (ComparisonOperator.EQUAL, 1)
    expected_user_provider_event_counts[MB_B_GUID, MB_B_B] = (ComparisonOperator.EQUAL, 1)
    expected_user_provider_event_counts[MB_B_GUID, MB_B_C] = (ComparisonOperator.EQUAL, 1)

    _impl_every_user_providers_case(get_enable_string_for_all_providers(kw_bitmask_string="0x220"), expected_user_provider_event_counts)
    
@testcase(suite = _profile_suite, name = "User providers, various providers and options", fixture = ProfileTestsFixture())
def test_user_providers_all_kw_220():    
    enable_string = f"--enable={MB_A_GUID}:~0x1:'stack'+*TLA+{TL_B_GUID}:0x11:1"

    filelist, processes = perform_profile_test("MBTLEmitAll", fixture.outfile, [enable_string])
    assert(len(processes) == 1)
    process = processes[0]

    expected_user_provider_event_counts = get_empty_user_provider_event_counts()

    expected_user_provider_event_counts[MB_A_GUID, MB_A_B] = (ComparisonOperator.EQUAL, 1)
    expected_user_provider_event_counts[MB_A_GUID, MB_A_C] = (ComparisonOperator.EQUAL, 1)
    expected_user_provider_event_counts[TL_A_GUID, TL_A_A] = (ComparisonOperator.EQUAL, 1)
    expected_user_provider_event_counts[TL_A_GUID, TL_A_B] = (ComparisonOperator.EQUAL, 1)
    expected_user_provider_event_counts[TL_B_GUID, TL_A_B] = (ComparisonOperator.EQUAL, 1)
    
    etl_content_predicates = []

    etl_content_predicates.append(
        GeneralEventCountByProviderAndEventIdSubsetPredicate(process, expected_user_provider_event_counts))
    
    expected_stack_counts = {
        (PERF_INFO_GUID, PERF_INFO_SAMPLED_PROFILE_ID): 0,
        (MB_A_GUID, MB_A_B): 1,
        (MB_A_GUID, MB_A_C): 1,
    }
    stack_counts_predicate = StackCountByProviderAndEventIdGTEPredicate(process, expected_stack_counts)

    etl_content_predicates.extend (get_basic_etl_content_predicates(processes,
                                                                    stack_count_predicate= stack_counts_predicate,
                                                                    sampled_profile_min=0))
    expectations = [ProfileTestFileExpectation("*.etl", 1, ETL_MIN_SIZE), EtlContentExpectation("*.etl", etl_content_predicates)]
    evaluate_profile_test(filelist, expectations)

@testcase(suite = _profile_suite, name = "User providers, multiple processes", fixture = ProfileTestsFixture())
def test_user_providers_multiple_processes():
    pths = launch_pth_processes(3, "MBTLEmitAll")

    with EtwprofProcess.attach_to_profilee(PTH_EXE_NAME, fixture.outfile, [get_enable_string_for_all_providers()]) as etwprof:
        wait_for_etwprof_session()

        for event in pths.values():
            event.signal()

        etwprof.wait()

        for p in pths:
            p.check()

    processInfos = [ProcessInfo(PTH_EXE_NAME, p.pid) for p in pths]

    etl_content_predicates = get_basic_etl_content_predicates(processInfos, sampled_profile_min=0)

    expected_user_provider_event_counts = get_all_1_user_provider_event_counts()

    for p in processInfos:
        etl_content_predicates.append(GeneralEventCountByProviderAndEventIdSubsetPredicate(p, expected_user_provider_event_counts))

    expectations = [ProfileTestFileExpectation("*.etl", 1, ETL_MIN_SIZE),
                    EtlContentExpectation("*.etl", etl_content_predicates)]
    evaluate_profile_test(list_files_in_dir(fixture.outdir), expectations)