# 4v4 deploy matrix

Use `location = 4v4_Full` for every 4v4 match. The location is the HSL 2026 small-field setup. The scenario and per-player RL mode select the behavior/RL version.

## Sabana 4v4 roster

| Player | Robot | IP | Default role |
| --- | --- | --- | --- |
| 1 | falcanao | 10.0.49.4 | Goalkeeper |
| 2 | juan | 10.0.49.5 | Field player |
| 3 | naonel | 10.0.49.10 | Field player |
| 4 | ospinao | 10.0.49.7 | Field player |

## 4v4 versions

| Public name | Scenario | Location | Deploy option | What it means |
| --- | --- | --- | --- | --- |
| `off` | `4v4_Full` | `4v4_Full` | `--rl-disable` | No field-player embedded PPO. |
| `striker_base` | `4v4_StrikerBase` | `4v4_Full` | `--rl-striker-base 2,3,4` | Old `original` striker PPO using `ppo_striker_hsl2026.onnx`. |
| `baseline_attack` | `4v4_BaselineAttack` | `4v4_Full` | `--rl-defender 2,3,4` | Defender/baseline attack PPO using the defender model. |
| `mixed_attack` | `4v4_MixedAttack` | `4v4_Full` | `--rl-mixed-attack 2,3,4` | Old `team_v4_2`, using `ppo_team_hsl2026_v4_2.onnx`. |
| `complete` | `4v4_Complete` | `4v4_Full` | `--rl-complete 2,3,4` | Old merged version, using `ppo_team_hsl2026_v5_merged.onnx`. |
| `gk` | Any 4v4 scenario | `4v4_Full` | `--rl-gk on` | Goalkeeper RL for player 1. |

Old command names still work: `--rl-striker`, `--rl-team-v42`, and `--rl-merged`.

## Example commands

Deploy all four Sabana robots with `complete` for field players and goalkeeper RL enabled:

```sh
cd /home/limao/workspace/semillero/SabanaHerons2026/Make/Common
./deploy Develop \
  -r 1 10.0.49.4 \
  -r 2 10.0.49.5 \
  -r 3 10.0.49.10 \
  -r 4 10.0.49.7 \
  -t 49 -c red -g green -m 49 \
  -s 4v4_Complete -l 4v4_Full \
  --rl-complete 2,3,4 --rl-gk on --goalkeeper-dive on
```

To test another field-player version, keep the same robot list and replace the scenario plus RL option:

| Test | Scenario | RL option |
| --- | --- | --- |
| No RL | `4v4_Full` | `--rl-disable --rl-gk off` |
| Striker base | `4v4_StrikerBase` | `--rl-striker-base 2,3,4 --rl-gk on` |
| Baseline attack | `4v4_BaselineAttack` | `--rl-defender 2,3,4 --rl-gk on` |
| Mixed attack | `4v4_MixedAttack` | `--rl-mixed-attack 2,3,4 --rl-gk on` |
| Complete | `4v4_Complete` | `--rl-complete 2,3,4 --rl-gk on` |

After all 4v4 versions are approved, create matching `7v7_*` scenarios and extend the player list/options to players `1..7`.
