"""Compatibility entry point; phone advertising scan verification is retired."""
from pathlib import Path
import runpy

runpy.run_path(str(Path(__file__).with_name('verify_ble_paired_presence_ui.py')), run_name='__main__')
