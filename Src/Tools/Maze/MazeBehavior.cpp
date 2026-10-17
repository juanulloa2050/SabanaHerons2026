/**
 * @file Tools/Maze/MazeBehavior.cpp
 *
 * Implementation of the maze phase machine, see the header.
 */

#include "MazeBehavior.h"
#include "Streaming/InStreams.h"

namespace Maze
{
  const char* phaseName(Phase phase)
  {
    switch(phase)
    {
      case Phase::notPlaying: return "notPlaying";
      case Phase::waitingForStart: return "waitingForStart";
      case Phase::exploration: return "exploration";
      case Phase::standby: return "standby";
      case Phase::speedRun: return "speedRun";
      default: return "finished";
    }
  }

  BehaviorStep MazeBehavior::update(const BehaviorInput& input)
  {
    BehaviorStep step;
    const Phase before = currentPhase;
    Phase next = before;
    std::vector<const char*> say;

    // The tap that starts an attempt: attempt 1 explores, later attempts run the known route if there is one.
    const auto startAttempt = [&]
    {
      if(input.speedRunFound)
      {
        next = Phase::speedRun;
        say.push_back(Phrases::speedRun);
      }
      else
      {
        next = Phase::exploration;
        say.push_back(Phrases::exploring);
      }
    };

    switch(before)
    {
      case Phase::notPlaying:
        if(input.playing)
        {
          next = Phase::waitingForStart;
          say.push_back(Phrases::ready);
        }
        break;
      case Phase::waitingForStart:
        if(!input.playing)
          next = Phase::notPlaying;
        else if(input.active && input.headFront)
          startAttempt();
        break;
      case Phase::exploration:
      case Phase::speedRun:
        if(!input.active)
        {
          next = Phase::standby;
          say.push_back(Phrases::standby);
        }
        else if(input.atGoal)
        {
          next = Phase::finished;
          say.push_back(Phrases::goal);
          say.push_back(Phrases::finished);
        }
        break;
      case Phase::finished:
        if(!input.active)
        {
          next = Phase::standby;
          say.push_back(Phrases::standby);
        }
        break;
      case Phase::standby:
        if(input.active && input.headFront)
          startAttempt();
        break;
    }

    // The start is deduced: tell which corner it is (once). The first frame only learns the state it finds.
    std::string startPhrase;
    if(seenFrame && !startWasKnown && input.startKnown && !input.startGiven)
      startPhrase = "start " + std::to_string(input.startCell);
    startWasKnown = input.startKnown;
    seenFrame = true;

    if(input.announce)
    {
      for(const char* phrase : say)
        step.phrases.emplace_back(phrase);
      if(!startPhrase.empty())
        step.phrases.push_back(startPhrase);
    }

    currentPhase = next;
    step.phase = next;
    step.changed = next != before;
    return step;
  }

  std::string tagPhrase(int tagId, bool seenRecently)
  {
    return seenRecently && tagId >= 0 ? "tag " + std::to_string(tagId) : "no tag";
  }

  bool AnnounceFile::load(const std::string& name, AnnounceFile& file)
  {
    // The other entries of the file are top-level keys that `AnnounceFile` does not declare; the map stream only
    // reports unused attributes of the records it enters, so they are ignored (see `StartFile::load`).
    InMapFile stream(name);
    if(!stream.exists())
      return false;
    stream >> file;
    return true;
  }
}
