# Gyro heading

How the maze localizer (`MazeLocalizer`, ticket MZ-020) knows which way the robot faces, and what it costs when the gyro drifts. Merged from MZ-038.

Related: [TICKETS_LABERINTO.md](TICKETS_LABERINTO.md) (MZ-020) · [PARAMETROS_LABERINTO.md](PARAMETROS_LABERINTO.md) (section 6, every key) · [SIMULACION_LABERINTO.md](SIMULACION_LABERINTO.md) (step 5, how to see it).

Status: the method and the offline results are done. The **real drift rate is pending** (section 6): it needs the Lab 1 logs (MZ-017), which do not exist yet.

## 1. Why a gyro heading

The test floor makes the robot slip, so the heading integrated from odometry drifts. The gyro measures the real rotation of the torso and does not slip. The NAO has no magnetometer, so the gyro yaw drifts too, but slowly and independently of the floor. Tags (MZ-018) are the absolute reference that removes the drift.

## 2. Formulas as implemented

All angles are in the maze frame: counterclockwise from east, maze north = +90°. Code: `Src/Tools/Maze/MazeHeading.h` and `MazeLocalization.h`; module glue in `MazeLocalizer.cpp`.

| Step | Formula |
|---|---|
| Reference | At each transition into `playing` ("positioned at start") the localizer stores `startAngle` (from `startHeading`) and `offset` = gyro yaw at that moment. |
| Rotation | `rotation = startAngle + (yaw − offset)` |
| Heading | `heading = snap90(rotation)`: the nearest of north, east, south, west. ±44° snaps to the same heading, ±46° to the next one (unit-tested). |
| Re-alignment | A tag seen with an angle ≤ `squareOnAngle` (25° by default) between the camera ray and the tag normal (`squareOnAngle`) corrects the heading by `rotationGain` (0.5) and moves `offset` so that the gyro agrees with the corrected pose. Walls are axis-aligned, so a square-on tag gives the true heading. |
| Drift | `MazeCellPose.headingDrift` is the net drift of the gyro offset since the last reset: `offset` now minus `offset` right after the reset, normalized to [−180°, 180°], positive when the gyro runs ahead of the truth. Corrections that cancel out do not add up, and the arbitrary gyro reference does not show. |
| Quality | `RobotPose.quality` grows worse with the time and the distance since the last tag fix (`superbMax*`, `okayMax*`); MZ-025 reads it. |

`useGyro = false` uses the odometry rotation instead (for comparison).

## 3. Face inference and false-positive filter

A tag id names the cell, not the face. The face is inferred from the heading:

1. `MazeTagLayout::candidateFaces` lists the faces of that cell that carry a tag in the current layout (no layout is hard-coded).
2. `chooseFace` picks the candidate whose expected normal (out of the wall, into the cell, seen from the localized rotation) is closest to the measured tag normal.
3. If the measured tag normal differs from the expected one by more than `maxFaceAngleError` (45° by default), the tag is **rejected**: it is a misread or a tag the robot cannot be seeing from this heading. This is the false-positive filter. `MazeMapper` has its own copy of the parameter; keep both equal.

A non-finite tag pose is also rejected, and the fix age is clamped if the clock goes backwards.

## 4. Drift and noise in simulation

SimRobot has neither gyro drift nor odometry noise. Three simulation-only keys of `mazeLocalizer.cfg` add them (leave all at 0 on the robot):

| Key | Effect |
|---|---|
| `injectedGyroDriftPerMinute` | Degrees per minute added to the gyro yaw. |
| `injectedOdometryTranslationNoise` | Std. dev. of the relative error of each odometry translation delta (0.05 = 5 %). |
| `injectedOdometryRotationNoise` | Std. dev. of the rotation error per metre walked; only matters with `useGyro = false`. |

The oracle (`oracledMazePerceptsProvider.cfg`) adds tag noise and `dropoutRate`.

## 5. Offline results (simulation)

Method: `Src/Apps/Tests/Maze/MazeLocalizerAcceptanceTest.cpp` with `Src/Apps/Tests/Maze/MazeLocalizerSim.h`. A virtual robot walks the exploration route of `maze_1` (11 cells, 7 turns, 43.2 s) with noisy odometry (systematic plus per-frame error), a drifting gyro with an arbitrary reference, and oracle tags at 10 Hz vision. It uses the same helpers as the modules, but not the module glue or SimRobot.

| Run | Gyro drift | Odometry error | Tag dropout | Final error | Cell | Fixes |
|---|---|---|---|---|---|---|
| Nominal | 3 °/min | 5 % (plus 2°/m rotation) | 0 | 2-3 mm, 0.3° (checkpoint max 8.2-13.1 mm) | 11/11 | 383 (8.9/s, 35 per cell, 92 % of vision frames) |
| Harsh | 15 °/min | +10 % bias | 0.17 | 7 mm, 0.4° | 11/11 | 324 (7.5/s) |
| No tags | 3 °/min | 5 % | 1.0 | 143 mm, 2.1° | — | 0 |

Quality without tags: `okay` after 9.3 s / 1504 mm, `poor` after 30.06 s (3867 mm), as designed.

Limits of these numbers: the true path follows the cell centres exactly (no sway or stopping error), the oracle sees every tag in range (no image processing), and the nominal and harsh drift values are assumptions until section 6 is filled.

### Tag-fix rate per layout mode

Same route, nominal noise.

| `mazeTagLayout.cfg` | Fixes per second | Fixes per cell | Final error |
|---|---|---|---|
| `allFaces` | 8.9 | 35 | 3 mm |
| `faces = [north]` | 1.9 | 7.6 | 40 mm |
| `walls` (checkerboard) | 7.0 | 27.6 | 9 mm |

## 6. Pending: real drift rate (needs Lab 1 logs, MZ-017)

`logger.cfg` already records `InertialData`. Until the logs exist, no number below is real.

| Case | Drift rate (°/min) | Total drift (°) | Duration | Log | Notes |
|---|---|---|---|---|---|
| Stand, 5 min | | | | | |
| Corridor walk | | | | | |

Procedure:

1. Take a Lab 1 log with the robot standing still for 5 min, and one with a corridor walk (straight, no turns).
2. Read the yaw of `InertialData.orientation3D` over time (unwrap it).
3. Stand: subtract the first yaw from the last, divide by the minutes. A straight line fit of yaw against time gives the same rate with less noise; write the line fit.
4. Walk: same, but over the walking time only. The walk rate can be higher than the stand rate (vibration), so keep both.
5. Fill the table. Then repeat the harsh run of section 5 with `injectedGyroDriftPerMinute` set to the measured rate, and set `rotationGain` and the quality limits if the result calls for it. Tick the drift criterion of MZ-020.

The drift rate decides how long the robot can go without a square-on tag fix before the heading snap picks the wrong direction: the snap fails when the error reaches 45°, i.e. after `45 / rate` minutes.

## 7. North-face-only experiment

Question: with tags only on the north face of each cell (the cheapest layout to build), does the localizer keep the cell, and how much coverage is lost on the other headings?

Method: offline simulation as in section 5 with `mode = faces; faces = [north];` in `mazeTagLayout.cfg`, nominal noise. Coverage = share of vision frames that give a tag fix, by the true heading of the robot.

| True heading | `faces = [north]` | `allFaces` |
|---|---|---|
| north | 90 % | 90 % |
| east | 13 % | 91 % |
| south | 0 % | 91 % |
| west | 39 % | 100 % |

Results:

- **North corridor** (6 cells facing north): the cell estimate is correct 6/6 and the final error is 2.5 mm.
- **South corridor** (same corridor walked south): 0 fixes, 137 mm drift at the end. The estimate holds only by odometry and gyro.
- **Whole route:** 1.9 fixes/s instead of 8.9/s, final error 40 mm instead of 3 mm.
- **Head scan** of ±60°: east rises from 13 % to 28 %, but north falls from 90 % to 58 % (the head spends time looking sideways). A scan helps the weak headings and hurts the strong one. The trade-off was printed by the test; it does not assert on it.

Consequence for the competition: if only the north faces carry tags (Q1), the robot gets almost no fixes while it faces south and few while it faces east. In the 6-cell corridor walked south the error grew to 137 mm (measured offline), past the 8 cm limit of MZ-020. Options, each costing time: turn to face north at a corridor end to get a fix, scan the head (weak gain, see above), or rely on wall corrections (MZ-045). The answer to Q1 decides which layout is real; the localizer needs only a config change for any of them.
