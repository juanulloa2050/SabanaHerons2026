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
- [ ] Verify READY timing with the real GameController: normal ready is 45 s; penalty kick ready is 30 s.
- [ ] Verify penalty kick duration against the real GameController: HSL rules define 60 s after play begins.
- [ ] Verify `stopped` / Stop Play behavior on real robots: robots must stop immediately, remain still, and not get up during a brief stop.
- [ ] Validate that `PENALTY_MOTION_IN_STOP` is received from the real GameController and does not get confused with motion in set.
- [ ] Implement/confirm Ball Stop Rule in `../GameController`: current source uses an overflowing primary timer and only sends `STATE_FINISHED` after `FinishHalf`, so period end is manual unless a newer GC changes this.
- [ ] Implement/confirm Penalty Kick Extension Rule in `../GameController`: period end must be delayed while a penalty kick substate is active; current `FinishHalf` is a manual action path.
- [ ] Verify timeout handling with the real GameController, including team timeout, referee timeout, retained restart, and dropped ball after referee timeout when no restart applies.
- [ ] Verify mercy rule handling with the real GameController: match ends at a 10-goal difference.
- [ ] Verify allowance-for-time-lost updates and resulting message budget changes from real GameController packets.
- [x] Add initial internal support for dropped ball READY/SET states when `kickingTeam == KICKING_TEAM_NONE`.
- [x] Treat dropped ball setup as no set play, no kicking team.
- [x] Enforce dropped ball positioning through illegal areas: own half plus center circle exclusion for SET.
- [ ] Verify dropped ball PLAYING semantics: ball is immediately in play, direct goals count, and there is no kickoff/two-touch restriction.
- [x] Update `Default` field dimensions to HSL 2026 Small baseline.
- [x] Update current Sabana `4v4_Full` location dimensions to HSL 2026 Small baseline.
- [x] Update current Sabana `3v3_Full` location dimensions to HSL 2026 Small baseline.
- [ ] Keep `Default`, `4v4_Full`, and `3v3_Full` on the previous ball configuration until the competition FIFA Mini Ball is selected and measured.
- [ ] Confirm the exact competition FIFA Mini Ball diameter, weight, friction, and kick distance parameters.
- [ ] Decide whether other legacy locations should remain scaled test fields or be migrated to HSL Small.
- [ ] Update SimRobot field assets to visually match HSL 2026 Small dimensions.
- [x] Implement direct vs indirect free kick semantics instead of collapsing both to pushing free kick behavior.
- [x] Add explicit internal states or metadata for throw-in vs kick-in if behavior must distinguish hand throw from ground kick.
- [x] Implement HSL goal-kick avoidance area: opponents must stay outside the entire penalty area.
- [ ] Verify free kick placement rules: defending-team free kicks in own goal area may be taken anywhere in that area; attacking indirect free kicks inside opponent penalty area move to the nearest parallel penalty-area line.
- [ ] Verify "Ball Free" behavior: free kicks expire after 45 s or game time expiration, and the defending team may then score directly even from an originally indirect free kick.
- [ ] Add dropped ball tests/simulation scripts.
- [x] Update global game stuck restart to dropped ball in simulator/referee scripts.
- [ ] Rework kick-off plays to avoid direct goals and enforce the two-touch rule for 3+ robots.
- [ ] Rework kick-off plays for 2-or-fewer robots: the taker must touch the ball at least once outside the center circle before scoring.
- [x] Ensure kickoff behavior respects designated kicking player when one robot is outside its own half inside the center circle.
- [ ] Add a READY-specific dropped ball positioning path for 3v3/4v4: dropped ball uses no set play, so it can inherit normal tactics with robots in opponent half or inside the center circle.
- [ ] Add own/opponent free kick strategies for direct free kick, indirect free kick, throw-in, and opponent goal kick.
- [ ] Add/validate own and opponent penalty kick strategies for HSL placement: keeper on goal line, one striker in opponent penalty area, all others outside penalty area and center-circle radius from penalty mark.
- [ ] Enforce penalty kick behavior: striker must not touch the ball a second time after it clearly moves, and goalkeeper must stay on feet until the kick or completion.
- [ ] Review penalty shootout workflow: up to 6 prepared robots, non-selected robots as substitutes, no added robots after shootout starts, no timeout during shootout.
- [ ] Review ball holding timing: goalkeeper 10 s in own penalty area, all others 5 s.
- [ ] Review request-for-pickup behavior: HSL only allows it for dangerous injury-risk situations.
- [ ] Review incapable robot behavior: active means walking/turning or searching/tracking ball; fallen robot must attempt get-up within 20 s and succeed within allowed attempts.
- [ ] Review player stance behavior: avoid stance wider than about 1.5x shoulder width for longer than 10 s.
- [ ] Review playing-with-arms/hands behavior: field players and goalkeepers outside own penalty area must not intentionally contact the ball with arms/hands.
- [ ] Review leaving-the-field behavior: carpet exit, goalpost/net contact longer than 5 s, or entangled appendages result in standard removal.
- [ ] Review standard removal return pose: touchline at own-half penalty-mark height, facing opposite touchline, preserving localization symmetry.
- [ ] Review goal-area occupancy: at most three players in own goal area during set and playing; pushed players have 5 s grace after push/get-up.
- [ ] Review team communication limits: UDP broadcast only, 512 B payload limit, port 10000 + team number, no unicast/ad-hoc, 12000 messages per game with HSL additions.
- [ ] Review debug communication limits: single UDP packet to one wired team device at most 1 Hz.
- [ ] Review team configuration for HSL Small Foundation 4v4 vs Advanced 7v7.
- [ ] Add a fourth active robot/role for Foundation if Sabana plays 4v4.
- [ ] Design 7v7 roles/tactics if Sabana plays Advanced.
- [ ] Validate localization and perception on the HSL Small field and FIFA Mini Ball.
- [ ] Validate localization/perception under HSL lighting constraints and small-field turf height, keeping the previous ball config until the real ball is selected.
- [ ] Define operational handling for defective ball: dropped ball if it fails during play; restart accordingly if it fails while not in play; retake penalty kick if it fails during forward penalty movement before contact/post/crossbar.

## Verification

- [x] Previous protocol migration built successfully with `Make/Linux/compile Develop SimulatedNao`.
- [x] Rebuild after HSL rules changes.
- [ ] Run real-environment smoke test for normal kickoff, dropped ball, free kick, stop play, extra time, timeout, and penalty shootout.
- [ ] Run simulator smoke test for normal kickoff, dropped ball, free kick, stop play, and penalty shootout if simulator validation becomes relevant again.
- [ ] Test with the real `../GameController` v20 packets.
