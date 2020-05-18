import imp
import glob
from os.path import *

suites = glob.glob(join(dirname(__file__), "*.py"))
for f in suites:
    if not basename(f).startswith("_"):
        imp.load_source(basename(f), f)