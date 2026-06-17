/**
 * @file SearchRestartBall.h
 */

#pragma once

#include "Tools/BehaviorControl/Strategy/ActiveRole.h"

class SearchRestartBall : public ActiveRole
{
  SkillRequest execute(const Agent& self, const Agents& teammates) override;
};
