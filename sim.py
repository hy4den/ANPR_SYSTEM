#!/usr/bin/env python3
"""Convenience entrypoint for running the ESP32 simulator from project root."""

from pathlib import Path
import runpy


if __name__ == "__main__":
    sim_path = Path(__file__).resolve().parent / "backend" / "tests" / "test_esp32_sim.py"
    runpy.run_path(str(sim_path), run_name="__main__")
