/**
 * @file SearchRestartBall.h
 */

// Sabana Herons: role that searches the ball at the predicted restart position.

#pragma once

#include "Tools/BehaviorControl/Strategy/ActiveRole.h"

class SearchRestartBall : public ActiveRole
{
  SkillRequest execute(const Agent& self, const Agents& teammates) override;
};
