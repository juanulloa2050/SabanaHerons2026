# Maze Challenge Tickets — Copa NAO CCM MX 2026

This file lists every ticket (MZ-001 … MZ-037) for the Maze challenge, grouped by sprint. Each
ticket says who owns it, how long it should take, what it depends on, which branch to use, and
how to tell that it is done. Start with the summary table, then open your sprint.

Related: [ROADMAP_LABERINTO.md](ROADMAP_LABERINTO.md) (architecture, Git workflow, validation
strategy) · [GANTT_LABERINTO.md](GANTT_LABERINTO.md) (schedule, lab access request).

---

## Summary

### Hours per person

| | Authoring | Lab sessions | Reviewing the other's PRs | **Total** |
|---|---|---|---|---|
| **Bryam** | 39.25 h | 7 h (Lab 3, 4, 5) | 3.75 h | **50.0 h** |
| **Wilson** | 29.25 h | 11 h (Lab 1–5) | 4.75 h | **45.0 h** |

Notes:
- **Authoring** means the owner's own work, including opening the PR, answering review comments and merging. Lab tickets are counted under *Lab sessions*, not under authoring.
- **Reviewing** means reviewing the other person's PRs. It appears in each ticket as "Review".
- Bryam's 39.25 h authoring = 38.75 h of own tickets + 0.5 h pairing on MZ-010.

### Tickets needing the lab or the physical robot

**5 of 34 scheduled tickets** need the lab: MZ-017, MZ-023, MZ-027, MZ-031, MZ-033 (11 h of room time in
total). Everything else is done at a desk, in simulation, or on recorded logs.

### TAG-DEP marker

⚠️ **TAG-DEP** marks tickets whose design depends on the organisers' answer about which walls
carry tags and at what height (question Q1, ticket MZ-006): MZ-013, MZ-015, MZ-017, MZ-018,
MZ-020, MZ-022. Until the answer arrives, they assume **one tag centred on every wall face of
every cell, tag centre at 30 cm above the floor**. If the answer differs, only the tag model in
`Src/Tools/Maze/` and the tag mounts change.

### All tickets

| ID | Title | Owner | Sprint | Author h | Review h | Lab | Depends on |
|---|---|---|---|---|---|---|---|
| MZ-001 | Create and publish `Maze-Challenge` | Bryam | 0 | 0.25 | – | no | – |
| MZ-002 | Request branch protection from repo owner | Wilson | 0 | 0.25 | – | no | MZ-001 |
| MZ-003 | Planning docs (roadmap, tickets, Gantt) | Bryam | 0 | 1 | W 0.25 | no | MZ-001 |
| MZ-004 | PR template | Wilson | 0 | 0.5 | B 0.25 | no | MZ-001 |
| MZ-005 | Registration confirmation | Wilson | 0 | 0.25 | – | no | – |
| MZ-006 | Organiser questions and answers | Wilson | 0 | 0.75 | B 0.25 | no | MZ-003 |
| MZ-007 | Maze/MazeSafe scenarios + start flow | Bryam | 0 | 2.5 | W 0.5 | no | MZ-001 |
| MZ-008 | Vendor apriltag + CMake | Bryam | 0 | 3 | W 0.5 | no | MZ-001 |
| MZ-009 | Maze format + generator | Wilson | 0 | 2 | B 0.5 | no | MZ-004 |
| MZ-010 | Maze → SimRobot scene converter | Wilson (+Bryam 0.5 pairing) | 0 | 2.5 | B 0.5 | no | MZ-007, MZ-009 |
| MZ-011 | Maze library + time-weighted planner + tests | Bryam | 1 | 6 | W 0.5 | no | MZ-009 |
| MZ-012 | Test mazes + expected results | Wilson | 1 | 1.5 | B 0.25 | no | MZ-009 |
| MZ-013 | Oracle maze percepts for simulation ⚠️ | Bryam | 1 | 2 | W 0.5 | no | MZ-010, MZ-011 |
| MZ-014 | Build the partial bench | Wilson | 1 | 3.5 | – | no | – |
| MZ-015 | Print and mount tags ⚠️ | Wilson | 1 | 1 | – | no | MZ-014 |
| MZ-016 | Corridor walk profile in simulation | Wilson | 1 | 3 | B 0.25 | no | MZ-007, MZ-010 |
| MZ-017 | Lab 1: log capture ⚠️ | Wilson | 1 | 2 | – | **yes** | MZ-007, MZ-014, MZ-015 |
| MZ-018 | AprilTag detector ⚠️ | Bryam | 2 | 5 | W 0.5 | no | MZ-008, MZ-017 |
| MZ-019 | Wall perceptor | Bryam | 2 | 4 | W 0.5 | no | MZ-010, MZ-017 |
| MZ-020 | Localizer + mapper ⚠️ | Bryam | 2 | 5 | W 0.5 | no | MZ-011, MZ-013 |
| MZ-021 | Headless simulation campaign runner | Wilson | 2 | 3 | B 0.5 | no | MZ-010 |
| MZ-022 | Tag accuracy report ⚠️ | Wilson | 2 | 2.5 | B 0.25 | no | MZ-017, MZ-018 |
| MZ-023 | Lab 2: calibration of motion costs | Wilson | 2 | 2 | – | **yes** | MZ-016, MZ-018 |
| MZ-024 | HandleMaze state machine + skills | Bryam | 3 | 5 | W 0.5 | no | MZ-011, MZ-020 |
| MZ-025 | Watchdog, safe mode, wall-follower | Bryam | 3 | 2.5 | W 0.25 | no | MZ-024 |
| MZ-026 | Simulation campaign + report | Wilson | 3 | 3 | B 0.25 | no | MZ-021, MZ-024 |
| MZ-027 | Lab 3: integrated bench run (pairing) | Bryam + Wilson | 3 | 2.5 + 2.5 | – | **yes** | MZ-024 |
| MZ-028 | Configuration tuning from lab data | Wilson | 3 | 1.5 | B 0.25 | no | MZ-023, MZ-027 |
| MZ-029 | Attempt-flow hardening | Bryam | 4 | 2.5 | W 0.25 | no | MZ-024, MZ-027 |
| MZ-030 | Competition checklist + 5-min calibration | Wilson | 4 | 2.5 | B 0.25 | no | MZ-027 |
| MZ-031 | Lab 4: competition rehearsal (pairing) | Bryam + Wilson | 4 | 2.5 + 2.5 | – | **yes** | MZ-029, MZ-030 |
| MZ-032 | Code freeze tag + deploy procedure | Wilson | 4 | 1 | B 0.25 | no | MZ-031 |
| MZ-033 | Lab 5: dress rehearsal (pairing) | Bryam + Wilson | Buffer | 2 + 2 | – | **yes** | MZ-032 |
| MZ-034 | Logistics and packing list | Wilson | Buffer | 0.5 | – | no | – |
| MZ-035 | Research and compare alternative maze-solving strategies (backlog, to review) | TBD | Backlog | not estimated | – | no | MZ-021, MZ-025 |
| MZ-036 | RL skills for in-place turns and corridor centring (backlog, after 3 Nov) | TBD | Backlog | not estimated | – | yes (validation) | MZ-016, MZ-024, MZ-026 |
| MZ-037 | Active perception: head and camera geometry, moving vs fixed head (backlog, to review) | TBD | Backlog | ~3 h (first guess) | – | yes (validation) | MZ-019, MZ-021, MZ-024 |

MZ-035, MZ-036 and MZ-037 are **backlog** items: they are not scheduled and not counted in the 50 h / 45 h budget. MZ-035 and MZ-037 are reviewed at the end of each sprint and pulled in only if there is slack (or by swapping hours with another ticket). MZ-036 is planned for after the competition (3 Nov).

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

### MZ-001 — Create and publish `Maze-Challenge`

| Field | Value |
|---|---|
| Owner | Bryam |
| Estimate | 0.25 h |
| Depends on | – |
| Lab / robot | no |
| Branch | none (this ticket creates the base branch) |
| Files | – |

**Description.** Create the integration branch from an up-to-date `master` and publish it.

```bash
git switch master && git pull
git switch -c Maze-Challenge
git push -u origin Maze-Challenge
```

**Acceptance criteria**
- [ ] `git ls-remote --heads origin Maze-Challenge` prints one line.
- [ ] `Maze-Challenge` points to the same commit as `master` at creation time.

### MZ-002 — Request branch protection from the repository owner

| Field | Value |
|---|---|
| Owner | Wilson |
| Estimate | 0.25 h |
| Depends on | MZ-001 |
| Lab / robot | no |
| Branch | none (settings change, done by the owner) |
| Files | – |

**Description.** Our GitHub permission is *write*, not *admin*, so only the owner
(`juanulloa2050`) can protect the branch. Send the request and verify the result.

**Steps**
1. Send this message to the repository owner:
   > Hi! For the Maze challenge we work on the branch `Maze-Challenge`. Could you add a branch protection rule in *Settings → Branches → Add branch ruleset* (or *Add rule*) for `Maze-Challenge`, with: require a pull request before merging, require 1 approval, block force pushes, and restrict deletions? Thank you!
2. When the owner confirms, check:
   ```bash
   gh api repos/juanulloa2050/SabanaHerons2026/branches/Maze-Challenge --jq .protected
   ```

**Acceptance criteria**
- [ ] The request was sent (date written in the PR of MZ-006).
- [ ] The command prints `true`, **or** the owner declined and the PR rules in the roadmap apply by agreement.

### MZ-003 — Planning documents (roadmap, tickets, Gantt)

| Field | Value |
|---|---|
| Owner | Bryam |
| Estimate | 1 h · Review: Wilson 0.25 h |
| Depends on | MZ-001 |
| Lab / robot | no |
| Branch | `docs/MZ-003-roadmap` |
| Files | `docs/ROADMAP_LABERINTO.md`, `docs/TICKETS_LABERINTO.md`, `docs/GANTT_LABERINTO.md` |

**Description.** Publish the plan, the tickets and the Gantt chart through a PR to `Maze-Challenge`.

**Acceptance criteria**
- [ ] PR targets `Maze-Challenge` and is approved by Wilson.
- [ ] The Mermaid Gantt renders on GitHub.

### MZ-004 — Pull request template

| Field | Value |
|---|---|
| Owner | Wilson |
| Estimate | 0.5 h · Review: Bryam 0.25 h |
| Depends on | MZ-001 |
| Lab / robot | no |
| Branch | `docs/MZ-004-pr-template` |
| Files | `.github/pull_request_template.md` (create) |

**Description.** Create the PR template so every PR description has the same sections.

**Steps**
```bash
git switch Maze-Challenge
git pull
git switch -c docs/MZ-004-pr-template
mkdir -p .github
```
1. Create `.github/pull_request_template.md` with **exactly** the template in section 9 of `docs/ROADMAP_LABERINTO.md` ("PR description template").
2. Commit and open the PR:
```bash
git add .github/pull_request_template.md
git commit -m "docs(MZ-004): add pull request template"
git push -u origin docs/MZ-004-pr-template
gh pr create --base Maze-Challenge --title "MZ-004: Pull request template" --body "Adds the PR template from docs/ROADMAP_LABERINTO.md section 9. Docs only, no build needed."
```

**Acceptance criteria**
- [ ] Opening a new PR on GitHub pre-fills the description with the template.
- [ ] The content matches section 9 of the roadmap.

### MZ-005 — Registration confirmation

| Field | Value |
|---|---|
| Owner | Wilson |
| Estimate | 0.25 h |
| Depends on | – |
| Lab / robot | no |
| Branch | none (administrative) |
| Files | – (result written in the PR of MZ-006) |

**Description.** The registration deadline was 1 Oct. Confirm with the faculty advisor that the
team's registration was recorded and that the confirmation e-mail arrived.

**Acceptance criteria**
- [ ] The advisor confirmed the registration in writing (e-mail or chat), and the date is recorded in `docs/maze/ORGANIZER_ANSWERS.md` (MZ-006).
- [ ] If it is **not** confirmed, Bryam and the advisor are alerted the same day.

### MZ-006 — Organiser questions and answers

| Field | Value |
|---|---|
| Owner | Wilson |
| Estimate | 0.75 h · Review: Bryam 0.25 h |
| Depends on | MZ-003 |
| Lab / robot | no |
| Branch | `docs/MZ-006-organizer-answers` |
| Files | `docs/maze/ORGANIZER_ANSWERS.md` (create) |

**Description.** Send questions Q1–Q7 from section 11 of the roadmap to the organisers (through
the advisor if required), and record the answers. Q1 (tag walls and height) is the most urgent
because six tickets depend on it.

**Steps**
1. Send the e-mail with Q1–Q7, copied word for word from the roadmap. Note the date.
2. Create the record file and open the PR. Update it through new commits on the same branch as answers arrive:
```bash
git switch Maze-Challenge
git pull
git switch -c docs/MZ-006-organizer-answers
mkdir -p docs/maze
```
3. `docs/maze/ORGANIZER_ANSWERS.md` has a table with the columns `# | Question | Date asked | Answer | Date answered | Source`. Below it, add a line for the registration check (MZ-005) and one for the branch-protection request (MZ-002).
```bash
git add docs/maze/ORGANIZER_ANSWERS.md
git commit -m "docs(MZ-006): record organizer questions and answers"
git push -u origin docs/MZ-006-organizer-answers
gh pr create --base Maze-Challenge --title "MZ-006: Organizer questions and answers" --body "Questions Q1-Q7 sent on <date>. Answers recorded as they arrive. Docs only."
```

**Acceptance criteria**
- [ ] The e-mail was sent by 8 Oct.
- [ ] The file lists all 7 questions with dates. Missing answers say "pending".
- [ ] When Q1 is answered, Bryam is told the same day (it affects the TAG-DEP tickets).

### MZ-007 — `Maze` and `MazeSafe` scenarios, `HandleMaze` stub, start flow

| Field | Value |
|---|---|
| Owner | Bryam |
| Estimate | 2.5 h · Review: Wilson 0.5 h |
| Depends on | MZ-001 |
| Lab / robot | no (boot checked in Lab 1) |
| Branch | `feat/MZ-007-maze-scenario` |
| Files | `Config/Scenarios/Maze/` and `Config/Scenarios/MazeSafe/` (create, from `Config/Scenarios/AnyPlaceDemo/`); `Src/Modules/BehaviorControl/SkillBehaviorControl/Options/HandleMaze.h` (create); `Src/Modules/BehaviorControl/SkillBehaviorControl/SkillBehaviorControl.h` (modify: `#include`) |

**Description.**
- Create both scenarios with only the modules the maze needs. Drop the ball, YOLO, team-communication and soccer-strategy providers. Keep `RobotPose` in `defaultRepresentations` until MZ-020.
- `skillBehaviorControl.cfg`: `options = [HandlePhysicalRobot, HandlePlayerState, HandleMaze];`.
- `HandleMaze` stub: in *playing*, stand. A single `headFront` tap (`hitStreak == 1`) says "start". A single `headRear` tap says "maze ready" (later: the last seen tag id, MZ-018). A single `headMiddle` tap runs the **motion test sequence** used in Lab 2: 4 cells forward, stop, turn 90° left, 90° right, 180°, each separated by a 2 s pause and a beep.
- `logger.cfg`: log `JPEGImage`, `CameraInfo`, `CameraMatrix`, `OdometryData`, `FrameInfo` and the maze representations once they exist. The logger records only outside *Initial* (`Src/Tools/Framework/BHLoggingController.cpp:33`), hence the 3-chest-tap start.

**Acceptance criteria**
- [ ] `Make/Linux/compile Develop Nao` and `Make/Linux/compile Develop SimRobot` pass.
- [ ] In SimRobot (`gc playing`), a simulated `headFront` tap makes the robot say "start".
- [ ] Deploy with `-s Maze` boots. Three chest taps reach *playing* (green chest LED).

### MZ-008 — Vendor `apriltag` as a prebuilt static library

| Field | Value |
|---|---|
| Owner | Bryam |
| Estimate | 3 h · Review: Wilson 0.5 h |
| Depends on | MZ-001 |
| Lab / robot | no |
| Branch | `feat/MZ-008-apriltag-lib` |
| Files | `Util/apriltag/{include,src,lib/V6,lib/Linux,build.sh,LICENSE.md}` (create); `Make/CMake/CMakeLists.txt`, `Make/CMake/B-Human.cmake` (modify) |

**Description.** Follow the libjpeg pattern (see roadmap 3.7).
- `build.sh` compiles the upstream AprilRobotics sources (pinned tag, BSD-2 licence kept). The NAO version uses clang `--target=x86_64-linux-gnu -march=silvermont -nostdinc -isystem Util/Buildchain/V6/include -O3`. The desktop version uses `-fPIC -O3`.
- Add imported targets `Nao::apriltag::apriltag` / `apriltag::apriltag` and link `B-Human` in both branches.

**Acceptance criteria**
- [ ] Both builds link a translation unit that calls `tag36h11_create()` / `tag36h11_destroy()`.
- [ ] `Util/apriltag/build.sh` rebuilds both `.a` files from a clean checkout.
- [ ] The upstream version and commit are recorded in `Util/apriltag/README.md`.

### MZ-009 — Maze file format and generator

| Field | Value |
|---|---|
| Owner | Wilson |
| Estimate | 2 h · Review: Bryam 0.5 h |
| Depends on | MZ-004 |
| Lab / robot | no |
| Branch | `feat/MZ-009-maze-generator` |
| Files | `Util/MazeTools/generate_maze.py`, `Util/MazeTools/test_generate_maze.py`, `Util/MazeTools/README.md`, `Config/Mazes/generated/*.maze` (create) |

**Description.** A small Python 3 script (standard library only) that writes random 8×8 mazes in
a text format that both people and programs can read.

**Format (`.maze`)**
- 17 lines of 17 characters each.
- Even rows and even columns are corners `+`.
- Horizontal walls are `-`, vertical walls are `|`, and a space means no wall.
- The cell centre holds `S` (start), `G` (goal) or a space.
- Row 0 of the cells is the top row, matching tag id 0 at the top-left. Example of the top-left corner:
```
+-+-+
|S  |
+ +-+
```

**Steps**
1. Create the branch:
   ```bash
   git switch Maze-Challenge
   git pull
   git switch -c feat/MZ-009-maze-generator
   ```
2. Write `Util/MazeTools/generate_maze.py` with these options:
   - `--seed INT` (required)
   - `--start {0,7,56,63}` (default 0)
   - `--goal ROW,COL` (default: the cell farthest from the start)
   - `--loops INT` (default 0: number of extra walls removed to create loops)
   - `--out PATH`
3. Use a randomized depth-first search. Then apply the rules:
   - outer border always closed;
   - start cell has walls on exactly 3 sides;
   - goal cell keeps no extra internal wall;
   - every cell is reachable (check with a breadth-first search).
4. Add `--validate PATH`, which prints `OK` or the first broken rule.
5. Write `test_generate_maze.py` (unittest) with 4 checks: same seed → same file; border closed; start has 3 walls; all cells reachable.
6. Generate 20 mazes:
   ```bash
   mkdir -p Config/Mazes/generated
   for s in $(seq 1 20); do python3 Util/MazeTools/generate_maze.py --seed $s --loops $((s % 4)) --out Config/Mazes/generated/maze_$s.maze; done
   python3 -m unittest discover -s Util/MazeTools -v
   ```
7. Commit and open the PR:
   ```bash
   git add Util/MazeTools Config/Mazes/generated
   git commit -m "feat(MZ-009): add maze file format and generator"
   git push -u origin feat/MZ-009-maze-generator
   cp .github/pull_request_template.md ~/pr-MZ-009.md
   gh pr create --base Maze-Challenge --title "MZ-009: Maze file format and generator" --body-file ~/pr-MZ-009.md
   ```

**Acceptance criteria**
- [ ] `python3 -m unittest discover -s Util/MazeTools -v` passes.
- [ ] `--validate` prints `OK` for all 20 generated mazes.
- [ ] `Util/MazeTools/README.md` documents the format with the example above.

### MZ-010 — Maze → SimRobot scene converter

| Field | Value |
|---|---|
| Owner | Wilson (Bryam 0.5 h pairing for the scene template) |
| Estimate | 2.5 h (+ Bryam 0.5 h) · Review: Bryam 0.5 h |
| Depends on | MZ-007, MZ-009 |
| Lab / robot | no |
| Branch | `feat/MZ-010-maze-scene` |
| Files | `Util/MazeTools/maze_to_scene.py` (create); `Config/Scenes/Maze.ros2`, `Config/Scenes/Maze.con`, `Config/Scenes/Includes/Maze/current.rsi2` (create) |

**Description.**
- In the pairing session, Bryam writes `Config/Scenes/Maze.ros2`, based on `Config/Scenes/RL1v0_3D.ros2`. It uses scenario `Maze`, location `Default`, one robot and no ball, and includes `Includes/Maze/current.rsi2`.
- Wilson writes the script that turns a `.maze` file into `current.rsi2`.

**Geometry rules** (from `Config/Scenes/Includes/Field2020SPL.rsi2`)
- In `BoxGeometry`, `depth` is the size along x, `width` along y, and `height` along z.
- Wall along x: `depth="0.515m" width="0.015m" height="0.6m"`. Wall along y: `depth="0.015m" width="0.515m" height="0.6m"`. Use `color="rgb(235, 235, 235)"` and `<Translation x=".." y=".." z="0.3m"/>`.
- The maze is centred on the simulator origin. Cell `(row, col)` centre is `x = (col − 3.5)·0.5 m`, `y = (3.5 − row)·0.5 m`.
- The robot starts at the centre of `S`, facing the open side, at `z="320mm"`.

**Steps**
```bash
git switch Maze-Challenge
git pull
git switch -c feat/MZ-010-maze-scene
python3 Util/MazeTools/maze_to_scene.py Config/Mazes/generated/maze_1.maze --out Config/Scenes/Includes/Maze/current.rsi2
Make/Linux/compile Develop SimRobot
Build/Linux/SimRobot/Develop/SimRobot Config/Scenes/Maze.ros2
```
Take a screenshot of the scene from above. Attach it to the PR together with the `.maze` file it came from.
```bash
git add Util/MazeTools/maze_to_scene.py Config/Scenes/Maze.ros2 Config/Scenes/Maze.con Config/Scenes/Includes/Maze/current.rsi2
git commit -m "feat(MZ-010): add maze to SimRobot scene converter"
git push -u origin feat/MZ-010-maze-scene
cp .github/pull_request_template.md ~/pr-MZ-010.md
gh pr create --base Maze-Challenge --title "MZ-010: Maze to SimRobot scene converter" --body-file ~/pr-MZ-010.md
```

**Acceptance criteria**
- [ ] SimRobot loads `Maze.ros2` without errors in the console.
- [ ] The top view matches the `.maze` drawing for 3 different mazes (screenshots in the PR).
- [ ] The robot stands in the start cell without touching walls.

---

## Sprint 1 — Core logic in simulation (Sat 10 – Thu 15 Oct)

### MZ-011 — Maze library and time-weighted planner with unit tests

| Field | Value |
|---|---|
| Owner | Bryam |
| Estimate | 6 h · Review: Wilson 0.5 h |
| Depends on | MZ-009 |
| Lab / robot | no |
| Branch | `feat/MZ-011-maze-planner` |
| Files | `Src/Tools/Maze/MazeGraph.h`, `Src/Tools/Maze/MazePlanner.{h,cpp}`, `Src/Tools/Maze/MazeFile.{h,cpp}` (create); `Src/Apps/Tests/Maze/*.cpp` (create); `Make/CMake/Tests.cmake` (modify if the Maze sources must be added to `Tests`) |

**Description.**
- Pure C++ with no framework dependency.
- Grid and edges as in roadmap 4.3, plus tag id ↔ cell conversion and the maze frame (4.5).
- Flood-fill exploration and time-weighted A* (4.6) on `(cell, heading, moving)`. Costs come from `MazeCosts` (defaults: 2.5 / 0.8 / 2.0 / 3.5 s).
- A loader for `.maze` files, used by the tests and by the oracle provider.

**Acceptance criteria**
- [ ] Tests compare against an exhaustive search on the MZ-012 fixtures: equal optimal cost, an admissible heuristic, and exact turn counts.
- [ ] Replanning on an 8×8 maze takes <5 ms on the desktop (measured in the tests).
- [ ] An unknown edge counts as open for exploration and as wall for the speed run (tested).

### MZ-012 — Test mazes and expected results

| Field | Value |
|---|---|
| Owner | Wilson |
| Estimate | 1.5 h · Review: Bryam 0.25 h |
| Depends on | MZ-009 |
| Lab / robot | no |
| Branch | `test/MZ-012-maze-fixtures` |
| Files | `Config/Mazes/fixtures/*.maze`, `Config/Mazes/fixtures/expected.csv`, `Util/MazeTools/solve_maze.py` (create) |

**Description.** Ten hand-picked mazes plus their expected best route, computed by an
independent Python solver. This gives the C++ planner something to be checked against.

**Steps**
1. Create the branch:
   ```bash
   git switch Maze-Challenge
   git pull
   git switch -c test/MZ-012-maze-fixtures
   mkdir -p Config/Mazes/fixtures
   ```
2. Draw 10 `.maze` files by hand (or edit generated ones):
   - a straight corridor;
   - a spiral;
   - a comb with many dead ends;
   - two equal-length routes, one with fewer turns;
   - a long straight route against a short zig-zag;
   - goal in the centre;
   - one start in each of the four corners;
   - a maze with loops.
3. Write `solve_maze.py`: Dijkstra over `(cell, heading)` with the costs `t_fwd = 2.5`, `t_start = 0.8`, `t_turn90 = 2.0`, `t_turn180 = 3.5`. Print `cells,turns90,turns180,time_s`.
4. Fill `expected.csv` with columns `file,cells,turns90,turns180,time_s`:
   ```bash
   for f in Config/Mazes/fixtures/*.maze; do echo "$(basename $f),$(python3 Util/MazeTools/solve_maze.py $f)"; done > Config/Mazes/fixtures/expected.csv
   ```
5. Check 2 mazes by hand (count cells and turns on the drawing). Write the result in the PR.
6. Commit and open the PR:
   ```bash
   git add Config/Mazes/fixtures Util/MazeTools/solve_maze.py
   git commit -m "test(MZ-012): add maze fixtures and expected results"
   git push -u origin test/MZ-012-maze-fixtures
   cp .github/pull_request_template.md ~/pr-MZ-012.md
   gh pr create --base Maze-Challenge --title "MZ-012: Maze fixtures and expected results" --body-file ~/pr-MZ-012.md
   ```

**Acceptance criteria**
- [ ] 10 fixtures, all `OK` with `generate_maze.py --validate`.
- [ ] `expected.csv` has 10 rows, and 2 of them were checked by hand.

### MZ-013 — Oracle maze percepts for simulation ⚠️ TAG-DEP

| Field | Value |
|---|---|
| Owner | Bryam |
| Estimate | 2 h · Review: Wilson 0.5 h |
| Depends on | MZ-010, MZ-011 |
| Lab / robot | no |
| Branch | `feat/MZ-013-oracle-percepts` |
| Files | `Src/Modules/Modeling/OracledMazePerceptsProvider/` (create); `Src/Representations/Perception/MazePercepts/{AprilTagPercept,WallPercept}.h` (create); `Config/Scenarios/Maze/threads.cfg` (sim variant) |

**Description.** Build `AprilTagPercept` and `WallPercept` from `GroundTruthRobotPose` and the
loaded `.maze` file, with parameters for Gaussian noise, dropout rate and maximum range. Tag
placement follows the TAG-DEP assumption.

**Acceptance criteria**
- [ ] In SimRobot, the representation view shows the correct tag ids and walls for 3 mazes.
- [ ] Dropout = 1.0 produces empty percepts, which is used later by the safe-mode tests.

### MZ-014 — Build the partial bench

| Field | Value |
|---|---|
| Owner | Wilson |
| Estimate | 3.5 h |
| Depends on | – |
| Lab / robot | no (built in a workshop, assembled in Lab 1) |
| Branch | none (results committed in the PR of MZ-022) |
| Files | `docs/maze/BENCH.md` (created in MZ-022) |

**Description.** Six free-standing panels that can be arranged as a corridor, a corner, a
T-junction and a dead end.

**Steps**
1. Cut 6 panels of **50 cm wide × 60 cm high** from 10 mm foam board (or 12–15 mm MDF). The 1–2 cm thickness matches the rules. Use a light, matte colour (white).
2. Give each panel two feet: L-brackets or wooden blocks, so it stands without leaning. Join panels at corners with clamps or strong tape.
3. Make a floor tape grid of 50 cm cells (2×3 cells) and a 1 m straight line for the calibration drill.
4. Draw the 4 layouts in `docs/maze/BENCH.md` (ASCII is fine) and record how many panels each uses: corridor 2 cells (4 panels), dead end (5), corner (4), T (5).
5. Measure and record: each panel's width, height and thickness (±1 mm); the corridor inner width when assembled (target 48–49 cm); the floor type; and the light level (phone lux app).

**Acceptance criteria**
- [ ] 6 panels stand alone, and a light push does not tip them.
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

**Steps**
1. Download the `tag36h11` images for ids 0, 1, 7, 8, 9, 56, 63 from the AprilRobotics `apriltag-imgs` repository (folder `tag36h11`).
2. Scale each so that the **black outer square measures exactly 150 mm**, and print on matte paper (no glossy finish).
3. Measure the printed square with a ruler. Reject anything outside 150 ± 1 mm.
4. Glue each tag to a panel, **centred horizontally, with the tag centre 30 cm above the floor** (the TAG-DEP assumption, to be changed if Q1 says otherwise).

**Acceptance criteria**
- [ ] 7 tags are printed and measured at 150 ± 1 mm, with the measurements recorded.
- [ ] The tags are flat (no bubbles), and the mounting height is recorded.

### MZ-016 — Corridor walk profile in simulation

| Field | Value |
|---|---|
| Owner | Wilson |
| Estimate | 3 h · Review: Bryam 0.25 h |
| Depends on | MZ-007, MZ-010 |
| Lab / robot | no |
| Branch | `config/MZ-016-walk-profile` |
| Files | `Config/Scenarios/Maze/walkingEngine.cfg`, `Config/Scenarios/Maze/walkingEngineCommon.cfg`, `Config/Scenarios/MazeSafe/walkingEngine.cfg`, `Config/Scenarios/MazeSafe/walkingEngineCommon.cfg` (create as copies) |

**Description.** Create the maze walk profiles as copies of the defaults, changing only a few
values. A scenario file overrides `Config/Robots/Default/` (roadmap 3.6). The whole file must be
copied, not only the changed lines.

**Steps**
1. Create the branch and copy the files:
   ```bash
   git switch Maze-Challenge
   git pull
   git switch -c config/MZ-016-walk-profile
   cp Config/Robots/Default/walkingEngine.cfg Config/Robots/Default/walkingEngineCommon.cfg Config/Scenarios/Maze/
   cp Config/Robots/Default/walkingEngine.cfg Config/Robots/Default/walkingEngineCommon.cfg Config/Scenarios/MazeSafe/
   ```
2. In `Config/Scenarios/Maze/walkingEngine.cfg`, set `maxSpeed` and `minSpeed` to 60 % (translation x, y and rotation). In `MazeSafe`, set them to 40 %.
3. In both `walkingEngineCommon.cfg` files, under `armParameters`: `armShoulderRoll` 7deg → 4deg, `armShoulderRollIncreaseFactor` 2 → 1, `armShoulderPitchFactor` 6 → 3.
4. Run 5 times in SimRobot on a straight corridor maze (fixture "straight corridor" from MZ-012), and record in the PR description:
   - the time to cross 4 cells;
   - whether any wall was touched (watch the arms in the 3D view);
   - the minimum distance from the arm to the wall (visual estimate).
5. Commit and open the PR:
   ```bash
   git add Config/Scenarios/Maze/walkingEngine*.cfg Config/Scenarios/MazeSafe/walkingEngine*.cfg
   git commit -m "feat(MZ-016): add conservative walk profiles for Maze and MazeSafe"
   git push -u origin config/MZ-016-walk-profile
   cp .github/pull_request_template.md ~/pr-MZ-016.md
   gh pr create --base Maze-Challenge --title "MZ-016: Maze and MazeSafe walk profiles" --body-file ~/pr-MZ-016.md
   ```

**Acceptance criteria**
- [ ] Both scenarios build and the robot walks in simulation without falling (5/5 runs).
- [ ] 0 wall touches in 5 runs, and the times are recorded in the PR.
- [ ] Only the parameters listed above differ from `Config/Robots/Default/` (check with `diff`).

### MZ-017 — Lab 1 (Tue 13 Oct, 2 h): log capture ⚠️ TAG-DEP

| Field | Value |
|---|---|
| Owner | Wilson |
| Estimate | 2 h (lab) |
| Depends on | MZ-007, MZ-014, MZ-015 |
| Lab / robot | **yes** |
| Branch | none (results committed in the PR of MZ-022) |
| Files | `docs/maze/LAB_LOG.md` (created in MZ-022); logs in `Config/Logs/` and the shared drive |

**Description.** Record real camera and odometry logs, so the detector and the perceptor can be
developed offline. Bryam is on call by phone.

**Steps**
1. Deploy by Ethernet and start the robot:
   ```bash
   Make/Common/deploy Develop <robot-ip> -s Maze -l Default -w NONE -b
   ```
   Then tap the chest 3 times (green chest LED = *playing*). The logger is now recording.
2. **Tag sweep:** place one tag panel in front of the robot at 0.25, 0.5, 1.0, 1.5 and 2.0 m (tape measure), each at 0°, 30° and 60° to the panel. Stay 5 s at each position, and write each position and the time in a notebook.
3. **Corridor walk:** build the 2-cell corridor and let the robot stand at both ends and in the middle for 5 s each, moving it by hand.
4. Stop the robot by holding all three head buttons for 1 s (unstiff), then download:
   ```bash
   Make/Common/downloadLogs <robot-ip>
   ```
5. Copy `Config/Logs/<date>/` to the shared drive.

**Acceptance criteria**
- [ ] At least 15 tag positions are recorded (5 distances × 3 angles) with their timestamps.
- [ ] The logs open in SimRobot with `Config/Scenes/ReplayRobot.ros2` (check one before leaving the lab).
- [ ] The robot booted with `-s Maze` and the chest flow worked (this confirms MZ-007 on the real robot).

---

## Sprint 2 — Perception and localization (Fri 16 – Wed 21 Oct)

### MZ-018 — AprilTag detector ⚠️ TAG-DEP

| Field | Value |
|---|---|
| Owner | Bryam |
| Estimate | 5 h · Review: Wilson 0.5 h |
| Depends on | MZ-008, MZ-017 |
| Lab / robot | no (validated on Lab 1 logs) |
| Branch | `feat/MZ-018-apriltag-detector` |
| Files | `Src/Modules/Perception/AprilTagDetector/AprilTagDetector.{h,cpp}` (create); `Src/Modules/Infrastructure/InterThreadProviders/PerceptionProviders.{h,cpp}` (modify: `ALIAS(AprilTagPercept)`); `Config/Scenarios/Maze/threads.cfg`, `Config/Scenarios/Maze/aprilTagDetector.cfg` (modify / create) |

**Description.** Roadmap 4.4. Upper thread only. Tag pose via `estimate_tag_pose`, transformed
with `CameraMatrix`. A decision-margin threshold is set in the config.

**Acceptance criteria**
- [ ] Ids are correct on ≥95 % of Lab 1 frames between 0.25 and 1.5 m.
- [ ] On the NAO, `STOPWATCH("AprilTagDetector")` averages ≤20 ms per upper frame. If it does not, detect every 2nd frame (parameter).
- [ ] No detections with a wrong id on the whole log (false-positive check).
- [ ] On the robot, a single `headRear` tap says the last seen tag id (used in Lab 2 and in the 5-minute calibration).

### MZ-019 — Wall perceptor

| Field | Value |
|---|---|
| Owner | Bryam |
| Estimate | 4 h · Review: Wilson 0.5 h |
| Depends on | MZ-010, MZ-017 |
| Lab / robot | no (simulation + Lab 1 logs) |
| Branch | `feat/MZ-019-wall-perceptor` |
| Files | `Src/Modules/Perception/WallPerceptor/` (create); `PerceptionProviders.{h,cpp}` (modify: `ALIAS(WallPercept)`); `Config/Scenarios/Maze/threads.cfg` |

**Description.** Column scan for the floor/wall boundary on `ECImage`, projected with
`Transformation::imageToRobot`. Output: the distance to the wall in front, left and right when
the head points that way.

**Acceptance criteria**
- [ ] In simulation, wall present or absent is ≥98 % correct for the 4 sides of visited cells (10 cells, 3 mazes).
- [ ] On Lab 1 corridor logs, the side-wall distance is within ±3 cm of the tape measurement.

### MZ-020 — Maze localizer and mapper ⚠️ TAG-DEP

| Field | Value |
|---|---|
| Owner | Bryam |
| Estimate | 5 h · Review: Wilson 0.5 h |
| Depends on | MZ-011, MZ-013 |
| Lab / robot | no |
| Branch | `feat/MZ-020-localizer-mapper` |
| Files | `Src/Modules/Modeling/MazeLocalizer/`, `Src/Modules/Modeling/MazeMapper/` (create); `Src/Representations/Modeling/MazeMap.h`, `Src/Representations/Modeling/MazeCellPose.h` (create); `Config/Scenarios/Maze/threads.cfg` (modify: `RobotPose` provided by `MazeLocalizer`, removed from `defaultRepresentations`) |

**Description.**
- **Localizer:** odometry prediction plus a tag fix (complementary filter, roadmap 4.5) and a heading snap on square-on tags.
- **Mapper:** edge evidence with hysteresis (3 observations). State is a module member, so it persists for the whole process. The attempt counter increments on Standby → start.

**Acceptance criteria**
- [ ] In simulation with noisy odometry, the pose error is ≤8 cm and ≤10° after 10 cells, and the cell estimate is 100 % correct.
- [ ] After a simulated pick-up and Standby, the map is identical and the attempt counter = 2.
- [ ] With tags hidden (dropout 1.0), the localizer reports low quality (used by MZ-025).

### MZ-021 — Headless simulation campaign runner

| Field | Value |
|---|---|
| Owner | Wilson |
| Estimate | 3 h · Review: Bryam 0.5 h |
| Depends on | MZ-010 |
| Lab / robot | no |
| Branch | `feat/MZ-021-sim-runner` |
| Files | `Util/MazeTools/run_sim_campaign.py` (create) |

**Description.** A script that runs many mazes in SimRobot one after another and writes one CSV
row per run. Use the headless `pybh` mode described in `docs/RL/Environment.md` ("Backend").
If that is not usable, fall back to starting `Build/Linux/SimRobot/Develop/SimRobot` once per
maze with a timeout.

**Steps**
1. Create the branch:
   ```bash
   git switch Maze-Challenge
   git pull
   git switch -c feat/MZ-021-sim-runner
   ```
2. Script options:
   - `--mazes Config/Mazes/generated`
   - `--attempts 2`
   - `--noise {low,mid,high}`
   - `--timeout-s 300`
   - `--out results.csv`
3. For each maze:
   - run `maze_to_scene.py`;
   - start the simulation;
   - read the robot's `BehaviorStatus` / `MazeMap` through the debug output (Bryam provides the field names in the review);
   - write `maze,attempt,noise,reached_goal,time_s,wall_contacts,max_stall_s`.
4. Test on 2 mazes. Put the CSV in the PR description.
5. Commit and open the PR:
   ```bash
   git add Util/MazeTools/run_sim_campaign.py
   git commit -m "feat(MZ-021): add headless maze simulation campaign runner"
   git push -u origin feat/MZ-021-sim-runner
   cp .github/pull_request_template.md ~/pr-MZ-021.md
   gh pr create --base Maze-Challenge --title "MZ-021: Headless simulation campaign runner" --body-file ~/pr-MZ-021.md
   ```

**Acceptance criteria**
- [ ] `python3 Util/MazeTools/run_sim_campaign.py --mazes Config/Mazes/fixtures --attempts 1 --noise low --out /tmp/r.csv` runs 2 mazes without manual clicks.
- [ ] The CSV has the 7 columns listed above.

### MZ-022 — Tag accuracy report ⚠️ TAG-DEP

| Field | Value |
|---|---|
| Owner | Wilson |
| Estimate | 2.5 h · Review: Bryam 0.25 h |
| Depends on | MZ-017, MZ-018 |
| Lab / robot | no (recorded logs) |
| Branch | `docs/MZ-022-tag-accuracy` |
| Files | `docs/maze/TAG_ACCURACY.md`, `docs/maze/LAB_LOG.md`, `docs/maze/BENCH.md` (create) |

**Description.** Replay the Lab 1 logs with the detector and compare what it measured with the
tape measurements. This PR also commits the bench sheet (MZ-014, MZ-015) and the Lab 1 notes
(MZ-017).

**Steps**
1. Create the branch, then open the replay scene:
   ```bash
   git switch Maze-Challenge
   git pull
   git switch -c docs/MZ-022-tag-accuracy
   Build/Linux/SimRobot/Develop/SimRobot Config/Scenes/ReplayRobot.ros2
   ```
   Choose a Lab 1 log in the replay dialog.
2. For each of the 15 recorded positions, jump to its timestamp and read `AprilTagPercept` in the representation view (id, distance, angle).
3. Fill this table in `TAG_ACCURACY.md`:

   | true distance | true angle | detected (y/n) | id ok | measured distance | error % |
   |---|---|---|---|---|---|

4. Add the summary: detection rate per distance, mean error, worst case.
5. Commit and open the PR:
   ```bash
   git add docs/maze/TAG_ACCURACY.md docs/maze/LAB_LOG.md docs/maze/BENCH.md
   git commit -m "docs(MZ-022): add tag accuracy report and lab 1 notes"
   git push -u origin docs/MZ-022-tag-accuracy
   cp .github/pull_request_template.md ~/pr-MZ-022.md
   gh pr create --base Maze-Challenge --title "MZ-022: Tag accuracy report and Lab 1 notes" --body-file ~/pr-MZ-022.md
   ```

**Acceptance criteria**
- [ ] All 15 positions are in the table.
- [ ] The summary states whether the MZ-018 targets are met (≥95 % detection, ≤5 % distance error up to 1.5 m).

### MZ-023 — Lab 2 (Tue 20 Oct, 2 h): calibration of motion costs

| Field | Value |
|---|---|
| Owner | Wilson |
| Estimate | 2 h (lab) |
| Depends on | MZ-016, MZ-018 |
| Lab / robot | **yes** |
| Branch | none (numbers committed in the PR of MZ-028) |
| Files | `docs/maze/LAB_LOG.md` (updated in MZ-028) |

**Description.** Measure, on the real lab floor, how long the robot needs for each motion and how
precise it is. These numbers replace the planner's default costs. Bryam is on call by phone.

**Steps**
1. Deploy and start (3 chest taps):
   ```bash
   Make/Common/deploy Develop <robot-ip> -s Maze -l Default -w NONE -b
   ```
2. With a stopwatch, measure 10 times each, using the motion test sequence (single `headMiddle` tap, MZ-007):
   - `t_fwd`: time per 50 cm while already walking (measure 4 cells and divide by 4);
   - `t_start`: extra time for the first cell from standstill;
   - `t_turn90` and `t_turn180`: in place.
3. For each straight run, measure the lateral drift over 1 m. For each turn, measure the heading error with floor tape marks.
4. With the panel tags, check live detection under the lab lighting: the robot says the id on a `headRear` tap. Do it at 0.5, 1.0 and 1.5 m.
5. Download the logs: `Make/Common/downloadLogs <robot-ip>`.

**Acceptance criteria**
- [ ] 10 measurements per motion, with mean and standard deviation in `LAB_LOG.md`.
- [ ] The live tag check passes at 0.5 m and 1.0 m.

---

## Sprint 3 — Behaviour and integration (Thu 22 – Sun 25 Oct)

### MZ-024 — `HandleMaze` state machine and maze skills

| Field | Value |
|---|---|
| Owner | Bryam |
| Estimate | 5 h · Review: Wilson 0.5 h |
| Depends on | MZ-011, MZ-020 |
| Lab / robot | no |
| Branch | `feat/MZ-024-handle-maze` |
| Files | `Src/Modules/BehaviorControl/SkillBehaviorControl/Options/HandleMaze.h` (modify); `Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Maze/{MazeFollowCorridor,MazeTurn,MazeScanWalls}.cpp` (create); `Src/Representations/BehaviorControl/SkillInterfaces.h` (modify); `Src/Modules/BehaviorControl/MazePlannerProvider/` and `Src/Representations/BehaviorControl/MazePlan.h` (create) |

**Description.** The states and transitions in roadmap 4.7. Skills are built on `WalkToPose`
(empty obstacle avoidance) and `LookAtAngles`. Turns happen only at cell centres.

**Acceptance criteria**
- [ ] In simulation, the full flow works on 3 mazes: WaitingForStart → Exploration → goal → pick-up → Standby → SpeedRun → Finished.
- [ ] In attempt 2, the plan uses known-open edges only (unit-tested in MZ-011, checked in a log).

### MZ-025 — Watchdog, safe mode and wall-follower fallback

| Field | Value |
|---|---|
| Owner | Bryam |
| Estimate | 2.5 h · Review: Wilson 0.25 h |
| Depends on | MZ-024 |
| Lab / robot | no |
| Branch | `feat/MZ-025-watchdog` |
| Files | `HandleMaze.h` (modify); `Config/Scenarios/Maze/handleMaze.cfg` (create) |

**Acceptance criteria**
- [ ] In simulation, no stall is longer than 3 s in 20 runs (CSV from MZ-021).
- [ ] With oracle dropout = 1.0, safe mode and then the wall-follower take over, and the robot reaches the goal in a maze without loops.

### MZ-026 — Simulation campaign and report

| Field | Value |
|---|---|
| Owner | Wilson |
| Estimate | 3 h · Review: Bryam 0.25 h |
| Depends on | MZ-021, MZ-024 |
| Lab / robot | no |
| Branch | `docs/MZ-026-sim-results` |
| Files | `docs/maze/SIM_RESULTS.md` (create) |

**Steps**
1. Create the branch and run the campaign at three noise levels:
   ```bash
   git switch Maze-Challenge
   git pull
   git switch -c docs/MZ-026-sim-results
   for n in low mid high; do python3 Util/MazeTools/run_sim_campaign.py --mazes Config/Mazes/generated --attempts 2 --noise $n --out ~/sim_$n.csv; done
   ```
2. In `docs/maze/SIM_RESULTS.md`, write:
   - a table per noise level (success rate, mean T1, mean T2, mean Tfinal = 0.3·T1 + 0.7·T2, wall contacts, max stall);
   - the 3 worst mazes with a short description of what went wrong.
3. Commit and open the PR:
   ```bash
   git add docs/maze/SIM_RESULTS.md
   git commit -m "docs(MZ-026): add maze simulation campaign results"
   git push -u origin docs/MZ-026-sim-results
   cp .github/pull_request_template.md ~/pr-MZ-026.md
   gh pr create --base Maze-Challenge --title "MZ-026: Simulation campaign results" --body-file ~/pr-MZ-026.md
   ```

**Acceptance criteria**
- [ ] 20 mazes × 2 attempts × 3 noise levels were run, or the report says which runs failed to start and why.
- [ ] The report says whether the Sprint 3 target is met (≥18/20 solved at noise "mid").

### MZ-027 — Lab 3 (Fri 23 Oct, 2.5 h): integrated bench run · pairing

| Field | Value |
|---|---|
| Owner | Bryam + Wilson |
| Estimate | 2.5 h each (lab) |
| Depends on | MZ-024 |
| Lab / robot | **yes** |
| Branch | none (results committed in the PR of MZ-028) |
| Files | `docs/maze/LAB_LOG.md` |

**Description.** First time the full software runs on the real robot between real walls.

**Steps**
1. Run on the 4 bench layouts (corridor, corner, T, dead end), 5 runs each where time allows.
2. Wilson measures the minimum lateral clearance (ruler at the closest point, video if possible) and counts wall contacts. Bryam watches the representations live over Ethernet.
3. Record each run in `LAB_LOG.md` with: layout, result, contacts, clearance, notes.

**Acceptance criteria**
- [ ] Corridor and corner: 5 runs each with 0 contacts and clearance ≥5 cm.
- [ ] T and dead end: the correct branch is chosen (dead end detected, robot turns around).
- [ ] Every failure has a log name and a note.

### MZ-028 — Configuration tuning from lab data

| Field | Value |
|---|---|
| Owner | Wilson |
| Estimate | 1.5 h · Review: Bryam 0.25 h |
| Depends on | MZ-023, MZ-027 |
| Lab / robot | no |
| Branch | `config/MZ-028-lab-tuning` |
| Files | `Config/Scenarios/Maze/mazePlanner.cfg`, `Config/Scenarios/Maze/walkingEngine*.cfg`, `Config/Scenarios/MazeSafe/walkingEngine*.cfg` (modify); `docs/maze/LAB_LOG.md` (modify) |

**Steps**
1. Create the branch:
   ```bash
   git switch Maze-Challenge
   git pull
   git switch -c config/MZ-028-lab-tuning
   ```
2. In `Config/Scenarios/Maze/mazePlanner.cfg`, replace `t_fwd`, `t_start`, `t_turn90` and `t_turn180` with the Lab 2 means.
3. If Lab 3 showed contacts or clearance <5 cm, lower the `maxSpeed` x of `Maze` by 10 % (only that value). Write the reason in the PR.
4. Add the Lab 2 and Lab 3 tables to `docs/maze/LAB_LOG.md`.
5. Rebuild and check in SimRobot that the robot still walks:
   ```bash
   Make/Linux/compile Develop SimRobot
   Build/Linux/SimRobot/Develop/SimRobot Config/Scenes/Maze.ros2
   ```
6. Commit and open the PR:
   ```bash
   git add Config/Scenarios/Maze Config/Scenarios/MazeSafe docs/maze/LAB_LOG.md
   git commit -m "fix(MZ-028): tune maze costs and walk speed from lab data"
   git push -u origin config/MZ-028-lab-tuning
   cp .github/pull_request_template.md ~/pr-MZ-028.md
   gh pr create --base Maze-Challenge --title "MZ-028: Tune maze costs and walk speed from lab data" --body-file ~/pr-MZ-028.md
   ```

**Acceptance criteria**
- [ ] The config values equal the measured means (rounded to 0.1 s).
- [ ] Every changed value is justified in the PR with a reference to `LAB_LOG.md`.

---

## Sprint 4 — Hardening and code freeze (Mon 26 – Wed 28 Oct)

### MZ-029 — Attempt-flow hardening

| Field | Value |
|---|---|
| Owner | Bryam |
| Estimate | 2.5 h · Review: Wilson 0.25 h |
| Depends on | MZ-024, MZ-027 |
| Lab / robot | no (checked in Lab 4) |
| Branch | `fix/MZ-029-attempt-flow` |
| Files | `HandleMaze.h`, `Src/Modules/Modeling/MazeLocalizer/`, `Src/Modules/Modeling/MazeMapper/` (modify) |

**Description.** Pick-up (ground contact lost) → Standby. Fall → get up → relocalize from the next
tag. The map persists across both attempts. Fixes for any Lab 3 failure.

**Acceptance criteria**
- [ ] In simulation, a fall in the middle of exploration recovers and the goal is still reached.
- [ ] Every Lab 3 failure has a fix or a documented decision.

### MZ-030 — Competition checklist and 5-minute calibration procedure

| Field | Value |
|---|---|
| Owner | Wilson |
| Estimate | 2.5 h · Review: Bryam 0.25 h |
| Depends on | MZ-027 |
| Lab / robot | no |
| Branch | `docs/MZ-030-competition-checklist` |
| Files | `docs/maze/COMPETITION_CHECKLIST.md` (create) |

**Steps**
1. Create the branch:
   ```bash
   git switch Maze-Challenge
   git pull
   git switch -c docs/MZ-030-competition-checklist
   ```
2. Write `docs/maze/COMPETITION_CHECKLIST.md` with 4 printable sections:
   1. **Day before:** batteries, laptop, Ethernet cable, frozen build compiled in Release, `MazeSafe` build ready, printed checklist.
   2. **Calibration Zone (≤5 min):** copy the table from roadmap section 7 and add a column "result" to fill in by hand.
   3. **Our turn:**
      - code loaded only by Ethernet: `Make/Common/deploy Release <ip> -s Maze -l Default -w NONE -b`;
      - Wi-Fi off (profile `NONE`) and verified;
      - 3 chest taps → *playing*;
      - activation by head button (`headFront`) within 10 s of the whistle;
      - between attempts, never hold the chest button;
      - touch the robot only when the judge allows it.
   4. **After our turn:** verify the robot is not connected to any wireless network, then store the logs (`Make/Common/downloadLogs <ip>`).
3. Add the conservative defaults list from roadmap section 6.
4. Commit and open the PR:
   ```bash
   git add docs/maze/COMPETITION_CHECKLIST.md
   git commit -m "docs(MZ-030): add competition checklist and calibration procedure"
   git push -u origin docs/MZ-030-competition-checklist
   cp .github/pull_request_template.md ~/pr-MZ-030.md
   gh pr create --base Maze-Challenge --title "MZ-030: Competition checklist and 5-minute calibration" --body-file ~/pr-MZ-030.md
   ```

**Acceptance criteria**
- [ ] It fits on 2 printed pages.
- [ ] Every item has a checkbox and can be checked by one person.

### MZ-031 — Lab 4 (Tue 27 Oct, 2.5 h): competition-mode rehearsal · pairing

| Field | Value |
|---|---|
| Owner | Bryam + Wilson |
| Estimate | 2.5 h each (lab) |
| Depends on | MZ-029, MZ-030 |
| Lab / robot | **yes** |
| Branch | none (notes appended to `docs/maze/LAB_LOG.md` in the PR of MZ-032) |
| Files | `docs/maze/LAB_LOG.md` |

**Description.** Run exactly as on competition day, following the printed checklist.

**Acceptance criteria**
- [ ] The 5-minute calibration drill is completed in ≤5 min (timed).
- [ ] 3 full attempt-1 → Standby → attempt-2 cycles on the bench, with no human touch except the pick-up.
- [ ] Every checklist item that was unclear is corrected in the checklist.

### MZ-032 — Code freeze and deploy procedure

| Field | Value |
|---|---|
| Owner | Wilson |
| Estimate | 1 h · Review: Bryam 0.25 h |
| Depends on | MZ-031 |
| Lab / robot | no |
| Branch | `docs/MZ-032-code-freeze` |
| Files | `docs/maze/DEPLOY.md` (create); `docs/maze/LAB_LOG.md` (Lab 4 notes) |

**Steps**
1. Create the branch:
   ```bash
   git switch Maze-Challenge
   git pull
   git switch -c docs/MZ-032-code-freeze
   ```
2. Write `docs/maze/DEPLOY.md` with the exact commands:
   ```bash
   git switch Maze-Challenge && git pull
   git checkout maze-freeze-2026-10-28
   Make/Linux/compile Release Nao
   Make/Common/deploy Release <ip> -s Maze -l Default -w NONE -b
   # fallback:
   Make/Common/deploy Release <ip> -s MazeSafe -l Default -w NONE -b
   ```
3. Add the Lab 4 notes to `LAB_LOG.md`, then commit and open the PR:
   ```bash
   git add docs/maze/DEPLOY.md docs/maze/LAB_LOG.md
   git commit -m "docs(MZ-032): add frozen deploy procedure and lab 4 notes"
   git push -u origin docs/MZ-032-code-freeze
   cp .github/pull_request_template.md ~/pr-MZ-032.md
   gh pr create --base Maze-Challenge --title "MZ-032: Code freeze deploy procedure" --body-file ~/pr-MZ-032.md
   ```
4. **After this PR is merged** (on 28 Oct), tag the frozen commit:
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

### MZ-033 — Lab 5 (Fri 30 Oct, 2 h): dress rehearsal · pairing

| Field | Value |
|---|---|
| Owner | Bryam + Wilson |
| Estimate | 2 h each (lab) |
| Depends on | MZ-032 |
| Lab / robot | **yes** |
| Branch | none, unless a blocking fix is needed (`fix/MZ-033-<slug>`, reviewed by both) |
| Files | – |

**Description.** The full competition procedure with the frozen tag, from deploy to "after our
turn". Only blocking problems may be fixed, through a PR reviewed by both people.

**Acceptance criteria**
- [ ] The full checklist is done once, timed, with no improvisation.
- [ ] The decision about `Maze` or `MazeSafe` defaults is written down.

### MZ-034 — Logistics and packing list

| Field | Value |
|---|---|
| Owner | Wilson |
| Estimate | 0.5 h |
| Depends on | – |
| Lab / robot | no |
| Branch | none |
| Files | – |

**Description.** Pack and confirm the logistics for the trip to Campus Ciudad de México on 3 Nov.

**Steps**
1. Prepare a packing list:
   - NAO v6 in its case, charger, spare battery;
   - laptop with the frozen build and charger;
   - 2 Ethernet cables and a USB-Ethernet adapter;
   - printed checklist (MZ-030), printed tags (spare), tape measure, floor tape.
2. Confirm transport and arrival time, so the team is there before 9:00.

**Acceptance criteria**
- [ ] The list is shared with the team by 1 Nov, and every item is checked off on 2 Nov.

---

## Backlog (to review, not scheduled)

### MZ-035 — Research and compare alternative maze-solving strategies

| Field | Value |
|---|---|
| Status | **Backlog — to review.** Not scheduled, not counted in the 50 h / 45 h budget. |
| Owner | TBD (decided when it is pulled into a sprint) |
| Estimate | Not estimated (first step of the ticket) |
| Depends on | MZ-021 (simulation runner), MZ-025 (wall-follower) |
| Lab / robot | no (simulation only) |
| Branch | `docs/MZ-035-strategy-comparison` (plus `feat/MZ-035-<slug>` if code is needed) |
| Files | `docs/maze/STRATEGY_COMPARISON.md` (create); possibly `Src/Tools/Maze/` and `Util/MazeTools/run_sim_campaign.py` (a strategy selector) |

**Description.** Look for and design other strategies that solve the maze, and compare them
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
| Status | **Backlog — after the competition (3 Nov 2026).** Not scheduled, not counted in the 50 h / 45 h budget. |
| Owner | TBD |
| Estimate | Not estimated (first step of the ticket) |
| Depends on | MZ-016 (walk profile, baseline), MZ-024 (maze skills, baseline), MZ-026 (simulation campaign, baseline numbers) |
| Lab / robot | **yes**, for validation on the bench (training runs in simulation) |
| Branch | `feat/MZ-036-rl-maze-motion` |
| Files | `Src/Libs/RL/` (observation encoder and action decoder for the maze skills); `Config/NeuralNets/RLPolicy/` (new ONNX policies and manifests); `Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Maze/` (switch between classic and learned skill); `docs/maze/RL_MOTION.md` (create) |

**Description.** Route planning stays classical: time-weighted A\* is already optimal for a
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
| Status | **Backlog — to review.** Not scheduled, not counted in the 50 h / 45 h budget. Can be pulled into Sprint 3 by swapping hours with another ticket. |
| Owner | TBD (suggested split: geometry study and simulation runs for Wilson, skill changes for Bryam) |
| Estimate | ~3 h first guess (≈1.5 h study and runs, ≈1.5 h skill changes); confirm in step 1 |
| Depends on | MZ-019 (wall perceptor), MZ-021 (simulation runner), MZ-024 (maze skills) |
| Lab / robot | **yes**, for validation (image blur and detection with a moving head can only be checked on real images) |
| Branch | `feat/MZ-037-active-perception` |
| Files | `Util/MazeTools/visibility_table.py` (create); `docs/maze/ACTIVE_PERCEPTION.md` (create); `Src/Modules/BehaviorControl/SkillBehaviorControl/Skills/Maze/MazeScanWalls.cpp` (modify); `Config/Scenarios/Maze/` (head-scan parameters) |

**Description.** Use the head's range of motion and the camera geometry to map the maze faster
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

