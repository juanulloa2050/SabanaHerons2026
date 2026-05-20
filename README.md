# Sabana-Herons Code Release

This is the 2024 Sabana-herons code release. this code is heavily based in B-Human 2023 code release [available in](https://wiki.b-human.de/coderelease2023/) as we are a small team mainly composed by undergrad students.

Nonetheless, we have added our own improvements to the code, and behaviors that adapt the best to our strategy, therefore, the following list is meant to describe those improvements and adaptations we've done:

## Improvements:

1. **5vs5 tactics and strategies**: Due to our number of available NaoV6 robots and team size, we're currently participating exclusively in SPL Shield Challenge, we've adapted and created B-Human code to work in 5vs5 matches.
2. **Fail analysis with back head button**: Due to the difficulty to rapidly and consistently detect fails in our current field setup (a kind of true official game field but - which due to installation can generate some problems in robots motion schema), we've developed a functionality that when can output the most **"Critical recent fails"** so we don't have to access each robot's log.
3. **Improved DribbleToGoal Skill**: We noted DribbleToGoal skill may encounter some edge cases for Striker, especially when arriving to the delimitation lines of field, which caused the striker to lose the ball and generate a Goal Kick In for adversaries or to shot at the post of the goal, we tried to improve that by almost never letting the striker shot the ball outside field boundary generating a KickIn or striking the Goal Post.
4. **Optimize the run time of 2D pose for Probabilistic Robot Self Locator**: We noticed the UKFPose2D may have some performance issues regarding Landmark Sensor Update, since it was generating unnecessary Landmark readings that would pollute the matrix calculations for pose provider, thus matrix calculations are more expensive we found a way to optimize it.
5. **Improve Image CNN Tools**: As far as we saw, some Image CNN Tools like Image Transforms were overkill, difficult to understand and were making unnecessary calculations. We improved some of these issues by partially refactoring some of these tools.

## Run this code

As we said, this code is heavily based in B-Human 2023 code release [available in](https://wiki.b-human.de/coderelease2023/), therefore and so far, you can just follow their documentation to build and run the code.

## RL and common SimRobot scenes

This repository also contains the C++ side of the RL bridge used by the sibling
`RL` repo.

The recent SimRobot work here was mainly about keeping common scenes from
freezing. The safe debugging order is:

1. compare against `master`
2. confirm the scene really leaves `standby` and reaches `playing`
3. only then inspect higher-level RL code

The core invariant that must stay intact is:

```text
Python -> RLSharedState -> SkillRequest -> SkillBehaviorControl ->
MotionRequest -> MotionEngine / WalkingEngine -> JointRequest -> SimRobot
```

Practical guardrails:

- do not assume manual `F5` for visible RL scenes
- do not break common scenario game-state flow when enabling RL
- do not fix freezes by bypassing the normal B-Human motion path

## Hybrid RL vs B-Human test

Selective RL override is available through:

- `PYBH_RL_OVERRIDE_TEAM`
- `PYBH_RL_ACTIVE_PLAYERS`

That override lives in `StrategyBehaviorControl`, so only the requested jersey
numbers are replaced by RL while the rest of the team keeps normal B-Human
behavior.

There is also a dedicated mixed scene for observing duels and `Zweikampf`:

```text
Config/Scenes/RLvsBH3v3_3D.ros2
Config/Scenes/RLvsBH3v3_3D.con
```

Layout:

- own team `24`
- `robot1`: B-Human goalkeeper
- `robot2` and `robot3`: RL-controlled field players
- opponent team: three B-Human players

The console enables `Drawings/Zweikampf` so the scene can be used together with
the Python runner from the `RL` repo to inspect when the duel skill gets
entered.
