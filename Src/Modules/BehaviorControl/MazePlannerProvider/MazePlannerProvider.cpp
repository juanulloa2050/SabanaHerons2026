/**
 * @file MazePlannerProvider.cpp
 *
 * Implementation of the maze plan provider, see the header.
 */

#include "MazePlannerProvider.h"
#include "Debugging/Debugging.h"
#include "Tools/Maze/MazeRoutes.h"
#include <algorithm>

MAKE_MODULE(MazePlannerProvider);

// The representation mirrors the order of the planner's actions.
static_assert(static_cast<int>(MazeRoute::forward) == static_cast<int>(Maze::Action::forward) &&
              static_cast<int>(MazeRoute::turnLeft) == static_cast<int>(Maze::Action::turnLeft) &&
              static_cast<int>(MazeRoute::turnRight) == static_cast<int>(Maze::Action::turnRight) &&
              static_cast<int>(MazeRoute::turnAround) == static_cast<int>(Maze::Action::turnAround),
              "MazeRoute::Action and Maze::Action must have the same order");

MazePlannerProvider::MazePlannerProvider() : MazePlannerProviderBase("mazePlanner.cfg")
{
  ensureValidCosts();
}

void MazePlannerProvider::ensureValidCosts()
{
  if(Maze::costsValid(costs()))
    return;
  OUTPUT_ERROR("MazePlannerProvider: mazePlanner.cfg: forwardMs, turn90Ms and turn180Ms must be positive and startMs not negative, using the defaults.");
  const Maze::MazeCosts defaults = Maze::validCosts(costs());
  forwardMs = defaults.forwardMs;
  startMs = defaults.startMs;
  turn90Ms = defaults.turn90Ms;
  turn180Ms = defaults.turn180Ms;
}

Maze::MazeCosts MazePlannerProvider::costs() const
{
  Maze::MazeCosts result;
  result.forwardMs = forwardMs;
  result.startMs = startMs;
  result.turn90Ms = turn90Ms;
  result.turn180Ms = turn180Ms;
  return result;
}

Maze::PlanKey MazePlannerProvider::currentKey() const
{
  Maze::PlanKey key;
  key.costs = costs();
  key.mapChangeCounter = theMazeMap.changeCounter;
  key.row = theMazeCellPose.row;
  key.col = theMazeCellPose.col;
  key.heading = static_cast<int>(theMazeCellPose.heading);
  key.valid = theMazeCellPose.insideMaze;
  return key;
}

void MazePlannerProvider::fill(MazeRoute& route, const Maze::RouteResult& result)
{
  route = MazeRoute();
  if(!result.found)
    return;
  route.found = true;
  route.estimatedTimeMs = result.timeMs;
  route.forwardSteps = result.cells;
  route.turns90 = result.turns90;
  route.turns180 = result.turns180;
  route.actions.reserve(result.actions.size());
  for(const Maze::Action action : result.actions)
    route.actions.push_back(static_cast<MazeRoute::Action>(static_cast<int>(action)));
  route.cells.reserve(result.states.size());
  for(const Maze::MazeState& state : result.states)
    route.cells.push_back(Maze::cellId(state.row, state.col));
  if(!result.actions.empty())
    route.nextAction = route.actions.front();
  const Maze::MazeState& target = result.firstTarget();
  route.targetRow = target.row;
  route.targetCol = target.col;
  route.targetHeading = static_cast<MazeCellPose::Heading>(static_cast<int>(target.heading));
}

void MazePlannerProvider::update(MazePlan& mazePlan)
{
  ensureValidCosts(); // The parameters can be changed at runtime (set parameters), not only when the module is created.
  const Maze::PlanKey key = currentKey();
  if(!computed || !(key == plannedKey))
  {
    const unsigned revision = computed ? cached.revision + 1 : 1;
    cached = MazePlan();
    cached.revision = revision;
    cached.computedTime = theFrameInfo.time;
    cached.row = theMazeCellPose.row;
    cached.col = theMazeCellPose.col;
    cached.heading = theMazeCellPose.heading;
    cached.mapChangeCounter = theMazeMap.changeCounter;
    cached.valid = theMazeCellPose.insideMaze;
    plannedKey = key;
    computed = true;
    if(cached.valid)
    {
      // The robot plans standing in the cell: the first step pays the start-up time.
      Maze::MazeState start;
      start.row = theMazeCellPose.row;
      start.col = theMazeCellPose.col;
      start.heading = static_cast<Maze::Heading>(static_cast<int>(theMazeCellPose.heading));
      start.moving = false;
      const Maze::RoutePair routes = Maze::planRoutes(theMazeMap.toGraph(), start, costs());
      cached.atGoal = routes.atGoal;
      fill(cached.exploration, routes.exploration);
      fill(cached.speedRun, routes.speedRun);
    }
  }
  mazePlan = cached;
}
