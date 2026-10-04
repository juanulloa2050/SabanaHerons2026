/**
 * @file SearchRestartBall.h
 */

/* SabanaHerons fork extension (B-Human 2023 base).
 * Execute the restart candidate search selected by the strategy using the existing
 * observation and walking skills.
 * Release overview and commit references: README.md.
 */

#pragma once

#include "Tools/BehaviorControl/Strategy/ActiveRole.h"

class SearchRestartBall : public ActiveRole
{
  SkillRequest execute(const Agent& self, const Agents& teammates) override;
};
