#include "MazeRoutes.h"

namespace Maze
{
  namespace
  {
    /** The planning states that `plan` passes through on `graph`, so that the route can be read cell by cell. */
    RouteResult describe(const MazePlanner& planner, const MazeGraph& graph, UnknownPolicy policy, const MazeState& start, const MazePlan& plan)
    {
      RouteResult result;
      if(!plan.found)
        return result;
      result.found = true;
      result.actions = plan.actions;
      result.cells = plan.cells;
      result.turns90 = plan.turns90;
      result.turns180 = plan.turns180;
      result.timeMs = plan.timeMs;
      result.states.reserve(plan.actions.size() + 1);
      MazeState state = start;
      result.states.push_back(state);
      for(const Action action : plan.actions)
      {
        MazeState next;
        MazeCost step;
        if(!planner.apply(graph, policy, state, action, next, step))
          return RouteResult(); // cannot happen: the actions come from the same rules
        result.states.push_back(next);
        state = next;
      }
      return result;
    }
  }

  bool costsValid(const MazeCosts& costs)
  {
    return costs.forwardMs > 0 && costs.startMs >= 0 && costs.turn90Ms > 0 && costs.turn180Ms > 0;
  }

  RoutePair planRoutes(const MazeGraph& map, const MazeState& start, const MazeCosts& costs)
  {
    RoutePair pair;
    if(!MazePlanner::isValid(start))
      return pair;
    pair.atGoal = isGoal(start.row, start.col);
    MazePlanner planner(costs);
    planner.computeCostToGo(map, UnknownPolicy::asOpen);
    pair.exploration = describe(planner, map, UnknownPolicy::asOpen, start, planner.routeFromTable(start));
    pair.speedRun = describe(planner, map, UnknownPolicy::asWall, start, planner.speedRun(map, start));
    return pair;
  }
}
