/**
 * @file CameraStream.h
 *
 * Representation produced by CameraStreamer module.
 * Carries streaming status for the current camera thread.
 */

#pragma once

#include "Streaming/AutoStreamable.h"

STREAMABLE(CameraStream,
{,
  (bool)(false) isStreaming,      /**< Whether the TCP server is bound and listening. */
  (int)(0)      port,             /**< Port the server is listening on (0 = not active). */
  (int)(0)      connectedClients, /**< Number of currently connected viewers. */
});
