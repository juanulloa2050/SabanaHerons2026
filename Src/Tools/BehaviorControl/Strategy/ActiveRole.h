/**
 * @file ActiveRole.h
 *
 * This file declares a base class for active roles.
 *
 * @author Arne Hasselbring
 */

/* SabanaHerons fork extension (B-Human 2023 base).
 * Register the restart-ball search role so strategy can select it through the normal role
 * interface.
 * Release overview and commit references: README.md.
 */

#pragma once

#include "Role.h"
#include "Streaming/Enum.h"

class ActiveRole : public Role
{
public:
  ENUM(Type,
  {,
    playBall,
    freeKickWall,
    closestToTeammatesBall,
    searchRestartBall,
    startSetPlay, /**< This role is only assigned during READY/SET to the player who has the start position of the set play. */
  });

  static Role::Type toRole(Type type)
  {
    return static_cast<Role::Type>(activeRoleBegin + static_cast<unsigned>(type));
  }
};
