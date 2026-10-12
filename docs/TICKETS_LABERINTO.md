# Maze Challenge Tickets — Copa NAO CCM MX 2026

This file lists every ticket (MZ-001 … MZ-044) for the Maze challenge. Each ticket says what it
builds, who owns it, how long it should take, what it depends on, which branch to use, and how to
tell that it is done. Start with the summary tables, then open your sprint. The order is:
scheduled software tickets by sprint, proposed software tickets, physical-robot tickets, backlog.

Related: [ROADMAP_LABERINTO.md](ROADMAP_LABERINTO.md) (architecture, Git workflow, validation
strategy) · [GANTT_LABERINTO.md](GANTT_LABERINTO.md) (schedule, lab access request).

---

## Summary

### Hours per person

| | Authoring | Lab sessions | **Total** |
|---|---|---|---|
| **Bryam** | 39.25 h | 7 h (Lab 3, 4, 5) | **46.25 h** |
| **Wilson** | 28.75 h (includes 3.5 h of the optional bench, MZ-014) | 11 h (Lab 1–5) | **39.75 h** |

Notes:
- **Authoring** means the owner's own work, including opening the PR, answering review comments and merging. Lab tickets are counted under *Lab sessions*, not under authoring.
- **Review time is not tracked.** Cross review still happens (roadmap section 9), but it has no hours in any ticket.
- Bryam's 39.25 h authoring = 38.75 h of own tickets + 0.5 h pairing on MZ-010.
- Without the optional bench (MZ-014), Wilson's total is 36.25 h.
- MZ-002 (branch protection) and MZ-005 (registration check) were removed as unnecessary. Their numbers are not reused.

### Physical-robot tickets are in their own section

The team is not sure how, or how much, the physical robot and the lab can be used. Every
ticket that needs them is therefore grouped in
**[Physical robot tickets](#physical-robot-tickets-testing-method-not-settled)** and listed in its
own table below. The software tickets must be finishable **without** them.

A `*` in a *Depends on* cell marks a **soft** dependency on a physical ticket: the ticket uses
the lab data when it exists, and falls back to simulation (rendered images, noisy odometry) when
it does not. A ticket whose acceptance criteria name real logs (for example MZ-022) can only be
closed when those logs exist.

### TAG-DEP marker

⚠️ **TAG-DEP** marks tickets whose design depends on the organisers' answer about which walls
carry tags and at what height (question Q1, ticket MZ-006): MZ-013, MZ-015, MZ-017, MZ-018,
MZ-020, MZ-022. Until the answer arrives, they assume **one tag centred on every wall face of
every cell, tag centre at 30 cm above the floor**. If the answer differs, only the tag model in
`Src/Tools/Maze/` and the tag mounts change.

### Software tickets (scheduled)

Tick the **Done** box of a ticket (`[ ]` → `[x]`) in this table **and** in the ticket's own
checklist (its *Done* row and every acceptance criterion) in the same PR that closes it. A ticket is
done only when the PR is merged and all its boxes are checked. Only the scheduled software tickets
have a Done column here; physical, proposed and backlog tickets use their own acceptance criteria.

| Done | ID | Title | Owner | Sprint | Author h | Lab | Depends on |
|---|---|---|---|---|---|---|---|
| [x] | MZ-001 | Create and publish `Maze-Challenge` | Bryam | 0 | 0.25 | no | – |
| [x] | MZ-003 | Planning docs (roadmap, tickets, Gantt) | Bryam | 0 | 1 | no | MZ-001 |
| [ ] | MZ-004 | PR template | Wilson | 0 | 0.5 | no | MZ-001 |
| [ ] | MZ-006 | Organiser questions and answers | Wilson | 0 | 0.75 | no | MZ-003 |
| [ ] | MZ-007 | Maze/MazeSafe scenarios + start flow | Bryam | 0 | 2.5 | no | MZ-001 |
| [ ] | MZ-008 | Vendor apriltag + CMake | Bryam | 0 | 3 | no | MZ-001 |
| [ ] | MZ-009 | Maze format + generator | Wilson | 0 | 2 | no | MZ-004 |
| [ ] | MZ-010 | Maze → SimRobot scene converter | Wilson (+Bryam 0.5 pairing) | 0 | 2.5 | no | MZ-007, MZ-009 |
| [ ] | MZ-011 | Maze library + time-weighted planner + tests | Bryam | 1 | 6 | no | MZ-009 |
| [ ] | MZ-012 | Test mazes + expected results | Wilson | 1 | 1.5 | no | MZ-009 |
| [ ] | MZ-013 | Oracle maze percepts for simulation ⚠️ | Bryam | 1 | 2 | no | MZ-010, MZ-011 |
| [ ] | MZ-016 | Corridor walk profile in simulation | Wilson | 1 | 3 | no | MZ-007, MZ-010 |
| [ ] | MZ-018 | AprilTag detector ⚠️ | Bryam | 2 | 5 | no | MZ-008, MZ-017* |
| [ ] | MZ-019 | Wall perceptor | Bryam | 2 | 4 | no | MZ-010, MZ-017* |
| [ ] | MZ-020 | Localizer + mapper ⚠️ | Bryam | 2 | 5 | no | MZ-011, MZ-013 |
| [ ] | MZ-021 | Headless simulation campaign runner | Wilson | 2 | 3 | no | MZ-010 |
| [ ] | MZ-022 | Tag accuracy report ⚠️ | Wilson | 2 | 2.5 | no | MZ-017*, MZ-018 |
| [ ] | MZ-024 | HandleMaze state machine + skills | Bryam | 3 | 5 | no | MZ-011, MZ-020 |
| [ ] | MZ-025 | Watchdog, safe mode, wall-follower | Bryam | 3 | 2.5 | no | MZ-024 |
| [ ] | MZ-026 | Simulation campaign + report | Wilson | 3 | 3 | no | MZ-021, MZ-024 |
| [ ] | MZ-028 | Configuration tuning from lab data | Wilson | 3 | 1.5 | no | MZ-024, MZ-023*, MZ-027* |
| [ ] | MZ-029 | Attempt-flow hardening | Bryam | 4 | 2.5 | no | MZ-024, MZ-027* |
| [ ] | MZ-030 | Competition checklist + 5-min calibration | Wilson | 4 | 2.5 | no | MZ-027* |
| [ ] | MZ-032 | Code freeze tag + deploy procedure | Wilson | 4 | 1 | no | MZ-031* |
| [ ] | MZ-034 | Logistics and packing list | Wilson | Buffer | 0.5 | no | – |

### Proposed software tickets (not in the hours above)

| ID | Title | Owner | Sprint | Author h | Lab | Depends on |
|---|---|---|---|---|---|---|
| MZ-038 | Gyro heading as the maze-north reference (proposed) | Bryam | Proposed | 2 | no (uses Lab 1 logs) | MZ-017*, MZ-020 |
| MZ-039 | Decision log and run metrics (proposed) | Wilson | Proposed | 2 | no | MZ-024 |
| MZ-041 | "Positioned at start" button and start head-scan (proposed) | Bryam | Proposed | 2 | no (checked in Lab 3) | MZ-018, MZ-024 |
| MZ-042 | Maze memory reset by the sit gesture (proposed) | Bryam | Proposed | 1 | no | MZ-020, MZ-024 |

These come from the corrections review. They are small. With review time gone, the plan has room for most of them: Bryam's add 5.0 h (46.25 → 51.25 h) and Wilson's add 2.0 h (MZ-039). MZ-040, also proposed, is in the physical table. Suggested order, by value for the run: MZ-038 → MZ-042 → MZ-041 → MZ-039.

### Physical robot tickets

Bodies are in the section *Physical robot tickets*, after the proposed tickets. Lab hours are counted in the table of hours above.

| ID | Title | Owner | Sprint | Author h | Lab | Depends on |
|---|---|---|---|---|---|---|
| MZ-014 | Build the partial bench (**optional**) | Wilson | 1 | 3.5 (optional) | no | – |
| MZ-015 | Print and mount tags ⚠️ | Wilson | 1 | 1 | no | MZ-014 |
| MZ-017 | Lab 1: log capture ⚠️ | Wilson | 1 | 2 | **yes** | MZ-007, MZ-014, MZ-015 |
| MZ-023 | Lab 2: calibration of motion costs | Wilson | 2 | 2 | **yes** | MZ-016, MZ-018 |
| MZ-027 | Lab 3: integrated bench run (pairing) | Bryam + Wilson | 3 | 2.5 + 2.5 | **yes** | MZ-024 |
| MZ-031 | Lab 4: competition rehearsal (pairing) | Bryam + Wilson | 4 | 2.5 + 2.5 | **yes** | MZ-029, MZ-030 |
| MZ-033 | Lab 5: dress rehearsal (pairing) | Bryam + Wilson | Buffer | 2 + 2 | **yes** | MZ-032 |
| MZ-040 | 2×4 route test: 0 → 3 → 11 → 8 (proposed) | Wilson | Proposed | 1.5 | yes (Lab 2/3) | MZ-009, MZ-016 |
| MZ-044 | Sonar feasibility spike, log only (backlog) | TBD | Backlog | ~1.5 h (first guess) | yes (Lab 2/3) | MZ-023 |

MZ-014 is **optional**: it depends on whether a physical maze can be built at all. MZ-040 and MZ-044 are proposed/backlog and not in the hours above.

### Backlog (not scheduled, not counted)

| ID | Title | Owner | Sprint | Author h | Lab | Depends on |
|---|---|---|---|---|---|---|
| MZ-035 | Research and compare alternative maze-solving strategies (backlog, to review) | TBD | Backlog | not estimated | no | MZ-021, MZ-025 |
| MZ-036 | RL skills for in-place turns and corridor centring (backlog, after 3 Nov) | TBD | Backlog | not estimated | yes (validation) | MZ-016, MZ-024, MZ-026 |
| MZ-037 | Active perception: head and camera geometry, moving vs fixed head (backlog, to review) | TBD | Backlog | ~3 h (first guess) | yes (validation) | MZ-019, MZ-021, MZ-024 |
| MZ-043 | Rolling turns and open-area diagonals for attempt 2 (backlog) | TBD | Backlog | ~4 h (first guess) | yes (validation) | MZ-011, MZ-023*, MZ-028 |

MZ-035, MZ-036, MZ-037 and MZ-043 are **backlog** items. MZ-035 and MZ-037 are reviewed at the end of each sprint and pulled in only if there is slack. MZ-036 is planned for after the competition (3 Nov). MZ-036, MZ-037 and MZ-043 need the physical robot only for their final validation step.

---

## How to read a ticket

Every ticket has the same layout:

1. **Header:** ID, title, and ⚠️ TAG-DEP when the design depends on organiser question Q1.
2. **Field table:** Done · Owner · Estimate · Depends on · Lab / robot · Branch · Files. A `(soft)` dependency (marked `*` in the summary tables) means "uses the data if it exists, simulation otherwise".
3. **Goal:** one or two sentences on what exists after the ticket that did not exist before, and why it matters.
4. **Scope:** the concrete things to build or write, and what is left out.
5. **Steps:** only the work that is specific to the ticket. The Git cycle, the build checks and the PR creation are in *Common reference* and are not repeated.
6. **Acceptance criteria:** checkboxes that someone other than the author can verify.

---

## Common reference for every ticket

**Repository:** `https://github.com/juanulloa2050/SabanaHerons2026`. Run all commands from the
repository root.

**Build checks** (required before asking for review, except docs-only PRs):

```bash
Make/Linux/compile Develop Nao
Make/Linux/compile Develop SimRobot
Make/Linux/compile Develop Tests && Build/Linux/Tests/Develop/Tests
```

**Git cycle** (replace `MZ-0XX`, the type and the slug):

```bash
git switch Maze-Challenge
git pull
git switch -c <type>/MZ-0XX-<slug>
# ... work ...
git status
git add <files of this ticket>
git commit -m "<type>(MZ-0XX): <what changed>"
git push -u origin <type>/MZ-0XX-<slug>
cp .github/pull_request_template.md ~/pr-MZ-0XX.md   # then fill it in with a text editor
gh pr create --base Maze-Challenge --title "MZ-0XX: <title>" --body-file ~/pr-MZ-0XX.md
```

After review approval, merge with `gh pr merge <PR number> --squash --delete-branch`.

**Where lab results go.** Logs are large and never committed. `Make/Common/downloadLogs <ip>`
stores them in `Config/Logs/<date>/<robot>/`, which git ignores. Copy them to the team's shared
drive. Lab results are written in `docs/maze/LAB_LOG.md` and committed through the PR named in
each lab ticket.

---

## Sprint 0 — Foundations (Tue 6 – Fri 9 Oct)

Goal of the sprint: a working PR flow, both scenarios booting, the apriltag library linked, and a maze → simulator pipeline. Milestone M0.

### MZ-001 — Create and publish `Maze-Challenge`

| Field | Value |
|---|---|
| Done | [x] |
| Owner | Bryam |
| Estimate | 0.25 h |
| Depends on | – |
| Lab / robot | no |
| Branch | none (this ticket creates the base branch) |
| Files | – |

**Goal.** Create the integration branch that every other ticket branches from and merges into.

**Scope.** One branch, `Maze-Challenge`, cut from an up-to-date `master` and pushed to `origin`.

**Steps**
```bash
git switch master && git pull
git switch -c Maze-Challenge
git push -u origin Maze-Challenge
```

**Acceptance criteria**
- [x] `git ls-remote --heads origin Maze-Challenge` prints one line.
- [x] `Maze-Challenge` points to the same commit as `master` at creation time.

### MZ-003 — Planning documents (roadmap, tickets, Gantt)

| Field | Value |
|---|---|
| Done | [x] |
| Owner | Bryam |
| Estimate | 1 h |
| Depends on | MZ-001 |
| Lab / robot | no |
| Branch | `docs/MZ-003-roadmap` |
| Files | `docs/ROADMAP_LABERINTO.md`, `docs/TICKETS_LABERINTO.md`, `docs/GANTT_LABERINTO.md` |

**Goal.** Put the architecture, the ticket list and the schedule in the repository, so both people work from the same plan.

**Scope.**
- `ROADMAP_LABERINTO.md`: rules that drive the design, fork audit, architecture, sprints, Git workflow.
- `TICKETS_LABERINTO.md`: this file.
- `GANTT_LABERINTO.md`: Mermaid chart and the lab access request.
- Corrections that arrive later (for example `docs/CORRECCIONES.md`) are applied to these three files through this branch.

**Acceptance criteria**
- [x] The PR targets `Maze-Challenge` and is approved by Wilson.
- [x] The Mermaid Gantt renders on GitHub.

### MZ-004 — Pull request template

| Field | Value |
|---|---|
| Done | [ ] |
| Owner | Wilson |
| Estimate | 0.5 h |
| Depends on | MZ-001 |
| Lab / robot | no |
| Branch | `docs/MZ-004-pr-template` |
| Files | `.github/pull_request_template.md` (create) |

**Goal.** Every PR description has the same sections: what changes, ticket, how it was tested, risks, checklist.

**Scope.** One file, `.github/pull_request_template.md`, with **exactly** the template in section 9 of `docs/ROADMAP_LABERINTO.md` ("PR description template"). Docs only, no build needed.

**Acceptance criteria**
- [ ] Opening a new PR on GitHub pre-fills the description with the template.
- [x] The content matches section 9 of the roadmap.

### MZ-006 — Organiser questions and answers

| Field | Value |
|---|---|
| Done | [ ] |
| Owner | Wilson |
| Estimate | 0.75 h |
| Depends on | MZ-003 |
| Lab / robot | no |
| Branch | `docs/MZ-006-organizer-answers` |
| Files | `docs/maze/ORGANIZER_ANSWERS.md` (create) |

**Goal.** Get official answers to the open rules questions, and keep one record of what was asked and answered. Q1 (which walls carry tags, and at what height) is the most urgent: six tickets depend on it (TAG-DEP).

**Scope.**
- An e-mail to the organisers (through the advisor if required) with Q1, Q3 and Q5–Q11 from section 11 of the roadmap, copied word for word. Q2 is resolved by the rules and Q4 is withdrawn; both stay in the table.
- `docs/maze/ORGANIZER_ANSWERS.md`: a table with the columns `# | Question | Date asked | Answer | Date answered | Source`, updated through new commits on the same branch as answers arrive.

**Steps**
1. Send the e-mail and note the date.
2. Create the record file and open the PR.
3. When an answer arrives, add it to the table. If it is Q1, tell Bryam the same day.

**Acceptance criteria**
- [ ] The e-mail was sent by 8 Oct.
- [ ] The file lists all 11 questions with dates. Q2 says "resolved by the rules", Q4 says "withdrawn", and missing answers say "pending".
- [ ] When Q1 is answered, Bryam is told the same day.

### MZ-007 — `Maze` and `MazeSafe` scenarios, `HandleMaze` stub, start flow

| Field | Value |
|---|---|
| Done | [ ] |
| Owner | Bryam |
| Estimate | 2.5 h |
| Depends on | MZ-001 |
| Lab / robot | no (boot checked in Lab 1) |
| Branch | `feat/MZ-007-maze-scenario` |
| Files | `Config/Scenarios/Maze/`, `Config/Scenarios/MazeSafe/` (create, from `Config/Scenarios/AnyPlaceDemo/`); `Src/Modules/BehaviorControl/SkillBehaviorControl/Options/HandleMaze.h` (create); `Src/Modules/BehaviorControl/SkillBehaviorControl/SkillBehaviorControl.h` (modify: `#include`) |

**Goal.** A NAO that boots with `-s Maze`, reaches *playing* with three chest taps, and answers each head-button gesture with a spoken phrase. This is the skeleton every later behaviour ticket plugs into.

**Scope.**
- **Scenarios.** `Maze` and `MazeSafe` contain only the modules the maze needs: drop the ball, YOLO, team-communication and soccer-strategy providers. Keep `RobotPose` in `defaultRepresentations` until MZ-020.
- **Behaviour.** `skillBehaviorControl.cfg` sets `options = [HandlePhysicalRobot, HandlePlayerState, HandleMaze];`. The `HandleMaze` stub stands still in *playing* and reacts to the head buttons:

  | Gesture | Stub behaviour |
  |---|---|
  | `headFront` ×1 | Says "start" (replaced by the *State announcements* of MZ-024) |
  | `headRear` ×1 | Says "maze ready" (later: the last seen tag id, MZ-018) |
  | `headMiddle` ×2 | Says "motion test" and runs the **motion test sequence** used in Lab 2: 4 cells forward, stop, turn 90° left, 90° right, 180°, each separated by a 2 s pause and a beep |
  | `headMiddle` ×1 | Reserved for *positioned at start* (MZ-041) |

  The full button map is in roadmap 3.4.
- **Logger.** `logger.cfg` records `JPEGImage`, `CameraInfo`, `CameraMatrix`, `OdometryData`, `InertialData` (gyro drift, MZ-038), `FrameInfo`, and the maze representations once they exist. The logger records only outside *Initial* (`Src/Tools/Framework/BHLoggingController.cpp:33`), hence the 3-chest-tap start.

**Acceptance criteria**
- [ ] `Make/Linux/compile Develop Nao` and `Make/Linux/compile Develop SimRobot` pass.
- [ ] In SimRobot (`gc playing`), a simulated `headFront` tap makes the robot say "start".
- [ ] Deploy with `-s Maze` boots. Three chest taps reach *playing* (green chest LED).

### MZ-008 — Vendor `apriltag` as a prebuilt static library

| Field | Value |
|---|---|
| Done | [ ] |
| Owner | Bryam |
| Estimate | 3 h |
| Depends on | MZ-001 |
| Lab / robot | no |
| Branch | `feat/MZ-008-apriltag-lib` |
| Files | `Util/apriltag/{include,src,lib/V6,lib/Linux,build.sh,LICENSE.md}` (create); `Make/CMake/CMakeLists.txt`, `Make/CMake/B-Human.cmake` (modify) |

**Goal.** Make the AprilRobotics `apriltag` C library available to the C++ build for both the NAO and the desktop, without adding C to the CMake languages (roadmap 3.7).

**Scope.** Follow the libjpeg pattern:
- `Util/apriltag/build.sh` compiles the upstream sources (pinned tag, BSD-2 licence kept). The NAO build uses clang `--target=x86_64-linux-gnu -march=silvermont -nostdinc -isystem Util/Buildchain/V6/include -O3`. The desktop build uses `-fPIC -O3`.
- Commit `lib/V6/libapriltag.a` and `lib/Linux/libapriltag.a`.
- In `Make/CMake/CMakeLists.txt`, add imported targets `Nao::apriltag::apriltag` and `apriltag::apriltag`. In `B-Human.cmake`, link them in both branches.

**Acceptance criteria**
- [ ] Both builds link a translation unit that calls `tag36h11_create()` / `tag36h11_destroy()`.
- [ ] `Util/apriltag/build.sh` rebuilds both `.a` files from a clean checkout.
- [ ] The upstream version and commit are recorded in `Util/apriltag/README.md`.

### MZ-009 — Maze file format and generator

| Field | Value |
|---|---|
| Done | [ ] |
| Owner | Wilson |
| Estimate | 2 h |
| Depends on | MZ-004 |
| Lab / robot | no |
| Branch | `feat/MZ-009-maze-generator` |
| Files | `Util/MazeTools/generate_maze.py`, `Util/MazeTools/test_generate_maze.py`, `Util/MazeTools/README.md`, `Config/Mazes/generated/*.maze` (create) |

**Goal.** A text format for 8×8 mazes that people and programs can read, and a generator that produces valid random mazes for the simulator, the planner tests and the campaigns. Python 3, standard library only.

**Scope.**
- **Format (`.maze`)**: 17 lines of 17 characters. Even rows and even columns are corners `+`. Horizontal walls are `-`, vertical walls are `|`, and a space means no wall. The cell centre holds `S` (start), `G` (goal) or a space. Cell row 0 is the top row, matching tag id 0 at the top-left. Top-left corner example:
  ```
  +-+-+
  |S  |
  + +-+
  ```
- **Generator** `generate_maze.py`: `--seed INT` (required), `--start {0,7,56,63}` (default 0), `--goal ROW,COL` (default: the cell farthest from the start), `--loops INT` (default 0: extra walls removed to create loops), `--out PATH`. Randomized depth-first search, then these rules: outer border always closed; start cell has walls on exactly 3 sides; goal cell keeps no extra internal wall; every cell is reachable (breadth-first check).
- **Validator**: `--validate PATH` prints `OK` or the first broken rule.
- **Tests** `test_generate_maze.py` (unittest): same seed → same file; border closed; start has 3 walls; all cells reachable.

**Steps**
1. Write the generator, the validator and the tests.
2. Generate 20 mazes and run the tests:
   ```bash
   mkdir -p Config/Mazes/generated
   for s in $(seq 1 20); do python3 Util/MazeTools/generate_maze.py --seed $s --loops $((s % 4)) --out Config/Mazes/generated/maze_$s.maze; done
   python3 -m unittest discover -s Util/MazeTools -v
   ```
3. Document the format with the example above in `Util/MazeTools/README.md`.

**Acceptance criteria**
- [ ] `python3 -m unittest discover -s Util/MazeTools -v` passes.
- [ ] `--validate` prints `OK` for all 20 generated mazes.
- [ ] `Util/MazeTools/README.md` documents the format with the example above.

### MZ-010 — Maze → SimRobot scene converter

| Field | Value |
|---|---|
| Done | [ ] |
| Owner | Wilson (+ Bryam 0.5 h pairing for the scene template) |
| Estimate | 2.5 h (+ 0.5 h Bryam) |
| Depends on | MZ-007, MZ-009 |
| Lab / robot | no |
| Branch | `feat/MZ-010-maze-scene` |
| Files | `Util/MazeTools/maze_to_scene.py` (create); `Config/Scenes/Maze.ros2`, `Config/Scenes/Maze.con`, `Config/Scenes/Includes/Maze/current.rsi2` (create) |

**Goal.** Load any `.maze` file in SimRobot with walls, a start cell and one NAO, so everything downstream can be tested in simulation.

**Scope.**
- **Scene template (pairing, Bryam).** `Config/Scenes/Maze.ros2`, based on `Config/Scenes/RL1v0_3D.ros2`: scenario `Maze`, location `Default`, one robot, no ball, and it includes `Includes/Maze/current.rsi2`.
- **Converter (Wilson).** `maze_to_scene.py` turns a `.maze` file into `current.rsi2`.
- **Geometry** (from `Config/Scenes/Includes/Field2020SPL.rsi2`): in `BoxGeometry`, `depth` is the size along x, `width` along y, `height` along z. A wall along x is `depth="0.515m" width="0.015m" height="0.6m"`; along y, `depth="0.015m" width="0.515m" height="0.6m"`. Use `color="rgb(235, 235, 235)"` and `<Translation x=".." y=".." z="0.3m"/>`. The maze is centred on the simulator origin: cell `(row, col)` centre is `x = (col − 3.5)·0.5 m`, `y = (3.5 − row)·0.5 m`. The robot starts at the centre of `S`, facing the open side, at `z="320mm"`.

**Steps**
1. Generate the scene and open it:
   ```bash
   python3 Util/MazeTools/maze_to_scene.py Config/Mazes/generated/maze_1.maze --out Config/Scenes/Includes/Maze/current.rsi2
   Make/Linux/compile Develop SimRobot
   Build/Linux/SimRobot/Develop/SimRobot Config/Scenes/Maze.ros2
   ```
2. Take a screenshot from above for 3 different mazes. Attach each one to the PR together with the `.maze` file it came from.

**Acceptance criteria**
- [ ] SimRobot loads `Maze.ros2` without errors in the console.
- [ ] The top view matches the `.maze` drawing for 3 different mazes (screenshots in the PR).
- [ ] The robot stands in the start cell without touching walls.

---

## Sprint 1 — Core logic in simulation (Sat 10 – Thu 15 Oct)

Goal of the sprint: a maze library and planner proven by unit tests, oracle percepts in simulation, and the walk profile. Milestone M1.

> Physical-robot tickets of this sprint: MZ-014 (optional bench), MZ-015, MZ-017 (Lab 1). See *Physical robot tickets* below.

### MZ-011 — Maze library and time-weighted planner with unit tests

| Field | Value |
|---|---|
| Done | [ ] |
| Owner | Bryam |
| Estimate | 6 h |
| Depends on | MZ-009 |
| Lab / robot | no |
| Branch | `feat/MZ-011-maze-planner` |
| Files | `Src/Tools/Maze/MazeGraph.h`, `Src/Tools/Maze/MazePlanner.{h,cpp}`, `Src/Tools/Maze/MazeFile.{h,cpp}` (create); `Src/Apps/Tests/Maze/*.cpp` (create); `Make/CMake/Tests.cmake` (modify if the Maze sources must be added to `Tests`) |

**Goal.** The brain of the maze, as a pure C++ library: it stores what is known about the maze and plans the cheapest route **in seconds**, not in cells (turning on a biped is expensive).

**Scope.** No framework dependency.
- **`MazeGraph`**: 8×8 grid, edge state `unknown/open/wall` (roadmap 4.3), tag id ↔ cell conversion and the maze frame (roadmap 4.5).
- **`MazeFile`**: loader for `.maze` files, used by the tests and by the oracle provider (MZ-013).
- **`MazePlanner`**, on the state `(cell, heading, moving)` (roadmap 4.6):
  - *exploration*: weighted flood-fill toward the goal, unknown edges counted as **open**, ties broken by fewest turns;
  - *speed run*: A* over **known-open edges only**, unknown edges counted as **wall**.
- **`MazeCosts`**: defaults `t_fwd` 2.5 s, `t_start` 0.8 s, `t_turn90` 2.0 s, `t_turn180` 3.5 s (replaced by Lab 2 data in MZ-028).

**Steps**
1. `MazeGraph` and `MazeFile`, with a round-trip test (load, save, compare).
2. `MazePlanner` exploration and speed run.
3. Tests against the MZ-012 fixtures and expected results.

**Acceptance criteria**
- [ ] Tests compare against an exhaustive search on the MZ-012 fixtures: equal optimal cost, an admissible heuristic, and exact turn counts.
- [ ] Replanning on an 8×8 maze takes <5 ms on the desktop (measured in the tests).
- [ ] An unknown edge counts as open for exploration and as wall for the speed run (tested).

### MZ-012 — Test mazes and expected results

| Field | Value |
|---|---|
| Done | [ ] |
| Owner | Wilson |
| Estimate | 1.5 h |
| Depends on | MZ-009 |
| Lab / robot | no |
| Branch | `test/MZ-012-maze-fixtures` |
| Files | `Config/Mazes/fixtures/*.maze`, `Config/Mazes/fixtures/expected.csv`, `Util/MazeTools/solve_maze.py` (create) |

**Goal.** Ten hand-picked mazes plus their expected best route, computed by a solver that is independent of the C++ planner, so the planner (MZ-011) has something to be checked against.

**Scope.**
- **Fixtures** (`.maze`, by hand or edited from generated ones): a straight corridor; a spiral; a comb with many dead ends; two equal-length routes, one with fewer turns; a long straight route against a short zig-zag; goal in the centre; one start in each of the four corners; a maze with loops.
- **`solve_maze.py`**: Dijkstra over `(cell, heading)` with `t_fwd = 2.5`, `t_start = 0.8`, `t_turn90 = 2.0`, `t_turn180 = 3.5`. It prints `cells,turns90,turns180,time_s`.
- **`expected.csv`** with columns `file,cells,turns90,turns180,time_s`.

**Steps**
1. Draw the 10 mazes (`mkdir -p Config/Mazes/fixtures`).
2. Write `solve_maze.py`.
3. Fill `expected.csv`:
   ```bash
   for f in Config/Mazes/fixtures/*.maze; do echo "$(basename $f),$(python3 Util/MazeTools/solve_maze.py $f)"; done > Config/Mazes/fixtures/expected.csv
   ```
4. Check 2 mazes by hand (count cells and turns on the drawing) and write the result in the PR.

**Acceptance criteria**
- [ ] 10 fixtures, all `OK` with `generate_maze.py --validate`.
- [ ] `expected.csv` has 10 rows, and 2 of them were checked by hand.

### MZ-013 — Oracle maze percepts for simulation ⚠️ TAG-DEP

| Field | Value |
|---|---|
| Done | [ ] |
| Owner | Bryam |
| Estimate | 2 h |
| Depends on | MZ-010, MZ-011 |
| Lab / robot | no |
| Branch | `feat/MZ-013-oracle-percepts` |
| Files | `Src/Modules/Modeling/OracledMazePerceptsProvider/` (create); `Src/Representations/Perception/MazePercepts/{AprilTagPercept,WallPercept}.h` (create); `Config/Scenarios/Maze/threads.cfg` (sim variant) |

**Goal.** Fake perception for simulation: publish the tags and walls the robot *would* see, computed from ground truth. Behaviour, planning and mapping can then be built and tested before the real detectors (MZ-018, MZ-019) exist.

**Scope.** `OracledMazePerceptsProvider` (Cognition, simulation only) builds `AprilTagPercept` and `WallPercept` from `GroundTruthRobotPose` and the loaded `.maze` file. Parameters: Gaussian noise, dropout rate and maximum range. Tag placement follows the TAG-DEP assumption (one tag on every wall face, centre at 30 cm).

**Acceptance criteria**
- [ ] In SimRobot, the representation view shows the correct tag ids and walls for 3 mazes.
- [ ] Dropout = 1.0 produces empty percepts, which is used later by the safe-mode tests.

### MZ-016 — Corridor walk profile in simulation

| Field | Value |
|---|---|
| Done | [ ] |
| Owner | Wilson |
| Estimate | 3 h |
| Depends on | MZ-007, MZ-010 |
| Lab / robot | no |
| Branch | `config/MZ-016-walk-profile` |
| Files | `Config/Scenarios/Maze/walkingEngine.cfg`, `Config/Scenarios/Maze/walkingEngineCommon.cfg`, `Config/Scenarios/MazeSafe/walkingEngine.cfg`, `Config/Scenarios/MazeSafe/walkingEngineCommon.cfg` (create as copies) |

**Goal.** A conservative walk for the 48 cm corridor: slower, with the arms closer to the body so they do not touch the walls. `Maze` uses 60 % of the default speed and `MazeSafe` 40 %.

**Scope.** The profiles are copies of the defaults with only a few values changed. A scenario file overrides `Config/Robots/Default/` (roadmap 3.6), so the **whole file** is copied, not only the changed lines.
- `walkingEngine.cfg`: `maxSpeed` and `minSpeed` at 60 % (`Maze`) or 40 % (`MazeSafe`), for translation x, y and rotation.
- `walkingEngineCommon.cfg`, under `armParameters`: `armShoulderRoll` 7deg → 4deg, `armShoulderRollIncreaseFactor` 2 → 1, `armShoulderPitchFactor` 6 → 3.

**Steps**
1. Copy the files:
   ```bash
   cp Config/Robots/Default/walkingEngine.cfg Config/Robots/Default/walkingEngineCommon.cfg Config/Scenarios/Maze/
   cp Config/Robots/Default/walkingEngine.cfg Config/Robots/Default/walkingEngineCommon.cfg Config/Scenarios/MazeSafe/
   ```
2. Apply the values above.
3. Run 5 times in SimRobot on the "straight corridor" fixture (MZ-012) and record in the PR: the time to cross 4 cells, whether any wall was touched (watch the arms in the 3D view), and the minimum arm-to-wall distance (visual estimate).

**Acceptance criteria**
- [ ] Both scenarios build and the robot walks in simulation without falling (5/5 runs).
- [ ] 0 wall touches in 5 runs, and the times are recorded in the PR.
- [ ] Only the parameters listed above differ from `Config/Robots/Default/` (check with `diff`).

---

## Sprint 2 — Perception and localization (Fri 16 – Wed 21 Oct)

Goal of the sprint: real tags and walls detected, a pose that stays inside the cell, and a map that persists. Milestone M2.

> Physical-robot tickets of this sprint: MZ-023 (Lab 2). See *Physical robot tickets* below.

### MZ-018 — AprilTag detector ⚠️ TAG-DEP

| Field | Value |
|---|---|
| Done | [ ] |
| Owner | Bryam |
| Estimate | 5 h |
| Depends on | MZ-008, MZ-017 (soft: Lab 1 logs) |
| Lab / robot | no (validated on Lab 1 logs; rendered simulation images otherwise) |
| Branch | `feat/MZ-018-apriltag-detector` |
| Files | `Src/Modules/Perception/AprilTagDetector/AprilTagDetector.{h,cpp}` (create); `Src/Modules/Infrastructure/InterThreadProviders/PerceptionProviders.{h,cpp}` (modify: `ALIAS(AprilTagPercept)`); `Config/Scenarios/Maze/threads.cfg`, `Config/Scenarios/Maze/aprilTagDetector.cfg` (modify / create) |

**Goal.** Read the maze's `tag36h11` tags from the upper camera and publish, for each one, its id and its pose in the robot frame. This is how the robot knows which cell it is in.

**Scope.** Roadmap 4.4. Upper thread only (Cognition has no full camera image).
- Wrap `ECImage.grayscaled` as `image_u8` without copying. `quad_decimate = 2`, `nthreads = 1`.
- Tag pose with `estimate_tag_pose` using the `CameraInfo` intrinsics, transformed to the robot frame with `CameraMatrix`.
- A decision-margin threshold and a size-versus-distance consistency check in `aprilTagDetector.cfg`.
- Self-test: on the robot, a single `headRear` tap says the last seen tag id ("tag 11"), or "no tag".

**Acceptance criteria**
- [ ] Ids are correct on ≥95 % of Lab 1 frames between 0.25 and 1.5 m.
- [ ] On the NAO, `STOPWATCH("AprilTagDetector")` averages ≤20 ms per upper frame. If it does not, detect every 2nd frame (parameter).
- [ ] No detections with a wrong id on the whole log (false-positive check).
- [ ] On the robot, a single `headRear` tap says the last seen tag id (used in Lab 2 and in the 5-minute calibration).

### MZ-019 — Wall perceptor

| Field | Value |
|---|---|
| Done | [ ] |
| Owner | Bryam |
| Estimate | 4 h |
| Depends on | MZ-010, MZ-017 (soft: Lab 1 logs) |
| Lab / robot | no (simulation, plus Lab 1 logs when they exist) |
| Branch | `feat/MZ-019-wall-perceptor` |
| Files | `Src/Modules/Perception/WallPerceptor/` (create); `PerceptionProviders.{h,cpp}` (modify: `ALIAS(WallPercept)`); `Config/Scenarios/Maze/threads.cfg` |

**Goal.** Tell, from the image alone, whether there is a wall in front, to the left and to the right of the robot, and how far away it is. This feeds the map with wall evidence for every cell the robot passes.

**Scope.** Upper thread. Column scan for the floor-to-wall boundary on `ECImage` (light walls, matte floor), projected to the floor with `Transformation::imageToRobot`. Output: `WallPercept` with the distance to the wall in front, left and right when the head points that way.

**Acceptance criteria**
- [ ] In simulation, wall present or absent is ≥98 % correct for the 4 sides of visited cells (10 cells, 3 mazes).
- [ ] On Lab 1 corridor logs, the side-wall distance is within ±3 cm of the tape measurement.

### MZ-020 — Maze localizer and mapper ⚠️ TAG-DEP

| Field | Value |
|---|---|
| Done | [ ] |
| Owner | Bryam |
| Estimate | 5 h |
| Depends on | MZ-011, MZ-013 |
| Lab / robot | no |
| Branch | `feat/MZ-020-localizer-mapper` |
| Files | `Src/Modules/Modeling/MazeLocalizer/`, `Src/Modules/Modeling/MazeMapper/` (create); `Src/Representations/Modeling/MazeMap.h`, `Src/Representations/Modeling/MazeCellPose.h` (create); `Config/Scenarios/Maze/threads.cfg` (modify: `RobotPose` provided by `MazeLocalizer`, removed from `defaultRepresentations`) |

**Goal.** Know **where** the robot is (cell, heading, and a pose inside the cell) and **what** it has learned (the wall map), and keep that knowledge for the whole process.

**Scope.**
- **`MazeLocalizer`** replaces `SelfLocator` (useless in a maze). It provides `RobotPose` and `MazeCellPose`: odometry prediction, a tag fix (complementary filter, roadmap 4.5), and a heading snap on square-on tags. The gyro heading comes later in MZ-038.
- **`MazeMapper`** provides `MazeMap`: fuses wall evidence per edge with hysteresis (3 consistent observations before `unknown → wall/open`). The map is a module member, so it **persists for the whole process**. The attempt counter increments on Standby → start.

**Acceptance criteria**
- [ ] In simulation with noisy odometry, the pose error is ≤8 cm and ≤10° after 10 cells, and the cell estimate is 100 % correct.
- [ ] After a simulated pick-up and Standby, the map is identical and the attempt counter = 2.
- [ ] With tags hidden (dropout 1.0), the localizer reports low quality (used by MZ-025).

### MZ-021 — Headless simulation campaign runner

| Field | Value |
|---|---|
| Done | [ ] |
| Owner | Wilson |
| Estimate | 3 h |
| Depends on | MZ-010 |
| Lab / robot | no |
| Branch | `feat/MZ-021-sim-runner` |
| Files | `Util/MazeTools/run_sim_campaign.py` (create) |

**Goal.** Run many mazes in SimRobot one after another without clicking, and write one CSV row per run. It is the measuring tool for MZ-025, MZ-026 and every strategy comparison.

**Scope.** Use the headless `pybh` mode of `docs/RL/Environment.md` ("Backend"). If that is not usable, start `Build/Linux/SimRobot/Develop/SimRobot` once per maze with a timeout.
- Options: `--mazes Config/Mazes/generated`, `--attempts 2`, `--noise {low,mid,high}`, `--timeout-s 300`, `--out results.csv`.
- For each maze: run `maze_to_scene.py`, start the simulation, read `BehaviorStatus` / `MazeMap` through the debug output (Bryam provides the field names in the PR review), and write `maze,attempt,noise,reached_goal,time_s,wall_contacts,max_stall_s`.

**Steps**
1. Write the script and test it on 2 mazes.
2. Put the CSV in the PR description.

**Acceptance criteria**
- [ ] `python3 Util/MazeTools/run_sim_campaign.py --mazes Config/Mazes/fixtures --attempts 1 --noise low --out /tmp/r.csv` runs 2 mazes without manual clicks.
- [ ] The CSV has the 7 columns listed above.

### MZ-022 — Tag accuracy report ⚠️ TAG-DEP

| Field | Value |
|---|---|
| Done | [ ] |
| Owner | Wilson |
| Estimate | 2.5 h |
| Depends on | MZ-017 (the Lab 1 logs are required to close this ticket), MZ-018 |
| Lab / robot | no (recorded logs) |
| Branch | `docs/MZ-022-tag-accuracy` |
| Files | `docs/maze/TAG_ACCURACY.md`, `docs/maze/LAB_LOG.md`, `docs/maze/BENCH.md` (create) |

**Goal.** Measure how well the detector works on **real** images: detection rate, id correctness and distance error, against the tape-measure values written in Lab 1. Also commits the Lab 1 notes and the bench sheet.

**Scope.**
- `TAG_ACCURACY.md`: one row per recorded position (15: 5 distances × 3 angles) and a summary (detection rate per distance, mean error, worst case).
- `LAB_LOG.md`: the Lab 1 notes (MZ-017). `BENCH.md`: the bench sheet (MZ-014, MZ-015). If the bench or the lab did not happen, those two files are skipped and this ticket cannot be closed.

**Steps**
1. Open the replay scene and choose a Lab 1 log:
   ```bash
   Build/Linux/SimRobot/Develop/SimRobot Config/Scenes/ReplayRobot.ros2
   ```
2. For each of the 15 positions, jump to its timestamp and read `AprilTagPercept` in the representation view (id, distance, angle).
3. Fill this table in `TAG_ACCURACY.md`, then add the summary:

   | true distance | true angle | detected (y/n) | id ok | measured distance | error % |
   |---|---|---|---|---|---|

**Acceptance criteria**
- [ ] All 15 positions are in the table.
- [ ] The summary states whether the MZ-018 targets are met (≥95 % detection, ≤5 % distance error up to 1.5 m).

---

## Sprint 3 — Behaviour and integration (Thu 22 – Sun 25 Oct)

Goal of the sprint: the full competition state machine, a watchdog that never lets the robot stall, and evidence from a simulation campaign. Milestone M3.

> Physical-robot tickets of this sprint: MZ-027 (Lab 3). See *Physical robot tickets* below.

### MZ-024 — `HandleMaze` state machine and maze skills

| Field | Value |
|---|---|
| Done | [ ] |
| Owner | Bryam |
| Estimate | 5 h |
| Depends on | MZ-011, MZ-020 |
| Lab / robot | no |
| Branch | `feat/MZ-024-handle-maze` |
| Files | `Src/Modules/BehaviorControl/SkillBehaviorControl/Options/HandleMaze.h` (modify); `Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Maze/{MazeFollowCorridor,MazeTurn,MazeScanWalls}.cpp` (create); `Src/Representations/BehaviorControl/SkillInterfaces.h` (modify); `Src/Modules/BehaviorControl/MazePlannerProvider/`, `Src/Representations/BehaviorControl/MazePlan.h` (create); `Config/Scenarios/Maze/handleMaze.cfg`, `Config/Scenarios/Maze/mazePlanner.cfg` (create) |

**Goal.** Replace the MZ-007 stub with the real competition behaviour: the robot explores, reaches the goal, waits for the second attempt and runs the fastest known route, and it tells the operator out loud what it is doing.

**Scope.**
- **States and transitions** of roadmap 4.7: `WaitingForStart`, `MazeExploration`, `ReturnToStart` (stretch), `Standby`, `SpeedRun`, `Finished`. The states `Unpositioned`, `StartScan` and `Sitting` come with MZ-041 and MZ-042.
- **`MazePlannerProvider`** wraps the MZ-011 library and provides `MazePlan` (cell path, action list, estimated time). It reads the motion costs from `mazePlanner.cfg`, created here with the `MazeCosts` defaults (`t_fwd` 2.5 s, `t_start` 0.8 s, `t_turn90` 2.0 s, `t_turn180` 3.5 s). MZ-028 replaces them with the Lab 2 means.
- **Skills**, built on `WalkToPose` (empty obstacle avoidance) and `LookAtAngles`: `MazeFollowCorridor` (centre the corridor, heading lock), `MazeTurn` (in place, at cell centres only), `MazeScanWalls` (head pan ≤2 s, only on unknown edges).
- **State announcements** (roadmap 4.7): every state change and button gesture is spoken with the `Say` skill, behind `announce.enabled` in `handleMaze.cfg`.

**Acceptance criteria**
- [ ] In simulation, the full flow works on 3 mazes: WaitingForStart → Exploration → goal → pick-up → Standby → SpeedRun → Finished.
- [ ] In attempt 2, the plan uses known-open edges only (unit-tested in MZ-011, checked in a log).
- [ ] Each row of the *State announcements* table is spoken (check the console `Say` output in SimRobot, and on the robot in Lab 3). "exploring" is spoken again on a second run.
- [ ] With `announce.enabled = false`, nothing is spoken.
- [ ] Changing `t_turn90` in `mazePlanner.cfg` changes the plan on a maze where two routes differ only by their turns (check in simulation).

### MZ-025 — Watchdog, safe mode and wall-follower fallback

| Field | Value |
|---|---|
| Done | [ ] |
| Owner | Bryam |
| Estimate | 2.5 h |
| Depends on | MZ-024 |
| Lab / robot | no |
| Branch | `feat/MZ-025-watchdog` |
| Files | `HandleMaze.h` (modify); `Config/Scenarios/Maze/handleMaze.cfg` (modify: watchdog parameters) |

**Goal.** The robot never stands still for more than 3 s without a reason (the judge may end the attempt after 5 s), and it keeps making progress when its perception fails.

**Scope.** Three escalating layers (roadmap 4.7):
1. **Watchdog:** if no motion command has made progress for 3 s, replan.
2. **Safe mode:** if the pose is lost (low localizer quality), walk cell by cell with a tag re-fix at every cell and a reduced speed. Says "safe mode".
3. **Wall-follower:** if safe mode also fails, follow the left wall. It guarantees progress in mazes without loops around the goal. Says "wall follower".

The thresholds live in `handleMaze.cfg`.

**Acceptance criteria**
- [ ] In simulation, no stall is longer than 3 s in 20 runs (CSV from MZ-021).
- [ ] With oracle dropout = 1.0, safe mode and then the wall-follower take over, and the robot reaches the goal in a maze without loops.

### MZ-026 — Simulation campaign and report

| Field | Value |
|---|---|
| Done | [ ] |
| Owner | Wilson |
| Estimate | 3 h |
| Depends on | MZ-021, MZ-024 |
| Lab / robot | no |
| Branch | `docs/MZ-026-sim-results` |
| Files | `docs/maze/SIM_RESULTS.md` (create) |

**Goal.** Evidence that the whole software solves mazes it has never seen, in both attempts and under three noise levels, plus the list of mazes where it fails.

**Scope.** Run `run_sim_campaign.py` on the 20 generated mazes, 2 attempts, noise `low`, `mid` and `high`. `SIM_RESULTS.md` has one table per noise level (success rate, mean T1, mean T2, mean Tfinal = 0.3·T1 + 0.7·T2, wall contacts, max stall) and the 3 worst mazes with a short note on what went wrong.

**Steps**
1. Run the campaign:
   ```bash
   for n in low mid high; do python3 Util/MazeTools/run_sim_campaign.py --mazes Config/Mazes/generated --attempts 2 --noise $n --out ~/sim_$n.csv; done
   ```
2. Write `docs/maze/SIM_RESULTS.md`.

**Acceptance criteria**
- [ ] 20 mazes × 2 attempts × 3 noise levels were run, or the report says which runs failed to start and why.
- [ ] The report says whether the Sprint 3 target is met (≥18/20 solved at noise "mid").

### MZ-028 — Configuration tuning from lab data

| Field | Value |
|---|---|
| Done | [ ] |
| Owner | Wilson |
| Estimate | 1.5 h |
| Depends on | MZ-024 (creates `mazePlanner.cfg`), MZ-023 (soft), MZ-027 (soft) |
| Lab / robot | no (needs the numbers from Labs 2 and 3; if there are none, the defaults stay) |
| Branch | `config/MZ-028-lab-tuning` |
| Files | `Config/Scenarios/Maze/mazePlanner.cfg`, `Config/Scenarios/Maze/walkingEngine*.cfg`, `Config/Scenarios/MazeSafe/walkingEngine*.cfg` (modify); `docs/maze/LAB_LOG.md` (modify) |

**Goal.** Replace the guessed motion costs and speeds with the values measured on the real floor.

**Scope.**
- `mazePlanner.cfg`: `t_fwd`, `t_start`, `t_turn90` and `t_turn180` set to the Lab 2 means (rounded to 0.1 s).
- If Lab 3 showed wall contacts or clearance <5 cm, lower the `maxSpeed` x of `Maze` by 10 % (only that value) and write the reason in the PR.
- The Lab 2 and Lab 3 tables go into `docs/maze/LAB_LOG.md`.

**Steps**
1. Edit the configs.
2. Rebuild and check in SimRobot that the robot still walks:
   ```bash
   Make/Linux/compile Develop SimRobot
   Build/Linux/SimRobot/Develop/SimRobot Config/Scenes/Maze.ros2
   ```

**Acceptance criteria**
- [ ] The config values equal the measured means (rounded to 0.1 s).
- [ ] Every changed value is justified in the PR with a reference to `LAB_LOG.md`.

---

## Sprint 4 — Hardening and code freeze (Mon 26 – Wed 28 Oct)

Goal of the sprint: an attempt flow that survives the real world, a written competition procedure, and a frozen, tagged build.

> Physical-robot tickets of this sprint: MZ-031 (Lab 4). See *Physical robot tickets* below.

### MZ-029 — Attempt-flow hardening

| Field | Value |
|---|---|
| Done | [ ] |
| Owner | Bryam |
| Estimate | 2.5 h |
| Depends on | MZ-024, MZ-027 (soft: Lab 3 failures) |
| Lab / robot | no (checked in Lab 4) |
| Branch | `fix/MZ-029-attempt-flow` |
| Files | `HandleMaze.h`, `Src/Modules/Modeling/MazeLocalizer/`, `Src/Modules/Modeling/MazeMapper/` (modify) |

**Goal.** The two-attempt flow survives what happens on a real competition table: the robot is picked up, it falls, and it still has its map and its position.

**Scope.**
- **Pick-up** (ground contact lost) → `Standby`: keep the map, reset the pose to the stored start cell, wait for the tap.
- **Fall** → get up → relocalize from the next tag seen.
- The map persists across both attempts.
- Fixes for any failure found in Lab 3.

**Acceptance criteria**
- [ ] In simulation, a fall in the middle of exploration recovers and the goal is still reached.
- [ ] Every Lab 3 failure has a fix or a documented decision.

### MZ-030 — Competition checklist and 5-minute calibration procedure

| Field | Value |
|---|---|
| Done | [ ] |
| Owner | Wilson |
| Estimate | 2.5 h |
| Depends on | MZ-027 (soft: Lab 3 findings) |
| Lab / robot | no |
| Branch | `docs/MZ-030-competition-checklist` |
| Files | `docs/maze/COMPETITION_CHECKLIST.md` (create) |

**Goal.** A printable checklist that one person can follow on competition day without improvising, including the 5-minute calibration drill.

**Scope.** `COMPETITION_CHECKLIST.md` with 4 sections, every item with a checkbox:
1. **Day before:** batteries, laptop, Ethernet cable, frozen build compiled in Release, `MazeSafe` build ready, printed checklist.
2. **Calibration Zone (≤5 min):** the table of roadmap section 7, plus a "result" column to fill in by hand.
3. **Our turn:** code loaded only by Ethernet (`Make/Common/deploy Release <ip> -s Maze -l Default -w NONE -b`); Wi-Fi off (profile `NONE`) and verified; 3 chest taps → *playing*; *positioned* tap and, after the whistle, `headFront` within 10 s; never hold the chest button between attempts; never hold the 3 head buttons between attempts (map reset, MZ-042); touch the robot only when the judge allows it.
4. **After our turn:** verify the robot is not connected to any wireless network, then store the logs (`Make/Common/downloadLogs <ip>`).

It also lists the conservative defaults of roadmap section 6.

**Acceptance criteria**
- [ ] It fits on 2 printed pages.
- [ ] Every item has a checkbox and can be checked by one person.

### MZ-032 — Code freeze and deploy procedure

| Field | Value |
|---|---|
| Done | [ ] |
| Owner | Wilson |
| Estimate | 1 h |
| Depends on | MZ-031 (soft: Lab 4 notes) |
| Lab / robot | no |
| Branch | `docs/MZ-032-code-freeze` |
| Files | `docs/maze/DEPLOY.md` (create); `docs/maze/LAB_LOG.md` (Lab 4 notes) |

**Goal.** Freeze the code on 28 Oct with a git tag, and write the exact commands to rebuild and deploy that tag, so the competition build is reproducible.

**Scope.** `DEPLOY.md` with:
```bash
git switch Maze-Challenge && git pull
git checkout maze-freeze-2026-10-28
Make/Linux/compile Release Nao
Make/Common/deploy Release <ip> -s Maze -l Default -w NONE -b
# fallback:
Make/Common/deploy Release <ip> -s MazeSafe -l Default -w NONE -b
```
Plus the Lab 4 notes in `LAB_LOG.md`.

**Steps**
1. Write `DEPLOY.md` and the Lab 4 notes, and open the PR.
2. **After the PR is merged** (on 28 Oct), tag the frozen commit:
   ```bash
   git switch Maze-Challenge && git pull
   git tag -a maze-freeze-2026-10-28 -m "Maze challenge code freeze"
   git push origin maze-freeze-2026-10-28
   ```

**Acceptance criteria**
- [ ] `git ls-remote --tags origin maze-freeze-2026-10-28` prints one line.
- [ ] `Make/Linux/compile Release Nao` passes on the tagged commit.

---

## Buffer — 29 Oct – 2 Nov (no new development)

> Physical-robot tickets of this sprint: MZ-033 (Lab 5). See *Physical robot tickets* below.

### MZ-034 — Logistics and packing list

| Field | Value |
|---|---|
| Done | [ ] |
| Owner | Wilson |
| Estimate | 0.5 h |
| Depends on | – |
| Lab / robot | no |
| Branch | none |
| Files | – |

**Goal.** Nothing is forgotten on 3 Nov, and the team is at Campus Ciudad de México before 9:00.

**Scope.**
- A packing list: NAO v6 in its case, charger and spare battery; laptop with the frozen build and charger; 2 Ethernet cables and a USB-Ethernet adapter; the printed checklist (MZ-030), spare printed tags, tape measure, floor tape.
- Confirmed transport and arrival time.

**Acceptance criteria**
- [ ] The list is shared with the team by 1 Nov, and every item is checked off on 2 Nov.

---

## Proposed (from the corrections review, not in the hour plan)

These tickets turn the notes in `docs/CORRECCIONES.md` into work. They are small and mostly touch `HandleMaze` and the localizer, so they should be built after MZ-024. Suggested order: MZ-038 → MZ-042 → MZ-041 → MZ-039.

### MZ-038 — Gyro heading as the maze-north reference

| Field | Value |
|---|---|
| Owner | Bryam |
| Estimate | 2 h |
| Depends on | MZ-017 (the Lab 1 log must include `InertialData`), MZ-020 |
| Lab / robot | no (drift measured on the Lab 1 logs) |
| Branch | `feat/MZ-038-gyro-heading` |
| Files | `Src/Modules/Modeling/MazeLocalizer/` (modify); `Src/Representations/Modeling/MazeCellPose.h` (modify: `headingSource`, `headingDrift`); `Config/Scenarios/Maze/logger.cfg` (add `InertialData`); `docs/maze/GYRO_HEADING.md` (create) |

**Goal and scope.** Make the robot always know where it faces (roadmap 4.5):
- Store the yaw of `InertialData.orientation3D` at the *positioned at start* moment as the maze-north offset. `heading = snap90(yaw − offset)`.
- Re-align the offset on every square-on tag (walls are axis-aligned).
- Use the heading to infer the wall face of a seen tag, and to reject a tag whose normal does not match the heading (a false positive filter).
- The NAO has no magnetometer, so the yaw drifts. The drift rate decides how often a tag fix is needed.

**Steps**
1. Add `InertialData` to the logger list, so Lab 1 logs carry it (a one-line change, do it before 13 Oct).
2. From the Lab 1 logs, plot the yaw drift over a 5 min stand and a corridor walk. Write the numbers in `GYRO_HEADING.md`.
3. Implement the offset, the snap and the tag-based re-alignment. Unit-test the snap with ±44° and ±46° inputs.
4. **Experiment:** in simulation and on the bench, put tags **only on the north face** and check that the localizer still keeps the cell estimate when the robot faces north. Document the coverage loss on other headings.

**Acceptance criteria**
- [ ] The drift rate (° per minute) is measured and recorded.
- [ ] In simulation with noisy odometry and a drifting gyro, the heading error stays ≤10° after 10 cells with tag fixes, and the cell estimate is 100 % correct.
- [ ] The north-face-only experiment is documented, with the coverage consequence.

### MZ-039 — Decision log and run metrics

| Field | Value |
|---|---|
| Owner | Wilson |
| Estimate | 2 h |
| Depends on | MZ-024 |
| Lab / robot | no (used in every lab afterwards) |
| Branch | `feat/MZ-039-decision-log` |
| Files | `Src/Representations/Modeling/MazeDecisionLog.h` (create, streamable); `Src/Modules/BehaviorControl/SkillBehaviorControl/Options/HandleMaze.h` (modify: emit events); `Config/Scenarios/Maze/logger.cfg`; `Util/MazeTools/summarize_run.py` (create) |

**Goal and scope.** A log that says **why** the robot did what it did, not only where it was.
The B-Human logger already records representations outside *Initial*. This ticket adds one
small event representation and a script that turns a log into a table.

Each event holds: time, state, cell pose and heading, the plan action, a reason code (`plan`, `replan`, `watchdog`, `tagFix`, `safeMode`, `wallFollower`), the map edges that changed, the tag ids used with the pose correction they caused (cm and °), the measured time for each cell against the plan estimate, stalls longer than 1 s, and suspected wall contacts.

`summarize_run.py` prints per attempt: total time, cells walked, replans, turns, tag fixes (and the mean correction), watchdog triggers, and "planned vs real time per cell". The last column feeds the Lab 2 calibration of `t_fwd` and `t_turn` (MZ-023, MZ-028).

**Steps**
1. Define the event struct and the reason codes.
2. Emit events from `HandleMaze`, `MazeLocalizer` and `MazeMapper` at each decision.
3. Write `summarize_run.py` and test it on a simulation log. The same fields extend the MZ-021 CSV.

**Acceptance criteria**
- [ ] A simulation run produces a log, and `summarize_run.py` prints the per-attempt table.
- [ ] Every watchdog trigger and every replan in the run has an event with a reason code.
- [ ] The log adds <1 ms per frame on average (`STOPWATCH`).

### MZ-041 — "Positioned at start" button and start head-scan

| Field | Value |
|---|---|
| Owner | Bryam |
| Estimate | 2 h |
| Depends on | MZ-018 (tag detector), MZ-024 |
| Lab / robot | no (checked in Lab 3) |
| Branch | `feat/MZ-041-start-scan` |
| Files | `HandleMaze.h` (modify); `Config/Scenarios/Maze/handleMaze.cfg` and `Config/Scenarios/MazeSafe/handleMaze.cfg` (add parameters) |

**Goal and scope.** The robot starts in the centre of the start cell, facing the open side. From
there it probably does not see any tag (the walls are at its back and sides). Two buttons
fix that:
- `headMiddle` ×1 = **positioned at start** (the operator has placed the robot). The robot starts collecting information.
- `headFront` ×1 = **start** (the referee's whistle), as before.

Between the two, an optional **start-scan** pans the head from side to side for
`startScan.durationMs` (default 5000 ms), reading the tags and walls already visible. Then the
head looks ahead and the robot waits. It identifies the start corner (0/7/56/63) and the
maze-north offset (MZ-038) there.

The scan is a parameter, because whether it is allowed depends on when the clock starts (Q11):
- `startScan.enabled`, `startScan.durationMs`, `startScan.yawRangeDeg`, `startScan.pitchDeg` (the tag is low: ~30 cm up, ~25 cm away from the side walls, so the head must pitch down).
- Deploy-time: the values live in `handleMaze.cfg`, so a redeploy changes them.
- Simulation: the same `.cfg` is loaded by SimRobot; change it there and re-run. Check in step 1 that `set parameters:` works at run time for the module, and note the result in the PR.

**Steps**
1. Add the parameters and the `Unpositioned` and `StartScan` states (roadmap 4.7). A `headFront` tap during the scan starts the run immediately.
2. Make the map collect nothing before *positioned*.
3. Add the sequence to the simulation scene tests: positioned → scan → start.

**Acceptance criteria**
- [ ] With `startScan.enabled = false`, *positioned* goes straight to `WaitingForStart`.
- [ ] With it enabled, the head pans for the configured time, then looks ahead and waits.
- [ ] In simulation, the start corner id is read during the scan on at least 3 generated mazes.
- [ ] The robot says "positioned", then "scanning" (if enabled), then "ready" (roadmap 4.7).

### MZ-042 — Maze memory reset by the sit gesture

| Field | Value |
|---|---|
| Owner | Bryam |
| Estimate | 1 h |
| Depends on | MZ-020, MZ-024 |
| Lab / robot | no |
| Branch | `feat/MZ-042-map-reset` |
| Files | `Src/Modules/Modeling/MazeMapper/` and `Src/Modules/Modeling/MazeLocalizer/` (modify); `HandleMaze.h` (modify) |

**Goal and scope.** Holding the 3 head buttons for 1 s is an existing B-Human gesture: it sets
`playerState = unstiff` and the robot sits (`GameStateProvider.cpp`). The maze reuses it:
- While `playerState == unstiff`, the mapper collects nothing.
- Entering that state clears `MazeMap`, the pose and the attempt counter.
- After the chest taps bring the robot back to *playing*, it says "map cleared", and collecting starts only after *positioned at start* (MZ-041).

This is the only way to clear the map without a redeploy or a crash. The same gesture between
attempts would destroy the map, so it is in the competition checklist (roadmap 10): do not
hold the head buttons between attempts.

**Acceptance criteria**
- [ ] In simulation, `playerState = unstiff` clears the map and the counter. Standby (pick-up) does **not**.
- [ ] After the reset, the robot explores as attempt 1.
- [ ] The robot says "map cleared" when it sits. Check in step 1 that speech works while the robot is unstiff, and note the result in the PR.
- [ ] The checklist item is in `docs/maze/COMPETITION_CHECKLIST.md` (MZ-030).

---

## Physical robot tickets (testing method not settled)

These tickets need the NAO, the lab or a physical maze. We are **not sure how we will test on hardware** (lab access, a physical maze and robot time are not guaranteed), so no software ticket may depend on them in a hard way. Their dates and hours are the plan if everything is available.

If a lab session is lost, the fallback is: keep the code frozen on simulation results, use the `Maze` conservative defaults, and treat the competition's 5-minute calibration zone (roadmap section 7) as the first real test.

Order: bench (MZ-014, MZ-015) → Lab 1 (MZ-017) → Lab 2 (MZ-023) → Lab 3 (MZ-027) → Lab 4 (MZ-031) → Lab 5 (MZ-033), then the proposed and backlog tickets that need the robot (MZ-040, MZ-044).

### MZ-014 — Build the partial bench (optional)

| Field | Value |
|---|---|
| Status | **Optional.** We do not know yet whether a physical maze can be built. Without it, MZ-015, MZ-017 and MZ-027 lose their walls and run on floor tape only (corridor width and wall clearance are then checked in simulation). |
| Owner | Wilson |
| Estimate | 3.5 h |
| Depends on | – |
| Lab / robot | no (built in a workshop, assembled in Lab 1) |
| Branch | none (results committed in the PR of MZ-022) |
| Files | `docs/maze/BENCH.md` (created in MZ-022) |

**Goal.** Six free-standing panels that can be arranged as a corridor, a corner, a T-junction and a dead end, so the robot can walk between real walls of the competition's size.

**Scope.** Panels of **50 cm wide × 60 cm high** in 10 mm foam board (or 12–15 mm MDF), light matte colour (white), with two feet each. A floor-tape grid of 50 cm cells (2×3) and a 1 m straight line for the calibration drill. A sheet with the 4 layouts: corridor of 2 cells (4 panels), dead end (5), corner (4), T (5).

**Steps**
1. Cut the 6 panels. The 1–2 cm thickness matches the rules.
2. Give each panel two feet (L-brackets or wooden blocks) so it stands without leaning. Join panels at corners with clamps or strong tape.
3. Make the floor tape grid and the 1 m line.
4. Draw the 4 layouts in `docs/maze/BENCH.md` (ASCII is fine) with the panel count of each.
5. Measure and record: each panel's width, height and thickness (±1 mm); the corridor inner width when assembled (target 48–49 cm); the floor type; the light level (phone lux app).

**Acceptance criteria**
- [ ] The 6 panels stand alone, and a light push does not tip them.
- [ ] The measured corridor width is 48–49 cm.
- [ ] `docs/maze/BENCH.md` is written (committed later with MZ-022).

### MZ-015 — Print and mount tags ⚠️ TAG-DEP

| Field | Value |
|---|---|
| Owner | Wilson |
| Estimate | 1 h |
| Depends on | MZ-014 |
| Lab / robot | no |
| Branch | none (documented in `docs/maze/BENCH.md`) |
| Files | – |

**Goal.** Seven printed `tag36h11` tags at the exact size of the rules, glued to the bench panels at the height we assume, so the detector sees what it will see in the competition.

**Scope.** Ids 0, 1, 7, 8, 9, 56 and 63, from the AprilRobotics `apriltag-imgs` repository (folder `tag36h11`). The **black outer square measures 150 mm**, printed on matte paper (no glossy finish), glued **centred horizontally with the tag centre 30 cm above the floor** (the TAG-DEP assumption, to be changed if Q1 says otherwise).

**Steps**
1. Download and scale the images so the black outer square is 150 mm.
2. Print and measure each one with a ruler. Reject anything outside 150 ± 1 mm.
3. Glue each tag to a panel and record the mounting height.

**Acceptance criteria**
- [ ] 7 tags are printed and measured at 150 ± 1 mm, with the measurements recorded.
- [ ] The tags are flat (no bubbles), and the mounting height is recorded.

### MZ-017 — Lab 1 (Tue 13 Oct, 2 h): log capture ⚠️ TAG-DEP

| Field | Value |
|---|---|
| Owner | Wilson (Bryam on call by phone) |
| Estimate | 2 h (lab) |
| Depends on | MZ-007, MZ-014, MZ-015 |
| Lab / robot | **yes** |
| Branch | none (results committed in the PR of MZ-022) |
| Files | `docs/maze/LAB_LOG.md` (created in MZ-022); logs in `Config/Logs/` and the shared drive |

**Goal.** Record real camera, odometry and gyro logs, so the tag detector (MZ-018) and the wall perceptor (MZ-019) can be developed offline, and the gyro drift can be measured (MZ-038).

**Scope.** Two recordings with the robot booted in `-s Maze` (which also confirms MZ-007 on hardware): a **tag sweep** (one tag panel at 0.25, 0.5, 1.0, 1.5 and 2.0 m, each at 0°, 30° and 60°, 5 s per position) and a **corridor walk** (the 2-cell corridor, robot standing 5 s at both ends and in the middle, moved by hand). `InertialData` must be in the log.

**Steps**
1. Deploy by Ethernet and start the robot. Three chest taps give *playing* (green chest LED), and the logger starts recording.
   ```bash
   Make/Common/deploy Develop <robot-ip> -s Maze -l Default -w NONE -b
   ```
2. Tag sweep. Write each position and its time in a notebook.
3. Corridor walk.
4. Stop the robot by holding all three head buttons for 1 s (unstiff), then download the logs and copy `Config/Logs/<date>/` to the shared drive:
   ```bash
   Make/Common/downloadLogs <robot-ip>
   ```

**Acceptance criteria**
- [ ] At least 15 tag positions are recorded (5 distances × 3 angles) with their timestamps.
- [ ] The logs open in SimRobot with `Config/Scenes/ReplayRobot.ros2` (check one before leaving the lab).
- [ ] The robot booted with `-s Maze` and the chest flow worked (this confirms MZ-007 on the real robot).

### MZ-023 — Lab 2 (Tue 20 Oct, 2 h): calibration of motion costs

| Field | Value |
|---|---|
| Owner | Wilson (Bryam on call by phone) |
| Estimate | 2 h (lab) |
| Depends on | MZ-016, MZ-018 |
| Lab / robot | **yes** |
| Branch | none (numbers committed in the PR of MZ-028) |
| Files | `docs/maze/LAB_LOG.md` (updated in MZ-028) |

**Goal.** Measure on the real lab floor how long each motion takes and how precise it is. These numbers replace the planner's default costs (MZ-028).

**Scope.** 10 repetitions each, with a stopwatch, using the motion test sequence (**double** `headMiddle` tap, MZ-007):
- `t_fwd`: time per 50 cm while already walking (4 cells divided by 4);
- `t_start`: extra time for the first cell from standstill;
- `t_turn90` and `t_turn180`: in place.

For each straight run, the lateral drift over 1 m. For each turn, the heading error against floor tape marks. A live tag check under the lab lighting: the robot says the id on a `headRear` tap, at 0.5, 1.0 and 1.5 m.

**Steps**
1. Deploy and start (3 chest taps):
   ```bash
   Make/Common/deploy Develop <robot-ip> -s Maze -l Default -w NONE -b
   ```
2. Run the measurements above and write them in a notebook.
3. Download the logs: `Make/Common/downloadLogs <robot-ip>`.

**Acceptance criteria**
- [ ] 10 measurements per motion, with mean and standard deviation in `LAB_LOG.md`.
- [ ] The live tag check passes at 0.5 m and 1.0 m.

### MZ-027 — Lab 3 (Fri 23 Oct, 2.5 h): integrated bench run · pairing

| Field | Value |
|---|---|
| Owner | Bryam + Wilson |
| Estimate | 2.5 h each (lab) |
| Depends on | MZ-024 |
| Lab / robot | **yes** |
| Branch | none (results committed in the PR of MZ-028) |
| Files | `docs/maze/LAB_LOG.md` |

**Goal.** The first time the full software runs on the real robot between real walls: does it keep its clearance, choose the right branch and recover from a dead end?

**Scope.** The 4 bench layouts (corridor, corner, T, dead end), 5 runs each where time allows. Wilson measures the minimum lateral clearance (ruler at the closest point, video if possible) and counts wall contacts. Bryam watches the representations live over Ethernet. Each run goes in `LAB_LOG.md` with layout, result, contacts, clearance and notes.

**Acceptance criteria**
- [ ] Corridor and corner: 5 runs each with 0 contacts and clearance ≥5 cm.
- [ ] T and dead end: the correct branch is chosen (dead end detected, robot turns around).
- [ ] Every failure has a log name and a note.

### MZ-031 — Lab 4 (Tue 27 Oct, 2.5 h): competition-mode rehearsal · pairing

| Field | Value |
|---|---|
| Owner | Bryam + Wilson |
| Estimate | 2.5 h each (lab) |
| Depends on | MZ-029, MZ-030 |
| Lab / robot | **yes** |
| Branch | none (notes appended to `docs/maze/LAB_LOG.md` in the PR of MZ-032) |
| Files | `docs/maze/LAB_LOG.md` |

**Goal.** Run exactly as on competition day, following the printed checklist, and correct the checklist where it is unclear.

**Scope.** The timed 5-minute calibration drill, then complete attempt-1 → Standby → attempt-2 cycles on the bench with no human touch except the pick-up.

**Acceptance criteria**
- [ ] The 5-minute calibration drill is completed in ≤5 min (timed).
- [ ] 3 full attempt-1 → Standby → attempt-2 cycles on the bench, with no human touch except the pick-up.
- [ ] Every checklist item that was unclear is corrected in the checklist.

### MZ-033 — Lab 5 (Fri 30 Oct, 2 h): dress rehearsal · pairing

| Field | Value |
|---|---|
| Owner | Bryam + Wilson |
| Estimate | 2 h each (lab) |
| Depends on | MZ-032 |
| Lab / robot | **yes** |
| Branch | none, unless a blocking fix is needed (`fix/MZ-033-<slug>`, reviewed by both) |
| Files | – |

**Goal.** One full run of the competition procedure with the frozen tag, from deploy to "after our turn", with no changes allowed except blocking fixes.

**Scope.** The whole printed checklist, timed. Only blocking problems may be fixed, through a PR reviewed by both people. At the end, the decision between `Maze` and `MazeSafe` defaults is written down.

**Acceptance criteria**
- [ ] The full checklist is done once, timed, with no improvisation.
- [ ] The decision about `Maze` or `MazeSafe` defaults is written down.

### MZ-040 — 2×4 route test: 0 → 3 → 11 → 8

| Field | Value |
|---|---|
| Owner | Wilson |
| Estimate | 1.5 h |
| Depends on | MZ-009 (generator), MZ-016 (walk profile) |
| Lab / robot | **yes**, runs inside Lab 2 and Lab 3 (no extra room time) |
| Branch | `test/MZ-040-route-2x4` |
| Files | `Config/Mazes/fixtures/route_2x4.maze` (create); `docs/maze/BENCH.md` (add the floor layout); the motion test of MZ-007 (add the route) |

**Goal and scope.** The lab cannot hold an 8×8, but a 2×4 block holds the motions that matter. It
keeps the real tag ids (`id = row·8 + col`): cells 0–3 in row 0 and cells 8–11 in row 1.
Route: straight 3 cells (0 → 3), turn right, 1 cell (3 → 11), turn right, straight 3 cells (11 → 8). See roadmap section 6.

**Steps**
1. Write the fixture for the simulation: 2×4 with the start at cell 0, open cells 0–3 and 8–11, and an open edge between 3 and 11 only.
2. Add the route to the motion test (double `headMiddle` tap): the sequence of actions is `forward 3`, `turnRight`, `forward 1`, `turnRight`, `forward 3`.
3. Lay a 2×4 grid of floor tape (50 cm cells) for Lab 2, with a mark at each cell centre. The 6 panels are not enough for walls on all 15 edges, so the walled version runs in simulation, and on the bench only with the panels around the two turns.
4. Record the final position error against the cell-8 centre, and the heading error, over 10 runs.

**Acceptance criteria**
- [ ] In simulation the route ends in cell 8 facing west, with a position error ≤8 cm.
- [ ] On the floor grid, 10 runs are measured and written in `docs/maze/LAB_LOG.md`.

### MZ-044 — Sonar feasibility spike (log only)

| Field | Value |
|---|---|
| Status | **Backlog — after the first lab data.** Not scheduled, not counted in the hour plan. |
| Owner | TBD |
| Estimate | ~1.5 h first guess |
| Depends on | MZ-023 (Lab 2 gives the slot for the log) |
| Lab / robot | **yes** (Lab 2 or Lab 3, 10 extra minutes) |
| Branch | `feat/MZ-044-sonar-log` |
| Files | `Src/Modules/Infrastructure/NaoProvider/NaoProvider.cpp` (read the LoLA `Sonar` keys, enable the emitters); `Src/Representations/Infrastructure/SensorData/SonarSensorData.h` (create); `Config/Scenarios/Maze/logger.cfg`; `docs/maze/SONAR.md` (create) |

**Goal and scope.** The assessment in roadmap 8.1 says the sonar is not useful against side walls
and for collision avoidance, and marginal as a front-wall cross-check. This spike replaces the
documentation numbers with real ones. The sonar has **no influence on control**: it is only logged.

**Steps**
1. Read the left and right sonar values in `NaoProvider`, publish them in a representation, and add it to the logger.
2. In the bench corridor, record the readings at 10, 20, 25, 50 and 75 cm in front of a panel, 10 samples each, with the robot standing and walking.
3. Write `docs/maze/SONAR.md`: the table of readings, the observed minimum range, the update rate and the noise while walking, and a decision (drop, or build the front-wall cross-check after 3 Nov).

**Acceptance criteria**
- [ ] The readings table exists, with the minimum reliable range and the update rate.
- [ ] The decision is written down, and no control module reads the sonar.

---

## Backlog (to review, not scheduled)

### MZ-035 — Research and compare alternative maze-solving strategies

| Field | Value |
|---|---|
| Status | **Backlog — to review.** Not scheduled, not counted in the hour plan. |
| Owner | TBD (decided when it is pulled into a sprint) |
| Estimate | Not estimated (first step of the ticket) |
| Depends on | MZ-021 (simulation runner), MZ-025 (wall-follower) |
| Lab / robot | no (simulation only) |
| Branch | `docs/MZ-035-strategy-comparison` (plus `feat/MZ-035-<slug>` if code is needed) |
| Files | `docs/maze/STRATEGY_COMPARISON.md` (create); possibly `Src/Tools/Maze/` and `Util/MazeTools/run_sim_campaign.py` (a strategy selector) |

**Goal and scope.** Look for and design other strategies that solve the maze, and compare them
with the chosen one (flood-fill exploration + time-weighted A\*, MZ-011) on the same mazes in
simulation. The goal is evidence for the strategy decision, and a ranked fallback if the main
strategy misbehaves on the real robot.

**Candidate strategies (starting list, extend it)**
- Left-hand wall-follower (already in MZ-025 as a fallback).
- Right-hand wall-follower.
- Pledge algorithm (wall-follower that escapes loops around the goal).
- Trémaux's algorithm (marks visited passages).
- Plain flood-fill / BFS without turn costs.
- Flood-fill exploration + time-weighted A\* (current choice).
- Variants of the current choice, for example exploring after the goal during the ≤2 min return.

**Steps**
1. List the strategies and, for each one, write a short note: how it works, whether it guarantees reaching the goal when the maze has loops, and what attempt 2 can reuse from attempt 1.
2. Estimate the effort to implement each one in `Src/Tools/Maze/` behind a strategy parameter. Bring the estimate to the sprint review before writing code.
3. If approved, run the MZ-026 campaign (same generated mazes, 2 attempts, noise levels) once per strategy.
4. Write `docs/maze/STRATEGY_COMPARISON.md`: one table per noise level with success rate, mean T1, mean T2, mean Tfinal = 0.3·T1 + 0.7·T2, wall contacts and max stall, followed by a recommendation.

**Acceptance criteria**
- [ ] At least 4 strategies are described, with their guarantees and limits.
- [ ] The decision to implement (or not) was taken at a sprint review and recorded in the document.
- [ ] If implemented: all strategies are compared on the same mazes and noise levels, and the recommendation is backed by the table.

### MZ-036 — RL skills for in-place turns and corridor centring

| Field | Value |
|---|---|
| Status | **Backlog — after the competition (3 Nov 2026).** Not scheduled, not counted in the hour plan. |
| Owner | TBD |
| Estimate | Not estimated (first step of the ticket) |
| Depends on | MZ-016 (walk profile, baseline), MZ-024 (maze skills, baseline), MZ-026 (simulation campaign, baseline numbers) |
| Lab / robot | **yes**, for validation on the bench (training runs in simulation) |
| Branch | `feat/MZ-036-rl-maze-motion` |
| Files | `Src/Libs/RL/` (observation encoder and action decoder for the maze skills); `Config/NeuralNets/RLPolicy/` (new ONNX policies and manifests); `Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Maze/` (switch between classic and learned skill); `docs/maze/RL_MOTION.md` (create) |

**Goal and scope.** Route planning stays classical: time-weighted A\* is already optimal for a
known map, so reinforcement learning cannot improve the route itself. Most of the run time is
spent walking and turning, so RL is aimed there instead. The goal is to train two low-level
skills with the team's existing RL stack and keep them only if they beat the classic B-Human
walk in simulation **and** on the bench.
1. **Fast in-place turn** (90° and 180°) that lowers `t_turn90` / `t_turn180` without losing balance.
2. **Corridor centring**: walk forward through a ~48 cm corridor with a larger lateral margin, at equal or higher speed.

Reuse the existing infrastructure: `Src/Libs/RL` (encoders, decoders, ONNX wrappers), the
`pybh` simulation environment (`docs/RL/Environment.md`, `docs/RL/Training.md`) and the policy
integration path described in `docs/RL/Integration.md`.

**Steps**
1. Measure the baseline with the classic walk: the Lab 2 times (MZ-023) and the simulation campaign (MZ-026).
2. Define observation, action and reward for each skill. Reward: time to complete, heading or lateral error, wall contact (large penalty), fall (terminal).
3. Train in simulation on corridors and junctions generated with `Util/MazeTools` (MZ-009, MZ-010).
4. Integrate behind a configuration switch (`classic` / `learned`) so the classic skill stays as the fallback.
5. Validate on the partial bench: 10 repetitions per skill, and compare with the baseline.

**Acceptance criteria**
- [ ] The baseline and the learned skill are measured on the same scenarios (simulation and bench).
- [ ] The learned skill is kept only if it is faster **and** has 0 wall contacts and 0 falls in 10 bench repetitions. Otherwise the result is documented and the classic skill stays the default.
- [ ] `docs/maze/RL_MOTION.md` records the reward design, the training setup and the comparison table.

### MZ-037 — Active perception: head and camera geometry, moving vs fixed head

| Field | Value |
|---|---|
| Status | **Backlog — to review.** Not scheduled, not counted in the hour plan. Can be pulled into Sprint 3 by swapping hours with another ticket. |
| Owner | TBD (suggested split: geometry study and simulation runs for Wilson, skill changes for Bryam) |
| Estimate | ~3 h first guess (≈1.5 h study and runs, ≈1.5 h skill changes); confirm in step 1 |
| Depends on | MZ-019 (wall perceptor), MZ-021 (simulation runner), MZ-024 (maze skills) |
| Lab / robot | **yes**, for validation (image blur and detection with a moving head can only be checked on real images) |
| Branch | `feat/MZ-037-active-perception` |
| Files | `Util/MazeTools/visibility_table.py` (create); `docs/maze/ACTIVE_PERCEPTION.md` (create); `Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Maze/MazeScanWalls.cpp` (modify); `Config/Scenarios/Maze/` (head-scan parameters) |

**Goal and scope.** Use the head's range of motion and the camera geometry to map the maze faster
in attempt 1, and measure whether moving the head is worth it compared with keeping it still.

Facts this ticket builds on:
- **Head range** (NAO v6 spec): yaw ±119.5°, pitch −38.5° to +29.5°. The robot can look at side walls, and partly behind, without turning its body. A head turn costs tenths of a second; a body turn costs ~2 s.
- **Camera field of view:** 54.7° × 42.5° (`Config/Robots/Default/cameraIntrinsics.cfg`).
- **Tag size in the image:** a 15 cm tag is ~90 px wide at 1 m and ~30 px at 3 m with the upper camera at 640×480, so tags several cells down a corridor should be readable.
- **Corridor view:** looking along a corridor shows the side openings of several cells, so the robot could map cells it has not walked through and skip dead ends.

**Head strategies to compare**

| ID | Strategy | Description |
|---|---|---|
| H0 | Fixed head | Head always forward. Side walls are only seen after turning the body. |
| H1 | Stop and scan | Current MZ-024 behaviour: stop at cells with unknown walls and pan the head (≤2 s). |
| H2 | Scan while walking | Sweep the head left and right during corridor walks, without stopping. |
| H3 | Look-ahead and information-driven gaze | Look down corridors to map several cells at once, and point the head at the unknown wall that would change the plan the most. |

**Steps**
1. Write `visibility_table.py`. From the centre of a cell, with the head at yaw −120° … +120° in 15° steps, compute which walls of the current and neighbouring cells are visible, and at what distance tags fall below 20 px wide. Use it to choose the head angles for H1–H3.
2. Implement H0–H3 as a parameter of `MazeScanWalls`. Keep H1 as the default until the comparison is done.
3. Run the MZ-026 simulation campaign once per strategy (same mazes and noise levels).
4. On the bench, record logs with H0 and H2 to measure blur and the tag and wall detection rate while the head moves.
5. Write `docs/maze/ACTIVE_PERCEPTION.md` with:
   - the visibility table;
   - per strategy: mean T1, map edges learned per second, % of the map known when the goal is reached, wall-classification errors and tag detection rate;
   - a recommendation.

**Acceptance criteria**
- [ ] The visibility table covers the current cell and at least 2 cells ahead along a corridor.
- [ ] H0–H3 are compared on the same mazes and noise levels, and the bench check reports detection with the head still and moving.
- [ ] A strategy replaces H1 only if it lowers mean T1 **without** raising wall-classification errors or wall contacts.

### MZ-043 — Rolling turns and open-area diagonals for attempt 2

| Field | Value |
|---|---|
| Status | **Backlog — after the first lab data.** Not scheduled, not counted in the hour plan. |
| Owner | TBD |
| Estimate | ~4 h first guess; confirm in step 1 |
| Depends on | MZ-011 (planner), MZ-023 (turn and step costs), MZ-028 (tuning) |
| Lab / robot | **yes**, for validation |
| Branch | `feat/MZ-043-rolling-turns` |
| Files | `Src/Tools/Maze/MazePlanner.{h,cpp}` (new action costs); `Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Maze/MazeTurn.cpp` (rolling variant); `Config/Scenarios/Maze/mazePlanner.cfg`; `docs/maze/ROLLING_TURNS.md` (create) |

**Goal and scope.** Attempt 1 turns in place at cell centres (safe, precise). Attempt 2 carries 70 %
of the score, so it may use faster motions **if they keep the wall clearance**.

1. **Rolling turn** (turn while walking, along an arc through the corner). Attempt 1 stays in place; attempt 2 uses the rolling turn on corners the map marks as known-open. The planner already tracks `moving ∈ {0,1}`, so the change is a new action `turnRolling` with its own cost and a clearance check.
2. **Diagonals.** Be careful with the geometry: a straight diagonal from the centre of cell (r, c) to the centre of (r+1, c+1) passes **exactly through the wall-end post** at the inner corner of an L-turn, so it is **not possible in a corridor** (the clearance is ≈10 cm per side and the wall is 1–2 cm thick). Diagonals are only valid across an **open 2×2 block** (no internal wall between the four cells). It saves about 0.4 cell per diagonal step (√2 vs 2 cells), but only if the maze has such blocks. Walk it as a sidestep plus forward with a fixed heading, so the tags stay in view.

**Steps**
1. Count open 2×2 blocks and L-turns in the generated mazes (MZ-009) to size the gain before building anything. If the mean saving is below ~3 s per run, stop and document it.
2. Measure the rolling-turn time and the lateral overshoot on the bench (10 repetitions), in the Lab 2 or Lab 3 motion test.
3. Add the actions and costs to the planner behind `rollingTurns.enabled` (default `false`) and `diagonals.enabled` (default `false`).
4. Run the MZ-026 campaign with and without them, same mazes.

**Acceptance criteria**
- [ ] A saving estimate exists before any code is written.
- [ ] If implemented: 0 wall contacts and 0 falls in 10 bench repetitions, and T2 is lower in the campaign.
- [ ] The defaults stay `false` unless both checks pass.
