"""Smoke test for the Praxis Python SDK."""
import json
import sys
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "python"))

import praxis  # noqa: E402


def test_engine_runs_a_task():
    engine = praxis.Engine(provider="fake", budget_turns=2)
    result = engine.run("demo task")
    assert result is not None
    assert result.turns >= 1
    assert result.completed is True


def test_engine_budget_limits_turns():
    engine = praxis.Engine(provider="fake", budget_turns=4)
    result = engine.run("loop until budget ends")
    assert result.turns <= 4


def test_telemetry_report_contains_speedup():
    engine = praxis.Engine(provider="fake", budget_turns=2)
    engine.run("demo")
    snap = json.loads(engine.telemetry().report())
    assert "speedup_x" in snap
