/**
 * @file CameraStreamer.cpp
 *
 * Implementation of the CameraStreamer module.
 * Streams JPEG camera frames over a non-blocking TCP server.
 */

#include "CameraStreamer.h"
#include "Streaming/Output.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

MAKE_MODULE(CameraStreamer);

// ── Frame header ──────────────────────────────────────────────────────────────
// Protocol: magic(4) + uint32_le(size)(4) + jpeg(size)
static const unsigned char MAGIC[4] = {'C', 'A', 'M', 'F'};

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
    sendFrame(theJPEGImage);

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
                << " (total=" << clients.size() << ")");
  }
}

void CameraStreamer::sendFrame(const JPEGImage& jpeg)
{
  if(jpeg.getSize() == 0)
    return;

  // Build 8-byte header: magic + uint32 LE size
  unsigned char header[8];
  header[0] = MAGIC[0]; header[1] = MAGIC[1];
  header[2] = MAGIC[2]; header[3] = MAGIC[3];
  const unsigned sz = jpeg.getSize();
  header[4] = static_cast<unsigned char>( sz        & 0xFF);
  header[5] = static_cast<unsigned char>((sz >>  8) & 0xFF);
  header[6] = static_cast<unsigned char>((sz >> 16) & 0xFF);
  header[7] = static_cast<unsigned char>((sz >> 24) & 0xFF);

  std::vector<int> toRemove;
  for(int fd : clients)
  {
    if(!sendAll(fd, header, 8) ||
       !sendAll(fd, jpeg.getData(), static_cast<int>(sz)))
    {
      ::close(fd);
      toRemove.push_back(fd);
    }
  }

  // Remove disconnected clients
  for(int fd : toRemove)
  {
    clients.erase(std::remove(clients.begin(), clients.end(), fd), clients.end());
    OUTPUT_TEXT("[CameraStreamer] Client disconnected (remaining=" << clients.size() << ")");
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
