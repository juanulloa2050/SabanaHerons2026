# Maze Challenge Tickets — Copa NAO CCM MX 2026

This file lists every ticket (MZ-001 … MZ-045) for the Maze challenge. Each ticket says what it
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
| **Bryam** | 40.75 h | 7 h (Lab 3, 4, 5) | **47.75 h** |
| **Wilson** | 28.75 h (includes 3.5 h of the optional bench, MZ-014) | 11 h (Lab 1–5) | **39.75 h** |

Notes:
- **Authoring** means the owner's own work, including opening the PR, answering review comments and merging. Lab tickets are counted under *Lab sessions*, not under authoring.
- **Review time is not tracked.** Cross review still happens (roadmap section 9), but it has no hours in any ticket.
- Bryam's 40.75 h authoring = 40.25 h of own tickets + 0.5 h pairing on MZ-010.
- Corridor look-ahead (seeing and mapping walls of cells ahead) added 1.5 h on 13 Oct: MZ-019 4 → 5 h, MZ-020 5 → 5.5 h.
- MZ-038 (gyro heading) was merged into MZ-020 on 16 Oct, because the test floor makes the robot slip and the odometry heading is not reliable: MZ-020 5.5 → 7.5 h. The totals above do not include these 2 h yet; they come from the proposed hours.
- Start-cell deduction (the organisers may not give the start corner) was added to MZ-024 on 16 Oct: MZ-024 5 → 6 h. The totals above do not include this 1 h yet.
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
MZ-020, MZ-022. The rules fix the family (`tag36h11`) and the size (15 × 15 cm), but not which
walls carry tags or at what height. Until the answer arrives, the default is **one tag centred on
every wall face of every cell, tag centre at 30 cm above the floor**.

**Tag layout switch.** No ticket hard-codes the layout. A single model, `MazeTagLayout`
(`Src/Tools/Maze/MazeTagLayout.{h,cpp}`, created in MZ-013), is loaded from
`Config/Scenarios/Maze/mazeTagLayout.cfg`. Every module that places, expects or interprets a tag
asks it. If Q1 changes the layout, only this file and the tag mounts change, with no code change.

| Parameter | Meaning | Default |
|---|---|---|
| `mode` | `allFaces`: a tag on every wall face of every cell. `faces`: only on the faces listed in `faces`. `walls`: only on the (cell, face) pairs listed in `walls`. | `allFaces` |
| `faces` | Subset of `north`, `east`, `south`, `west` (maze frame, roadmap 4.5). Used when `mode = faces`. Example, east face only: `faces = [east];` | all four |
| `walls` | List of `{cell, face}`. Used when `mode = walls`, for a layout the organisers give wall by wall. | empty |
| `centreHeight` | Height of the tag centre above the floor, in mm | 300 |
| `size` | Edge of the black outer square, in mm (rules) | 150 |

- `hasTag(cell, face, maze)`: a face carries a tag only if the layout lists it **and** the maze has a wall there.
- `candidateFaces(cell)`: the faces that may carry the tag with that id. Side inference picks the candidate that matches the heading, and rejects a detection whose face is not a candidate (a false-positive filter).
- Faces are absolute (maze frame). A layout relative to the robot's path (for example "the wall on the robot's right") is not a static layout and is not covered. If Q1 answers that, it needs a new design decision.

### Software tickets (scheduled)

Tick the **Done** box of a ticket (`[ ]` → `[x]`) in this table **and** in the ticket's own
checklist (its *Done* row and every acceptance criterion) in the same PR that closes it. A ticket is
done only when the PR is merged and all its boxes are checked. Only the scheduled software tickets
have a Done column here; physical, proposed and backlog tickets use their own acceptance criteria.

| Done | ID | Title | Owner | Sprint | Author h | Lab | Depends on |
|---|---|---|---|---|---|---|---|
| [x] | MZ-001 | Create and publish `Maze-Challenge` | Bryam | 0 | 0.25 | no | – |
| [x] | MZ-003 | Planning docs (roadmap, tickets, Gantt) | Bryam | 0 | 1 | no | MZ-001 |
| [x] | MZ-004 | PR template | Wilson | 0 | 0.5 | no | MZ-001 |
| [ ] | MZ-006 | Organiser questions and answers | Wilson | 0 | 0.75 | no | MZ-003 |
| [x] | MZ-007 | Maze/MazeSafe scenarios + start flow | Bryam | 0 | 2.5 | no | MZ-001 |
| [x] | MZ-008 | Vendor apriltag + CMake | Bryam | 0 | 3 | no | MZ-001 |
| [x] | MZ-009 | Maze format + generator | Wilson | 0 | 2 | no | MZ-004 |
| [x] | MZ-010 | Maze → SimRobot scene converter | Wilson (+Bryam 0.5 pairing) | 0 | 2.5 | no | MZ-007, MZ-009 |
| [x] | MZ-011 | Maze library + time-weighted planner + tests | Bryam | 1 | 6 | no | MZ-009 |
| [x] | MZ-012 | Reference solver + expected results | Wilson | 1 | 1.5 | no | MZ-009 |
| [x] | MZ-013 | Oracle maze percepts for simulation ⚠️ | Bryam | 1 | 2 | no | MZ-010, MZ-011 |
| [x] | MZ-016 | Corridor walk profile in simulation | Wilson | 1 | 3 | no | MZ-007, MZ-010 |
| [x] | MZ-018 | AprilTag detector ⚠️ | Bryam | 2 | 5 | no | MZ-008, MZ-017* |
| [x] | MZ-019 | Wall perceptor + corridor look-ahead | Bryam | 2 | 5 | no | MZ-010, MZ-017* |
| [x] | MZ-020 | Localizer + mapper + gyro heading ⚠️ | Bryam | 2 | 7.5 | no | MZ-011, MZ-013, MZ-017* |
| [ ] | MZ-021 | Headless simulation campaign runner | Wilson | 2 | 3 | no | MZ-010 |
| [ ] | MZ-022 | Tag accuracy report ⚠️ | Wilson | 2 | 2.5 | no | MZ-017*, MZ-018 |
| [x] | MZ-024 | HandleMaze state machine + skills + start-cell deduction | Bryam | 3 | 6 | no | MZ-011, MZ-020 |
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
| MZ-039 | Decision log and run metrics (proposed) | Wilson | Proposed | 2 | no | MZ-024 |
| MZ-041 | "Positioned at start" button and start head-scan (proposed) | Bryam | Proposed | 2 | no (checked in Lab 3) | MZ-018, MZ-024 |
| MZ-042 | Maze memory reset by the sit gesture (proposed) | Bryam | Proposed | 1 | no | MZ-020, MZ-024 |
| MZ-045 | Pose correction from wall distances (proposed, only if the logs need it) | Bryam | Proposed | 3 | no (uses Lab 1 logs) | MZ-019, MZ-020, MZ-017* |
| MZ-046 | Blocked-walk detection and no false goal arrival (proposed, from the MZ-019 closed-loop run) | Bryam | Done | 3 | yes (bumper check) | MZ-019, MZ-020, MZ-024 |
| MZ-024.1 | Exploring return to start (subtask of MZ-024, done) | Bryam | Done | 3 | no | MZ-024 |

These come from the corrections review. They are small. With review time gone, the plan has room for most of them: Bryam's add 3.0 h, plus the 2 h of MZ-038 now inside MZ-020 (47.75 → 52.75 h), and Wilson's add 2.0 h (MZ-039). MZ-040, also proposed, is in the physical table. Suggested order, by value for the run: MZ-042 → MZ-041 → MZ-039. MZ-045 (3 h, not in these numbers) is built only if the MZ-020 logs show the cell is lost between tag fixes.

### Physical robot tickets

Bodies are in the section *Physical robot tickets*, after the proposed tickets. Lab hours are counted in the table of hours above.

| ID | Title | Owner | Sprint | Author h | Lab | Depends on |
|---|---|---|---|---|---|---|
| MZ-014 | Build the partial bench (**optional**) | Wilson | 1 | 3.5 (optional) | no | – |
| MZ-015 | Print and mount tags ⚠️ | Wilson | 1 | 1 | no | MZ-014 |
| MZ-017 | Lab 1: log capture ⚠️ | Wilson | 1 | 2 | **yes** | MZ-007, MZ-014, MZ-015 |
| MZ-018.1 | Tune the AprilTag detector on the robot ⚠️ (subtask of MZ-018) | Wilson | 2 | 1.5 | **yes** | MZ-018, MZ-015 |
| MZ-020.1 | Measure the real gyro drift from the Lab 1 logs (subtask of MZ-020) | Bryam | 2 | 0.5 | no (uses Lab 1 logs) | MZ-020, MZ-017 |
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
| Files | `docs/maze/ROADMAP_LABERINTO.md`, `docs/maze/TICKETS_LABERINTO.md`, `docs/maze/GANTT_LABERINTO.md` |

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
| Done | [x] |
| Owner | Wilson |
| Estimate | 0.5 h |
| Depends on | MZ-001 |
| Lab / robot | no |
| Branch | `docs/MZ-004-pr-template` |
| Files | `.github/pull_request_template.md` (create) |

**Goal.** Every PR description has the same sections: what changes, ticket, how it was tested, risks, checklist.

**Scope.** One file, `.github/pull_request_template.md`, with **exactly** the template in section 9 of `docs/maze/ROADMAP_LABERINTO.md` ("PR description template"). Docs only, no build needed.

**Acceptance criteria**
- [x] Opening a new PR on GitHub pre-fills the description with the template.
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
- An e-mail to the organisers (through the advisor if required) with Q1, Q3 and Q6–Q13 from section 11 of the roadmap, copied word for word. Q2 and Q5 are resolved (by the rules and by the arena image: the goal is the central 2×2 block) and Q4 is withdrawn; all three stay in the table.
- `docs/maze/ORGANIZER_ANSWERS.md`: a table with the columns `# | Question | Date asked | Answer | Date answered | Source`, updated through new commits on the same branch as answers arrive.

**Steps**
1. Send the e-mail and note the date.
2. Create the record file and open the PR.
3. When an answer arrives, add it to the table. If it is Q1, tell Bryam the same day.

**Acceptance criteria**
- [ ] The e-mail was sent by 8 Oct.
- [ ] The file lists all 13 questions with dates. Q2 and Q5 say "resolved", Q4 says "withdrawn", and missing answers say "pending".
- [ ] When Q1 is answered, Bryam is told the same day.

### MZ-007 — `Maze` and `MazeSafe` scenarios, `HandleMaze` stub, start flow

| Field | Value |
|---|---|
| Done | [x] |
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
  | `headRear` ×1 | Says the last seen tag id ("tag 11") or "no tag" (MZ-018) |
  | `headMiddle` ×2 | Says "motion test" and runs the **motion test sequence** used in Lab 2: 4 cells forward, stop, turn 90° left, 90° right, 180°, each separated by a 2 s pause and a beep |
  | `headMiddle` ×1 | Reserved for *positioned at start* (MZ-041) |

  The full button map is in roadmap 3.4.
- **Logger.** `logger.cfg` records `JPEGImage`, `CameraInfo`, `CameraMatrix`, `OdometryData`, `InertialData` (gyro drift, MZ-020), `FrameInfo`, and the maze representations once they exist. The logger records only outside *Initial* (`Src/Tools/Framework/BHLoggingController.cpp:33`), hence the 3-chest-tap start.

**Acceptance criteria**
- [x] `Make/Linux/compile Develop Nao` and `Make/Linux/compile Develop SimRobot` pass.
- [x] In SimRobot (`gc playing`), a simulated `headFront` tap makes the robot say "start". Simulated with `dr option:HandleMaze:headFront on`; sound needs `dr unmute on` in the simulator.
- [ ] Deploy with `-s Maze` boots. Three chest taps reach *playing* (green chest LED). Checked on the robot in Lab 1 (MZ-017).

### MZ-008 — Vendor `apriltag` as a prebuilt static library

| Field | Value |
|---|---|
| Done | [x] |
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
- [x] Both builds link a translation unit that calls `tag36h11_create()` / `tag36h11_destroy()`.
- [x] `Util/apriltag/build.sh` rebuilds both `.a` files from a clean checkout.
- [x] The upstream version and commit are recorded in `Util/apriltag/README.md`.

### MZ-009 — Maze file format and generator

| Field | Value |
|---|---|
| Done | [x] |
| Owner | Wilson |
| Estimate | 2 h |
| Depends on | MZ-004 |
| Lab / robot | no |
| Branch | `feat/MZ-009-maze-generator` |
| Files | `Util/MazeTools/generate_maze.py`, `Util/MazeTools/test_generate_maze.py`, `Util/MazeTools/README.md`, `Config/Mazes/generated/*.maze` (create) |

**Goal.** A text format for 8×8 mazes that people and programs can read, and a generator that produces valid random mazes for the simulator, the planner tests and the campaigns. Python 3, standard library only.

**Scope.**
- **Format (`.maze`)**: 17 lines of 17 characters. Even rows and even columns are corners `+`. Horizontal walls are `-`, vertical walls are `|`, and a space means no wall. The cell centre holds `S` (start), `G` (one of the 4 goal-block cells) or a space. Cell row 0 is the top row, matching tag id 0 at the top-left. Top-left corner example:
  ```
  +-+-+
  |S  |
  + +-+
  ```
- **Generator** `generate_maze.py`: `--seed INT` (required), `--start {0,7,56,63}` (default 0), `--loops INT` (default 0: extra walls removed to create loops), `--out PATH`. Randomized depth-first search, then these rules: outer border always closed; start cell has walls on exactly 3 sides; the goal is always the central 2×2 block (cells 27, 28, 35, 36), marked with four `G`, with no internal walls inside it; every cell is reachable (breadth-first check).
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
- [x] `python3 -m unittest discover -s Util/MazeTools -v` passes.
- [x] `--validate` prints `OK` for all 20 generated mazes.
- [x] `Util/MazeTools/README.md` documents the format with the example above.

### MZ-010 — Maze → SimRobot scene converter

| Field | Value |
|---|---|
| Done | [x] |
| Owner | Wilson (+ Bryam 0.5 h pairing for the scene template) |
| Estimate | 2.5 h (+ 0.5 h Bryam) |
| Depends on | MZ-007, MZ-009 |
| Lab / robot | no |
| Branch | `feat/MZ-010-maze-scene` |
| Files | `Util/MazeTools/maze_to_scene.py` (create); `Config/Scenes/Maze.ros2`, `Config/Scenes/Maze.con`, `Config/Scenes/Includes/Maze/current.rsi2` (create) |

**Goal.** Load any `.maze` file in SimRobot with walls, a start cell and one NAO, so everything downstream can be tested in simulation.

**Scope.**
- **Scene template (pairing, Bryam).** `Config/Scenes/Maze.ros2`, based on `Config/Scenes/RL1v0_3D.ros2`: scenario `Maze`, location `Default`, one robot, no ball. It does not include the SPL field; it includes `Includes/Maze/current.rsi2`, which provides the walls, the floor and the robot. `Maze.con` runs `ar off` so the automatic referee does not penalize the robot.
- **Converter (Wilson).** `maze_to_scene.py` turns a `.maze` file into `current.rsi2`.
- **Walls, floor and goal patch.** Each wall has a `BoxAppearance` (visible) and a `BoxGeometry` (collision). The converter also generates a matte floor slab slightly larger than the maze and an appearance-only patch of a different colour over the goal block (cells 27, 28, 35, 36). Floor, goal and wall colours are placeholders, set as constants in `maze_to_scene.py`, until the organisers confirm them.
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
- [x] SimRobot loads `Maze.ros2` without errors in the console.
- [x] The top view matches the `.maze` drawing for 3 different mazes (screenshots in the PR).
- [x] The robot stands in the start cell without touching walls.

---

## Sprint 1 — Core logic in simulation (Sat 10 – Thu 15 Oct)

Goal of the sprint: a maze library and planner proven by unit tests, oracle percepts in simulation, and the walk profile. Milestone M1.

> Physical-robot tickets of this sprint: MZ-014 (optional bench), MZ-015, MZ-017 (Lab 1). See *Physical robot tickets* below.

### MZ-011 — Maze library and time-weighted planner with unit tests

| Field | Value |
|---|---|
| Done | [x] |
| Owner | Bryam |
| Estimate | 6 h |
| Depends on | MZ-009 |
| Lab / robot | no |
| Branch | `feat/MZ-011-maze-planner` |
| Files | `Src/Tools/Maze/MazeGraph.h`, `Src/Tools/Maze/MazePlanner.{h,cpp}`, `Src/Tools/Maze/MazeFile.{h,cpp}` (create); `Src/Apps/Tests/Maze/*.cpp` (create); `Make/CMake/Tests.cmake` (modify: `Src/Tools` is not compiled into `Tests`, so the Maze sources and the `Src` include path are added) |

**Goal.** The brain of the maze, as a pure C++ library: it stores what is known about the maze and plans the cheapest route **in seconds**, not in cells (turning on a biped is expensive).

**Scope.** No framework dependency.
- **`MazeGraph`**: 8×8 grid, edge state `unknown/open/wall` (roadmap 4.3), tag id ↔ cell conversion and the maze frame (roadmap 4.5).
- **`MazeFile`**: loader and writer for `.maze` files, used by the tests and by the oracle provider (MZ-013).
- **`MazePlanner`**, on the state `(cell, heading, moving)` with the cost model and tie-break rule of roadmap 4.6:
  - *exploration*: weighted flood-fill from the goal (a Dijkstra over the states, run backwards), unknown edges counted as **open**;
  - *speed run*: A* over **known-open edges only**, unknown edges counted as **wall**.
- **`MazeCosts`**: defaults `t_fwd` 2.5 s, `t_start` 0.8 s, `t_turn90` 2.0 s, `t_turn180` 3.5 s (replaced by Lab 2 data in MZ-028).
- **Exploration simulator** (test code only): the same sensing model as `solve_maze.py --explore` (MZ-012), so the C++ exploration trace can be compared with the Python one.

**Steps**
1. `MazeGraph` and `MazeFile`, with a round-trip test (load, save, compare) on the 20 mazes of `Config/Mazes/generated/`.
2. `MazePlanner` exploration and speed run.
3. Tests against `Config/Mazes/generated/expected.csv` (MZ-012). The test finds the files with `File::getBHDir()`.

**Acceptance criteria**
- [x] On the 20 generated mazes, the optimal route of the planner matches `expected.csv` exactly: `cells`, `turns90`, `turns180` and `time_s`.
- [x] On the 20 generated mazes, the simulated exploration matches `expected.csv` exactly: `explore_cells`, `explore_turns` and `explore_time_s`.
- [x] Before comparing, the test checks `maze_sha` against the maze file and fails with "expected.csv is stale, run solve_maze.py --update" when they differ.
- [x] The heuristic is admissible: never above the true cost-to-go, checked on every state of the 20 mazes.
- [x] Rotating each maze by 90°, 180° and 270° in memory (the start moves to the other corners) gives the same optimal time and turn counts.
- [x] Replanning on an 8×8 maze takes <5 ms on the desktop (measured in the tests).
- [x] An unknown edge counts as open for exploration and as wall for the speed run (tested).

### MZ-012 — Reference solver and expected results

| Field | Value |
|---|---|
| Done | [x] |
| Owner | Wilson |
| Estimate | 1.5 h |
| Depends on | MZ-009 |
| Lab / robot | no |
| Branch | `test/MZ-012-maze-solver` |
| Files | `Util/MazeTools/solve_maze.py`, `Util/MazeTools/test_solve_maze.py`, `Config/Mazes/generated/expected.csv` (create); `Util/MazeTools/README.md` (modify) |

**Goal.** The expected best route and the expected exploration run for the 20 mazes already in `Config/Mazes/generated/`, computed by a Python solver that is independent of the C++ planner. The planner (MZ-011) is checked against it, and anyone can draw the route on a maze. No new mazes are drawn.

**Scope.** Cost model, `moving` rule and tie-break rule of roadmap 4.6, with `t_fwd = 2.5`, `t_start = 0.8`, `t_turn90 = 2.0`, `t_turn180 = 3.5`. Times are added in integer milliseconds, so ties are exact.
- **Optimal route** (always computed): Dijkstra over `(cell, heading, moving)` with the whole maze known. This is the independent reference for the planner.
- **Exploration run** (always computed): simulates attempt 1 with weighted flood-fill. The robot starts in its start state and knows only the outer border. On arriving in a cell (the start included) it learns the 4 edges of that cell. Before each action it replans from its current state with unknown edges counted as open, and takes the first action of the plan. The run ends when it enters any goal cell.
- **`--draw`**: prints the maze with the optimal route as arrows (`^ > v <`, the heading in each cell of the route), then the maze with every cell visited in the exploration marked `*`.
- **`--update CSV`**: writes the result into the CSV as an upsert keyed by `file`. Running the same maze again replaces its row instead of adding a new one. Rows stay sorted by `file`, so a diff shows only what changed.
- **`expected.csv`** columns: `file,maze_sha,cells,turns90,turns180,time_s,explore_cells,explore_turns,explore_time_s`.
  - `file` is the maze file name, the key of the row.
  - `maze_sha` is the FNV-1a 32-bit hash of the file bytes, as 8 hex digits. It is chosen because the C++ test can compute it in a few lines. It detects a maze edited after its row was written.
  - `cells` counts forward steps; `turns90` and `turns180` count turn actions; `explore_turns` is `turns90 + turns180` of the exploration run.
  - Times are in seconds with 3 decimals.
  - The values hold for the default costs. If the costs change (MZ-028), the CSV is regenerated.

**Steps**
1. Write `solve_maze.py` and `test_solve_maze.py` (`python3 -m unittest discover -s Util/MazeTools -v`, as for the other tools).
2. Fill `expected.csv`:
   ```bash
   python3 Util/MazeTools/solve_maze.py Config/Mazes/generated/maze_*.maze --update Config/Mazes/generated/expected.csv
   ```
3. Check 2 mazes by hand (count cells and turns on the `--draw` output) and write the result in the PR.

**Acceptance criteria**
- [x] `expected.csv` has 20 rows, one per generated maze, and 2 of them were checked by hand.
- [x] Running `--update` twice on the same maze leaves one row for it (tested).
- [x] Editing a maze changes its `maze_sha` (tested).
- [x] `--draw` output for 1 maze is pasted in the PR.

### MZ-013 — Oracle maze percepts for simulation ⚠️ TAG-DEP

| Field | Value |
|---|---|
| Done | [x] |
| Owner | Bryam |
| Estimate | 2 h |
| Depends on | MZ-010, MZ-011 |
| Lab / robot | no |
| Branch | `feat/MZ-013-oracle-percepts` |
| Files | `Src/Modules/Modeling/OracledMazePerceptsProvider/` (create); `Src/Representations/Perception/MazePercepts/{AprilTagPercept,WallPercept}.h` (create); `Src/Tools/Maze/MazeTagLayout.{h,cpp}`, `Config/Scenarios/Maze/mazeTagLayout.cfg`, `Src/Apps/Tests/Maze/MazeTagLayoutTest.cpp` (create); `Config/Scenes/Maze.con` (sim variant, `mr` the oracle); `Src/Tools/Maze/MazeOracle.{h,cpp}` (oracle geometry, unit-tested); `Util/MazeTools/{maze_to_scene,make_tag_textures}.py`, `Config/Scenes/Textures/AprilTags/` (visible tags) |

**Goal.** Fake perception for simulation: publish the tags and walls the robot *would* see, computed from ground truth. Behaviour, planning and mapping can then be built and tested before the real detectors (MZ-018, MZ-019) exist.

**Scope.** `OracledMazePerceptsProvider` (Cognition, simulation only) builds `AprilTagPercept` and `WallPercept` from `GroundTruthRobotPose` and the loaded `.maze` file. Parameters: Gaussian noise, dropout rate and maximum range. Tag placement comes from `MazeTagLayout` (see *Tag layout switch* above), which this ticket creates; the default is one tag on every wall face, centre at 30 cm. `WallPercept` also carries the walls of the cells ahead along the gaze, up to the maximum range, in the same format as MZ-019, so look-ahead mapping (MZ-020) can be tested before the real perceptor exists.

**Delivered beyond the original scope.** The oracle follows whichever of the 20 mazes the scene was generated from (`maze_to_scene.py` writes `current.maze` next to `current.rsi2`). It reports every tag in range, fully inside the horizontal field of view, facing the camera and not hidden by a wall, not only the tags of the gaze corridor. The generated scene shows the tags (tag36h11 textures, same layout as the oracle), so the real detector (MZ-018) can also be tried in simulation.

**Acceptance criteria**
- [x] In SimRobot, the representation view shows the correct tag ids and walls for 3 mazes.
- [x] Dropout = 1.0 produces empty percepts, which is used later by the safe-mode tests.
- [x] `MazeTagLayoutTest` passes: with `allFaces` every wall face has a tag; with `faces = [east]` only east wall faces do; with `walls` only the listed pairs do; and no tag is ever placed where the maze has no wall.
- [x] Changing `mode` in `mazeTagLayout.cfg` changes the oracle tags in SimRobot without recompiling.

### MZ-016 — Corridor walk profile in simulation

| Field | Value |
|---|---|
| Done | [x] |
| Owner | Wilson |
| Estimate | 3 h |
| Depends on | MZ-007, MZ-010 |
| Lab / robot | no |
| Branch | `config/MZ-016-walk-profile` |
| Files | `Config/Scenarios/Maze/walkingEngine.cfg`, `Config/Scenarios/Maze/walkingEngineCommon.cfg`, `Config/Scenarios/MazeSafe/walkingEngine.cfg`, `Config/Scenarios/MazeSafe/walkingEngineCommon.cfg` (create as copies); `Config/Robots/Nao/Nao/walkingEngine.cfg` (remove so scenario profiles load in SimRobot) |

**Goal.** A conservative walk for the 48 cm corridor: slower, with the arms closer to the body so they do not touch the walls. `Maze` uses 60 % of the default speed and `MazeSafe` 40 %.

**Scope.** The profiles are copies of the defaults with only a few values changed. A scenario file overrides `Config/Robots/Default/` (roadmap 3.6), so the **whole file** is copied, not only the changed lines. SimRobot searches `Config/Robots/Nao/Nao/` before the scenario folder, so its `walkingEngine.cfg` took precedence and hid both scenario speed profiles. That robot-specific file was removed so Maze and MazeSafe limits apply in simulation. The accepted side effect is that other SimRobot scenes use `Robots/Default` walking speeds instead of the Nao-specific 300/230 mm/s profile.
- `walkingEngine.cfg`: `maxSpeed` and `minSpeed` at 60 % (`Maze`) or 40 % (`MazeSafe`), for translation x, y and rotation.
- `walkingEngineCommon.cfg`, under `armParameters`: `armShoulderRoll` 7deg → 4deg, `armShoulderRollIncreaseFactor` 2 → 1, `armShoulderPitchFactor` 6 → 3.

**Steps**
1. Copy the files:
   ```bash
   cp Config/Robots/Default/walkingEngine.cfg Config/Robots/Default/walkingEngineCommon.cfg Config/Scenarios/Maze/
   cp Config/Robots/Default/walkingEngine.cfg Config/Robots/Default/walkingEngineCommon.cfg Config/Scenarios/MazeSafe/
   ```
2. Apply the values above.
3. Remove `Config/Robots/Nao/Nao/walkingEngine.cfg` so SimRobot loads the selected scenario's speed profile.
4. Run 5 times per scenario in SimRobot on the same 8-cell straight corridor of `Config/Mazes/generated/maze_2.maze`. Record in the PR the time to cross 4 cells, whether any wall was touched, and the visual estimate of minimum arm-to-wall distance.

**Acceptance criteria**
- [x] Both scenarios load in SimRobot and the robot walks without falling (5/5 runs per scenario).
- [x] 0 wall touches in 5 runs per scenario, and the times are recorded in the PR.
- [x] Only the listed speed and arm parameters differ from `Config/Robots/Default/` in the four scenario files (checked with `diff`).

---

## Sprint 2 — Perception and localization (Fri 16 – Wed 21 Oct)

Goal of the sprint: real tags and walls detected, a pose that stays inside the cell, and a map that persists. Milestone M2.

> Physical-robot tickets of this sprint: MZ-023 (Lab 2). See *Physical robot tickets* below.

### MZ-018 — AprilTag detector ⚠️ TAG-DEP

| Field | Value |
|---|---|
| Done | [x] |
| Owner | Bryam |
| Estimate | 5 h |
| Depends on | MZ-008, MZ-017 (soft: Lab 1 logs) |
| Lab / robot | no (validated on Lab 1 logs; rendered simulation images otherwise) |
| Branch | `feat/MZ-018-apriltag-detector` |
| Files | `Src/Modules/Perception/AprilTagDetector/AprilTagDetector.{h,cpp}` (create); `Src/Tools/Maze/AprilTagGeometry.{h,cpp}` and `Src/Apps/Tests/Maze/AprilTagGeometryTest.cpp` (create); `Src/Modules/Infrastructure/InterThreadProviders/PerceptionProviders.{h,cpp}` (modify: `PerceptionAprilTagPerceptProvider`); `Config/Scenarios/{Maze,MazeSafe}/threads.cfg` and `aprilTagDetector.cfg` (modify / create); `Config/Scenes/Includes/AprilTagViews.con` (create); `HandleMaze.h`, `SkillBehaviorControl.h` (modify) |

**Goal.** Read the maze's `tag36h11` tags from the upper camera and publish, for each one, its id and its pose in the robot frame. This is how the robot knows which cell it is in.

**Scope.** Roadmap 4.4. Upper thread only (Cognition has no full camera image).
- Wrap `ECImage.grayscaled` as `image_u8` without copying. `quad_decimate = 2`, `nthreads = 1`.
- Tag pose with `estimate_tag_pose` using the `CameraInfo` intrinsics, transformed to the robot frame with `CameraMatrix`.
- A decision-margin threshold and a size-versus-distance consistency check in `aprilTagDetector.cfg`.
- The tag size for pose estimation comes from `MazeTagLayout.size` (150 mm). The detector does not depend on which walls carry tags.
- Self-test: on the robot, a single `headRear` tap says the last seen tag id ("tag 11"), or "no tag".
- The values of `aprilTagDetector.cfg` are explained and tuned on the real robot in the subtask MZ-018.1 (Wilson).

**Delivered beyond the original scope.**
- `call Includes/AprilTagViews` in the `RemoteRobot` scene shows the detections live (see `SIMULACION_LABERINTO.md`, step 6).
- Cognition always takes the newest upper-camera percept. In the Maze scenario Cognition drops an upper frame that arrives after a newer lower frame, which lost every tag when 2 or more were in view. The same tag can therefore repeat over several Cognition frames; consumers (MZ-020) tell repeats apart by `timestamp`.
- Frame conversion and filters are unit-tested (`AprilTagGeometryTest`).

**Acceptance criteria**
- [x] ~~Ids are correct on ≥95 % of Lab 1 frames between 0.25 and 1.5 m.~~ Moved to MZ-022, which measures the detector on the Lab 1 logs (decision, 16 Oct).
- [x] ~~On the NAO, `STOPWATCH("AprilTagDetector")` averages ≤20 ms per upper frame.~~ Moved to MZ-018.1, together with the `frameSkip` parameter that fixes it (decision, 16 Oct).
- [x] ~~No detections with a wrong id on the whole log (false-positive check).~~ Moved to MZ-022 (decision, 16 Oct).
- [x] On the robot, a single `headRear` tap says the last seen tag id. Checked on the NAO on 16 Oct with one tag and with several tags in view.

### MZ-019 — Wall perceptor and corridor look-ahead

| Field | Value |
|---|---|
| Done | [x] |
| Owner | Bryam |
| Estimate | 5 h (4 h perceptor + 1 h look-ahead) |
| Depends on | MZ-010, MZ-017 (soft: Lab 1 logs) |
| Lab / robot | no (simulation, plus Lab 1 logs when they exist) |
| Branch | `feat/MZ-019-wall-perceptor` |
| Files | `Src/Modules/Perception/WallPerceptor/` (create); `PerceptionProviders.{h,cpp}` (modify: `ALIAS(WallPercept)`); `Config/Scenarios/Maze/threads.cfg` |

**Goal.** Tell, from the image alone, which walls are around the robot **and along the corridor ahead**, and how far away they are. This feeds the map with wall evidence for the current cell and for cells the robot has not walked through yet, so it can avoid a dead end before entering it. The planner already replans whenever an edge changes (roadmap 4.6). Example: in `maze_1.maze`, at cell 10 facing east, seeing that cell 11 is closed to the south and east saves the 9.3 s detour into the dead end.

**Scope.** Upper thread.
- **Near walls.** Column scan for the floor-to-wall boundary on the grey `ECImage`, projected to the floor with `Transformation::imageToRobot` (uses `CameraMatrix`). Output: `WallPercept` with the distance to the wall in front, left and right when the head points that way.
- **Boundary by gradient, not colour.** The rules fix light walls and a matte uniform floor, but not the floor colour (Q13). The boundary is found as the brightness step between the floor and the wall: the floor is horizontal and the wall vertical, so they get different light even with the same paint. A colour threshold may be added only once Q13 is answered and shows a clear contrast. Tags (MZ-018) confirm walls without needing contrast.
- **Look-ahead.** Along the gaze line, the percept also reports, for each cell ahead, whether its side walls are present or open. A front wall at distance *d* means the edges before *d* are open and the edge at *d* is a wall. The range depends on motion: `lookAheadRangeWalking` (default 2 cells), because the torso pitches while walking, and `lookAheadRangeStanding` (default 3 cells, up to 4 if the measurements allow it) while the robot stands and scans. Beyond a corner nothing is visible: walls are ≥60 cm high, above the camera.
- **Grid snap.** Walls can only sit on the 500 mm grid. Each measurement is snapped to the nearest edge and kept only if it lies within `snapToleranceMm` (default 150 mm) of that edge; otherwise it is dropped. Every observation carries its distance, so the mapper can weight it (MZ-020).
- **Why the range is limited.** Pixel resolution is not the limit (about 2 cm per pixel at 4 cells). The camera angle error is: with the camera about 0.5 m above the floor, a pitch error of 1° moves a floor point by about 6 cm at 1.25 m, 11 cm at 1.75 m and 19 cm at 2.25 m, and twice that with 2° (first estimate, measured in this ticket). A measurement is useful while this error stays inside `snapToleranceMm`. Far edges are confirmed by tags instead (MZ-018): the tag id names the cell, so a far tag cannot be put on the wrong edge.

**Acceptance criteria**
- [ ] In simulation, wall present or absent is ≥98 % correct for the 4 sides of visited cells (10 cells, 3 mazes).
- [ ] On Lab 1 corridor logs, the side-wall distance is within ±3 cm of the tape measurement.
- [x] In simulation, the side walls of the next cell ahead are ≥95 % correct, and no edge beyond the active range (walking or standing) is reported.
- [ ] The distance error at 0.5, 1.0, 1.5, 2.0 and 2.5 m is measured, standing and walking (Lab 1 logs, or rendered images if the logs do not exist), and written in the PR; `lookAheadRangeWalking`, `lookAheadRangeStanding` and `snapToleranceMm` are set from it.
- [x] With the floor and walls set to the same light colour in the simulation scene, wall present or absent is still ≥90 % correct for the 4 sides of visited cells (gradient check).

**Note (16 Oct).** The grid snap uses `RobotPose` from `MazeLocalizer`, decided 2026-10-16 (Upper modules can `REQUIRE(RobotPose)`, e.g. `BallPerceptor`). That is why MZ-020 was done first.

**Delivered (branch `feat/MZ-019-wall-perceptor`, HEAD `b87f8eac`).**
- `WallPerceptor` (Upper) with its cores in `Src/Tools/Maze/` (`MazeWallScan`, `MazeWallView`); Cognition gets the percept through a hand-written copy provider (`PerceptionWallPerceptProvider`, as for the AprilTags) instead of `ALIAS(WallPercept)`, because Cognition skips Upper frames. `wallPerceptor.cfg` in `Maze` and `MazeSafe` (`PARAMETROS_LABERINTO.md`, section 5).
- Values set from the data: `lookAheadRangeWalking` **1** (the ticket default was 2), `lookAheadRangeStanding` 3, `snapToleranceMm` 150.
- Polarity-agnostic boundary (in simulation the walls are darker than the floor), vertical-extent check, goal-patch rejection (saturation veto `maxWallSaturation`, no walls between the goal cells), learned floor with reset.
- Mapper: an edge with a seen tag ignores the open observations of the wall percept.
- `WallPerceptEvaluator` and the scenes `MazeEval`, `MazeVision`, `MazeVisionEval`; `maze_to_scene.py --same-colour-floor`. How to run: `SIMULACION_LABERINTO.md`, step 7.

**Evidence (2026-10-20, simulation, one 420 s `MazeEval` run per maze, about 11600 frames).** (a) = visited-cell sides, (b) = side walls of the next cell ahead, (c) = cells beyond the active range. The latest run of each maze is shown; `b87f8eac` (goal patch fix) was run on maze_19 and maze_7 only.

| Maze | Run | (a) correct / known (coverage) | (b) accuracy (coverage) | (c) |
|---|---|---|---|---|
| maze_1 | T3.1, `e2ebdbce` | 100 % (109/109, 97 %) | 99.4 % (87 %) | 0 |
| maze_19 | T3.2, `b87f8eac` | 98.3 % (178/181) | 97.6 % (18 %) | 0 |
| maze_7 | T3.2, `b87f8eac` | 90.1 % (163/181) | 97.3 % (11 %) | 0 |
| maze_1, floor in the wall colour | T3.1, `e2ebdbce` | 99.1 % (108/109, 97 %) | 99.4 % (87 %) | 0 |

Before `b87f8eac` (T3.1) maze_19 had (a) 95.6 % (173/181) and (b) 97.1 % (77 % coverage); maze_7 had (a) 99.3 % (136/137) and (b) 99.1 % (84 % coverage). The first matrix (T3, before the fixes) had (a) 79.6-94.4 % and (b) 91.2-98.6 %.

- **Criterion 1, visited sides ≥98 %: not met.** maze_1 100 % and maze_19 98.3 % pass; maze_7 is at 90.1 %. The loss is one edge, 3,2 north (a true wall, votes wall 52 : open 500-600): the robot loiters at (1505, 2355), 145 mm from that wall on a cell border, and the one edge is counted over many repeated visits. An offline replay of 250 dumped frames gives the same boundary points with the old and the new scan, so the error depends on the trajectory and the pose, not on `b87f8eac`; it is one run and it is not proven absent from the T3.1 baseline (which had a different trajectory). Not fixed.
- **Criterion 2, Lab 1 corridor logs ±3 cm: pending.** The MZ-017 logs do not exist. Nothing was measured on real images.
- **Criterion 3, next cell ≥95 % and nothing beyond the range: met in simulation.** (b) 97.3-99.4 % on the three mazes; (c) 0 on all runs (maze_19 had 1 cell beyond the range in the first matrix; the cause was inferred, a skew between the Upper and Cognition `MotionInfo`, and not reproduced, so one clean run does not prove it cannot recur). The coverage of (b) on the last runs is low (18 % and 11 %), see below; maze_1 was not run again after `b87f8eac`.
- **Criterion 4, distance error: measured; the values are set.** Rendered images, standing and walking, 0.5 to 2.5 m (table in `PARAMETROS_LABERINTO.md`, section 5; mean / p95 in mm: standing ≤13 / ≤24 up to 2.5 m except maze_7 at 1.5 m 27 / 52; walking 14-18 / 22-69 at 0.5 m, 17-22 / 42-53 at 1.0 m, 32-36 / 70-94 at 1.5 m, 85-128 / 157-918 at 2.0 m). Hence `lookAheadRangeWalking` 1, `lookAheadRangeStanding` 3, `snapToleranceMm` 150. The table still has to be written in the PR description. Not measured on real images.
- **Criterion 5, same colour ≥90 %: met in simulation.** 99.1 % (108/109) on maze_1 with the floor in the wall colour; measured with the T3.1 code, not repeated after `b87f8eac`.

**Known limitations (accepted for the PR on 2026-10-20).**
- Coverage loss on the goal patch: while the robot stands on the goal patch the floor reference would be the patch, so every frame is rejected and gives no evidence. The (b) coverage fell from 77 % to 18 % on maze_19 and from 84 % to 11 % on maze_7, and the known edges of maze_19 from 32672 to 10456. The accuracy holds; what the camera could see from inside the goal is lost. A per-frame plain-floor reference from the uncoloured pixels would keep those walls; it is not done.
- Low-vote true walls voted open remain in maze_19 (2,7 north 2:7, 3,1 north 1:3, 3,6 east 5:5 tie, 4,1 west 4:7) and one in maze_7 (1,0 north 0:6).
- The real wall polarity is unknown (Q13): the simulator renders the walls darker than the floor, the rules say lighter. The boundary accepts either sign, but the colour and brightness thresholds (`minGradient`, `minWallContrast`, `maxWallSaturation`) were set on simulated images only.
- One run per maze, no variance estimate; the cause of the maze_7 loitering and of the occasional bad simulator starts is not investigated.
- The tag flag of the mapper never clears, so a misdecoded tag would block the open evidence of that edge for good.

**Closed (22 Oct 2026).** Done in simulation and delivered together with MZ-046. Criterion 1 (maze_7 at 90.1 %) and criterion 2 (Lab 1 logs) stay open as listed above; the distance table of criterion 4 is in `PARAMETROS_LABERINTO.md`, section 5.

The ticket stays open until criterion 2 is measured on the Lab 1 logs and criterion 1 is met or accepted by the team.

### MZ-020 — Maze localizer, mapper and gyro heading ⚠️ TAG-DEP

| Field | Value |
|---|---|
| Done | [x] |
| Owner | Bryam |
| Estimate | 7.5 h (5 h + 0.5 h look-ahead evidence + 2 h gyro heading, merged from MZ-038) |
| Depends on | MZ-011, MZ-013, MZ-017* (gyro drift from the Lab 1 logs) |
| Lab / robot | no |
| Branch | `feat/MZ-020-localizer-mapper` |
| Files | `Src/Modules/Modeling/MazeLocalizer/`, `Src/Modules/Modeling/MazeMapper/` (create); `Src/Representations/Modeling/MazeMap.h`, `Src/Representations/Modeling/MazeCellPose.h` (create; `MazeCellPose` includes `headingSource` and `headingDrift`); `Config/Scenarios/Maze/threads.cfg` (modify: `RobotPose` provided by `MazeLocalizer`, removed from `defaultRepresentations`); `docs/maze/GYRO_HEADING.md` (create) |

**Goal.** Know **where** the robot is (cell, heading, and a pose inside the cell) and **what** it has learned (the wall map), and keep that knowledge for the whole process.

**Scope.**
- **`MazeLocalizer`** replaces `SelfLocator` (useless in a maze). It provides `RobotPose` and `MazeCellPose`: odometry prediction, a tag fix (complementary filter, roadmap 4.5), a heading snap on square-on tags, and the gyro heading below. The face a seen tag is on comes from `MazeTagLayout::candidateFaces`, so no layout is hard-coded.
- **Gyro heading (merged from MZ-038).** The test floor makes the robot slip, so the odometry heading drifts; the gyro measures the real body rotation and does not slip. Store the yaw of `InertialData.orientation3D` at the *positioned at start* moment as the maze-north offset: `heading = snap90(yaw − offset)`. Re-align the offset on every square-on tag (walls are axis-aligned). Use the heading to infer the wall face of a seen tag, and to reject a tag whose normal does not match the heading (a false-positive filter). The NAO has no magnetometer, so the yaw drifts; the drift rate decides how often a tag fix is needed. `logger.cfg` already records `InertialData`.
- **`MazeMapper`** provides `MazeMap`: fuses wall evidence per edge with hysteresis (3 consistent observations before `unknown → wall/open`). Evidence is accepted for **any edge in view**, not only the edges of the current cell: wall percepts already snapped to the grid (MZ-019) and tag detections (MZ-018), where the tag id names the cell and so the edge. The weight of a wall-percept observation drops with its distance (parameter); a decoded tag counts as full evidence at any distance it can be read. The map is a module member, so it **persists for the whole process**. The attempt counter increments on Standby → start.

**Gyro steps (from MZ-038)**
1. From the Lab 1 logs (or a simulated drifting gyro if they do not exist), plot the yaw drift over a 5 min stand and a corridor walk. Write the numbers in `GYRO_HEADING.md`.
2. Implement the offset, the snap and the tag-based re-alignment. Unit-test the snap with ±44° and ±46° inputs.
3. **Experiment:** put tags **only on the north face** (in simulation, `mode = faces; faces = [north];` in `mazeTagLayout.cfg`; on the bench, mount only north tags) and check that the localizer still keeps the cell estimate when the robot faces north. Document the coverage loss on other headings.

**Delivered beyond the original scope.**
- Pure helpers in `Src/Tools/Maze` with unit tests: heading math (`MazeHeading.h`), localization filter (`MazeLocalization.h`), map fusion with hysteresis (`MazeMapFusion.h`), flat edge indexing (`MazeEdgeIndex.h`, out-of-range reads give `unknown`) and the module session logic (`MazeSession.h`: attempt counter, quality gate). Both modules call the same helpers.
- Offline acceptance suite (`MazeLocalizerAcceptanceTest.cpp`, `MazeLocalizerSim.h` in `Src/Apps/Tests/Maze`): a walking robot with noisy odometry, a drifting gyro and oracle percepts. It stands in for the runs that need MZ-024 and MZ-021, which do not exist yet.
- `EmptyWallPerceptProvider` (Perception) is a placeholder on the robot until MZ-019 exists. In simulation `Maze.con` replaces it with the oracle (`mr WallPercept OracledMazePerceptsProvider`).
- `RobotPose` is now in the maze frame (x east, y north, origin at the outer bottom-left corner, mm). `MazeLocalizer` provides it in `Maze` and `MazeSafe`; it is removed from `defaultRepresentations`.
- Simulation-only injection keys (`injectedGyroDriftPerMinute`, `injectedOdometryTranslationNoise`, `injectedOdometryRotationNoise`) and field drawings `representation:MazeMap`, `module:MazeLocalizer:pose`, `module:MazeMapper:lookAhead` (see `SIMULACION_LABERINTO.md`, step 5).
- The start pose defaults to cell 0, heading south (the only open side of the start cell) until MZ-024 (`start.cell`) exists.
- Every key is documented in `PARAMETROS_LABERINTO.md` (section 6); the gyro method and the offline results in `GYRO_HEADING.md`.

**Ticket correction.** In `maze_1.maze`, cell 11 is closed east and south (open west and north), not a classic dead end. The robot avoids the detour by seeing those walls from cell 10 (look-ahead evidence), not by seeing the dead end earlier.

**Acceptance criteria.** Ticked on 16 Oct (decision of the owner) on the offline evidence (`Build/Linux/Tests/Develop/Tests`, 199 passed) plus the SimRobot check of 16 Oct: the cell estimate follows the robot and walls already seen stay in the map across cell boundaries (after the fix of the erased walls). The drift rate needs the Lab 1 logs and moved to MZ-020.1. The per-item SimRobot runs below are optional follow-ups, not blockers.
- [x] In simulation with noisy odometry and a drifting gyro, the pose error is ≤8 cm and ≤10° after 10 cells with tag fixes, and the cell estimate is 100 % correct. *Offline:* 11 cells, 5 % odometry error, 3°/min drift: final error 2-3 mm and 0.3°, checkpoint maximum 8.2-13.1 mm, cell 11/11. Harsh (15°/min, +10 % odometry bias, dropout 0.17): 7 mm, 0.4°, 11/11. *Optional:* SimRobot run with the injection keys.
- [x] ~~The gyro drift rate (° per minute) is measured and recorded in `GYRO_HEADING.md`.~~ Moved to MZ-020.1 (decision, 16 Oct): it needs the Lab 1 logs (MZ-017); the table and the procedure are in `GYRO_HEADING.md`, section 6.
- [x] The north-face-only experiment is documented, with the coverage consequence. *Offline, documented in `GYRO_HEADING.md`, section 7:* tag-fix share by true heading north 90 %, east 13 %, south 0 %, west 39 % (`allFaces` 90/91/91/100 %). North corridor: cell 6/6, 2.5 mm; south over the same corridor: 0 fixes, 137 mm drift. Head scan ±60° raises east to 28 % but lowers north to 58 %. *Optional:* bench check with real north-only tags.
- [x] After a simulated pick-up and Standby, the map is identical and the attempt counter = 2. *Offline:* map equal before and after (also at the transition frame), attempt 2, pose reset to cell 0. *Optional:* SimRobot run (`SIMULACION_LABERINTO.md`, step 5, *Pick-up and Standby*).
- [x] With tags hidden (dropout 1.0), the localizer reports low quality (used by MZ-025). *Offline:* `okay` at 9.3 s / 1504 mm, `poor` at 30.06 s (3867 mm); position error 143 mm and 2.1° after the route. *Optional:* SimRobot run with `dropoutRate = 1`.
- [x] In simulation with the oracle percepts (MZ-013, maximum range 2 cells), edges of cells the robot has not visited reach `wall`/`open`, and on `maze_1.maze` the robot does not enter the dead end at cell 11. *Offline:* 96 edge-sides decided, 56 cells never entered, 0 wrong; the look-ahead run never enters cell 11, while a baseline that learns only on arrival enters it as the 6th cell. Standing in cell 10 facing east, 7 vision frames (0.6 s: one observation per edge every 250 ms) close cell 11 (east and south). The real walk is checked in MZ-024, which makes the robot explore.
- [x] The localizer runs with each `mode` of `mazeTagLayout.cfg` (a config change only), and the tag-fix rate per mode is recorded in the PR. *Offline (same 11-cell route, nominal noise):*

  | `mode` | Fixes/s | Fixes per cell | Final error |
  |---|---|---|---|
  | `allFaces` | 8.9 | 35 | 3 mm |
  | `faces = [north]` | 1.9 | 7.6 | 40 mm |
  | `walls` (checkerboard) | 7.0 | 27.6 | 9 mm |

  Recorded in PR #15. *Optional:* SimRobot run with each layout (regenerate the scene each time).

**Known limits of the offline numbers.** The path follows the cell centres exactly (no sway or stopping error), the oracle sees every tag (no image processing), the planner state comes from the estimate but the motion is ideal, and the mapper takes one observation per edge every 250 ms and skips the wall look-ahead within 100 mm of a cell boundary (fix of 16 Oct: SimRobot showed walls erased when the estimated and the true cell differed). The noise sequences are deterministic on the Linux toolchain only.

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
- [ ] `python3 Util/MazeTools/run_sim_campaign.py --mazes Config/Mazes/generated --attempts 1 --noise low --out /tmp/r.csv` runs 2 mazes without manual clicks.
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
- [ ] Ids are correct on ≥95 % of Lab 1 frames between 0.25 and 1.5 m (moved from MZ-018).
- [ ] No detection with a wrong id on the whole log (false-positive check, moved from MZ-018).

---

## Sprint 3 — Behaviour and integration (Thu 22 – Sun 25 Oct)

Goal of the sprint: the full competition state machine, a watchdog that never lets the robot stall, and evidence from a simulation campaign. Milestone M3.

> Physical-robot tickets of this sprint: MZ-027 (Lab 3). See *Physical robot tickets* below.

### MZ-024 — `HandleMaze` state machine and maze skills

| Field | Value |
|---|---|
| Done | [x] |
| Owner | Bryam |
| Estimate | 6 h (5 h + 1 h start-cell deduction) |
| Depends on | MZ-011, MZ-020 |
| Lab / robot | no |
| Branch | `feat/MZ-024-handle-maze` |
| Files | `Src/Modules/Modeling/MazeMapper/`, `Src/Modules/Modeling/MazeLocalizer/` (modify: start-relative frame and re-anchoring); `Src/Modules/BehaviorControl/SkillBehaviorControl/Options/HandleMaze.h` (modify); `Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Maze/{MazeFollowCorridor,MazeTurn,MazeScanWalls}.cpp` (create); `Src/Representations/BehaviorControl/SkillInterfaces.h` (modify); `Src/Modules/BehaviorControl/MazePlannerProvider/`, `Src/Representations/BehaviorControl/MazePlan.h` (create); `Config/Scenarios/Maze/handleMaze.cfg`, `Config/Scenarios/Maze/mazePlanner.cfg` (create) |

**Goal.** Replace the MZ-007 stub with the real competition behaviour: the robot explores, reaches the goal, waits for the second attempt and runs the fastest known route, and it tells the operator out loud what it is doing.

**Scope.**
- **States and transitions** of roadmap 4.7: `WaitingForStart`, `MazeExploration`, `ReturnToStart` (MZ-024.1), `Standby`, `SpeedRun`, `Finished`. The states `Unpositioned`, `StartScan` and `Sitting` come with MZ-041 and MZ-042.
- **`MazePlannerProvider`** wraps the MZ-011 library and provides `MazePlan` (cell path, action list, estimated time). It reads the motion costs from `mazePlanner.cfg`, created here with the `MazeCosts` defaults (`t_fwd` 2.5 s, `t_start` 0.8 s, `t_turn90` 2.0 s, `t_turn180` 3.5 s). MZ-028 replaces them with the Lab 2 means.
- **Skills**, built on `WalkToPose` (empty obstacle avoidance) and `LookAtAngles`: `MazeFollowCorridor` (centre the corridor, heading lock), `MazeTurn` (in place, at cell centres only), `MazeScanWalls` (head pan ≤2 s, only on unknown edges).
- **State announcements** (roadmap 4.7): every state change and button gesture is spoken with the `Say` skill, behind `announce.enabled` in `handleMaze.cfg`.
- **Start cell, given or deduced.** The organisers may not say which corner the robot starts in. `start.cell` in `handleMaze.cfg` is `0`, `7`, `56` or `63` when it is known, and `-1` (default) to deduce it:
  - The start is a corner with walls on 3 sides and the robot faces the open side (rules; Q6). That leaves 8 hypotheses: 4 corners × 2 possible open sides.
  - The mapper and localizer work in a **start-relative frame** until the start is known. Exploration does not wait: the goal is the central 2×2 block, which is the same cells under all 8 hypotheses.
  - Every decoded tag removes the hypotheses that cannot explain it (its id names a cell, `MazeTagLayout::candidateFaces` names the face). When one hypothesis is left, the map and pose are re-anchored to the absolute grid, `MazeMap` stores the start cell, and the robot says "start <id>". Each corner's neighbours are unique (0 → 1, 8 · 7 → 6, 15 · 56 → 48, 57 · 63 → 55, 62), so one tag near the start is usually enough.
  - If `start.cell` is given and a tag contradicts it, the given value wins and the conflict is logged (the given value is the operator's decision). The warning names the kind of observation (`tag`, `boundary edge`, `position`).
  - A value other than `-1`, `0`, `7`, `56`, `63` is rejected when the configuration loads (error, treated as `-1`).
  - **`start.heading`** (added 2026-10-16): a corner has two open sides, so a given `start.cell` also needs the heading the robot faces (`north`, `east`, `south`, `west`; it must point into the maze). It is ignored when `start.cell` is `-1`; an invalid one is an error and `start.cell` is treated as `-1`. It replaces `startCell`/`startHeading` in `mazeLocalizer.cfg`/`mazeMapper.cfg`: `handleMaze.cfg` is the single source.
- **Pick-up rule.** The robot is *active* when the game state is `playing` and `FallDownState` is not `pickedUp` (upright without ground contact for a minimum time). Penalized counts as picked up: the robot is active only when it is playing, not penalized and not picked up (`Maze::isActive`). Leaving active (a pick-up, a penalty such as the chest button, or `gc initial` in simulation) goes to `Standby` ("standby"); the localizer puts the pose at the start and the attempt counter grows when it is active again, and the robot says "ready" once per re-activation (put down, unpenalized, `gc playing`) before the tap says "exploring" or "speed run". While penalized, `HandlePlayerState` stands and says "Penalized" / "Not penalized" and `HandleMaze` is not executed at all (CABSL stops at the first root option that acts); `MazeBehavior` sees the new `attempt` of `MazeMap` when the robot is unpenalized, goes to `Standby` and says "ready" (no "standby" is said for a penalty), so it never walks without a tap. A fall is not a pick-up and changes neither.
- **Real robot (no GameController).** Chest tap from `initial` → penalized → playing; a chest tap while playing → penalized (B-Human says "Penalized"; `HandleMaze` does not run), another tap → playing ("Not penalized", then `Standby` and "ready"); a pick-up = `Standby`; 3 head buttons held 1 s → `unstiff` (sits, limp).

**Acceptance criteria**
- [x] In simulation, the full flow works on 3 mazes: WaitingForStart → Exploration → goal → pick-up → Standby → SpeedRun → Finished.
- [x] With `start.cell = -1`, on 4 generated mazes (one per start corner, `--start`), the start cell and heading are deduced correctly and "start <id>" is spoken.
- [x] With `start.cell = -1` and no tag in view at the start, the robot explores in the start-relative frame, and after re-anchoring its map equals the map of the same run with `start.cell` given. *(Offline; see the table.)*
- [x] With `start.cell` given, the deduction is skipped, and a contradicting tag is logged without changing the start.
- [x] In attempt 2, the plan uses known-open edges only (unit-tested in MZ-011, checked in a log).
- [x] Each row of the *State announcements* table is spoken (check the console `Say` output in SimRobot, and on the robot in Lab 3). "exploring" is spoken again on a second run. *(SimRobot done; the robot check stays with Lab 3, MZ-027.)*
- [x] With `announce.enabled = false`, nothing is spoken.
- [x] Changing `t_turn90` in `mazePlanner.cfg` changes the plan on a maze where two routes differ only by their turns (check in simulation).

*Evidence: unit and acceptance tests (`Build/Linux/Tests/Develop/Tests`, 304 passed) and the SimRobot runs of 18 Oct 2026 (steps in `SIMULACION_LABERINTO.md`, *Running the full flow*).*

| Criterion | Offline evidence | SimRobot (18 Oct 2026) |
|---|---|---|
| Full flow on 3 mazes | `MazeBehavior` tests (every transition and phrase), `MazeDrive` tests (action locking, scan once per cell, timeouts), pick-up tests of the localizer and mapper. | Done: waitingForStart → exploration → finished → standby → speedRun → finished on the fixtures and generated mazes. |
| Deduction on 4 corners | `MazeStartAcceptance`: all-faces tags deduce 8 of 8 starts within 1 cell; north/west-only tags without a tag in the start cell: worst 4 cells, always correct. | Done on `start_0_south`, `start_7_west`, `start_56_north`, `start_63_west`: "start hypothesis … (revision 1, 1 left, known)" and "start <id>" spoken. |
| No tag at the start, map equals given-start map | `MazeStartAcceptance`: deduced and given-start maps equal for 4 corners with exact sensors, pose error < 1 mm. | Not compared in SimRobot: every fixture shows a tag from the start cell, so the start was known at revision 1. |
| Given start, contradicting tag | `MazeStartAcceptance`: 80 conflicts, start unchanged; a carried robot put down elsewhere counts conflicts and never changes a known start. | Done: `start.cell = 7` on a cell-0 maze warns ("Latest: tag" / "boundary edge"), `MazeStart` stays 7. A wrong given start corrupts the map, so attempt 2 found no route (the watchdog is MZ-025). |
| Attempt 2 known-open edges only | `MazeRoutes`: the speed run never uses an edge that is not known open. | Done: "all edges known to be open" (after the log fix `12b66340`; the log had counted turns as edges). |
| Announcements, second run | `MazeBehavior`: every phrase of the table, "exploring" again in attempt 2, "ready" after a pick-up or penalty; `SpeechQueue` tests. | Done: ready, tag, motion test, exploring (twice), goal, finished, standby, speed run, start <id>; `pr manual` / `pr none` → "Penalized", "Not penalized", "ready", no walking without a tap. |
| `announce.enabled = false` | `MazeBehavior`: no phrases, phases unchanged. | Done: silent. |
| `t_turn90` changes the plan | `MazeRoutes`: default `turn90Ms` gives a 6-cell staircase with 5 turns, 5000 gives an 11-cell detour with 2 turns; a runtime cost change invalidates the cached plan. | Done: `set parameters:MazePlannerProvider` changed the speed-run route. |

Open after MZ-024: whether dragging the robot in the SimRobot 3D view reports `FallDownState` `pickedUp` was not tried (`gc initial` / `gc playing` and `pr` were used); the robot checks belong to Lab 3 (MZ-027).

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

These tickets turn the notes in `docs/CORRECCIONES.md` into work. They are small and mostly touch `HandleMaze` and the localizer, so they should be built after MZ-024. Suggested order: MZ-042 → MZ-041 → MZ-039.

### MZ-038 — Gyro heading as the maze-north reference (merged into MZ-020)

Merged into **MZ-020** on 16 Oct: the test floor makes the robot slip, so the gyro heading is needed from the first localizer, not after it. The scope, steps and acceptance criteria now live in MZ-020. The number is not reused.

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
maze-north offset (MZ-020) there.

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

### MZ-045 — Pose correction from wall distances

| Field | Value |
|---|---|
| Owner | Bryam |
| Estimate | 3 h |
| Depends on | MZ-019, MZ-020, MZ-017* (real-floor logs) |
| Lab / robot | no (uses the Lab 1 logs) |
| Branch | `feat/MZ-045-wall-pose-correction` |
| Files | `Src/Modules/Modeling/MazeLocalizer/` (modify); `Src/Representations/Modeling/MazeCellPose.h` (modify: `wallFixRate`) |

**Goal.** Correct the pose between tag fixes. The test floor makes the robot slip, so odometry
loses distance and lateral position while no tag is in view. A wall seen by MZ-019 is an
almost always visible reference, so it can correct what the gyro (heading) and the tags
(occasional absolute fix) leave open.

**Trigger.** Build this ticket only if the MZ-020 logs show that the cell estimate is lost, or
the in-cell error grows past the MZ-020 limits, between two tag fixes. Otherwise it stays
proposed.

**Scope.**
- **Side walls** of the current cell → lateral position in the corridor and the angle to the corridor axis.
- **Front wall** of the current or next cell → longitudinal position, the error that slipping causes most.
- Each correction is weighted by the distance error that MZ-019 measured at that range; walls past a parameter range (`wallFix.maxRangeMm`) are ignored.
- Only walls that `MazeMap` already holds as `wall` are used, so an unconfirmed percept cannot move the pose.

**Acceptance criteria**
- [ ] In simulation with the oracle percepts, tags hidden (dropout 1.0) and noisy odometry, the cell estimate stays 100 % correct after 10 cells, and the in-cell error is at most the MZ-020 limits.
- [ ] On the Lab 1 logs, the lateral and longitudinal error between tag fixes is lower with the wall fix than without it, and both numbers are written in the PR.
- [ ] A percept of an edge the map holds as `open` or `unknown` never changes the pose (unit test).

**Note (MZ-020).** Wall percepts snapped with the localizer pose must not be fed back as pose correction (circular). This ticket must use the raw distances (`frontDistance`, `leftDistance`, `rightDistance`).

### MZ-046 — Blocked-walk detection and no false goal arrival

| Field | Value |
|---|---|
| Done | [x] |
| Owner | Bryam |
| Estimate | 3 h first guess |
| Depends on | MZ-019, MZ-020, MZ-024 |
| Lab / robot | no for the simulation part; **yes** to validate the foot bumpers |
| Branch | `feat/MZ-046-blocked-walk` (may be delivered on `feat/MZ-019-wall-perceptor` if done together with MZ-019) |
| Files | `Src/Tools/Maze/MazeDrive.cpp`, `Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Maze/MazeFollowCorridor.cpp`, `Src/Modules/BehaviorControl/SkillBehaviorControl/Options/HandleMaze.h` (modify); `Src/Tools/Maze/MazeMapFusion.*` (modify: wall from a blocked walk); `Config/Scenarios/Maze/` (parameters); tests in `Src/Apps/Tests/Maze/` |

**Problem.** Found in the first closed-loop run of MZ-019 (scene `MazeVision`, 20 Oct): the walls
come from the real `WallPerceptor`, so some walls are unknown when the robot plans. The upper
camera sees the floor only from ≈610 mm ahead, so the walls of the robot's own cell (250 mm) are
never seen.

**Current behaviour, with code locations:**
- Unknown edges count as open in the exploration plan (`MazeRoutes.cpp:52-53`).
- A walk or turn is chosen at the cell centre and kept until its skill finishes or times out (`HandleMaze.h:431-449`, `MazeDrive.cpp:58-71`, walk timeout 20 s).
- The walk finishes on odometry alone (`MazeFollowCorridor.cpp:46-50`).
- Consequences:
  - A robot pushing into a wall still "arrives" in the next cell.
  - When that cell is a goal cell, `HandleMaze` goes to `finished` (`HandleMaze.h:164`) without having reached the goal.
  - A wall the map learns during the walk does not stop it (seen with the 26–27 wall in maze_19).

**Scope.**
- **Detect a blocked walk.**
  - Progress against the commanded walk: the pose or odometry advance is much smaller than expected over a time window.
  - On the real robot, also the foot bumpers (`FootBumperState`).
  - On a blocked walk, stop, step back to the cell centre, and record that edge as `wall` in `MazeMap` with a source the wall percept cannot overturn. The tag veto of MZ-019 is a model for this.
- **Abort on a map change.** When the edge being driven through becomes `wall` in `MazeMap` during the walk or turn, abort the action and replan.
- **No false arrival.** A walk aborted as blocked never counts as reaching the next cell, so `atGoal` cannot become true through a wall.
- **Foot bumpers in simulation.** `KeyStates` is never set in simulation (`LogDataProvider` update is empty), and `NaoV6H25.rsi2` has no touch sensor. Wiring a SimRobot `CollisionSensor` to `KeyStates` is out of scope and a possible follow-up. The simulation part relies on progress detection.

**Acceptance criteria**
- [x] In `MazeVision` with maze_19, the robot never reaches `finished` unless the true robot position is in a goal cell (check against the ground-truth pose). *(3 runs of 419 s: 6 `finished`, all with the true position in a goal cell; before the two fixes below, 3 runs showed a false `finished` with the robot pushing on the wall 27–19 from the north.)*
- [x] A walk into an unknown wall is detected as blocked within a parameter time (`blockedTimeout`, set from the run). The edge becomes `wall`, and the robot replans without a second contact on that wall. *(6 of 6 contacts, 1.1–1.3 s after the contact; see the evidence.)*
- [x] An edge that becomes `wall` during a walk aborts the walk before the contact (unit or scene test). *(Unit tests `MazeDrive.AWalkIsAbortedWhenItsEdgeBecomesAWallBeforeTheSkillIsDone` and `MazeBlockedEdgeChannel.*`; no scene case was observed, the contacts in the runs were all unknown walls.)*
- [x] Unit tests: blocked detection on a synthetic progress trace, and a blocked-walk wall is not turned open by later wall-percept open observations.
- [ ] Real robot (lab): a foot-bumper contact against a panel marks the edge, as a short log in the PR. *(Pending lab, T6; the ticket is closed for the simulation part on 22 Oct 2026.)*

**Evidence (simulation, 21 Oct 2026).** `MazeVisionEval.ros2` (MazeVision + `WallPerceptEvaluator`), maze_19, headless (`QT_QPA_PLATFORM=offscreen`, 420 s), ground-truth pose in the maze frame logged with temporary instrumentation (never committed).

- **Signal (T1, 2 runs, 8 contacts, ~10 000 free walking frames).** The pose and `OdometryData` follow the commanded walk (~91 mm/s) while the true robot stands (9 ± 9 mm/s), so progress cannot detect a blocked walk. The yaw rate (`InertialData.gyro.z`) can: standard deviation over 250 ms is 0.06 free (p99 ≤ 0.19) and 1.74 rad/s blocked (≥ 0.96), rising within ~120 ms of the contact. Joint errors, `ArmContactModel` and `FallDownState` do not separate; there is no sonar module.
- **Rule and parameters** (`blockedWalk` in `handleMaze.cfg`, `PARAMETROS_LABERINTO.md` section 8): a walk is commanded (`MotionInfo` walking, forward speed ≥ `minWalkSpeed` 100 mm/s) and the deviation of the yaw rate over `gyroWindowMs` 250 exceeds `gyroStdThreshold` 0.5 rad/s for `blockedTimeoutMs` 1000 without a gap. The commanded speed dips under 100 mm/s for up to ~300 ms now and then (also while pushing), so the alarm survives a dip of `walkGapMs` 400. On the robot a `FootBumperState` contact for `bumperHoldMs` 100 during a forward walk blocks too (not in SimRobot).
- **Results with the final code** (3 runs of 419 s; the same two walls are hit in each run: cell 2,3 facing south at ~60-72 s and cell 3,7 facing west at ~118-144 s):

| Check | Result |
|---|---|
| Contacts (true position stalls < 30 mm/s for > 0.5 s while a walk is commanded) | 6 (2 per run) |
| Detected as blocked | 6 of 6, 1.1–1.3 s after the contact (1 s hold + the 120 ms window rise) |
| False detections (blocked while the true robot moved freely) / missed contacts | 0 / 0 |
| Edge becomes `wall` in `MazeMap` (mapper line `MazeMapper: blocked at cell ... now a wall.`) | 6 of 6, in the next frame |
| Step back walks backwards; true distance to the centre of the origin cell | 8, 18, 24, 46, 50, 56 mm when the backwards walk ends (15–69 mm 0.6 s later) |
| Second contact on a blocked wall | 0 |
| `finished` / "goal" with the true position outside a goal cell | 0 of 6 (the first `finished` of each run 1.1–1.2 s after the true robot entered the goal block) |

- **False arrival before the fixes.** With the walk of T4 only, three runs finished with the true robot at (1.72 m, 2.61 m), 10 cm short of the wall between cells 19 and 27, in cell 19 (row 2, col 3): odometry was already in the goal cell. Two defects, each fixed with a regression test: (1) `arrivalAllowed()` was false only after the detection, so the pose cell counted as an arrival while the walk still ran during the 1 s hold (`fix(MZ-046): a pose in the next cell is no arrival while the walk still runs`); (2) the commanded speed fell to 81 and 87 mm/s during the contact and each dip restarted the 1 s alarm, so nothing was detected (`fix(MZ-046): a dip of the commanded walk speed does not restart the gyroscope alarm`, `walkGapMs`).
- **Known limitations.**
  - The gyro threshold is derived in simulation and the oscillation may be an artefact of the simulated contact; recalibrate `gyroStdThreshold` on the robot.
  - The foot bumpers exist only on the robot (`KeyStates` is never set in SimRobot); the bumper path is covered by unit tests only (lab criterion pending).
  - Odometry follows the commanded walk, so after a contact the pose is 130–190 mm ahead of the true position (and the step back ends 10–70 mm from the centre of the origin cell, not exactly on it).
  - Only forward pushes and one maze were measured; both walls are hit head-on, not at an angle.
  - A block published in the same frame as a rebuild of the map under another start hypothesis is dropped by the mapper (warning `MazeMapper: a blocked edge ... is dropped`); the second contact is detected again.
  - Natural contacts are few (the wall percept sees most walls), so the 6 contacts are 2 distinct walls repeated.

### MZ-047 — Scan the next cell from the current one, with a held ±30° look

| Field | Value |
|---|---|
| Owner | Bryam |
| Estimate | ~4 h first guess (≈1 h baseline runs, ≈2 h code and tests, ≈1 h runs and docs) |
| Depends on | MZ-019, MZ-024, MZ-046 (the blocked-walk detector stays as the safety net) |
| Lab / robot | no for the simulation part; yes later, to confirm the head angles and dwell on real images |
| Branch | `feat/MZ-047-scan-next-cell`, created from the tip of `feat/MZ-019-wall-perceptor` (chained PR, see *Delivery*) |
| Files | `Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Maze/MazeScanWalls.cpp` (modify); `Src/Tools/Maze/MazeDrive.{h,cpp}` (modify: when to scan, `scanTimeoutMs`); `Src/Modules/BehaviorControl/SkillBehaviorControl/Options/HandleMaze.h` (modify: drive input); tests in `Src/Apps/Tests/Maze/MazeDriveTest.cpp`, `MazeRoutesTest.cpp`; docs listed below |

This ticket is a scheduled slice of MZ-037: it changes the H1 "stop and scan" strategy and leaves the comparison of other gaze strategies to MZ-037.

**Problem.** The robot scans the walls of a cell **after it arrives** there. That is too late: the upper camera only sees the floor from 546 mm ahead (630 mm from the robot origin), or 389 mm with the head at its maximum pitch. The walls of the cell the robot stands in are only 250 mm away. The robot never sees them, at any pan or tilt. An unknown edge counts as open, so the robot can plan through a wall it never saw and walk into it. MZ-046 then detects the block 1.1–1.3 s after the contact.

In maze_19 (scene `MazeVision`) this happens at two walls:
- The robot comes up column 2 to cell 18, turns east, walks to cell 19, scans there and turns south. It then walks into the wall 19|27 in every run.
- The same happens with the wall 32|33 when the robot comes south from cell 25 to cell 33.

**Measurement behind the change (21 Oct 2026).** Only the pan-0 upper-camera frame at cell 19 was measured in SimRobot. The rest comes from an emulation of the camera geometry and the `WallView` snap rules, validated against that frame: 56/56 boundary points, first wall point at 644–654 mm. The raw data and the emulation scripts were scratch files and are not in the repository.

| Scan position, facing the next cell | Wall of the next cell seen by the upper camera | Edge marked by the mapper |
|---|---|---|
| Centre of the cell itself (19, 33), today | 0 mm at any pan or tilt | no |
| Midpoint of the shared edge (18\|19, 25\|33) | 0 mm | no: the mapper ignores the look-ahead more than `maxOffsetFromCentre` 150 mm from a cell centre (`MazeStartAnchoredMap.cpp` `lookAheadUsable`) |
| Centre of the previous cell (18, 25) | 124 mm (6–9 points) at pan −30° and tilt 0.38; 177–208 mm at the maximum head pitch | yes, if the head holds that pan long enough (inferred) |
| Two cells before (17) | 292–333 mm at pan −30° or 0° | yes, if held about 1 s (inferred) |

The head pitch limit falls with pan: 0.515 rad at 0°, 0.42 at ±28°, 0.31 at ±60°. A tilt of 0.38 rad still fits at ±30°.

**Current behaviour, with code locations:**
- **When.** A scan runs once per cell and phase, on arrival, before any turn. It runs only when the robot's **own** cell has an unknown edge (`MazeDrive.cpp:145`, `scannedCell` at `MazeDrive.h:178`). Both exploration and return to start scan. The speed run never does.
- **How.** `MazeScanWalls` pans +60° for 550 ms, −60° for 800 ms and 0° for 450 ms, at tilt 0.38 and 200°/s. Its parameters are `DEFINES_PARAMETERS` in the skill and have no cfg. The head only crosses ±30° on its way to ±60°, so it never holds a near side wall of the next cell in view.
- **Timeout.** `scanTimeoutMs` is 2500 (`MazeDrive.h:32`).
- **Mapper.**
  - Per frame, the perceptor needs at least 3 snapped points (`wallPerceptor.cfg`).
  - The gaze is snapped to the nearest maze heading. At pan ±30° it snaps to the robot heading, so the look-ahead reports the left, right and front edges of the cells straight ahead, up to 3 cells while standing.
  - The mapper takes at most one observation per edge every `minObservationInterval` 250 ms. Each observation is weighted 1 up to 500 mm, then less down to 0 at 2500 mm. An edge is decided at a score of `threshold` 3 (`mazeMapper.cfg`, `MazeMapFusion.cpp`).
  - So a side needs about 3–4 observations, that is a hold of **0.75–1 s**, to decide an edge by itself. Today's scan gives each side about 0.2–0.25 s.

**Scope.**
- **When to scan.**
  - Scan the **next** cell from the centre of the current one, after the turn toward it and before the walk.
  - The trigger is that the next cell has an unknown edge visible from there: its left, right or front edge.
  - Remove the scan on arrival.
  - Key the bookkeeping by cell and heading, so a replan to another heading may scan again.
  - If the scan turns the planned edge into a wall, the robot replans and turns again as today.
- **How to scan.** Change `MazeScanWalls` to:
  1. hold pan +30°;
  2. hold pan −30°;
  3. hold pan 0°.
  - The hold time per side is a parameter. Start at 1000 ms and set it from the runs.
  - Pan angle and tilt are parameters too. Keep 0.38 unless the runs show that a larger tilt within the pitch limit sees more of the wall.
  - Raise `scanTimeoutMs` above the new scan length. Today's 2500 would cut a 3 s scan.
- **Unchanged.**
  - The goal look of MZ-024.1 (`requestScan()` at the first return cell) keeps its scan in that cell. Its look-ahead into the neighbouring goal cells depends on it (`MazeRoutes.TheReturnTriesTheUnknownShortcutInsteadOfTheWalkedLoop`). If it can use the new ±30° scan, it may; otherwise keep today's ±60° sequence for it.
  - The speed run never scans.
  - The look-ahead while walking stays as it is (range 1).
- **Out of scope.**
  - A wall perceptor on the lower camera. That is the real fix for walls 250 mm away: the lower camera sees 319–373 mm of the wall base from the cell centre.
  - Scanning while walking (MZ-037 H2).
  - The start scan of MZ-041.

**Trade-off to watch.** Today the ±60° scan on arrival looks down the side corridors of the current cell, up to 3 cells. The new scan looks only forward (±30° snaps to the robot heading). So side corridors are seen only after turning into them. The exploration may need more turns or reach the goal later. The runs must show this cost.

**Steps**
1. **Baseline.** With the current code, run `MazeVisionEval.ros2` headless, 420 s each:
   - 3 runs on maze_19, plus 1 run on each of 2 other mazes from `Config/Mazes/generated/`.
   - Record per run:
     - the contacts and blocked walks (`MazeBlocked:` lines; ground truth with the stash instrumentation below);
     - the time to the first `finished`;
     - the final `WallPerceptEvaluator` summary.
   - In one run, confirm from `JointAngles` that the head really reaches the commanded pan. A measurement that overrode the head motion request saw the joints stay at pan 0; the normal skill path was not checked.
2. **The skill.** Change `MazeScanWalls` (held ±30°, then 0°, with parameters) and `scanTimeoutMs`.
3. **The drive.** Move the trigger in `MazeDrive`, with its `HandleMaze` input for the unknown edges of the next cell. Update the tests listed below and add new ones.
4. **Runs and docs.** Repeat the runs of step 1 with the new code and compare. Update the docs and this ticket with the evidence.

**Tests to update** (`Src/Apps/Tests/Maze/`):
- In `MazeDriveTest.cpp`:
  - `ACellWithUnknownEdgesIsScannedOnceAndOnlyWhenExploring`
  - `TheGoalLookScansOnceEvenWithoutUnknownEdgesThenFollowsTheRoute`
  - `TheGoalLookWaitsForAPlanOfThePoseAndIsNotKeptByAReset`
  - `ASpeedRunIsNeverForcedToLook`
  - `ANewPhaseMayScanTheCellAgain`
  - `ActionsEndWhenTheSkillIsDoneOrTheTimeoutPasses`
  - `ScansAndIdleAreNeverAbortedByTheMap`
  - `APoseInTheNextCellIsNoArrivalWhileTheWalkRunsAndCountsOnceTheDriveChoosesAgain`
- In `MazeRoutesTest.cpp`: the `scan()` helper, which models three gazes on arrival, and `TheReturnTriesTheUnknownShortcutInsteadOfTheWalkedLoop`.

**Docs to update:**
- `TICKETS_LABERINTO.md`: MZ-024 skills, the test table and the look-ahead section. Its range "2 walking" is out of date: the cfg says 1.
- `ROADMAP_LABERINTO.md`: the MazeExploration and ReturnToStart rows, and the WallPerceptor row, which also says "2 walking".
- `PARAMETROS_LABERINTO.md`: sections 5, 8 (`scanTimeoutMs` and the skill parameters) and 13.
- `SIMULACION_LABERINTO.md`: step 2 and the goal look.
- The header comments of `HandleMaze.h` and `MazeDrive.h`.

**Acceptance criteria**
- [ ] Unit tests:
  - the drive scans after the turn toward a next cell with unknown edges, and before the walk;
  - it never scans on arrival;
  - it scans again after a replan to another heading;
  - the goal look and the speed run behave as before.
- [ ] `MazeScanWalls` holds each side for its hold parameter. The scan finishes before `scanTimeoutMs`.
- [ ] In `MazeVisionEval` with maze_19 (3 runs), the robot never walks into the wall 19|27 or 32|33. Each wall is in `MazeMap` before the robot would walk through it. Check this against the ground-truth pose.
- [ ] Over the step 1 runs:
  - contacts per run are not higher than the baseline;
  - the time to the first `finished` and the `WallPerceptEvaluator` accuracy are reported against the baseline;
  - a slower run is acceptable only if this ticket states it and the reason.
- [ ] No `finished` with the true robot outside a goal cell (MZ-046 criterion, still true).

**Delivery (chained PRs).**
- **PR order.**
  1. The MZ-019 PR, `feat/MZ-019-wall-perceptor` → `Maze-Challenge`, carries MZ-019 and MZ-046 and is reviewed on its own.
  2. The MZ-047 PR, `feat/MZ-047-scan-next-cell` → `feat/MZ-019-wall-perceptor`, then shows only the MZ-047 commits.
- **Create the branch:** `git switch feat/MZ-019-wall-perceptor && git switch -c feat/MZ-047-scan-next-cell`.
- **If the MZ-019 branch moves.** When `feat/MZ-019-wall-perceptor` gets new commits, for example review fixes, rebase MZ-047 onto it so the PR diff stays only MZ-047.
- **After MZ-019 merges.** Retarget the MZ-047 PR to `Maze-Challenge` and rebase it onto `Maze-Challenge` if the diff shows MZ-019 commits.
- **PR body.** It states the dependency on the MZ-019 PR, with this diagram:

```
Maze-Challenge
 └─ feat/MZ-019-wall-perceptor   (MZ-019 + MZ-046)
     └─ feat/MZ-047-scan-next-cell 📍 (this PR)
```

- **Size.** Keep the PR within about 400 changed lines (additions plus deletions) with its tests and docs. If it grows past that, split it into the skill and timeout change and then the drive trigger, each with its tests.

**Handoff for a new session.** Create `feat/MZ-047-scan-next-cell` from `feat/MZ-019-wall-perceptor` (see *Delivery*) and work there.
- **State.** MZ-046 is committed and reviewed on the MZ-019 branch; this ticket is the passive `713f0ce7`. On the new branch, the first RDD assess uses the branch point (the MZ-019 tip) as its base. MZ-046 T6 (bumper in the lab) is still open on the MZ-019 branch and is independent of this ticket.
- **Ground truth.** `git stash@{0}` ("MZ-046 T5 temporary instrumentation") logs the ground-truth pose for the contact count. Apply it only for runs and never commit it.
- **How to run.** Follow `SIMULACION_LABERINTO.md`: step 3 generates the maze scene, "Seeing a blocked walk" says how to read a blocked walk, and section 7 covers `MazeVisionEval` runs.
- **Tag caveat.** In simulation the AprilTags come from the oracle, which ignores the vertical field of view. It reports tags on the walls of the robot's own cell that the real camera cannot see, so a tag can mark a wall in simulation that the robot would miss on real images.
- **GPU hang.** One 21 Oct session saw the Intel GPU hang while SimRobot ran, which took the desktop down. If it repeats, run SimRobot with `prime-run`.

### MZ-024.1 — Exploring return to start (subtask of MZ-024)

| Field | Value |
|---|---|
| Done | [x] |
| Owner | Bryam |
| Estimate | 3 h |
| Depends on | MZ-024 |
| Lab / robot | no |
| Branch | `feat/MZ-024.1-return-to-start` |
| Files | `Src/Tools/Maze/MazePlanner.{h,cpp}` (modify: target cell set); `Src/Apps/Tests/Maze/MazePlannerTest.cpp` (modify); `Src/Modules/BehaviorControl/MazePlannerProvider/` (modify); `Src/Modules/BehaviorControl/SkillBehaviorControl/Options/HandleMaze.h` (modify: `ReturnToStart` state); `Config/Scenarios/Maze/handleMaze.cfg` (modify) |

**Goal.** After attempt 1 reaches the goal, walk back to the start cell **while still exploring**:
the return plan is optimistic (unknown edges assumed open, like `MazeExploration`), so it tries
routes that may be shorter than the known one. Example, `maze_19.maze`: after reaching cell 36 by
the long eastern loop, the return tries 35 → 43 → 42 → 34 through unknown edges instead of walking
the same loop back. Every edge seen on the way goes into `MazeMap`, so attempt 2 can use it.

**Scope.**
- **Planner target set.** `MazePlanner` today plans only to the central goal block (`isGoal` in `MazeGraph.h`). Add a target cell set to `computeCostToGo`, `speedRun` and `heuristic`, defaulting to the goal block, so the existing tests and callers do not change. The return uses the target {start cell}: the start cell stored in `MazeMap` (from the `.maze` file in simulation; given by `start.cell` or deduced from the tags, MZ-024).
- **`ReturnToStart` state** (roadmap 4.7): the same plan-walk-replan loop as `MazeExploration`, with the start cell as the target. It ends at the start cell, after `returnToStart.maxDurationMs`, or on pick-up, and always goes to `Standby`. It says "returning" when it begins.
- **Parameters** in `handleMaze.cfg`, so the trigger can follow the organisers' answers (Q3, Q9) without a code change:

| Parameter | Meaning | Default |
|---|---|---|
| `returnToStart.enabled` | `false`: stand at the goal (`Finished`), as without this ticket | `false` |
| `returnToStart.trigger` | `timer`: start the return `delayMs` after the goal is confirmed. `button`: start it on a single `headFront` tap while standing at the goal (that tap is free in this state). | `timer` |
| `returnToStart.delayMs` | Wait after the goal is confirmed, for `trigger = timer` | 5000 |
| `returnToStart.maxDurationMs` | Return time limit (rules: ≤2 min; whether it counts inside the 5 min is Q3) | 120000 |

**Acceptance criteria**
- [x] Unit test: on `maze_19.maze` with the map known after the walk in the example, a return plan from cell 36 to the start passes through 35 → 43 → 42 → 34 (unknown edges assumed open), not back along the walked loop. *(After the goal look; see below.)*
- [x] The existing planner tests pass unchanged with the default target (the goal block).
- [x] In simulation with `trigger = timer`, the return starts `delayMs` (±0.5 s) after the "goal" announcement; with `trigger = button`, it starts only after the `headFront` tap. *(The exact delay is checked offline with the injected clock.)*
- [x] With `returnToStart.enabled = false`, the robot stays at the goal (`Finished`), as before.
- [x] In simulation on 3 mazes, the robot reaches the start cell, or stops at `maxDurationMs`, and goes to `Standby`; the edges found on the way are in `MazeMap`, and the attempt-2 plan uses them (check in a log).

**Evidence:** unit and acceptance tests (`Build/Linux/Tests/Develop/Tests`, 329 passed) and the SimRobot runs of 18 Oct 2026 (button order in `SIMULACION_LABERINTO.md`, *Running the return to start*). `handleMaze.cfg` (`Maze` and `MazeSafe`) now sets `enabled = true` and `trigger = button`.

- Example log (`maze_19`, after the return, `pr manual`/`pr none`, robot turned by hand): `HandleMaze: speed run over 16 cells [0 8 16 24 24 25 25 33 33 34 34 42 42 43 43 35] ... all edges known to be open.`
- Known limitation, outside this ticket: if the robot turns 180° at the start without a pick-up (the speed run then begins with `turnAround`), the localizer can lock on a wrong pose (one run: true cell 0 facing south, estimate cell 32 facing north, `quality = superb`, 8957 tag fixes, 0 rejections), and the mapper writes wrong walls. The procedure is therefore a penalty (`pr manual`/`pr none`, or a pick-up on the robot) and a turn by hand before attempt 2. Tag fixes only match faces within 45° of the estimate, so a heading lost by more than that is never recovered.
- Start deduction while standing needs a tag whose whole square is inside the camera's field of view; `maze_19` (corridor ending 3 cells away) needed the oracle's `maxTagDistance` raised to 2000 mm (`oracledMazePerceptsProvider.cfg`; the real detector allows 3000 mm).

**Offline evidence (branch `feat/MZ-024.1-return-to-start`).**
- `MazeRoutes.TheReturnTriesTheUnknownShortcutInsteadOfTheWalkedLoop`: `maze_19`, map known after the walk (the oracle look-ahead of the wall scan at every cell of the long eastern loop). Before the goal look the return is 36 → 44 → 43 → 42 → 34 → … (44-43 unknown ties with 35 and saves a start-up); after the goal look (one wall scan at 36, the gaze south sees 44 and the wall 44-43) it is 36 → 35 → 43 → 42 → 34 → … → 0 and never enters 37.
- Goal look and attempt: when the return begins, `HandleMaze` forces one `MazeScanWalls` at the goal cell even without unknown edges (`DriveController::requestScan`, tests in `MazeDriveTest`); only a goal reached by the exploration arms the return (`MazeReturnToStart.OnlyAGoalOfTheExplorationArmsTheReturn`).
- The planner tests are unchanged and pass with the default target; `MazePlanner.TheDefaultTargetSetIsTheGoalBlock` compares the default with an explicit goal block.
- `MazeReturnToStart.*` (phase machine, injected clock): disabled → "goal", "finished" and no return; timer at `delayMs` (before/after); button only on a tap; end at the start, at `maxDurationMs` (also without route) or on pick-up; new attempt during the return → `standby`.

---

## Physical robot tickets (testing method not settled)

These tickets need the NAO, the lab or a physical maze. We are **not sure how we will test on hardware** (lab access, a physical maze and robot time are not guaranteed), so no software ticket may depend on them in a hard way. Their dates and hours are the plan if everything is available.

If a lab session is lost, the fallback is: keep the code frozen on simulation results, use the `Maze` conservative defaults, and treat the competition's 5-minute calibration zone (roadmap section 7) as the first real test.

Order: bench (MZ-014, MZ-015) → Lab 1 (MZ-017) → detector tuning (MZ-018.1) → Lab 2 (MZ-023) → Lab 3 (MZ-027) → Lab 4 (MZ-031) → Lab 5 (MZ-033), then the proposed and backlog tickets that need the robot (MZ-040, MZ-044).

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

**Scope.** Ids 0, 1, 7, 8, 9, 56 and 63, from the AprilRobotics `apriltag-imgs` repository (folder `tag36h11`). The **black outer square measures 150 mm**, printed on matte paper (no glossy finish), glued **centred horizontally with the tag centre at `centreHeight` above the floor** (30 cm by default), on the faces that `mazeTagLayout.cfg` lists. If Q1 changes the layout, the file and the mounts change together.

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

**Goal.** Record real camera, odometry and gyro logs, so the tag detector (MZ-018) and the wall perceptor (MZ-019) can be developed offline, and the gyro drift can be measured (MZ-020).

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

### MZ-018.1 — Tune the AprilTag detector on the real robot ⚠️ TAG-DEP

Subtask of MZ-018. The detector is merged with untuned defaults; this ticket only sets the parameters from real images. The direction of the pose and the `headRear` self-test were already checked on the robot in MZ-018.

| Field | Value |
|---|---|
| Owner | Wilson (Bryam on call by phone) |
| Estimate | 1.5 h (robot) |
| Depends on | MZ-018, MZ-015 (printed tags; one tag on a box or wall is enough if the bench is not built) |
| Lab / robot | **yes** (robot available now, 16 Oct) |
| Branch | `chore/MZ-018.1-tag-detector-tuning` |
| Files | `Config/Scenarios/Maze/aprilTagDetector.cfg` and `Config/Scenarios/MazeSafe/aprilTagDetector.cfg` (same values in both); results in `docs/maze/LAB_LOG.md` (section *MZ-018.1*) |

**Goal.** Choose the values of `aprilTagDetector.cfg` from measurements on the NAO, so that tags are read at the distances the maze needs (0.25 to 1.5 m) without wrong ids and within the time budget.

**What each value does.** The tag size (150 mm) is not here; it comes from `mazeTagLayout.cfg`. The filters run in this order: id, hamming and margin first, then the pose, then distance and size.

| Parameter | Default | What it does | Raise it → | Lower it → | How to choose it |
|---|---|---|---|---|---|
| `minDecisionMargin` | 30 | Confidence of the decoding: how clearly the bits of the tag differ from the threshold. Good, close tags score far above 50; far, blurred or dark tags score less. | Fewer false positives, but far or dark tags disappear. | More far tags, more risk of noise being read as a tag. | Write down the `decisionMargin` of every correct detection and of every false one. Use a value below the smallest correct margin at 1.5 m and above every false one. |
| `maxHamming` | 0 | How many bits the library may correct to read an id (it corrects up to 2). 0 accepts only perfect reads. | 1–2: more reads of blurred tags, but a damaged tag can be read as another id. | – (0 is the minimum). | Keep 0 unless tags are lost at 1.0–1.5 m while walking. Try 1 only then, and check that no wrong id appears. A wrong id is worse than a missed tag: it puts the robot in the wrong cell. |
| `sizeTolerance` | 0.5 | Consistency check: the distance from the pose must match the distance implied by the tag's size in the image (±50 %). It drops poses that the library got wrong. | Accepts more poses, including bad ones. | Rejects bad poses, but may drop correct tags seen at an angle or near the image border. | At 30° and 60°, check that no correct tag is dropped (the tag is drawn in the image but missing from the percept). Then lower it in steps of 0.1 while that stays true. |
| `frameSkip` | 1 | Run the detector on every n-th upper image only (1 = every image). The percept is empty on the skipped images. | Less CPU, fewer detections per second. | – (1 is the minimum). | Read the time in the `timing` view of the Upper thread (`AprilTagDetector`). Keep 1 if the average is ≤ 20 ms; otherwise use 2. |
| `maxDistance` | 3000 mm | Tags farther than this are dropped, because a far pose is less precise. | Far tags are used. | Far tags are ignored. | Measure the distance error (reported vs. tape) at 0.5, 1.0, 1.5 and 2.0 m. Set it to the largest distance where the error is ≤ 5 cm. |
| `maxId` | 63 | The maze uses ids 0–63 (row × 8 + column); a larger id is a false positive. | – | – | Do not tune. Change it only if the maze grid changes. |
| `quadDecimate` | 2 | The image is shrunk by this factor to find the tag squares (the id is still read at full size). 1 = no shrink. | Faster, but small (far) tags are lost. | Detects smaller, farther tags, but costs much more time (1 is about 3–4× slower than 2). | Compare 1, 2 and 3: the farthest distance where the tag is still read, and the time. Keep the largest value that still reads tags at 1.5 m. |

**Changing a value live.** The values can be changed without redeploying. In the RemoteRobot console, `for Upper get parameters:AprilTagDetector` prints a `set ...` line with all the values. Edit the number and press Enter. These changes are lost on restart, so copy the final values into both `.cfg` files.

**Steps**
1. Deploy and start the robot standing, with the head still:
   ```bash
   Make/Linux/compile Develop Nao
   Make/Common/deploy Develop <robot-ip> -s Maze
   ```
2. Open `Config/Scenes/RemoteRobot.ros2`, connect to the robot, and type `call Includes/AprilTagViews`. The `upper` image shows each tag with a green outline and its id. `for Upper get representation:AprilTagPercept` prints the id, the pose in the robot frame and the `decisionMargin`.
3. **Sweep with the defaults.** Place one tag at 0.25, 0.5, 1.0, 1.5 and 2.0 m from the robot, each at 0°, 30° and 60°, with the tag centre at 30 cm height. For each position write: whether the id was read, the id, the `decisionMargin`, the reported distance (the norm of the x and y of `tagInRobot`) against the tape, and the time in the `timing` view.
4. **False positives.** Turn the robot (or the head) towards the room without any tag for 1 minute: lights, windows, chairs, posters. Write down every detection and its `decisionMargin`.
5. **Tune.** Change one value at a time (the table above, from top to bottom), repeating the positions that the change affects.
6. Write the results table and the chosen values in `docs/maze/LAB_LOG.md` (section *MZ-018.1*), copy the values into both `aprilTagDetector.cfg` files, and open a PR from `chore/MZ-018.1-tag-detector-tuning`.

**Acceptance criteria**
- [ ] The sweep table (5 distances × 3 angles) is in `LAB_LOG.md` with id, `decisionMargin`, reported vs. measured distance and time.
- [ ] With the chosen values, ids are read at 0.25–1.5 m at 0° and 30°, and there is no wrong id in the whole session, including the false-positive minute.
- [ ] The average `AprilTagDetector` time is ≤ 20 ms, or `frameSkip = 2` is set.
- [ ] Both `aprilTagDetector.cfg` files have the chosen values, each changed value with a one-line comment giving its reason.

### MZ-020.1 — Measure the real gyro drift from the Lab 1 logs

Subtask of MZ-020. The gyro heading is merged and checked in simulation with an injected drift; this ticket only measures the drift of the real NAO gyro, which the simulator does not have.

| Field | Value |
|---|---|
| Done | [ ] |
| Owner | Bryam |
| Estimate | 0.5 h |
| Depends on | MZ-020, MZ-017 (Lab 1 logs with `InertialData`) |
| Lab / robot | no (uses the Lab 1 logs) |
| Branch | `docs/MZ-020.1-gyro-drift` |
| Files | `docs/maze/GYRO_HEADING.md` (section 6, the empty drift table) |

**Goal.** Know how fast the real yaw drifts, so the team knows how often the localizer needs a tag fix.

**Steps.** Follow the procedure in `GYRO_HEADING.md`, section 6: from the Lab 1 logs, take the yaw of `InertialData.orientation3D` over a 5 min stand and over a corridor walk, and fill the table (° per minute). If the walking drift is larger than the 3°/min of the offline runs, repeat the acceptance suite with the measured value (`injectedGyroDriftPerMinute`) and record the result.

**Acceptance criteria**
- [ ] The gyro drift rate (° per minute), standing and walking, is in `GYRO_HEADING.md`.
- [ ] The offline acceptance runs still meet the MZ-020 limits with the measured drift.

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
| Files | `Config/Mazes/bench/route_2x4.maze` (create); `docs/maze/BENCH.md` (add the floor layout); the motion test of MZ-007 (add the route) |

**Goal and scope.** The lab cannot hold an 8×8, but a 2×4 block holds the motions that matter. It
keeps the real tag ids (`id = row·8 + col`): cells 0–3 in row 0 and cells 8–11 in row 1.
Route: straight 3 cells (0 → 3), turn right, 1 cell (3 → 11), turn right, straight 3 cells (11 → 8). See roadmap section 6.

**Steps**
1. Write the maze file for the simulation: 2×4 with the start at cell 0, open cells 0–3 and 8–11, and an open edge between 3 and 11 only.
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

Seeing and mapping the walls of cells ahead (corridor look-ahead) is no longer part of this ticket:
it was moved into the scheduled tickets MZ-019 and MZ-020 on 13 Oct. This ticket keeps the gaze
strategy: where and when to point the head.

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
| H3 | Look-ahead and information-driven gaze | Look down corridors to map several cells at once (the perception and mapping side is scheduled in MZ-019 and MZ-020), and point the head at the unknown wall that would change the plan the most. |

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
