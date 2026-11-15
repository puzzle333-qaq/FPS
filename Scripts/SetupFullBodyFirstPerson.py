"""Compatibility entry point: the project now uses Mesh1P arms with Mesh3P legs."""
import runpy
from pathlib import Path
runpy.run_path(str(Path(__file__).with_name("SetupHybridFirstPerson.py")))
