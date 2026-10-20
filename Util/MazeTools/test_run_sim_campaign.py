"""Unit tests for run_sim_campaign (run: python3 -m unittest discover -s Util/MazeTools -v)."""

import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import run_sim_campaign as rsc  # noqa: E402

HANDLE_MAZE = """\
// Settings of the maze behaviour.
start = {
  cell = -1;
  heading = south;
};

// enabled = false in this comment must not be edited, nor its { brace
announce = {
  enabled = true;
};

returnToStart = {
  enabled = true; // trailing comment
  trigger = button;
  delayMs = 5000;
  maxDurationMs = 120000;
};
"""


class BlockSettingTest(unittest.TestCase):
    def setUp(self):
        self.directory = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.directory, ignore_errors=True)
        self.path = self.directory / "handleMaze.cfg"
        self.path.write_text(HANDLE_MAZE, encoding="utf-8")

    def test_edits_only_the_named_block(self):
        rsc.replace_block_setting(self.path, "returnToStart", "enabled", "false")
        text = self.path.read_text(encoding="utf-8")
        self.assertIn("announce = {\n  enabled = true;\n};", text)
        self.assertIn("  enabled = false; // trailing comment", text)
        self.assertIn("// enabled = false in this comment", text)

    def test_run_settings(self):
        scenario = self.directory / "scenario"
        shutil.copytree(rsc.SCENARIO_TEMPLATE, scenario)
        rsc.apply_run_settings(scenario, Path("Config/Scenes/Includes/X/current.maze"), 2, 60)
        module = (scenario / "mazeRunMetricsProvider.cfg").read_text(encoding="utf-8")
        self.assertIn('mazeFile = "Config/Scenes/Includes/X/current.maze";', module)
        self.assertIn("attempts = 2;", module)
        self.assertIn(f"startTapAfterMs = {rsc.START_TAP_AFTER_MS};", module)
        self.assertIn("attemptTimeoutMs = 60000;", module)
        behavior = (scenario / "handleMaze.cfg").read_text(encoding="utf-8")
        self.assertRegex(behavior, r"announce = \{\s*enabled = true;")
        self.assertRegex(behavior, r"returnToStart = \{\s*enabled = true;\s*trigger = timer;")
        rsc.apply_run_settings(scenario, Path("Config/Scenes/Includes/X/current.maze"), 1, 60)
        behavior = (scenario / "handleMaze.cfg").read_text(encoding="utf-8")
        self.assertRegex(behavior, r"announce = \{\s*enabled = true;")
        self.assertRegex(behavior, r"returnToStart = \{\s*enabled = false;")

    def test_missing_block_or_key_is_an_error(self):
        with self.assertRaises(rsc.CampaignError):
            rsc.replace_block_setting(self.path, "missing", "enabled", "true")
        with self.assertRaises(rsc.CampaignError):
            rsc.replace_block_setting(self.path, "announce", "delayMs", "1")


class AttemptParsingTest(unittest.TestCase):
    OUTPUT = (
        "noise line\n"
        'MZ021_METRICS {"attempt":1,"reached_goal":true,"time_s":93.4,"wall_contacts":0,"max_stall_s":4.2,"status":"goal"}\n'
        "MZ021_METRICS not json\n"
        'MZ021_METRICS {"attempt":2,"reached_goal":false,"time_s":0.0,"wall_contacts":0,"max_stall_s":0.0,"status":"not_started"}\n'
        "MZ021_DONE\n"
    )

    def test_records_are_keyed_by_attempt(self):
        attempts = rsc.collect_attempts(self.OUTPUT)
        self.assertEqual(sorted(attempts), [1, 2])
        self.assertTrue(attempts[1]["reached_goal"])
        self.assertEqual(attempts[1]["time_s"], 93.4)
        self.assertTrue(rsc.has_complete_metrics(attempts[1]))

    def test_rows(self):
        attempts = rsc.collect_attempts(self.OUTPUT)
        first = rsc.attempt_row("maze_1.maze", 1, "low", attempts[1])
        self.assertEqual((first["reached_goal"], first["time_s"], first["wall_contacts"], first["max_stall_s"]),
                         (True, 93.4, 0, 4.2))
        second = rsc.attempt_row("maze_1.maze", 2, "low", attempts[2])
        self.assertEqual((second["reached_goal"], second["time_s"], second["max_stall_s"]), (False, "", ""))
        missing = rsc.attempt_row("maze_1.maze", 2, "low", {})
        self.assertEqual((missing["reached_goal"], missing["time_s"]), ("", ""))
        self.assertEqual(tuple(first), rsc.CSV_FIELDS)

    def test_wall_clock_limit_covers_every_attempt_and_the_return(self):
        self.assertGreater(rsc.wall_clock_limit_s(2, 300), 2 * 300 + rsc.RETURN_TIMEOUT_MS / 1000)


if __name__ == "__main__":
    unittest.main()
