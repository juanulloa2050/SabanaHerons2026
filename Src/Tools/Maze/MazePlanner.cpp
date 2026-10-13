#include "MazePlanner.h"
#include <algorithm>
#include <cstdlib>
#include <queue>

namespace Maze
{
  namespace
  {
    constexpr std::array<Action, actionCount> actionOrder = {Action::forward, Action::turnLeft, Action::turnRight, Action::turnAround};

    MazeState stateFromIndex(int index)
    {
      MazeState state;
      state.moving = index % 2 == 1;
      state.heading = static_cast<Heading>(index / 2 % headingCount);
      const int cell = index / (2 * headingCount);
      state.row = cellRow(cell);
      state.col = cellCol(cell);
      return state;
    }

    struct Entry
    {
      MazeCost priority;
      MazeCost cost;
      int index;

      /** Reversed so that std::priority_queue pops the smallest priority first; the index makes ties deterministic. */
      bool operator<(const Entry& other) const
      {
        if(priority == other.priority)
          return index > other.index;
        return other.priority < priority;
      }
    };
  }

  int MazePlanner::stateIndex(const MazeState& state)
  {
    return ((cellId(state.row, state.col) * headingCount) + static_cast<int>(state.heading)) * 2 + (state.moving ? 1 : 0);
  }

  bool MazePlanner::apply(const MazeGraph& graph, UnknownPolicy policy, const MazeState& state, Action action, MazeState& next, MazeCost& cost) const
  {
    next = state;
    switch(action)
    {
      case Action::forward:
        if(!graph.canPass(state.row, state.col, state.heading, policy) ||
           !inside(state.row + rowDelta[static_cast<std::size_t>(state.heading)], state.col + colDelta[static_cast<std::size_t>(state.heading)]))
          return false;
        next.row += rowDelta[static_cast<std::size_t>(state.heading)];
        next.col += colDelta[static_cast<std::size_t>(state.heading)];
        next.moving = true;
        cost = {costs.forwardMs + (state.moving ? 0 : costs.startMs), 0};
        return true;
      case Action::turnLeft:
        next.heading = turned(state.heading, -1);
        cost = {costs.turn90Ms, 1};
        break;
      case Action::turnRight:
        next.heading = turned(state.heading, 1);
        cost = {costs.turn90Ms, 1};
        break;
      default:
        next.heading = turned(state.heading, 2);
        cost = {costs.turn180Ms, 1};
        break;
    }
    next.moving = false;
    return true;
  }

  void MazePlanner::computeCostToGo(const MazeGraph& newGraph, UnknownPolicy newPolicy)
  {
    graph = newGraph;
    policy = newPolicy;
    table.fill(MazeCost::infinity());
    std::priority_queue<Entry> queue;
    for(int i = 0; i < stateCount; ++i)
    {
      const MazeState state = stateFromIndex(i);
      if(isGoal(state.row, state.col))
      {
        table[static_cast<std::size_t>(i)] = {0, 0};
        queue.push({{0, 0}, {0, 0}, i});
      }
    }
    while(!queue.empty())
    {
      const Entry entry = queue.top();
      queue.pop();
      if(table[static_cast<std::size_t>(entry.index)] < entry.cost)
        continue;
      const MazeState target = stateFromIndex(entry.index);
      // Predecessors are the other states of the same cell and the cell behind (same heading).
      for(int i = 0; i < 10; ++i)
      {
        MazeState previous = target;
        if(i < 8)
        {
          previous.heading = static_cast<Heading>(i / 2);
          previous.moving = i % 2 == 1;
        }
        else
        {
          previous.row -= rowDelta[static_cast<std::size_t>(target.heading)];
          previous.col -= colDelta[static_cast<std::size_t>(target.heading)];
          previous.moving = i == 9;
          if(!inside(previous.row, previous.col))
            continue;
        }
        if(isGoal(previous.row, previous.col))
          continue;
        for(const Action action : actionOrder)
        {
          MazeState next;
          MazeCost step;
          if(!apply(graph, policy, previous, action, next, step) || stateIndex(next) != entry.index)
            continue;
          const MazeCost candidate = entry.cost + step;
          MazeCost& best = table[static_cast<std::size_t>(stateIndex(previous))];
          if(candidate < best)
          {
            best = candidate;
            queue.push({candidate, candidate, stateIndex(previous)});
          }
        }
      }
    }
  }

  MazeCost MazePlanner::costToGo(const MazeState& state) const
  {
    return table[static_cast<std::size_t>(stateIndex(state))];
  }

  bool MazePlanner::nextAction(const MazeState& state, Action& action) const
  {
    if(isGoal(state.row, state.col))
      return false;
    MazeCost best = MazeCost::infinity();
    bool found = false;
    for(const Action candidate : actionOrder)
    {
      MazeState next;
      MazeCost step;
      if(!apply(graph, policy, state, candidate, next, step))
        continue;
      const MazeCost toGo = costToGo(next);
      if(toGo.isInfinite())
        continue;
      const MazeCost value = step + toGo;
      if(value < best)
      {
        best = value;
        action = candidate;
        found = true;
      }
    }
    return found;
  }

  void MazePlanner::append(MazePlan& plan, Action action, int timeMs) const
  {
    plan.actions.push_back(action);
    plan.timeMs += timeMs;
    if(action == Action::forward)
      ++plan.cells;
    else if(action == Action::turnAround)
      ++plan.turns180;
    else
      ++plan.turns90;
  }

  MazePlan MazePlanner::routeFromTable(const MazeState& start) const
  {
    MazePlan plan;
    MazeState state = start;
    while(!isGoal(state.row, state.col))
    {
      Action action = Action::forward;
      MazeState next;
      MazeCost step;
      if(plan.actions.size() > static_cast<std::size_t>(stateCount) || !nextAction(state, action) || !apply(graph, policy, state, action, next, step))
        return MazePlan();
      append(plan, action, step.timeMs);
      state = next;
    }
    plan.found = true;
    return plan;
  }

  int MazePlanner::heuristic(const MazeState& state) const
  {
    int best = MazeCost::infinity().timeMs;
    for(int row = 3; row <= 4; ++row)
      for(int col = 3; col <= 4; ++col)
      {
        const int dRow = std::abs(row - state.row);
        const int dCol = std::abs(col - state.col);
        best = std::min(best, (dRow + dCol) * costs.forwardMs + (dRow != 0 && dCol != 0 ? costs.turn90Ms : 0));
      }
    return best;
  }

  MazePlan MazePlanner::speedRun(const MazeGraph& knownGraph, const MazeState& start) const
  {
    constexpr UnknownPolicy speedPolicy = UnknownPolicy::asWall;
    std::array<MazeCost, stateCount> best;
    std::array<int, stateCount> parent;
    std::array<Action, stateCount> parentAction;
    best.fill(MazeCost::infinity());
    parent.fill(-1);
    parentAction.fill(Action::forward);

    std::priority_queue<Entry> queue;
    const int startIndex = stateIndex(start);
    best[static_cast<std::size_t>(startIndex)] = {0, 0};
    queue.push({{heuristic(start), 0}, {0, 0}, startIndex});
    int goalIndex = -1;
    while(!queue.empty())
    {
      const Entry entry = queue.top();
      queue.pop();
      if(best[static_cast<std::size_t>(entry.index)] < entry.cost)
        continue; // outdated entry
      const MazeState state = stateFromIndex(entry.index);
      if(isGoal(state.row, state.col))
      {
        goalIndex = entry.index;
        break;
      }
      // The heuristic is admissible but not consistent, so a state may be expanded again after an improvement.
      for(const Action action : actionOrder)
      {
        MazeState next;
        MazeCost step;
        if(!apply(knownGraph, speedPolicy, state, action, next, step))
          continue;
        const MazeCost cost = entry.cost + step;
        const int nextIndex = stateIndex(next);
        if(cost < best[static_cast<std::size_t>(nextIndex)])
        {
          best[static_cast<std::size_t>(nextIndex)] = cost;
          parent[static_cast<std::size_t>(nextIndex)] = entry.index;
          parentAction[static_cast<std::size_t>(nextIndex)] = action;
          queue.push({{cost.timeMs + heuristic(next), cost.turns}, cost, nextIndex});
        }
      }
    }

    MazePlan plan;
    if(goalIndex < 0)
      return plan;
    std::vector<Action> reversed;
    for(int index = goalIndex; parent[static_cast<std::size_t>(index)] >= 0; index = parent[static_cast<std::size_t>(index)])
      reversed.push_back(parentAction[static_cast<std::size_t>(index)]);
    MazeState state = start;
    for(auto action = reversed.rbegin(); action != reversed.rend(); ++action)
    {
      MazeState next;
      MazeCost step;
      if(!apply(knownGraph, speedPolicy, state, *action, next, step))
        return MazePlan(); // cannot happen: the actions were produced by the same rules
      append(plan, *action, step.timeMs);
      state = next;
    }
    plan.found = true;
    return plan;
  }
}
