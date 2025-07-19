import importlib.util
import importlib.machinery

import glob
from os.path import *

def load_source(modname, filename):
    loader = importlib.machinery.SourceFileLoader(modname, filename)
    spec = importlib.util.spec_from_file_location(modname, filename, loader=loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module

suites = glob.glob(join(dirname(__file__), "*.py"))
for f in suites:
    if not basename(f).startswith("_"):
        load_source(basename(f), f)