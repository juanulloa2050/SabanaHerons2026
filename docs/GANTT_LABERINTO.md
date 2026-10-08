# Maze Challenge Schedule and Lab Access Request — Copa NAO CCM MX 2026

SabanaHerons develops the autonomous maze solver for the NAO v6 between **6 and 28 October
2026**. The competition is on **3 November 2026** at Tec de Monterrey, Campus Ciudad de México.
Most of the work happens in simulation and on recorded data. The physical robot and the lab are
needed for only **5 short sessions (11 hours in total)**. Those sessions are highlighted in the
chart and itemised in the lab access request below.

Related: [ROADMAP_LABERINTO.md](ROADMAP_LABERINTO.md) · [TICKETS_LABERINTO.md](TICKETS_LABERINTO.md)

---

## Gantt chart

```mermaid
gantt
    title Maze Challenge - SabanaHerons - Copa NAO CCM MX 2026
    dateFormat YYYY-MM-DD
    axisFormat %d %b
    todayMarker off

    section Sprint 0 - Foundations
    MZ-001 Create Maze-Challenge branch (Bryam)       :t01, 2026-10-06, 1d
    MZ-002 Branch protection request (Wilson)         :t02, 2026-10-06, 1d
    MZ-005 Registration confirmation (Wilson)         :t05, 2026-10-06, 1d
    MZ-003 Roadmap tickets Gantt (Bryam)              :t03, 2026-10-06, 2d
    MZ-004 PR template (Wilson)                       :t04, 2026-10-07, 1d
    MZ-006 Organiser questions (Wilson)               :t06, 2026-10-07, 2d
    MZ-007 Maze scenarios and start flow (Bryam)      :t07, 2026-10-07, 2d
    MZ-009 Maze generator (Wilson)                    :t09, 2026-10-08, 1d
    MZ-008 AprilTag library (Bryam)                   :t08, 2026-10-08, 2d
    MZ-010 Maze to simulator scene (Wilson + Bryam)   :t10, 2026-10-09, 1d
    M0 Foundations ready                              :milestone, m0, 2026-10-09, 0d

    section Sprint 1 - Core logic in simulation
    MZ-011 Maze library and planner (Bryam)           :t11, 2026-10-10, 3d
    MZ-012 Test mazes and expected results (Wilson)   :t12, 2026-10-10, 1d
    MZ-014 Build partial bench (Wilson)               :t14, 2026-10-10, 2d
    MZ-015 Print and mount tags (Wilson)              :t15, 2026-10-12, 1d
    LAB 1 - MZ-017 Log capture (Wilson)               :crit, t17, 2026-10-13, 1d
    MZ-013 Oracle percepts for simulation (Bryam)     :t13, 2026-10-13, 2d
    MZ-016 Corridor walk profile in sim (Wilson)      :t16, 2026-10-14, 2d
    M1 Planner proven in simulation                   :milestone, m1, 2026-10-15, 0d

    section Sprint 2 - Perception and localization
    MZ-018 AprilTag detector (Bryam)                  :t18, 2026-10-16, 2d
    MZ-021 Simulation campaign runner (Wilson)        :t21, 2026-10-16, 2d
    MZ-019 Wall perceptor (Bryam)                     :t19, 2026-10-18, 2d
    MZ-022 Tag accuracy report (Wilson)               :t22, 2026-10-18, 2d
    LAB 2 - MZ-023 Motion cost calibration (Wilson)   :crit, t23, 2026-10-20, 1d
    MZ-020 Localizer and mapper (Bryam)               :t20, 2026-10-20, 2d
    M2 Perception and localization ready              :milestone, m2, 2026-10-21, 0d

    section Sprint 3 - Behaviour and integration
    MZ-024 Maze state machine and skills (Bryam)      :t24, 2026-10-22, 1d
    LAB 3 - MZ-027 Integrated bench run (Bryam + Wilson) :crit, t27, 2026-10-23, 1d
    MZ-025 Watchdog and safe mode (Bryam)             :t25, 2026-10-24, 2d
    MZ-026 Simulation campaign report (Wilson)        :t26, 2026-10-24, 2d
    MZ-028 Tuning from lab data (Wilson)              :t28, 2026-10-25, 1d
    M3 Integrated system in simulation                :milestone, m3, 2026-10-25, 0d

    section Sprint 4 - Hardening and freeze
    MZ-029 Attempt-flow hardening (Bryam)             :t29, 2026-10-26, 1d
    MZ-030 Competition checklist (Wilson)             :t30, 2026-10-26, 1d
    LAB 4 - MZ-031 Competition rehearsal (Bryam + Wilson) :crit, t31, 2026-10-27, 1d
    MZ-032 Code freeze and deploy procedure (Wilson)  :t32, 2026-10-28, 1d
    Code freeze                                       :milestone, m4, 2026-10-28, 0d

    section Buffer - no new development
    LAB 5 - MZ-033 Dress rehearsal (Bryam + Wilson)   :crit, t33, 2026-10-30, 1d
    MZ-034 Logistics and packing (Wilson)             :t34, 2026-11-01, 1d
    Travel to Campus Ciudad de Mexico (Bryam + Wilson) :t35, 2026-11-02, 1d
    Competition Copa NAO CCM MX 2026                  :milestone, m5, 2026-11-03, 0d
```

### Legend

| Style in the chart | Meaning |
|---|---|
| **Red bar** (`crit`), name starts with **LAB** | Needs the lab and the physical NAO robot |
| Normal bar | Desk work: development, simulation, recorded logs, scripts, documentation |
| Diamond | Milestone: end of a sprint, code freeze, competition |
| Name in parentheses | Person responsible (Bryam, Wilson, or both when pairing) |

**At a glance**

- 5 of 34 tickets need the lab.
- 11 hours of lab time out of 95 hours of total effort (Bryam 50 h, Wilson 45 h).

| Sprint | Dates | Milestone |
|---|---|---|
| Sprint 0 — Foundations | 6–9 Oct | M0 (9 Oct) |
| Sprint 1 — Core logic in simulation | 10–15 Oct | M1 (15 Oct) |
| Sprint 2 — Perception and localization | 16–21 Oct | M2 (21 Oct) |
| Sprint 3 — Behaviour and integration | 22–25 Oct | M3 (25 Oct) |
| Sprint 4 — Hardening and freeze | 26–28 Oct | Code freeze (28 Oct) |
| Buffer — rehearsal and travel | 29 Oct – 2 Nov | Competition (3 Nov) |

---

## Lab access request

**To:** University lab management
**From:** SabanaHerons robotics team (Bryam, Wilson)
**Purpose:** preparation for the Maze category of the Copa NAO CCM MX 2026 (3 Nov 2026, Tec de Monterrey, Campus Ciudad de México)

### Requested sessions

| # | Date | Duration | Objective | What is tested | People |
|---|---|---|---|---|---|
| 1 | Tue 13 Oct 2026 | 2 h | Record real camera and motion data | AprilTag markers seen by the robot's camera at 0.25–2 m and 0–60°; robot standing in a corridor of the test bench | 1 |
| 2 | Tue 20 Oct 2026 | 2 h | Calibrate motion on the real floor | Time and precision of steps and turns (10 repetitions each); live marker recognition under the lab's lighting | 1 |
| 3 | Fri 23 Oct 2026 | 2.5 h | First run of the full software between real walls | Corridor, corner, T-junction and dead end on the partial bench: wall clearance, wall contacts, decisions | 2 |
| 4 | Tue 27 Oct 2026 | 2.5 h | Competition-mode rehearsal | Complete two-attempt procedure and the timed 5-minute calibration drill | 2 |
| 5 | Fri 30 Oct 2026 | 2 h | Dress rehearsal with the final software | Full competition checklist from start to finish, no changes allowed | 2 |
| | | **11 h total** | | | |

Four of the five sessions fall in the second half of October, when the software is mature
enough to benefit from real-world testing.

### Space

| | Size | Use |
|---|---|---|
| **Minimum** | **2.5 m × 2 m** of clear, flat floor | A partial test bench of 2×3 cells (1 m × 1.5 m) built from 6 free-standing panels, plus a 1 m straight line for calibration and a safety margin around the robot |
| **Ideal (if available)** | **4 m × 4 m plus 0.5 m margin per side (5 m × 5 m)** | The footprint of the full competition maze (8×8 cells of 50 cm), should we be able to build more panels later |
| Floor | Uniform, matte, hard (no thick carpet) | Similar to the competition floor |
| Storage (if possible) | Space for 6 panels of 50 × 60 cm between sessions | Avoids rebuilding the bench each time |

### Equipment

| Item | Provided by |
|---|---|
| NAO v6 robot, charger and spare battery | Team / university robotics inventory |
| Ethernet cable (robot ↔ laptop) and laptop with the development environment | Team |
| Printer access for the AprilTag markers (matte paper, A4 or letter, black and white) | University |
| Test-bench materials: 6 foam-board or MDF panels (50 × 60 cm, 1–2 cm thick), supports or clamps, floor tape | Team |
| Tape measure, ruler, protractor | Team |
| Wall power outlet near the bench | Lab |

### Justification

The competition maze is 4 × 4 m with 64 cells, and we cannot build it before the event. Our plan
does not depend on it.

- **Simulation and recorded data first.** Most development runs in the B-Human robot simulator and on recorded robot data: the route planner, the map memory between the two attempts, the decision logic and the safety watchdog. These are tested on dozens of generated mazes without using the robot or the lab.
- **The lab only for what cannot be simulated.** The lab is requested only to validate what the simulator cannot reproduce faithfully:
  - the real camera and real marker recognition under real lighting;
  - the precision of steps and turns on a real floor;
  - the robot's clearance when walking between real walls (corridors are about 48 cm wide for a 27.5 cm wide robot).
- **A small partial bench.** A cheap bench of 6 panels covers every situation a maze can present: a corridor, a corner, a T-junction and a dead end.
- **Short, focused sessions.** Five sessions of 2–2.5 hours (11 h in total) keep the use of the robot and the space to a minimum.
