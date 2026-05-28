# HSL 2026 Migration Checklist

Sources checked:

- `../SPL-Rules-2024.pdf`: Standard Platform League 2024 rules, aligned with the old codebase assumptions.
- `../Rules-HSL-2026.pdf`: Humanoid Soccer League draft rules as of 2026-05-26.
- `../GameController3`: old SPL GameController protocol v18.
- `../GameController`: newer HSL GameController protocol v20.

## GameController v20 Protocol

- [x] Update local `RoboCupGameControlData.h` to protocol version 20.
- [x] Replace removed `competitionPhase` wire field with `stopped`.
- [x] Add `RobotInfo::cautions`.
- [x] Add HSL competition types: Small, Middle, Large.
- [x] Add HSL phases: normal, penalty shoot-out, extra time, timeout.
- [x] Replace old SPL set plays with HSL set plays: direct free kick, indirect free kick, throw-in, goal kick, corner kick, penalty kick.
- [x] Replace SPL penalty constants with HSL penalty constants.
- [x] Map HSL penalties to internal `GameState::PlayerState`.
- [x] Handle GameController `stopped` as internal `GameState::stopped`.
- [x] Keep compatibility aliases for old simulator/internal code while preserving v20 wire format.
- [x] Rework simulator GameController commands to expose HSL names directly instead of old SPL aliases.
- [x] Use `RobotInfo::cautions` explicitly in behavior/debug output.
- [x] Model `PENALTY_SENT_OFF` as a permanent player removal instead of only treating it as manual penalized.
- [x] Separate `PENALTY_MOTION_IN_STOP` from `PENALTY_MOTION_IN_SET` in internal player states.

## HSL 2026 Rules

- [x] Update core timings: free kicks are 45 s and delayed GameController playing signal is 10 s.
- [x] Add initial internal support for dropped ball READY/SET states when `kickingTeam == KICKING_TEAM_NONE`.
- [x] Treat dropped ball setup as no set play, no kicking team.
- [x] Enforce dropped ball positioning through illegal areas: own half plus center circle exclusion for SET.
- [x] Update `Default` field dimensions to HSL 2026 Small baseline.
- [x] Update current Sabana `4v4_Full` location dimensions to HSL 2026 Small baseline.
- [x] Update current Sabana `3v3_Full` location dimensions to HSL 2026 Small baseline.
- [ ] Update `Default`, `4v4_Full`, and `3v3_Full` ball radius when the competition FIFA Mini Ball is in use.
- [ ] Confirm the exact competition FIFA Mini Ball diameter, weight, friction, and kick distance parameters.
- [ ] Decide whether other legacy locations should remain scaled test fields or be migrated to HSL Small.
- [ ] Update SimRobot field assets to visually match HSL 2026 Small dimensions.
- [x] Implement direct vs indirect free kick semantics instead of collapsing both to pushing free kick behavior.
- [x] Add explicit internal states or metadata for throw-in vs kick-in if behavior must distinguish hand throw from ground kick.
- [x] Implement HSL goal-kick avoidance area: opponents must stay outside the entire penalty area.
- [ ] Add dropped ball tests/simulation scripts.
- [x] Update global game stuck restart to dropped ball in simulator/referee scripts.
- [ ] Rework kick-off plays to avoid direct goals and enforce the two-touch rule for 3+ robots.
- [ ] Add own/opponent free kick strategies for direct free kick, indirect free kick, throw-in, and opponent goal kick.
- [ ] Review ball holding timing: goalkeeper 10 s in own penalty area, all others 5 s.
- [ ] Review request-for-pickup behavior: HSL only allows it for dangerous injury-risk situations.
- [ ] Review team configuration for HSL Small Foundation 4v4 vs Advanced 7v7.
- [ ] Add a fourth active robot/role for Foundation if Sabana plays 4v4.
- [ ] Design 7v7 roles/tactics if Sabana plays Advanced.
- [ ] Validate localization and perception on the HSL Small field and FIFA Mini Ball.

## Verification

- [x] Previous protocol migration built successfully with `Make/Linux/compile Develop SimulatedNao`.
- [x] Rebuild after HSL rules changes.
- [ ] Run simulator smoke test for normal kickoff, dropped ball, free kick, stop play, and penalty shootout.
- [ ] Test with the real `../GameController` v20 packets.
