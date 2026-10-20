"""Compatibility entry point; advertising scan builds are historical artifacts.

The paired migration verifier writes separate stage-D records and preserves the
old ble-presence-build.json/firmware artifacts unchanged.
"""
from pathlib import Path
import runpy
import sys
sys.argv = [sys.argv[0], '--stage', 'D']
runpy.run_path(str(Path(__file__).with_name('verify_ble_paired_trial.py')), run_name='__main__')
