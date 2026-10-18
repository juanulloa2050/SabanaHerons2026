#!/usr/bin/env python3
"""Run a batch of maze scenes in SimRobot and collect campaign metrics.

The SimRobot fallback executes the scene's .con file, so maze runs need no
manual console commands. Metrics are accepted as JSON records printed by the
simulation in the provisional form ``MZ021_METRICS { ... }``. Bryam is expected
to provide the actual BehaviorStatus/MazeMap field names and debug format in
the PR review; replace this adapter with that mapping before treating the CSV
as measured results. Until then, missing values stay blank rather than being
guessed.

Noise presets use the MZ-020 nominal and harsh values as a reference: low uses
zero injected motion noise, mid uses 3 deg/min gyro drift and 5% odometry
translation noise, and high uses 15 deg/min, 10% translation noise and 17%
percept dropout. The high profile's 10% is a standard deviation; MZ-020's
offline harsh case used a systematic +10% bias, which the current config does
not inject. Scenario files are copied to a temporary directory so the normal
Maze configuration is never edited.
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import re
import shutil
import subprocess
import sys
import time
import uuid
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
SCENE_TEMPLATE = ROOT / "Config/Scenes/Maze.ros2"
CONSOLE_TEMPLATE = ROOT / "Config/Scenes/Maze.con"
SCENARIO_TEMPLATE = ROOT / "Config/Scenarios/Maze"
CONVERTER = ROOT / "Util/MazeTools/maze_to_scene.py"
DEFAULT_SIMROBOT = ROOT / "Build/Linux/SimRobot/Develop/SimRobot"
CSV_FIELDS = (
    "maze",
    "attempt",
    "noise",
    "reached_goal",
    "time_s",
    "wall_contacts",
    "max_stall_s",
)
METRIC_MARKER = "MZ021_METRICS "

# (gyro drift degrees/min, odometry translation stddev, oracle dropout)
NOISE_PRESETS = {
    "low": (0.0, 0.0, 0.0),
    "mid": (3.0, 0.05, 0.0),
    "high": (15.0, 0.10, 0.17),
}


class CampaignError(Exception):
    """A campaign could not be prepared or started."""


def positive_int(value: str) -> int:
    try:
        parsed = int(value)
    except ValueError:
        raise argparse.ArgumentTypeError("must be an integer") from None
    if parsed < 1:
        raise argparse.ArgumentTypeError("must be greater than zero")
    return parsed


def positive_float(value: str) -> float:
    try:
        parsed = float(value)
    except ValueError:
        raise argparse.ArgumentTypeError("must be a number") from None
    if parsed <= 0:
        raise argparse.ArgumentTypeError("must be greater than zero")
    return parsed


def natural_sort_key(path: Path) -> tuple[Any, ...]:
    return tuple(int(part) if part.isdigit() else part.casefold()
                 for part in re.split(r"(\d+)", path.name))


def find_mazes(argument: str, maximum: int) -> list[Path]:
    """Resolve a directory, glob, or one or more .maze paths."""
    candidates: list[Path] = []
    for item in argument.split(os.pathsep):
        path = Path(item).expanduser()
        if not path.is_absolute():
            path = ROOT / path
        if path.is_dir():
            candidates.extend(sorted(path.glob("*.maze"), key=natural_sort_key))
        elif any(char in str(path) for char in "*?["):
            candidates.extend(sorted(path.parent.glob(path.name), key=natural_sort_key))
        elif path.is_file():
            candidates.append(path)
        else:
            raise CampaignError(f"maze path does not exist: {item}")

    # Stable de-duplication keeps an explicitly listed file from running twice.
    unique = list(dict.fromkeys(p.resolve() for p in sorted(candidates, key=natural_sort_key)))
    if not unique:
        raise CampaignError(f"no .maze files found in: {argument}")
    return unique[:maximum]


def replace_setting(path: Path, key: str, value: str) -> None:
    pattern = re.compile(rf"^(\s*{re.escape(key)}\s*=\s*)[^;]*(;.*)$", re.MULTILINE)
    original = path.read_text(encoding="utf-8")
    updated, count = pattern.subn(lambda match: f"{match.group(1)}{value}{match.group(2)}", original, count=1)
    if count != 1:
        raise CampaignError(f"cannot find setting {key!r} in {path}")
    path.write_text(updated, encoding="utf-8")


def apply_noise_profile(scenario: Path, maze_copy: Path, noise: str) -> None:
    gyro_drift, odometry_noise, dropout = NOISE_PRESETS[noise]
    localizer = scenario / "mazeLocalizer.cfg"
    oracle = scenario / "oracledMazePerceptsProvider.cfg"
    replace_setting(localizer, "injectedGyroDriftPerMinute", f"{gyro_drift:g}")
    replace_setting(localizer, "injectedOdometryTranslationNoise", f"{odometry_noise:g}")
    replace_setting(oracle, "dropoutRate", f"{dropout:g}")
    replace_setting(oracle, "mazeFile", f'"{maze_copy.as_posix()}"')


def collect_metrics(output: str) -> dict[str, Any]:
    """Read the latest structured metric record, if the behavior emits one."""
    latest: dict[str, Any] = {}
    for line in output.splitlines():
        marker_at = line.find(METRIC_MARKER)
        if marker_at < 0:
            continue
        payload = line[marker_at + len(METRIC_MARKER):].strip()
        try:
            values = json.loads(payload)
        except json.JSONDecodeError:
            continue
        if isinstance(values, dict):
            latest.update(values)
    return latest


def make_run_files(maze: Path, noise: str) -> tuple[Path, Path, Path, list[Path]]:
    """Create an isolated scene, console, include and scenario for one run."""
    run_id = f"MazeCampaign_{uuid.uuid4().hex[:12]}"
    scenario = ROOT / "Config/Scenarios" / run_id
    scene = ROOT / "Config/Scenes" / f"{run_id}.ros2"
    console = ROOT / "Config/Scenes" / f"{run_id}.con"
    # Keep the include one directory below Includes, like Includes/Maze. The
    # converter writes texture paths relative to that depth.
    include_dir = ROOT / "Config/Scenes/Includes" / run_id
    include = include_dir / "current.rsi2"
    cleanup_paths = [scenario, scene, console, include_dir]

    try:
        shutil.copytree(SCENARIO_TEMPLATE, scenario)
        include_dir.mkdir(parents=True, exist_ok=False)
        converted = subprocess.run(
            [sys.executable, str(CONVERTER), str(maze), "--out", str(include)],
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=False,
        )
        if converted.returncode:
            raise CampaignError(converted.stderr.strip() or f"maze conversion failed: {maze}")

        maze_copy = include.with_suffix(".maze")
        apply_noise_profile(scenario, maze_copy.relative_to(ROOT), noise)

        scene_text = SCENE_TEMPLATE.read_text(encoding="utf-8")
        scene_text = scene_text.replace(
            'Includes/Maze/current.rsi2',
            f'Includes/{run_id}/current.rsi2',
        )
        scene_text, replaced = re.subn(
            r'(<Compound\s+name="scenarios">\s*<Compound\s+name=")Maze("\s*/>)',
            rf'\g<1>{run_id}\g<2>',
            scene_text,
            count=1,
        )
        if replaced != 1:
            raise CampaignError("could not point the scene at its temporary Maze scenario")
        scene.write_text(scene_text, encoding="utf-8")

        # Maze.con already disables the referee and enters playing. The debug
        # request is the same start gesture described by the maze instructions.
        console_text = CONSOLE_TEMPLATE.read_text(encoding="utf-8").rstrip()
        console.write_text(console_text + "\ndr option:HandleMaze:headFront on\n", encoding="utf-8")
        return scene, console, include, cleanup_paths
    except Exception:
        cleanup(cleanup_paths)
        raise


def cleanup(paths: list[Path]) -> None:
    for path in paths:
        if path.is_dir():
            shutil.rmtree(path, ignore_errors=True)
        else:
            path.unlink(missing_ok=True)


def run_simrobot(executable: Path, scene: Path, timeout_s: float) -> tuple[str, bool, float]:
    start = time.monotonic()
    try:
        completed = subprocess.run(
            [str(executable), str(scene)],
            cwd=ROOT,
            capture_output=True,
            text=True,
            timeout=timeout_s,
            check=False,
        )
        return completed.stdout + "\n" + completed.stderr, False, time.monotonic() - start
    except subprocess.TimeoutExpired as exc:
        stdout = exc.stdout or ""
        stderr = exc.stderr or ""
        if isinstance(stdout, bytes):
            stdout = stdout.decode(errors="replace")
        if isinstance(stderr, bytes):
            stderr = stderr.decode(errors="replace")
        return stdout + "\n" + stderr, True, time.monotonic() - start


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mazes", default="Config/Mazes/generated",
                        help="directory, glob or path-separated list of .maze files")
    parser.add_argument("--attempts", type=positive_int, default=2,
                        help="attempts to run for each maze (default: 2)")
    parser.add_argument("--noise", choices=tuple(NOISE_PRESETS), default="low",
                        help="noise profile to apply (default: low)")
    parser.add_argument("--timeout-s", type=positive_float, default=300,
                        help="maximum wall-clock seconds per run (default: 300)")
    parser.add_argument("--out", type=Path, default=Path("results.csv"),
                        help="CSV output path (default: results.csv)")
    parser.add_argument("--max-mazes", type=positive_int, default=2,
                        help="maximum mazes from the input (default: 2; use 20 for all generated mazes)")
    parser.add_argument("--simrobot", type=Path, default=DEFAULT_SIMROBOT,
                        help="SimRobot executable path")
    args = parser.parse_args(argv)

    try:
        mazes = find_mazes(args.mazes, args.max_mazes)
        executable = args.simrobot.expanduser()
        if not executable.is_absolute():
            executable = ROOT / executable
        if not executable.is_file():
            raise CampaignError(
                f"SimRobot executable not found: {executable}\n"
                "Build it with: Make/Linux/compile Develop SimRobot"
            )
        out = args.out.expanduser()
        if not out.is_absolute():
            out = Path.cwd() / out
        out.parent.mkdir(parents=True, exist_ok=True)

        completed_runs = 0
        with out.open("w", newline="", encoding="utf-8") as csv_file:
            writer = csv.DictWriter(csv_file, fieldnames=CSV_FIELDS)
            writer.writeheader()
            csv_file.flush()
            for maze in mazes:
                for attempt in range(1, args.attempts + 1):
                    scene = console = include = None
                    paths: list[Path] = []
                    try:
                        scene, console, include, paths = make_run_files(maze, args.noise)
                        output, timed_out, elapsed = run_simrobot(executable, scene, args.timeout_s)
                        metrics = collect_metrics(output)
                        row = {
                            "maze": maze.name,
                            "attempt": attempt,
                            "noise": args.noise,
                            "reached_goal": metrics.get("reached_goal", ""),
                            "time_s": metrics.get("time_s", ""),
                            "wall_contacts": metrics.get("wall_contacts", ""),
                            "max_stall_s": metrics.get("max_stall_s", ""),
                        }
                        writer.writerow(row)
                        csv_file.flush()
                        completed_runs += 1
                        result = "timeout" if timed_out else "SimRobot closed"
                        print(f"[{completed_runs}] {maze.name}, attempt {attempt}: {result} ({elapsed:.1f}s)")
                        if not metrics:
                            print(
                                "  warning: SimRobot did not emit MZ021_METRICS; "
                                "result columns are blank until behavior telemetry is available.",
                                file=sys.stderr,
                            )
                    finally:
                        cleanup(paths)
        print(f"CSV written: {out}")
        return 0
    except CampaignError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    except OSError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
