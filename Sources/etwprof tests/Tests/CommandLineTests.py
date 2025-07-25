"Tests for various command line combinations. No actual functionality is tested, just command line processing"
import os
from test_framework import *
from TestUtils import *

_cmd_suite = TestSuite("Command line tests")

def _create_valid_profile_args(args):
    result = ["profile", "-t=something.exe", "--outdir=C:"]
    result.extend(args)

    return result

def _run_command_line_test(args):
    # Makes etwprof handle arguments, then exit instead of taking any action. We insert this as the second argument, as:
    # - the first argument might be a (positional) argument, specifying a command
    # - the last group of arguments might be specifying a full command line of a new process to be started
    args.insert(1, "--noaction")
    return run_etwprof(args)

@testcase(suite = _cmd_suite, name = "No args")
def test_no_args():
    expect_nonzero(_run_command_line_test ([]))

@testcase(suite = _cmd_suite, name = "Help")
def test_help_short():
    expect_zero(_run_command_line_test(["-h"]))
    expect_zero(_run_command_line_test(["--help"]))

@testcase(suite = _cmd_suite, name = "Version")
def test_version():
    expect_zero(_run_command_line_test(["--version"]))

@testcase(suite = _cmd_suite, name = "Unknown commands")
def test_unknown_commands():
    expect_nonzero(_run_command_line_test(["hello"]))
    expect_nonzero(_run_command_line_test(["validate cert"]))
    expect_nonzero(_run_command_line_test(["123 456"]))
    expect_nonzero(_run_command_line_test(["porfile"]))

@testcase(suite = _cmd_suite, name = "Profile command")
def test_profile_command():
    expect_zero(_run_command_line_test(["profile", "-t=something.exe", "-o=C:\\whatever.etl"]))
    expect_zero(_run_command_line_test(["profile", "-t=123456", "-o=C:\\whatever.etl"]))
    expect_zero(_run_command_line_test(["profile", "-t=something.exe", "--outdir=C:\\"]))
    expect_zero(_run_command_line_test(["profile", "-t=something.exe", "--outdir=%TMP%"]))
    expect_zero(_run_command_line_test(["profile", "--outdir=C:\\", "--", get_cmd_path(), "--arg1=a", "--test", "--", "-a"]))

    expect_zero(_run_command_line_test(_create_valid_profile_args(["--nologo"])))

    expect_zero(_run_command_line_test(_create_valid_profile_args(["--children"])))
    expect_zero(_run_command_line_test(_create_valid_profile_args(["--children", "--waitchildren"])))

    expect_zero(_run_command_line_test(_create_valid_profile_args(["-m"])))
    expect_zero(_run_command_line_test(_create_valid_profile_args(["-m", "--mflags=0x3456"])))

    expect_zero(_run_command_line_test(_create_valid_profile_args(["--compress=7z"])))
    expect_zero(_run_command_line_test(_create_valid_profile_args(["--compress=etw"])))
    expect_zero(_run_command_line_test(_create_valid_profile_args(["--compress=off"])))

    expect_zero(_run_command_line_test(_create_valid_profile_args(["--rate=1000"])))

    expect_zero(_run_command_line_test(_create_valid_profile_args(["--scache"])))

class _RealWorldTestsFixture:
    def setup(self):
        self.dir = create_temp_dir()

    def teardown(self):
        os.rmdir(self.dir)

@testcase(suite = _cmd_suite, name = "Profile command real-world", fixture = _RealWorldTestsFixture())
def test_profile_command_real_world():
    expect_zero(_run_command_line_test(["profile", "-t=notepad.exe", "-o=C:\\mytrace.etl"]))
    expect_zero(_run_command_line_test(["profile", "--outdir=C:\\", "--", get_cmd_path(), "/C", "exit"]))
    expect_zero(_run_command_line_test(["profile", "-t=17816", "--outdir=%TMP%", "--cswitch", "--compress=7z"]))
    expect_zero(_run_command_line_test(["profile", "-v", "--nologo", "-t=notepad.exe", f"--outdir={fixture.dir}", "-m", "--rate=100", "--children"]))
    expect_zero(_run_command_line_test(["profile", "-t=notepad.exe", "--outdir=%USERPROFILE%", "--enable=Microsoft-Windows-RPC", "--debug"]))

@testcase(suite = _cmd_suite, name = "Argument parsing errors")
def test_argument_parsing():
    expect_nonzero(_run_command_line_test(["-"]))
    expect_nonzero(_run_command_line_test(["profile=3", "-t=something.exe", "--outdir=C:"]))
    expect_nonzero(_run_command_line_test(["profile", "-tsomething.exe", "--outdir=C:"]))
    expect_nonzero(_run_command_line_test(_create_valid_profile_args(["--mflags="])))
    expect_nonzero(_run_command_line_test(_create_valid_profile_args(["--mflags"])))
    expect_nonzero(_run_command_line_test(_create_valid_profile_args(["-mflags"])))

@testcase(suite = _cmd_suite, name = "Profile command erroneous")
def test_profile_erroneous():
    expect_nonzero(_run_command_line_test(["profile"]))  # Target and output parameters are missing
    expect_nonzero(_run_command_line_test(["profile", "-t=123", "-o=path.etl", "--outdir=C:"]))  # Both -o and --outdir
    expect_nonzero(_run_command_line_test(["profile", "-t=something.exe", "-o=C:\\Windows"]))   # Folder instead of a file
    expect_nonzero(_run_command_line_test(["profile", "-t=something.exe", "-o=C:\\this_folder_does_not_exist\1.etl"]))
    expect_nonzero(_run_command_line_test(["profile", "-t=something.exe", "--outdir=C:\\this_folder_does_not_exist"]))
    expect_nonzero(_run_command_line_test(["profile", "-t=123", "-o=C:\\test.etlbadextenstion"]))
    expect_nonzero(_run_command_line_test(["profile", "-t=123", "--compress=7z", "-o=C:\\test.etl"]))   # etl instead of 7z as extension
    expect_nonzero(_run_command_line_test(["profile", "-t=123", "--outdir=C:\\", "--", get_cmd_path()])) # Both a target is specified, and an executable is provided to be launched
    expect_nonzero(_run_command_line_test(["profile", "--outdir=C:\\", "-m", "--", get_cmd_path()])) # Minidump requested, even though the profiled process is to be launched
    expect_nonzero(_run_command_line_test(["profile", "--outdir=C:\\", "-m", "--", "DoesNotExist.exe"])) # Process to be launched does not exist
    expect_nonzero(_run_command_line_test(["profile", "--outdir=C:\\", "--"])) # Process to be launched is not specified

    expect_nonzero(_run_command_line_test(_create_valid_profile_args(["--mflags=0"])))   # w/o -m

    expect_nonzero(_run_command_line_test(_create_valid_profile_args(["--waitchildren"])))   # w/o --children

    # Invalid compression methods
    expect_nonzero(_run_command_line_test(_create_valid_profile_args(["--compress=7y"])))
    expect_nonzero(_run_command_line_test(_create_valid_profile_args(["--compress=on"])))

    # Invalid profile rates
    expect_nonzero(_run_command_line_test(_create_valid_profile_args(["--rate=0"])))
    expect_nonzero(_run_command_line_test(_create_valid_profile_args(["--rate=1236548"])))
    expect_nonzero(_run_command_line_test(_create_valid_profile_args(["--rate=ABC"])))
    expect_nonzero(_run_command_line_test(_create_valid_profile_args(["--rate=-3"])))

class _RespFileFixture:
    def setup(self):
        self.files = {}
        self.files["empty"] = create_temp_file("")
        self.files["valid"] = create_temp_file("profile -t=notepad.exe --outdir=%TMP%")
        self.files["partial"] = create_temp_file("--outdir=%TMP%")
        self.files["utf-16"] = create_temp_file(u"profile -t=notepad.exe --outdir=%TMP%".encode("utf-16"))

        self.files["recursive"] = create_temp_file("")
        with open(self.files["recursive"], "w") as f:
            f.write(f"@{self.files['recursive']}")

        self.files["chained_B"] = create_temp_file("-t=notepad.exe --outdir=%TMP%")
        self.files["chained_A"] = create_temp_file(f"profile @{self.files['chained_B']}")

    def teardown(self):
        for _, file in self.files.items():
            os.remove(file)
        
@testcase(suite = _cmd_suite, name = "Response files", fixture = _RespFileFixture())
def test_response_files():
    expect_nonzero(_run_command_line_test(_create_valid_profile_args([f"@C:\\does_not_exist.txt"])))
    expect_nonzero(_run_command_line_test(_create_valid_profile_args([f"@{fixture.files['empty']}"])))
    expect_nonzero(_run_command_line_test(_create_valid_profile_args([f"@{fixture.files['recursive']}"])))
    expect_nonzero(_run_command_line_test(_create_valid_profile_args([f"@{fixture.files['chained_A']}"])))

    expect_zero(_run_command_line_test([f"@{fixture.files['valid']}"]))
    expect_zero(_run_command_line_test([f"@{fixture.files['utf-16']}"]))
    expect_zero(_run_command_line_test(_create_valid_profile_args([f"@{fixture.files['partial']}"])))

class _EmulateModeFixture:
    def setup(self):
        self.etl = create_temp_file("")
        new_name = "".join([self.etl, ".etl"])
        os.rename(self.etl, new_name)
        self.etl = new_name

    def teardown(self):
        os.remove(self.etl)

@testcase(suite = _cmd_suite, name = "Emulate mode", fixture = _EmulateModeFixture())
def test_emulate_mode():
    expect_nonzero(_run_command_line_test(["profile", r"--emulate=C:\does_not_exist.etl", "-t=123", r"-o=%TMP%\ot.etl"]))
    # Target is specified by exe name, should be PID
    expect_nonzero(_run_command_line_test(["profile", f"--emulate={fixture.etl}", "-t=notepad.exe", r"-o=%TMP%\ot.etl"]))
    # A minidump is requested
    expect_nonzero(_run_command_line_test(["profile", f"--emulate={fixture.etl}", "-t=notepad.exe", "-m", r"-o=%TMP%\ot.etl"]))
    
    expect_zero(_run_command_line_test(["profile", f"--emulate={fixture.etl}", "-t=123456", r"-o=%TMP%\o.etl"]))

@testcase(suite = _cmd_suite, name = "User providers")
def test_user_providers():
    expect_zero(_run_command_line_test(_create_valid_profile_args(["--enable=*Microsoft-Windows-WER-PayloadHealth"])))
    expect_zero(_run_command_line_test(_create_valid_profile_args(["--enable=017247f2-7e96-11dc-8314-0800200c9a66"])))
    expect_zero(_run_command_line_test(_create_valid_profile_args(["--enable=017247F2-7E96-11DC-8314-0800200C9A66"])))  # Same as above, but with capital letters
    expect_zero(_run_command_line_test(_create_valid_profile_args(["--enable=Microsoft-Windows-RPC"])))
    expect_zero(_run_command_line_test(_create_valid_profile_args(["--enable=Microsoft-Windows-RPC::"]))) # Should be the same as above
    expect_zero(_run_command_line_test(_create_valid_profile_args(["--enable=Microsoft-Windows-RPC:0xa"])))
    expect_zero(_run_command_line_test(_create_valid_profile_args(["--enable=Microsoft-Windows-RPC::2"])))
    expect_zero(_run_command_line_test(_create_valid_profile_args(["--enable=Microsoft-Windows-RPC::'stack'"])))
    expect_zero(_run_command_line_test(_create_valid_profile_args(["--enable=*Microsoft-Windows-WER-PayloadHealth+Microsoft-Windows-RPC"])))
    expect_zero(_run_command_line_test(_create_valid_profile_args(["--enable=Microsoft-Windows-RPC+Microsoft-Windows-Services+017247f2-7e96-11dc-8314-0800200c9a66"])))
    expect_zero(_run_command_line_test(_create_valid_profile_args(["--enable=Microsoft-Windows-Win32k:~0x0200000010000000:4'stack'"])))
    expect_zero(_run_command_line_test(_create_valid_profile_args(["--enable=Microsoft-Windows-Win32k:0x10000000:'stack'+Microsoft-Windows-RPC+37f342ed-e45c-4b26-b3fe-450af9817fcd:0xff:2"])))
    # Provider w/ GUID that does not exist; etwprof should still accept it, as it might be a TraceLogging provider
    expect_zero(_run_command_line_test(_create_valid_profile_args(["--enable=6069C3C9-55F2-4614-9AF3-AF6892FCDC5F"])))

@testcase(suite = _cmd_suite, name = "User providers erroneous")
def test_user_providers_erroneous():
    expect_nonzero(_run_command_line_test(_create_valid_profile_args(["--enable=DoesNotExist"])))
    # Looks almost like a GUID, but it's not; etwprof should interpret it as a registered name
    expect_nonzero(_run_command_line_test(_create_valid_profile_args(["--enable=017247f2-7e96-11dc-8314-0800200c9a6"])))
    expect_nonzero(_run_command_line_test(_create_valid_profile_args(["--enable=+++"])))
    expect_nonzero(_run_command_line_test(_create_valid_profile_args(["--enable=':::''':':'::+"])))
    expect_nonzero(_run_command_line_test(_create_valid_profile_args(["--enable=*Microsoft-Windows-WER-PayloadHealth++Microsoft-Windows-RPC"])))
    expect_nonzero(_run_command_line_test(_create_valid_profile_args(["--enable=*Microsoft-Windows-RPC:::"])))
    expect_nonzero(_run_command_line_test(_create_valid_profile_args(["--enable=*Microsoft-Windows-RPC::'haha'"])))
    expect_nonzero(_run_command_line_test(_create_valid_profile_args(["--enable=*Microsoft-Windows-RPC:0xa:TEST"])))
    
    # Specifying the same provider more than once with...
    # ... registered names
    expect_nonzero(_run_command_line_test(_create_valid_profile_args(["--enable=Microsoft-Windows-RPC+Microsoft-Windows-RPC"])))
    # ... a registered name then GUID
    expect_nonzero(_run_command_line_test(_create_valid_profile_args(["--enable=Microsoft-Windows-RPC+6ad52b32-d609-4be9-ae07-ce8dae937e39"])))
    # ... a generated name, and a registered name
    expect_nonzero(_run_command_line_test(_create_valid_profile_args(["--enable=*Microsoft-Windows-WER-PayloadHealth+Microsoft-Windows-WER-PayloadHealth"])))
    # ... a generated name, and a GUID
    expect_nonzero(_run_command_line_test(_create_valid_profile_args(["--enable=*Microsoft-Windows-WER-PayloadHealth+4afddfde-002d-51ac-c109-c3b7897858d0"])))
    # ... a generated name, a GUID, and a registered name
    expect_nonzero(_run_command_line_test(_create_valid_profile_args(["--enable=*Microsoft-Windows-WER-PayloadHealth+4afddfde-002d-51ac-c109-c3b7897858d0+Microsoft-Windows-WER-PayloadHealth"])))