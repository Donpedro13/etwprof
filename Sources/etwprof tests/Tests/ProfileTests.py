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

@testcase(suite = _profile_suite, name = "Target specified by name", fixture = ProfileTestsFixture())
def test_target_name():
    filelist, processes = perform_profile_test("DoNothing", fixture.outdir, options= ProfileTestOptions.TARGET_ID_NAME)
    evaluate_simple_profile_test(filelist, "*.etl", processes)

@testcase(suite = _profile_suite, name = "Multiple target processes with same name", fixture = ProfileTestsFixture())
def test_multiple_target_processes_same_name():
    with PTHProcess(), PTHProcess(), EtwprofProcess(PTH_EXE_NAME, fixture.outfile) as etwprof:
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
    filelist, processes =  perform_profile_test("DoNothing", fixture.outfile, ["--debug"])
    file_expectations = [ProfileTestFileExpectation(fixture.outfile, 1, ETL_MIN_SIZE),
                            ProfileTestFileExpectation("*.raw.etl", 1, ETL_MIN_SIZE)]
    evaluate_simple_profile_test(filelist, fixture.outfile, processes, None, file_expectations)
    
@testcase(suite = _profile_suite, name = "Profiling stopped with CTRL+C", fixture = ProfileTestsFixture())
def test_ctrl_c_stop():
    with PTHProcess() as pth, pth.get_event_for_sync() as e, EtwprofProcess(PTH_EXE_NAME, fixture.outfile) as etwprof:
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
        etl_content_predicates = get_basic_etl_content_predicates([target_process], sampled_profile_min=0)

        etl_content_predicates.append(ZeroContextSwitchCountPredicate(target_process))
        etl_content_predicates.append(ZeroReadyThreadCountPredicate(target_process))

        expectations = [ProfileTestFileExpectation("*.etl", 1, ETL_MIN_SIZE), EtlContentExpectation("*.etl", etl_content_predicates)]
        evaluate_profile_test(list_files_in_dir(fixture.outdir), expectations)

@testcase(suite = _profile_suite, name = "ETW session killed", fixture = ProfileTestsFixture())
def test_etw_session_killed():
    # Start PTH (since we never signal its event, it will wait forever), then we pull the rug from under etwprof
    with PTHProcess() as pth, EtwprofProcess(PTH_EXE_NAME, fixture.outfile) as etwprof:
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
    with PTHProcess() as pth, pth.get_event_for_sync() as e, EtwprofProcess(PTH_EXE_NAME, fixture.outfile) as etwprof:
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
    if is_win7_or_earlier():
        return  # Not supported on Win 7

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
    if is_win7_or_earlier():
        return  # Only one kernel session at once is supported on Win 7

    with PTHProcess() as pth,    \
         pth.get_event_for_sync() as e,          \
         EtwprofProcess(PTH_EXE_NAME, fixture.outfile) as etwprof1, \
         EtwprofProcess(PTH_EXE_NAME, fixture.outdir) as etwprof2:
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

# TODO
# User providers...