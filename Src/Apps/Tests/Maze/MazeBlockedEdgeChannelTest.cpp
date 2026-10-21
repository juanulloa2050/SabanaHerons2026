/**
 * @file MazeBlockedEdgeChannelTest.cpp
 *
 * Tests of the hand-over of a blocked wall from the behaviour to the mapper (ticket MZ-046): each block is applied once, a block of
 * another frame is dropped, and the blocked walk of the drive ends up as a wall in the start-anchored map.
 */

#include "Tools/Maze/MazeBlockedEdgeChannel.h"
#include "Tools/Maze/MazeStartAnchoredMap.h"

#include <gtest/gtest.h>

namespace
{
  using namespace Maze;

  BlockedEdge edgeAt(int row, int col, Heading heading)
  {
    BlockedEdge edge;
    edge.row = row;
    edge.col = col;
    edge.heading = heading;
    return edge;
  }
}

GTEST_TEST(MazeBlockedEdgeChannel, NothingIsPublishedBeforeTheFirstBlock)
{
  BlockedEdgeSender sender;
  BlockedEdgeReceiver receiver;
  EXPECT_EQ(sender.message.counter, 0u);
  EXPECT_EQ(receiver.receive(sender.message, 0), BlockedEdgeAction::none);
}

GTEST_TEST(MazeBlockedEdgeChannel, AMessageIsAppliedOnceHoweverOftenItIsRead)
{
  BlockedEdgeSender sender;
  BlockedEdgeReceiver receiver;
  sender.send(edgeAt(3, 4, Heading::east), 2, 1000);
  EXPECT_EQ(receiver.receive(sender.message, 2), BlockedEdgeAction::apply);
  for(int frame = 0; frame < 100; ++frame)
    EXPECT_EQ(receiver.receive(sender.message, 2), BlockedEdgeAction::none) << "the message stays published";

  sender.send(edgeAt(3, 4, Heading::east), 2, 9000); // The same edge blocked again is a new block.
  EXPECT_EQ(sender.message.counter, 2u);
  EXPECT_EQ(receiver.receive(sender.message, 2), BlockedEdgeAction::apply);
}

GTEST_TEST(MazeBlockedEdgeChannel, ABlockOfAnotherFrameIsDroppedOnceAndNotAppliedLater)
{
  BlockedEdgeSender sender;
  BlockedEdgeReceiver receiver;
  sender.send(edgeAt(3, 4, Heading::east), 1, 1000);
  EXPECT_EQ(receiver.receive(sender.message, 2), BlockedEdgeAction::dropped) << "the anchor moved on";
  EXPECT_EQ(receiver.receive(sender.message, 2), BlockedEdgeAction::none);
  EXPECT_EQ(receiver.receive(sender.message, 1), BlockedEdgeAction::none) << "and it is not a block of the old frame again";
}

GTEST_TEST(MazeBlockedEdgeChannel, ARestartedBehaviourIsHeardAgain)
{
  BlockedEdgeSender sender;
  BlockedEdgeReceiver receiver;
  sender.send(edgeAt(1, 1, Heading::north), 0, 10);
  sender.send(edgeAt(1, 1, Heading::north), 0, 20);
  ASSERT_EQ(receiver.receive(sender.message, 0), BlockedEdgeAction::apply);

  BlockedEdgeSender restarted; // Counter back at 0, then 1.
  restarted.send(edgeAt(2, 2, Heading::south), 0, 30);
  EXPECT_EQ(receiver.receive(restarted.message, 0), BlockedEdgeAction::apply);
}

GTEST_TEST(MazeBlockedEdgeChannel, ABlockedWalkOfTheDriveEndsAsAWallInTheMap)
{
  // Exploration from cell 9 = (1, 1) east to (1, 2); the walk is blocked, the pose is past the wall already.
  DriveController drive;
  DriveInput input;
  input.planValid = input.planCurrent = input.routeFound = input.hasAction = true;
  input.nextAction = Action::forward;
  input.targetRow = 1;
  input.targetCol = 2;
  input.targetHeading = Heading::east;
  input.cell = cellId(1, 1);
  ASSERT_EQ(drive.choose(input, 0).next, DriveNext::walk);
  ASSERT_TRUE(drive.reportBlocked(1100));
  ASSERT_TRUE(drive.blockedEdge().has_value());

  StartAnchoredMap map;
  ASSERT_TRUE(map.setGiven(0, Heading::east));
  BlockedEdgeSender sender;
  BlockedEdgeReceiver receiver;
  sender.send(*drive.blockedEdge(), map.status().revision, 1100);

  ASSERT_EQ(receiver.receive(sender.message, map.status().revision), BlockedEdgeAction::apply);
  EXPECT_TRUE(map.observeBlocked(sender.message.edge.row, sender.message.edge.col, sender.message.edge.heading));
  EXPECT_EQ(map.map().graph.get(1, 1, Heading::east), EdgeState::wall);
  EXPECT_EQ(map.map().graph.get(1, 2, Heading::west), EdgeState::wall);
  EXPECT_FALSE(drive.arrivalAllowed());

  // The map knows the wall now: the next action is chosen again, and a walk through that edge would be aborted.
  EXPECT_EQ(drive.choose(input, 2000).next, DriveNext::walk);
  EXPECT_TRUE(drive.actionOver(false, 2001, map.map().graph));
}
