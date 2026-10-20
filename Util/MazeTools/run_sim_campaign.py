#!/usr/bin/env python3
"""Run a batch of maze scenes in SimRobot and collect campaign metrics.

One SimRobot process runs one maze, with all its attempts, and its scene console script (.con) starts it without any manual
command. The metrics come from the simulation itself: the module MazeRunMetricsProvider (Src/Modules/Modeling) measures every
attempt from the ground truth robot pose and the maze file, taps the head front to start it, and prints one line per attempt on
the standard error stream, which is all a headless SimRobot shows::

    MZ021_METRICS {"attempt":1,"reached_goal":true,"time_s":93.4,"wall_contacts":0,"max_stall_s":4.2,"speed_run":false,"status":"goal"}
    MZ021_DONE

The runner turns each ``MZ021_METRICS`` line into a CSV row and stops the process at ``MZ021_DONE``. The metric definitions are in
Tools/Maze/MazeRunTracker.h and in docs/maze/TICKETS_LABERINTO.md (MZ-021). ``status`` is ``goal``, ``timeout`` or
``not_started``. Attempt 2 is a second attempt in the same simulation: the robot walks back to the start on its own
(``returnToStart`` with the timer trigger) and the module taps again when it stands there, so the map of attempt 1 is kept. Unlike
the competition, the robot is not lifted and placed by hand: the localizer is not reset. If attempt 1 does not reach the goal,
or the robot is not back in time, attempt 2 is ``not_started``: its row has ``reached_goal`` false and the other columns blank,
and it is not a tooling error. A missing attempt line gives a row with blank metrics, and by default makes the command exit
nonzero; ``--allow-missing-metrics`` is only for checking scene launch and CSV plumbing.

SimRobot runs offscreen (``QT_QPA_PLATFORM=offscreen``) unless ``--show`` is given. ``--timeout-s`` is the limit of one attempt in
simulated time, written to the module as ``attemptTimeoutMs``. The wall-clock limit that kills the process of one maze is
``attempts * timeout + return timeout + margin``, which assumes that the simulation runs about as fast as real time; a slower
simulation hits the wall-clock limit first and is reported as a timeout.

Noise presets use the MZ-020 nominal and harsh values as a reference: low uses
zero injected motion noise, mid uses 3 deg/min gyro drift and 5% odometry
translation noise, and high uses 15 deg/min, 10% translation noise and 17%
percept dropout. The high profile's 10% is a standard deviation; MZ-020's
offline harsh case used a systematic +10% bias, which the current config does
not inject. The normal Maze configuration is never edited: each run gets its own scene,
console script, include directory and a copy of the Maze scenario, written next to the templates in Config/Scenes and
Config/Scenarios under a ``MazeCampaign_<id>`` name (not in a temporary directory, because SimRobot resolves them relative to the
repository) and removed after each run. They are in .gitignore; a run that was killed hard can leave them behind.
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
from queue import Empty, Queue
from threading import Thread
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
DONE_MARKER = "MZ021_DONE"
MODULE_CONSOLE_LINE = "for Cognition mr MazeRunMetrics MazeRunMetricsProvider"

# Settings written to the copied scenario (times in ms of simulated time). The return limit covers the delay and the longest return
# of handleMaze.cfg, the settle time of the module and a margin.
START_TAP_AFTER_MS = 3000
RETURN_DELAY_MS = 2000
RETURN_MAX_MS = 120000
RETURN_TIMEOUT_MS = 150000
STARTUP_MARGIN_S = 30.0

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


def replace_block_setting(path: Path, block: str, key: str, value: str) -> None:
    """Replace ``key`` inside ``block = {...};`` only (the first match in that block).

    ``enabled`` appears in several blocks of handleMaze.cfg, so a plain first-match replacement would edit the wrong one. Text
    after ``//`` is ignored when looking for braces and keys.
    """
    start_pattern = re.compile(rf"^\s*{re.escape(block)}\s*=\s*\{{")
    key_pattern = re.compile(rf"(?<![\w.])({re.escape(key)}\s*=\s*)[^;]*(;)")
    lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
    depth = 0
    in_block = False
    replaced = False
    for index, line in enumerate(lines):
        code, comment_mark, comment = line.partition("//")
        offset = 0
        if not in_block:
            match = start_pattern.match(code)
            if not match:
                continue
            in_block = True
            offset = match.end()
            depth = 1
        prefix, rest = code[:offset], code[offset:]
        if not replaced:
            rest, count = key_pattern.subn(lambda match: f"{match.group(1)}{value}{match.group(2)}", rest, count=1)
            replaced = count == 1
        depth += rest.count("{") - rest.count("}")
        lines[index] = prefix + rest + comment_mark + comment
        if depth <= 0:
            break
    if not in_block:
        raise CampaignError(f"cannot find block {block!r} in {path}")
    if not replaced:
        raise CampaignError(f"cannot find setting {key!r} in block {block!r} of {path}")
    path.write_text("".join(lines), encoding="utf-8")


def apply_noise_profile(scenario: Path, maze_copy: Path, noise: str) -> None:
    gyro_drift, odometry_noise, dropout = NOISE_PRESETS[noise]
    localizer = scenario / "mazeLocalizer.cfg"
    oracle = scenario / "oracledMazePerceptsProvider.cfg"
    replace_setting(localizer, "injectedGyroDriftPerMinute", f"{gyro_drift:g}")
    replace_setting(localizer, "injectedOdometryTranslationNoise", f"{odometry_noise:g}")
    replace_setting(oracle, "dropoutRate", f"{dropout:g}")
    replace_setting(oracle, "mazeFile", f'"{maze_copy.as_posix()}"')
    # The evaluator of MZ-019 (only after that branch is merged) reads the same maze.
    evaluator = scenario / "wallPerceptEvaluator.cfg"
    if evaluator.is_file():
        replace_setting(evaluator, "mazeFile", f'"{maze_copy.as_posix()}"')


def apply_run_settings(scenario: Path, maze_copy: Path, attempts: int, timeout_s: float) -> None:
    """Point the metrics module at the maze and set the attempts, and make the robot return to the start for attempt 2."""
    module = scenario / "mazeRunMetricsProvider.cfg"
    replace_setting(module, "mazeFile", f'"{maze_copy.as_posix()}"')
    replace_setting(module, "attempts", str(attempts))
    replace_setting(module, "startTapAfterMs", str(START_TAP_AFTER_MS))
    replace_setting(module, "attemptTimeoutMs", str(int(timeout_s * 1000)))
    replace_setting(module, "returnTimeoutMs", str(RETURN_TIMEOUT_MS))
    behavior = scenario / "handleMaze.cfg"
    replace_block_setting(behavior, "returnToStart", "enabled", "true" if attempts == 2 else "false")
    replace_block_setting(behavior, "returnToStart", "trigger", "timer")
    replace_block_setting(behavior, "returnToStart", "delayMs", str(RETURN_DELAY_MS))
    replace_block_setting(behavior, "returnToStart", "maxDurationMs", str(RETURN_MAX_MS))


def collect_attempts(output: str) -> dict[int, dict[str, Any]]:
    """Read the ``MZ021_METRICS`` records of a run, keyed by attempt; a later record of an attempt updates the earlier one."""
    attempts: dict[int, dict[str, Any]] = {}
    for line in output.splitlines():
        marker_at = line.find(METRIC_MARKER)
        if marker_at < 0:
            continue
        payload = line[marker_at + len(METRIC_MARKER):].strip()
        try:
            values = json.loads(payload)
        except json.JSONDecodeError:
            continue
        if isinstance(values, dict) and isinstance(values.get("attempt"), int):
            attempts.setdefault(values["attempt"], {}).update(values)
    return attempts


def has_complete_metrics(metrics: dict[str, Any]) -> bool:
    return all(field in metrics and metrics[field] not in (None, "") for field in CSV_FIELDS[3:])


def attempt_row(maze: str, attempt: int, noise: str, metrics: dict[str, Any]) -> dict[str, Any]:
    """The CSV row of one attempt; blank where the run printed nothing. A not started attempt only says that it did not reach the goal."""
    if metrics.get("status") == "not_started":
        metrics = {"reached_goal": False}
    return {
        "maze": maze,
        "attempt": attempt,
        "noise": noise,
        "reached_goal": metrics.get("reached_goal", ""),
        "time_s": metrics.get("time_s", ""),
        "wall_contacts": metrics.get("wall_contacts", ""),
        "max_stall_s": metrics.get("max_stall_s", ""),
    }


def make_run_files(maze: Path, noise: str, attempts: int, timeout_s: float) -> tuple[Path, Path, Path, list[Path]]:
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
        apply_run_settings(scenario, maze_copy.relative_to(ROOT), attempts, timeout_s)

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

        # Maze.con already disables the referee and enters playing. The metrics module starts the attempts itself: a head tap
        # in the console script would come before the behaviour is ready and be lost.
        console_text = CONSOLE_TEMPLATE.read_text(encoding="utf-8").rstrip()
        console.write_text(console_text + "\n" + MODULE_CONSOLE_LINE + "\n", encoding="utf-8")
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


def wall_clock_limit_s(attempts: int, timeout_s: float) -> float:
    """Wall-clock seconds after which the process of one maze is killed: every attempt, the return, and the start-up."""
    return attempts * timeout_s + RETURN_TIMEOUT_MS / 1000 + STARTUP_MARGIN_S


def run_simrobot(
    executable: Path,
    scene: Path,
    limit_s: float,
    show: bool = False,
) -> tuple[str, bool, float, int | None, bool]:
    """Run one maze; returns (output, timed out, elapsed seconds, return code, stopped on MZ021_DONE)."""
    start = time.monotonic()
    environment = os.environ.copy()
    if not show:
        environment["QT_QPA_PLATFORM"] = "offscreen"
    process = subprocess.Popen(
        [str(executable), str(scene)],
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
        env=environment,
    )
    output_queue: Queue[str | None] = Queue()
    output_lines: list[str] = []

    def forward_output() -> None:
        assert process.stdout is not None
        for line in process.stdout:
            output_queue.put(line)
        output_queue.put(None)

    reader = Thread(target=forward_output, daemon=True)
    reader.start()

    deadline = start + limit_s
    timed_out = False
    stopped_on_done = False
    output_closed = False
    while not output_closed:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            timed_out = True
            break
        try:
            line = output_queue.get(timeout=min(remaining, 0.1))
        except Empty:
            continue
        if line is None:
            output_closed = True
            break
        output_lines.append(line)
        if DONE_MARKER in line:
            stopped_on_done = True
            break

    if process.poll() is None and (timed_out or stopped_on_done):
        process.terminate()
        try:
            process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            process.kill()
    returncode = process.wait()
    reader.join(timeout=2)
    while True:
        try:
            line = output_queue.get_nowait()
        except Empty:
            break
        if line is not None:
            output_lines.append(line)
    return "".join(output_lines), timed_out, time.monotonic() - start, returncode, stopped_on_done


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--mazes", default="Config/Mazes/generated",
                        help="directory, glob or path-separated list of .maze files")
    parser.add_argument("--attempts", type=int, choices=(1, 2), default=2,
                        help="attempts to run for each maze, in one simulation (default: 2)")
    parser.add_argument("--noise", choices=tuple(NOISE_PRESETS), default="low",
                        help="noise profile to apply (default: low)")
    parser.add_argument("--timeout-s", type=positive_float, default=300,
                        help="maximum simulated seconds per attempt (default: 300); the wall-clock limit of one maze is "
                             "attempts * this + the return timeout + a start-up margin")
    parser.add_argument("--out", type=Path, default=Path("results.csv"),
                        help="CSV output path (default: results.csv)")
    parser.add_argument("--max-mazes", type=positive_int, default=2,
                        help="maximum mazes from the input (default: 2; use 20 for all generated mazes)")
    parser.add_argument("--simrobot", type=Path, default=DEFAULT_SIMROBOT,
                        help="SimRobot executable path")
    parser.add_argument("--show", action="store_true",
                        help="keep the SimRobot window (default: offscreen, no window)")
    parser.add_argument("--allow-missing-metrics", action="store_true",
                        help="return success when telemetry is unavailable; for scene/CSV plumbing only")
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
        campaign_incomplete = False
        simulation_failed = False
        with out.open("w", newline="", encoding="utf-8") as csv_file:
            writer = csv.DictWriter(csv_file, fieldnames=CSV_FIELDS)
            writer.writeheader()
            csv_file.flush()
            for maze in mazes:
                paths: list[Path] = []
                try:
                    scene, console, include, paths = make_run_files(maze, args.noise, args.attempts, args.timeout_s)
                    output, timed_out, elapsed, returncode, stopped_on_done = run_simrobot(
                        executable, scene, wall_clock_limit_s(args.attempts, args.timeout_s), args.show
                    )
                    recorded = collect_attempts(output)
                    result = (
                        "timeout" if timed_out else
                        "all attempts measured" if stopped_on_done else
                        f"SimRobot exited {returncode}"
                    )
                    for attempt in range(1, args.attempts + 1):
                        metrics = recorded.get(attempt, {})
                        not_started = metrics.get("status") == "not_started"
                        writer.writerow(attempt_row(maze.name, attempt, args.noise, metrics))
                        csv_file.flush()
                        completed_runs += 1
                        if not_started:
                            print(f"[{completed_runs}] {maze.name}, attempt {attempt}: not started (attempt 1 did not "
                                  "reach the goal, or the robot did not come back to the start in time)")
                            continue
                        missing_metrics = [
                            field for field in CSV_FIELDS[3:]
                            if field not in metrics or metrics[field] is None
                        ]
                        print(f"[{completed_runs}] {maze.name}, attempt {attempt}: "
                              f"{metrics.get('status', 'no metrics')}, speed_run={metrics.get('speed_run', '?')} ({result}, {elapsed:.1f}s for the maze)")
                        if missing_metrics:
                            campaign_incomplete = True
                            print(
                                "  warning: missing metrics: " + ", ".join(missing_metrics),
                                file=sys.stderr,
                            )
                    if len(recorded) < args.attempts:
                        # The module says why it cannot measure (for instance an unreadable maze) on the error stream.
                        for line in output.splitlines():
                            if "MazeRunMetricsProvider" in line:
                                print(f"  {line.strip()}", file=sys.stderr)
                    if timed_out or (returncode not in (0, None) and not stopped_on_done):
                        campaign_incomplete = True
                        simulation_failed = True
                        detail = "reached the wall-clock limit" if timed_out else f"exited with status {returncode}"
                        print(f"  warning: simulation of {maze.name} {detail}.", file=sys.stderr)
                finally:
                    cleanup(paths)
        print(f"CSV written: {out}")
        if simulation_failed:
            print("error: one or more SimRobot runs timed out or exited unsuccessfully.", file=sys.stderr)
            return 1
        if campaign_incomplete and not args.allow_missing_metrics:
            print(
                "error: campaign is missing metrics; check the MZ021_METRICS lines of the run, or use "
                "--allow-missing-metrics only for a plumbing check.",
                file=sys.stderr,
            )
            return 1
        return 0
    except CampaignError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    except OSError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
