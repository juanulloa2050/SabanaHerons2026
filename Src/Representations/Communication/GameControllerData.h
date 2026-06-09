/**
 * @file GameControllerData.h
 *
 * This file declares a representation that contains the data received from a GameController.
 *
 * @author Thomas Röfer
 * @author Arne Hasselbring
 */

#pragma once

#include "Network/RoboCupGameControlData.h"
#include "Streaming/Streamable.h"

struct GameControllerData : public RoboCup::RoboCupGameControlData, public Streamable
{
  /** Constructor. */
  GameControllerData();

  uint8_t competitionPhase = COMPETITION_PHASE_ROUNDROBIN; /**< Legacy internal field for modules that still distinguish round-robin/play-off behavior. */
  unsigned timeLastPacketReceived = 0; /**< Time when the last GameController packet has been received. */
  bool isTrueData = false; /**< Whether the GameController packet does not delay some transitions that are normally signaled by a whistle. */

protected:
  /**
   * Read this object from a stream.
   * @param stream The stream from which the object is read.
   */
  void read(In& stream) override;

  /**
   * Write this object to a stream.
   * @param stream The stream to which the object is written.
   */
  void write(Out& stream) const override;

private:
  static void reg();

  using RoboCup::RoboCupGameControlData::header; // Hide, because it is not streamed
  using RoboCup::RoboCupGameControlData::version; // Hide, because it is not streamed
};
