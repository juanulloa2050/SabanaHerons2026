#include "SimRobotHost.h"

#include "Platform/File.h"

#include <SimRobotCore2D.h>

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QLibrary>
#include <QSettings>
#include <QVector>

#include <memory>
#include <vector>

namespace
{
QString detectBHDir()
{
  QString bhDir;
#ifdef BHUMAN_PREFIX_PATH
  bhDir = QFileInfo(QStringLiteral(BHUMAN_PREFIX_PATH)).absoluteFilePath();
#else
  bhDir = QFileInfo(QString::fromUtf8(File::getBHDir())).absoluteFilePath();
#endif

  if(QFileInfo(bhDir + "/Config/Scenes").exists())
    return bhDir;

  const QString projectRoot = QFileInfo(QDir(bhDir).absoluteFilePath("../..")).absoluteFilePath();
  if(QFileInfo(projectRoot + "/Config/Scenes").exists())
    return projectRoot;

  return bhDir;
}
}

class HeadlessSimRobotApplication : public SimRobot::Application
{
public:
  HeadlessSimRobotApplication()
  {
    ensureQtApplication();
    appPath = QCoreApplication::applicationFilePath();
    if(appPath.isEmpty())
      appPath = detectBHDir();
    settings = std::make_unique<QSettings>("B-Human", "pybh-simrobot");
    layoutSettings = std::make_unique<QSettings>("B-Human", "pybh-simrobot-layout");
    moduleDir = findModuleDir();
  }

  ~HeadlessSimRobotApplication() override
  {
    unloadScene();
  }

  bool loadScene(const QString& sceneFile)
  {
    unloadScene();

    const QFileInfo fileInfo(sceneFile);
    if(!fileInfo.exists())
    {
      lastErrorMessage = "Scene file does not exist: " + sceneFile;
      return false;
    }

    const QString canonical = fileInfo.canonicalFilePath();
    filePath = canonical.isEmpty() ? fileInfo.absoluteFilePath() : canonical;
    compiled = false;
    running = false;
    lastErrorMessage.clear();
    lastWarningTitle.clear();
    lastWarningMessage.clear();

    const QString suffix = fileInfo.suffix().toLower();
    const QString coreModule = suffix == "ros2d" ? "SimRobotCore2D" : "SimRobotCore2";
    if(!loadModule(coreModule))
      return false;
    if(!compileModules())
      return false;

    running = true;
    return true;
  }

  void unloadScene()
  {
    running = false;
    compiled = false;
    objectsByName.clear();
    childrenByParent.clear();
    registeredModules.clear();
    statusLabels.clear();

    for(auto module = loadedModules.rbegin(); module != loadedModules.rend(); ++module)
      (*module).reset();
    loadedModules.clear();
    loadedModulesByName.clear();

    filePath.clear();
  }

  bool step(unsigned int steps)
  {
    if(!compiled && !compileModules())
      return false;

    for(unsigned int i = 0; i < steps; ++i)
      for(const auto& loadedModule : loadedModules)
        loadedModule->module->update();
    return true;
  }

  QString lastError() const
  {
    if(!lastWarningMessage.isEmpty())
      return lastWarningTitle.isEmpty() ? lastWarningMessage : lastWarningTitle + ": " + lastWarningMessage;
    return lastErrorMessage;
  }

  bool isLoaded() const
  {
    return !filePath.isEmpty() && compiled;
  }

  SimRobot::Object* getObject(const QString& fullName, int kind = 0) const
  {
    auto it = objectsByName.constFind(fullName);
    if(it == objectsByName.constEnd())
      return nullptr;
    SimRobot::Object* object = it.value();
    return kind == 0 || object->getKind() == kind ? object : nullptr;
  }

private:
  struct LoadedModule
  {
    using CreateModuleProc = SimRobot::Module* (*)(SimRobot::Application&);

    QString name;
    QLibrary library;
    SimRobot::Module* module = nullptr;
    bool compiled = false;

    LoadedModule(const QString& name, const QString& path) :
      name(name),
      library(path)
    {}

    ~LoadedModule()
    {
      delete module;
      if(library.isLoaded())
        library.unload();
    }
  };

  void ensureQtApplication()
  {
    if(QApplication::instance())
      return;

    if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
      qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));

    static int argc = 1;
    static char programName[] = "pybh-simrobot";
    static char* argv[] = {programName, nullptr};
    ownedApplication = std::make_unique<QApplication>(argc, argv);
  }

  QString findModuleDir() const
  {
    const QString bhDir = detectBHDir();
    const QStringList candidates{
      bhDir + "/Build/Linux/SimRobot/Develop",
      bhDir + "/Build/Linux/SimRobot/Release",
      QFileInfo(appPath).absolutePath()
    };

    for(const QString& candidate : candidates)
      if(QFileInfo(candidate + "/libSimRobotCore2D.so").exists())
        return candidate;

    return candidates.front();
  }

  QString modulePathFor(const QString& name) const
  {
#ifdef WINDOWS
    return moduleDir + "/" + name + ".dll";
#elif defined(MACOS)
    return moduleDir + "/lib" + name + ".dylib";
#else
    return moduleDir + "/lib" + name + ".so";
#endif
  }

  bool compileModules()
  {
    if(compiled)
      return true;

    bool success = true;
    for(std::size_t i = 0; i < loadedModules.size(); ++i)
    {
      LoadedModule& loadedModule = *loadedModules[i];
      if(!loadedModule.compiled)
      {
        loadedModule.compiled = loadedModule.module->compile();
        if(!loadedModule.compiled)
          success = false;
      }
    }

    if(!success)
    {
      if(lastErrorMessage.isEmpty())
        lastErrorMessage = "SimRobot module compilation failed.";
      return false;
    }

    compiled = true;
    for(const auto& loadedModule : loadedModules)
      loadedModule->module->link();
    return true;
  }

  bool registerObject(const SimRobot::Module&, SimRobot::Object& object, const SimRobot::Object* parent, int) override
  {
    objectsByName.insert(object.getFullName(), &object);
    childrenByParent[parent].append(&object);
    return true;
  }

  bool unregisterObject(const SimRobot::Object& object) override
  {
    objectsByName.remove(object.getFullName());
    childrenByParent.remove(&object);
    for(auto it = childrenByParent.begin(); it != childrenByParent.end(); ++it)
      it.value().removeOne(const_cast<SimRobot::Object*>(&object));
    return true;
  }

  SimRobot::Object* resolveObject(const QString& fullName, int kind = 0) override
  {
    return getObject(fullName, kind);
  }

  SimRobot::Object* resolveObject(const QVector<QString>& parts, const SimRobot::Object* parent = nullptr, int kind = 0) override
  {
    QString fullName = parent ? parent->getFullName() : QString();
    for(const QString& part : parts)
    {
      if(!fullName.isEmpty())
        fullName += '.';
      fullName += part;
    }
    return getObject(fullName, kind);
  }

  int getObjectChildCount(const SimRobot::Object& object) override
  {
    return static_cast<int>(childrenByParent.value(&object).size());
  }

  SimRobot::Object* getObjectChild(const SimRobot::Object& object, int index) override
  {
    const QVector<SimRobot::Object*>& children = childrenByParent[&object];
    return index >= 0 && index < children.size() ? children[index] : nullptr;
  }

  bool addStatusLabel(const SimRobot::Module&, SimRobot::StatusLabel* statusLabel) override
  {
    statusLabels.emplace_back(statusLabel);
    return true;
  }

  bool registerModule(const SimRobot::Module&, const QString& displayName, const QString& name) override
  {
    registeredModules.insert(name, displayName);
    return true;
  }

  bool loadModule(const QString& name) override
  {
    if(loadedModulesByName.contains(name))
      return true;

    const QString path = modulePathFor(name);
    auto loadedModule = std::make_unique<LoadedModule>(name, path);
    if(!loadedModule->library.load())
    {
      lastErrorMessage = loadedModule->library.errorString();
      return false;
    }

    auto createModule = reinterpret_cast<LoadedModule::CreateModuleProc>(loadedModule->library.resolve("createModule"));
    if(!createModule)
    {
      lastErrorMessage = "Could not resolve createModule in " + path;
      return false;
    }

    loadedModule->module = createModule(*this);
    if(!loadedModule->module)
    {
      lastErrorMessage = "createModule returned null for " + path;
      return false;
    }

    loadedModulesByName.insert(name, loadedModule.get());
    loadedModules.push_back(std::move(loadedModule));
    return true;
  }

  bool openObject(const SimRobot::Object&) override { return true; }
  bool closeObject(const SimRobot::Object&) override { return true; }

  bool selectObject(const SimRobot::Object& object) override
  {
    for(const auto& loadedModule : loadedModules)
      loadedModule->module->selectedObject(object);
    return true;
  }

  void showWarning(const QString& title, const QString& message) override
  {
    lastWarningTitle = title;
    lastWarningMessage = message;
  }

  void setStatusMessage(const QString& message) override
  {
    statusMessage = message;
  }

  const QString& getFilePath() const override { return filePath; }
  const QString& getAppPath() const override { return appPath; }
  QSettings& getSettings() override { return *settings; }
  QSettings& getLayoutSettings() override { return *layoutSettings; }
  bool isSimRunning() override { return running; }
  void simReset() override
  {
    if(!filePath.isEmpty())
      loadScene(filePath);
  }
  void simStart() override { running = true; }
  void simStep() override { step(1); }
  void simStop() override { running = false; }

  std::unique_ptr<QApplication> ownedApplication;
  std::unique_ptr<QSettings> settings;
  std::unique_ptr<QSettings> layoutSettings;
  QString appPath;
  QString filePath;
  QString moduleDir;
  QString statusMessage;
  QString lastErrorMessage;
  QString lastWarningTitle;
  QString lastWarningMessage;
  bool compiled = false;
  bool running = false;
  std::vector<std::unique_ptr<LoadedModule>> loadedModules;
  QHash<QString, LoadedModule*> loadedModulesByName;
  QHash<QString, QString> registeredModules;
  QHash<QString, SimRobot::Object*> objectsByName;
  QHash<const SimRobot::Object*, QVector<SimRobot::Object*>> childrenByParent;
  std::vector<std::unique_ptr<SimRobot::StatusLabel>> statusLabels;
};

SimRobotHost& SimRobotHost::instance()
{
  static SimRobotHost host;
  return host;
}

SimRobotHost::SimRobotHost() :
  app(std::make_unique<HeadlessSimRobotApplication>())
{}

SimRobotHost::~SimRobotHost() = default;

bool SimRobotHost::loadScene(const std::string& scenePath)
{
  return app->loadScene(QString::fromStdString(scenePath));
}

void SimRobotHost::unloadScene()
{
  app->unloadScene();
}

bool SimRobotHost::step(unsigned int steps)
{
  return app->step(steps);
}

bool SimRobotHost::isLoaded() const
{
  return app->isLoaded();
}

std::string SimRobotHost::lastError() const
{
  return app->lastError().toStdString();
}

bool SimRobotHost::getRobotPose(const std::string& robotName, float& x, float& y, float& theta) const
{
  const QString fullName = "RoboCup.robots." + QString::fromStdString(robotName);
  auto* body = dynamic_cast<SimRobotCore2D::Body*>(app->getObject(fullName, SimRobotCore2D::body));
  if(!body)
    return false;

  float position[2];
  body->getPose(position, &theta);
  x = position[0];
  y = position[1];
  return true;
}

bool SimRobotHost::setRobotPose(const std::string& robotName, float x, float y, float theta)
{
  const QString fullName = "RoboCup.robots." + QString::fromStdString(robotName);
  auto* body = dynamic_cast<SimRobotCore2D::Body*>(app->getObject(fullName, SimRobotCore2D::body));
  if(!body)
    return false;

  const float position[2] = {x, y};
  body->move(position, theta);
  body->resetDynamics();
  return true;
}

bool SimRobotHost::getBallState(float& x, float& y, float& vx, float& vy) const
{
  auto* body = dynamic_cast<SimRobotCore2D::Body*>(app->getObject("RoboCup.balls.ball", SimRobotCore2D::body));
  if(!body)
    return false;

  float position[2];
  float linear[2];
  float theta = 0.f;
  body->getPose(position, &theta);
  body->getVelocity(linear);
  x = position[0];
  y = position[1];
  vx = linear[0];
  vy = linear[1];
  return true;
}

bool SimRobotHost::setBallState(float x, float y, float vx, float vy)
{
  auto* body = dynamic_cast<SimRobotCore2D::Body*>(app->getObject("RoboCup.balls.ball", SimRobotCore2D::body));
  if(!body)
    return false;

  const float position[2] = {x, y};
  const float velocity[2] = {vx, vy};
  body->move(position, 0.f);
  body->resetDynamics();
  body->setVelocity(velocity);
  return true;
}
