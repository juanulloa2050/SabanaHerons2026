/**
 * @file Tools/Maze/MazeBlockedEdgeChannel.h
 *
 * How a blocked wall travels from the behaviour to the mapper (MZ-046), free of representations and modules so that it can be
 * unit tested. The behaviour publishes the edge in a message with a counter that grows with every block; the mapper acts on a
 * message once, whatever the number of frames it stays published and however often the module runs. The message is in the frame of
 * the anchor (the working hypothesis of the start), whose `revision` it carries: if the anchor changed on the way, the cell is in
 * another frame and the wall would land on an edge the robot never touched, so the mapper drops it instead.
 */

#pragma once

#include "MazeDrive.h"
#include <optional>

namespace Maze
{
  /** The latest blocked wall, the content of the representation `MazeBlockedEdge`. */
  struct BlockedEdgeMessage
  {
    unsigned counter = 0;  /**< Grows with every published block; 0: none yet. */
    BlockedEdge edge;
    int revision = 0;      /**< `MazeStart::revision` of the frame the edge is in. */
    unsigned time = 0;     /**< When the block was published (ms). */
  };

  /** The behaviour's side: remembers the latest block. */
  class BlockedEdgeSender
  {
  public:
    /** Publishes a new block; the counter grows by one. */
    void send(const BlockedEdge& edge, int revision, unsigned time)
    {
      message.counter = message.counter + 1u;
      message.edge = edge;
      message.revision = revision;
      message.time = time;
    }

    BlockedEdgeMessage message;
  };

  /** What the mapper does with a message. */
  enum class BlockedEdgeAction
  {
    none,   /**< Nothing new. */
    apply,  /**< A new block in the current frame: map it. */
    dropped, /**< A new block, but the frame changed since it was published: it is not mapped. */
  };

  /** The mapper's side: acts on each message once. */
  class BlockedEdgeReceiver
  {
  public:
    /**
     * @param revision The revision of the frame the mapper works in now.
     * @return What to do with `message`; a counter that differs from the last one seen (also a smaller one: the behaviour restarted) is new.
     */
    [[nodiscard]] BlockedEdgeAction receive(const BlockedEdgeMessage& message, int revision)
    {
      if(message.counter == 0 || message.counter == seen)
        return BlockedEdgeAction::none;
      seen = message.counter;
      return message.revision == revision ? BlockedEdgeAction::apply : BlockedEdgeAction::dropped;
    }

  private:
    unsigned seen = 0;
  };
}
