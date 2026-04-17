/**
 * @file CameraStreamer.cpp
 *
 * Implementation of the CameraStreamer module.
 * Streams proper YCbCr JPEG frames over a non-blocking TCP server.
 *
 * The CameraImage is stored as YUYV (4:2:2): each pair of visual pixels
 * shares one U and one V sample.  JPEGImage encodes this raw data as a
 * fake 4-channel JCS_CMYK JPEG (no color conversion), which viewers like
 * OpenCV/cv2.imdecode cannot display correctly (colors appear as purple/green).
 *
 * CameraStreamer encodes the image itself using libjpeg with JCS_YCbCr input,
 * producing a standard JPEG that any viewer can display.
 */

#include "CameraStreamer.h"
#include "Streaming/Output.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <jpeglib.h>

MAKE_MODULE(CameraStreamer);

// ── Frame header ──────────────────────────────────────────────────────────────
// Protocol: magic(4) + uint32_le(size)(4) + jpeg(size)
static const unsigned char MAGIC[4] = {'C', 'A', 'M', 'F'};

// ── libjpeg destination manager helpers ──────────────────────────────────────
// Called when the output buffer is full — should never happen with our
// pre-allocated buffer sized at raw image bytes (always > any JPEG output).
static boolean csOnDestEmpty(j_compress_ptr) { return false; }
static void csOnDestIgnore(j_compress_ptr) {}

// ── Constructor / Destructor ──────────────────────────────────────────────────

CameraStreamer::CameraStreamer()
{
  // Server is initialized lazily on first update() so CameraInfo is available.
}

CameraStreamer::~CameraStreamer()
{
  for(int fd : clients)
    ::close(fd);
  if(serverFd >= 0)
    ::close(serverFd);
}

// ── Module update ─────────────────────────────────────────────────────────────

void CameraStreamer::update(CameraStream& cameraStream)
{
  // Lazy init: pick port based on which camera thread we are in
  if(serverFd < 0 && enabled)
  {
    const int port = (theCameraInfo.camera == CameraInfo::upper) ? upperPort : lowerPort;
    if(initServer(port))
    {
      activePort = port;
      OUTPUT_TEXT("[CameraStreamer] Listening on port " << port
                  << " (" << (theCameraInfo.camera == CameraInfo::upper ? "upper" : "lower") << ")");
    }
    else
    {
      OUTPUT_TEXT("[CameraStreamer] Failed to bind port " << port << ": " << std::strerror(errno));
    }
  }

  if(serverFd < 0)
  {
    cameraStream.isStreaming      = false;
    cameraStream.port             = 0;
    cameraStream.connectedClients = 0;
    return;
  }

  acceptClients();
  if(!clients.empty())
    encodeAndSend();

  cameraStream.isStreaming      = true;
  cameraStream.port             = activePort;
  cameraStream.connectedClients = static_cast<int>(clients.size());
}

// ── Private helpers ───────────────────────────────────────────────────────────

bool CameraStreamer::initServer(int port)
{
  serverFd = ::socket(AF_INET, SOCK_STREAM, 0);
  if(serverFd < 0)
    return false;

  // Allow fast restart after crash
  int optval = 1;
  ::setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

  // Non-blocking accept
  ::fcntl(serverFd, F_SETFL, O_NONBLOCK);

  sockaddr_in addr{};
  addr.sin_family      = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port        = htons(static_cast<uint16_t>(port));

  if(::bind(serverFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
  {
    ::close(serverFd);
    serverFd = -1;
    return false;
  }

  ::listen(serverFd, 4);
  return true;
}

void CameraStreamer::acceptClients()
{
  for(;;)
  {
    const int clientFd = ::accept(serverFd, nullptr, nullptr);
    if(clientFd < 0)
      break; // EAGAIN / EWOULDBLOCK — no more pending connections

    // Disable Nagle: send small headers + large JPEG payload without delay
    int flag = 1;
    ::setsockopt(clientFd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    // Non-blocking sends
    ::fcntl(clientFd, F_SETFL, O_NONBLOCK);

    clients.push_back(clientFd);
    OUTPUT_TEXT("[CameraStreamer] New client on port " << activePort
                << " (total=" << static_cast<unsigned int>(clients.size()) << ")");
  }
}

void CameraStreamer::encodeAndSend()
{
  // Visual image dimensions
  const int vW = static_cast<int>(theCameraImage.width) * 2;  // pairs × 2
  const int vH = static_cast<int>(theCameraImage.height);
  if(vW <= 0 || vH <= 0)
    return;

  // Pre-allocate buffers (lazy, only grows)
  const size_t maxJpeg = static_cast<size_t>(vW * vH * 3); // upper bound
  if(jpegBuf.size() < maxJpeg)
    jpegBuf.resize(maxJpeg);

  const size_t rowBytes = static_cast<size_t>(vW * 3); // Y,Cb,Cr per pixel
  if(rowBuf.size() < rowBytes)
    rowBuf.resize(rowBytes);

  // ── libjpeg compress ──────────────────────────────────────────────────────
  jpeg_compress_struct cInfo;
  jpeg_error_mgr jem;
  cInfo.err = jpeg_std_error(&jem);
  jpeg_create_compress(&cInfo);

  // Set up in-memory destination (same pattern as JPEGImage.cpp)
  if(!cInfo.dest)
    cInfo.dest = static_cast<jpeg_destination_mgr*>(
                 (*cInfo.mem->alloc_small)(reinterpret_cast<j_common_ptr>(&cInfo),
                                           JPOOL_PERMANENT,
                                           sizeof(jpeg_destination_mgr)));
  cInfo.dest->init_destination    = csOnDestIgnore;
  cInfo.dest->empty_output_buffer = csOnDestEmpty;
  cInfo.dest->term_destination    = csOnDestIgnore;
  cInfo.dest->next_output_byte    = static_cast<JOCTET*>(jpegBuf.data());
  cInfo.dest->free_in_buffer      = jpegBuf.size();

  cInfo.image_width      = static_cast<JDIMENSION>(vW);
  cInfo.image_height     = static_cast<JDIMENSION>(vH);
  cInfo.input_components = 3;
  cInfo.in_color_space   = JCS_YCbCr; // standard displayable color space

  jpeg_set_defaults(&cInfo);
  cInfo.dct_method = JDCT_FASTEST;
  jpeg_set_quality(&cInfo, 75, true);

  jpeg_start_compress(&cInfo, true);

  while(cInfo.next_scanline < cInfo.image_height)
  {
    const int y = static_cast<int>(cInfo.next_scanline);
    unsigned char* dst = rowBuf.data();

    // Each theCameraImage column x holds a YUYV pair (y0, u, y1, v)
    // representing visual pixels (2x, y) and (2x+1, y).
    // libjpeg JCS_YCbCr expects [Y, Cb, Cr] per visual pixel.
    for(int x = 0; x < static_cast<int>(theCameraImage.width); ++x)
    {
      const auto& px = theCameraImage(static_cast<unsigned int>(x),
                                       static_cast<unsigned int>(y));
      // Visual pixel 2x
      *dst++ = px.y0;
      *dst++ = px.u;   // Cb
      *dst++ = px.v;   // Cr
      // Visual pixel 2x+1 (shares U/V with 2x — 4:2:2 → 4:4:4 upsampling)
      *dst++ = px.y1;
      *dst++ = px.u;
      *dst++ = px.v;
    }

    JSAMPROW rowPtr = rowBuf.data();
    jpeg_write_scanlines(&cInfo, &rowPtr, 1);
  }

  jpeg_finish_compress(&cInfo);
  const unsigned sz = static_cast<unsigned>(
      reinterpret_cast<unsigned char*>(cInfo.dest->next_output_byte) - jpegBuf.data());
  jpeg_destroy_compress(&cInfo);

  if(sz == 0)
    return;

  // ── Build ball-detection metadata (13 bytes) ─────────────────────────────
  // Layout: uint8 status | float32_LE x | float32_LE y | float32_LE radius
  // status: 0=notSeen  1=seen  2=guessed  (matches BallPercept::Status enum)
  unsigned char meta[13];
  meta[0] = static_cast<unsigned char>(theBallPercept.status);
  const float bx = theBallPercept.positionInImage.x();
  const float by = theBallPercept.positionInImage.y();
  const float br = theBallPercept.radiusInImage;
  std::memcpy(&meta[1], &bx, 4);
  std::memcpy(&meta[5], &by, 4);
  std::memcpy(&meta[9], &br, 4);

  // ── Build BallSpots payload (1 + N*8 bytes) ───────────────────────────────
  // Layout: uint8 count | (int32_LE x, int32_LE y) × count
  // BallSpots are candidates BEFORE classification — useful to diagnose whether
  // the pipeline even proposes the trionda as a candidate region.
  const auto& spots = theBallSpots.ballSpots;
  const uint8_t numSpots = static_cast<uint8_t>(std::min(spots.size(), size_t(50)));
  std::vector<unsigned char> spotsData(1u + static_cast<size_t>(numSpots) * 8u);
  spotsData[0] = numSpots;
  for(int i = 0; i < static_cast<int>(numSpots); ++i)
  {
    const int32_t sx = spots[static_cast<size_t>(i)].x();
    const int32_t sy = spots[static_cast<size_t>(i)].y();
    std::memcpy(spotsData.data() + 1 + i * 8,     &sx, 4);
    std::memcpy(spotsData.data() + 1 + i * 8 + 4, &sy, 4);
  }

  // ── Build raw patch payload ───────────────────────────────────────────────
  // Layout: uint8 valid | [uint16_LE patchSize | float32_LE × patchSize²×3]
  const bool patchValid = theRawBallPatch.valid && !theRawBallPatch.data.empty();
  std::vector<unsigned char> patchData;
  if(patchValid)
  {
    const uint16_t ps = static_cast<uint16_t>(theRawBallPatch.patchSize);
    const size_t   nf = theRawBallPatch.data.size();  // ps*ps*3 floats
    patchData.resize(1u + 2u + nf * 4u);
    patchData[0] = 1u;
    patchData[1] = static_cast<unsigned char>( ps       & 0xFF);
    patchData[2] = static_cast<unsigned char>((ps >> 8) & 0xFF);
    std::memcpy(patchData.data() + 3, theRawBallPatch.data.data(), nf * 4u);
  }
  else
  {
    patchData.resize(1u);
    patchData[0] = 0u;
  }

  // ── Send header + JPEG + metadata to all clients ──────────────────────────
  unsigned char header[8];
  header[0] = MAGIC[0]; header[1] = MAGIC[1];
  header[2] = MAGIC[2]; header[3] = MAGIC[3];
  header[4] = static_cast<unsigned char>( sz        & 0xFF);
  header[5] = static_cast<unsigned char>((sz >>  8) & 0xFF);
  header[6] = static_cast<unsigned char>((sz >> 16) & 0xFF);
  header[7] = static_cast<unsigned char>((sz >> 24) & 0xFF);

  std::vector<int> toRemove;
  for(int fd : clients)
  {
    if(!sendAll(fd, header, 8) ||
       !sendAll(fd, jpegBuf.data(), static_cast<int>(sz)) ||
       !sendAll(fd, meta, 13) ||
       !sendAll(fd, spotsData.data(), static_cast<int>(spotsData.size())) ||
       !sendAll(fd, patchData.data(), static_cast<int>(patchData.size())))
    {
      ::close(fd);
      toRemove.push_back(fd);
    }
  }

  for(int fd : toRemove)
  {
    clients.erase(std::remove(clients.begin(), clients.end(), fd), clients.end());
    OUTPUT_TEXT("[CameraStreamer] Client disconnected (remaining=" << static_cast<unsigned int>(clients.size()) << ")");
  }
}

bool CameraStreamer::sendAll(int fd, const unsigned char* data, int len)
{
  int sent = 0;
  while(sent < len)
  {
    const int n = static_cast<int>(::send(fd, data + sent, static_cast<size_t>(len - sent), MSG_DONTWAIT));
    if(n > 0)
    {
      sent += n;
    }
    else if(n == 0)
    {
      return false; // disconnected
    }
    else
    {
      if(errno == EAGAIN || errno == EWOULDBLOCK)
        return false; // client too slow — drop this frame
      return false;   // error
    }
  }
  return true;
}
