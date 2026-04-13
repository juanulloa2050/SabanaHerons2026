/**
 * @file CameraStreamer.h
 *
 * Declares a module that streams JPEG camera frames over TCP while BHuman runs.
 * Upper camera → port upperPort (default 7777)
 * Lower camera → port lowerPort (default 7778)
 *
 * Wire protocol per frame:
 *   [4 bytes] magic  "CAMF"
 *   [4 bytes] uint32 little-endian JPEG size
 *   [N bytes] raw JPEG data
 *
 * All socket calls are non-blocking; the module never delays the perception thread.
 */

#pragma once

#include "Representations/Infrastructure/CameraInfo.h"
#include "Representations/Infrastructure/CameraStream.h"
#include "Representations/Infrastructure/JPEGImage.h"
#include "Framework/Module.h"
#include <vector>

MODULE(CameraStreamer,
{,
  REQUIRES(JPEGImage),
  REQUIRES(CameraInfo),
  PROVIDES(CameraStream),
  LOADS_PARAMETERS(
  {,
    (int)(7777) upperPort, /**< TCP port for the upper camera stream. */
    (int)(7778) lowerPort, /**< TCP port for the lower camera stream. */
    (bool)(true) enabled,  /**< Set to false to disable streaming entirely. */
  }),
});

class CameraStreamer : public CameraStreamerBase
{
public:
  CameraStreamer();
  ~CameraStreamer();

private:
  int serverFd = -1;               /**< Listening server socket. */
  int activePort = 0;              /**< Port this instance is bound to. */
  std::vector<int> clients;        /**< Connected client sockets. */

  void update(CameraStream& cameraStream) override;

  /** Try to bind and listen on the given port. Returns true on success. */
  bool initServer(int port);

  /** Accept any pending connections (non-blocking). */
  void acceptClients();

  /** Send a JPEG frame to all connected clients, dropping any that fail. */
  void sendFrame(const JPEGImage& jpeg);

  /** Send exactly `len` bytes to `fd`; returns false if the client disconnected. */
  static bool sendAll(int fd, const unsigned char* data, int len);
};
