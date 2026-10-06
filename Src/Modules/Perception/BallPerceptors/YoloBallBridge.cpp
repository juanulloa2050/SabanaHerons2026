/**
 * @file YoloBallBridge.cpp
 */

#include "YoloBallBridge.h"
#include "Tools/Math/Transformation.h"
#include "Streaming/Output.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <chrono>
#include <cstring>

MAKE_MODULE(YoloBallBridge);

static const unsigned char MAGIC_DETS[4] = {'D','E','T','S'};

static long long nowMs()
{
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

YoloBallBridge::YoloBallBridge()
{
  if(!enabled) return;
  const int port = (theCameraInfo.camera == CameraInfo::upper) ? upperPort : lowerPort;
  if(initServer(port))
    OUTPUT_TEXT("[YoloBallBridge] Listening on port " << port
                << " (" << (theCameraInfo.camera == CameraInfo::upper ? "upper" : "lower") << ")");
  else
    OUTPUT_TEXT("[YoloBallBridge] Failed to bind port " << port << ": " << std::strerror(errno));
}

YoloBallBridge::~YoloBallBridge()
{
  if(clientFd >= 0) ::close(clientFd);
  if(serverFd >= 0) ::close(serverFd);
}

bool YoloBallBridge::initServer(int port)
{
  serverFd = ::socket(AF_INET, SOCK_STREAM, 0);
  if(serverFd < 0) return false;

  int opt = 1;
  ::setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
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
  ::listen(serverFd, 2);
  return true;
}

void YoloBallBridge::acceptClient()
{
  if(serverFd < 0) return;
  const int fd = ::accept(serverFd, nullptr, nullptr);
  if(fd < 0) return;

  if(clientFd >= 0) ::close(clientFd);

  int flag = 1;
  ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

  // 100ms recv timeout so we never block the camera thread
  struct timeval tv{};
  tv.tv_sec  = 0;
  tv.tv_usec = 100000;
  ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  clientFd = fd;
  OUTPUT_TEXT("[YoloBallBridge] PC connected");
}

void YoloBallBridge::pollDetections()
{
  if(clientFd < 0) return;

  // Non-blocking peek: is there a message waiting?
  unsigned char magic[4];
  const int n = static_cast<int>(::recv(clientFd, magic, 4, MSG_DONTWAIT));
  if(n <= 0)
  {
    if(n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK))
    {
      ::close(clientFd);
      clientFd = -1;
      OUTPUT_TEXT("[YoloBallBridge] PC disconnected");
    }
    return;
  }
  if(n != 4 || std::memcmp(magic, MAGIC_DETS, 4) != 0) return;

  // Read count (blocking with timeout set on socket)
  unsigned char count = 0;
  if(::recv(clientFd, &count, 1, MSG_WAITALL) != 1) return;

  latest.clear();
  if(count > 0)
  {
    latest.resize(count);
    const int expected = static_cast<int>(count) * static_cast<int>(sizeof(Det));
    if(::recv(clientFd, latest.data(), expected, MSG_WAITALL) != expected)
    {
      latest.clear();
      return;
    }
  }
  lastDetTimeMs = nowMs();
}

void YoloBallBridge::update(BallPercept& bp)
{
  if(!enabled)
  {
    bp.status = BallPercept::notSeen;
    return;
  }

  acceptClient();
  pollDetections();

  const long long age = nowMs() - lastDetTimeMs;
  if(!latest.empty() && age < static_cast<long long>(timeoutMs))
  {
    const Det& d = latest[0];
    const Vector2f imgPos(d.x, d.y);

    Vector2f fieldPos;
    // Primary: project onto horizontal plane at ball-centre height (radius above ground).
    // Fallback for upper cam: if that fails (ray doesn't intersect plane, common when
    // head tilts up), retry with height=0 (ground plane). This gives a less accurate
    // but still usable position rather than never publishing seen for upper cam.
    bool geomOk = Transformation::imageToRobotHorizontalPlane(
        imgPos, theBallSpecification.radius,
        theCameraMatrix, theCameraInfo, fieldPos)
      && std::isfinite(fieldPos.x()) && std::isfinite(fieldPos.y());

    if(!geomOk && theCameraInfo.camera == CameraInfo::upper)
      geomOk = Transformation::imageToRobotHorizontalPlane(
          imgPos, 0.f, theCameraMatrix, theCameraInfo, fieldPos)
        && std::isfinite(fieldPos.x()) && std::isfinite(fieldPos.y());

    // Third fallback (upper only): estimate distance from apparent ball radius using
    // the pinhole camera model. Works even when the ray doesn't intersect any horizontal
    // plane (ball above camera horizon, or head tilted up during walking).
    if(!geomOk && theCameraInfo.camera == CameraInfo::upper && d.r > 0.f)
    {
      const float estimatedDist =
          theCameraInfo.focalLength * theBallSpecification.radius / d.r;
      const float horizAngle = std::atan2(
          imgPos.x() - theCameraInfo.width * 0.5f, theCameraInfo.focalLength);
      fieldPos = Vector2f(estimatedDist * std::cos(horizAngle),
                          estimatedDist * std::sin(horizAngle));
      geomOk = estimatedDist > 0.f
               && std::isfinite(fieldPos.x()) && std::isfinite(fieldPos.y());
    }

    if(geomOk)
    {
      consecutiveSeen++;
      const int minConsec = (theCameraInfo.camera == CameraInfo::upper)
                            ? upperMinConsecutive : lowerMinConsecutive;
      if(consecutiveSeen >= minConsec)
      {
        bp.status          = BallPercept::seen;
        bp.positionInImage = imgPos;
        bp.radiusInImage   = d.r;
        bp.positionOnField = fieldPos;
        bp.radiusOnField   = theBallSpecification.radius;
        bp.covarianceOnField = Matrix2f::Identity() * 10000.f;
      }
      else
      {
        bp.status = BallPercept::notSeen;
      }
    }
    else
    {
      consecutiveSeen = 0;
      bp.status = BallPercept::notSeen;
    }
  }
  else
  {
    consecutiveSeen = 0;
    bp.status = BallPercept::notSeen;
  }
}

void YoloBallBridge::update(RawBallPatch& rp)
{
  rp.valid = false;
  rp.data.clear();
}
