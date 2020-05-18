from collections import defaultdict
from fnmatch import fnmatch
import os
import re
import sys
import traceback
from typing import List, Callable, Optional

_ASSERT_EXPR_REGEX = re.compile(r"(assert_|expect_).*?\((?P<expr>.*)\).*")

class TestFailure:
    def __init__(self, desc: str, expr_desc: str, nskip = 1, frame_summary_list = None):
        self._desc = desc
        self._expr_desc = expr_desc
        self._expr = "UNKNOWN"
        self._loc = "UNKNOWN"

        ss = None
        if not frame_summary_list:
            tb = traceback.walk_stack(None)
            ss = traceback.StackSummary.extract(tb)
        else:
            ss = traceback.StackSummary.from_list(frame_summary_list)

        frame_of_interest = ss[1 + nskip]
        self._expr = frame_of_interest.line

        self._loc = "{module}!{function} Line {lineno}".format(module = os.path.basename(frame_of_interest.filename),
                                                               function = frame_of_interest.name,
                                                               lineno = frame_of_interest.lineno)

        # A bit naive and lousy, but works for most cases...
        match = _ASSERT_EXPR_REGEX.match(frame_of_interest.line)
        if match:
            self._expr = match.group("expr")

    @classmethod
    def from_exception(cls, e):
        return cls("Unhandled exception", f"raised an exception of type {type(e).__name__}", frame_summary_list = traceback.extract_tb(e.__traceback__))

    @property
    def description(self):
        return self._desc

    @property
    def expression(self):
        return self._expr

    @property
    def expression_description(self):
        return self._expr_desc

    @property
    def location(self):
        return self._loc

class TestCase:
    def __init__(self, name: str, function: Callable, fixture = None):
        self._name = name
        self._function = function
        self._suite = None
        self._failures = []
        self._fixture = fixture

    @property
    def name(self) -> str:
        return self._name

    @property
    def full_name(self) -> str:
        name = ""
        if self._suite:
            name = self._suite.name

        return "".join([name, "::", self._name])

    @property
    def suite(self):
        return self._suite

    @suite.setter
    def suite(self, suite):
        self._suite = suite

    def add_failure(self, failure: TestFailure):
        self._failures.append(failure)

    def has_failures(self) -> bool:
        return len(self._failures) > 0

    @property
    def failures(self):
        return self._failures

    @property
    def fixture(self):
        return self._fixture

    def invoke(self):
        if self._fixture:
            self._fixture.setup()
        
        try:
            self._function()
        except:
            raise
        finally:
            if self._fixture:
                self._fixture.teardown()

class TestSuite:
    def __init__(self, name: str):
        self._name = name
        self._cases = []

        test_suites.add_suite(self)

    def __len__(self):
        return len(self.cases)

    @property
    def name(self) -> str:
        return self._name

    @property
    def cases(self) -> List[TestCase]:
        return self._cases

    def add_case(self, case: TestCase):
        self._cases.append(case)
        case.suite = self

class TestSuiteRegister:
    def __init__(self):
        self._suites = []

    def __iter__(self):
        return iter(self.suites)

    @property
    def suites(self) -> List[TestCase]:
        return self._suites

    def add_suite(self, suite: TestSuite):
        self._suites.append(suite)

class TestRunner:
    def _default_callback(*args, **kwargs):
        pass

    def __init__(self):
        self._cases_by_suites = defaultdict(list) # map of cases keyed by suites
        self._failed_cases = []

        # Callbacks
        self.on_start : Callable[int, int] = self._default_callback                          # Parameters: no. of suites, no. of cases
        self.on_end : Callable[List[TestCases]] = self._default_callback                     # Parameters: list of failed cases
        self.on_suite_start : Callable[TestSuite, List[TestCase]] = self._default_callback   # Parameters: suite, list of cases
        self.on_suite_end : Callable[TestSuite] = self._default_callback                     # Parameters: suite
        self.on_case_start : Callable[TestCase] = self._default_callback                     # Parameters: case
        self.on_case_end : Callable[TestCase] = self._default_callback                       # Parameters: case

        for suite in test_suites:
            for case in suite.cases:
                self._cases_by_suites[suite].append(case)

    def run(self, filter = "*"):
        cases_by_suites_filtered = None
        if filter == "*":
            cases_by_suites_filtered = self._cases_by_suites
        else:
            cases_by_suites_filtered = defaultdict(list)
            for suite, cases in self._cases_by_suites.items():
                for case in cases:
                    if fnmatch(case.full_name, filter):
                        cases_by_suites_filtered[suite].append(case)

        self.on_start(len(cases_by_suites_filtered), self._number_of_cases(cases_by_suites_filtered))

        for suite, cases in cases_by_suites_filtered.items():
            self.on_suite_start(suite, cases)

            for case in cases:
                self.on_case_start(case)
                try:
                    case.invoke()
                    self.on_case_end(case)
                except AssertionFailedError as e:
                    case.add_failure(e.failure)
                    self._failed_cases.append(case)
                    
                    self.on_case_end(case)
                except RuntimeError as e:
                    case.add_failure(TestFailure.from_exception(e))
                    self._failed_cases.append(case)
                    
                    self.on_case_end(case)

            self.on_suite_end(suite)

        self.on_end(self._failed_cases)

    def has_failures(self) -> bool:
        return len(self._failed_cases) > 0

    def _number_of_cases(self, dict):
        sum = 0
        for suite, cases in dict.items():
            sum = sum + len(cases)

        return sum

# Decorator for test cases
def testcase(suite, name, fixture = None):
    def helper(func):
        def testcase_decorator():
            global _current_case
            _current_case = case

            func.__globals__["fixture"] = fixture

            func()

        case = TestCase(name, testcase_decorator, fixture)
        suite.add_case(case)

        return testcase_decorator   # Not used

    return helper

class AssertionFailedError(RuntimeError):
    def __init__(self, desc: str):
        self._failure = TestFailure(ASSERT_MSG, desc, 3)

        super().__init__(f"{ASSERT_MSG} at {self._failure.location}:\n\"{self._failure.expression}\" {desc}!")

    @property
    def failure(self):
        return self._failure

# Assertions
ASSERT_MSG = "Assertion failed"
IMPL_SKIP = 2

def _assert_true_impl(to_test, fatal):
    if not to_test:
        DESC = "was not True"
        if fatal:
            raise AssertionFailedError(DESC)
        else:
            _current_case.add_failure(TestFailure(ASSERT_MSG, DESC, nskip = IMPL_SKIP))

def _assert_false_impl(to_test):
    if to_test:
        DESC = "was not False"
        if fatal:
            raise AssertionFailedError(DESC)
        else:
            _current_case.add_failure(TestFailure(ASSERT_MSG, DESC, nskip = IMPL_SKIP))

def _assert_zero_impl(to_test, fatal):
    if to_test != 0:
        desc = f"was not zero (actual: {to_test})"
        if fatal:
            raise AssertionFailedError(desc)
        else:
            _current_case.add_failure(TestFailure(ASSERT_MSG, desc, nskip = IMPL_SKIP))

def _assert_nonzero_impl(to_test, fatal):
    if to_test == 0:
        DESC = "was zero"
        if fatal:
            raise AssertionFailedError(DESC)
        else:
            _current_case.add_failure(TestFailure(ASSERT_MSG, DESC, nskip = IMPL_SKIP))

def assert_true(to_test):
    _assert_true_impl(to_test, True)

def assert_false(to_test):
    _assert_false_impl(to_test, True)

def assert_zero(to_test):
    _assert_zero_impl(to_test, True)

def assert_nonzero(to_test):
    _assert_nonzero_impl(to_test, True)

def expect_true(to_test):
    _assert_true_impll(to_test, False)

def expect_false(to_test):
    _assert_false_impl(to_test, False)

def expect_zero(to_test):
    _assert_zero_impl(to_test, False)

def expect_nonzero(to_test):
    _assert_nonzero_impl(to_test, False)

test_suites = TestSuiteRegister()
_current_case = None