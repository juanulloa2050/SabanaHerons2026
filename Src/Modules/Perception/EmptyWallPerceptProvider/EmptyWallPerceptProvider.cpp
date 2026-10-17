/**
 * @file EmptyWallPerceptProvider.cpp
 *
 * Implementation of the placeholder wall percept, see the header.
 */

#include "EmptyWallPerceptProvider.h"

MAKE_MODULE(EmptyWallPerceptProvider);

void EmptyWallPerceptProvider::update(WallPercept& wallPercept)
{
  wallPercept = WallPercept();
  wallPercept.timestamp = theFrameInfo.time;
}
