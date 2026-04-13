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
 *   [N bytes] raw JPEG data (standard YCbCr JPEG, displayable by any viewer)
 *
 * All socket calls are non-blocking; the module never delays the perception thread.
 */

#pragma once

#include "Representations/Infrastructure/CameraImage.h"
#include "Representations/Infrastructure/CameraInfo.h"
#include "Representations/Infrastructure/CameraStream.h"
#include "Framework/Module.h"
#include <vector>

MODULE(CameraStreamer,
{,
  REQUIRES(CameraImage),
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
  std::vector<unsigned char> jpegBuf; /**< Pre-allocated JPEG output buffer. */
  std::vector<unsigned char> rowBuf;  /**< Per-scanline YCbCr row buffer. */

  void update(CameraStream& cameraStream) override;

  /** Try to bind and listen on the given port. Returns true on success. */
  bool initServer(int port);

  /** Accept any pending connections (non-blocking). */
  void acceptClients();

  /**
   * Encode theCameraImage as a proper YCbCr JPEG and send to all clients.
   * Converts YUYV → YCbCr on the fly; each visual pixel gets its own Y,
   * with U/V shared within each pair (4:2:2 upsampled to 4:4:4 for libjpeg).
   */
  void encodeAndSend();

  /** Send exactly `len` bytes to `fd`; returns false if the client disconnected. */
  static bool sendAll(int fd, const unsigned char* data, int len);
};
