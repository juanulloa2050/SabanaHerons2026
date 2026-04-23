#pragma once

#include <SimRobot.h>
#include <QString>
#include <memory>
#include <string>

class HeadlessSimRobotApplication;

class SimRobotHost
{
public:
  static SimRobotHost& instance();

  bool loadScene(const std::string& scenePath);
  void unloadScene();
  bool step(unsigned int steps = 1);

  bool isLoaded() const;
  std::string lastError() const;

  bool getRobotPose(const std::string& robotName, float& x, float& y, float& theta) const;
  bool setRobotPose(const std::string& robotName, float x, float y, float theta);

  bool getBallState(float& x, float& y, float& vx, float& vy) const;
  bool setBallState(float x, float y, float vx, float vy);

private:
  SimRobotHost();
  ~SimRobotHost();

  SimRobotHost(const SimRobotHost&) = delete;
  SimRobotHost& operator=(const SimRobotHost&) = delete;

  std::unique_ptr<HeadlessSimRobotApplication> app;
};
