/**
 * @file SettingsArea.cpp
 *
 * This file implements a class that represents the settings area on the right side
 * of the dialog. It consists of two tab groups and a few buttons.
 *
 * @author Thomas Röfer
 */

#include "SettingsArea.h"
#include <iostream>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QEvent>
#include <QFormLayout>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QRadioButton>
#include <QRegularExpression>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QToolTip>
#include "Streaming/InStreams.h"
#include "../../Util/SimRobot/Src/SimRobot/Theme.h"

/** A horizontal line to separate different rows in the preset tabs. */
class Line : public QFrame
{
  void updateColor(QWidget* widget)
  {
    QPalette pal = palette();
    QColor base = pal.dark().color();
    pal.setColor(QPalette::WindowText, Theme::isDarkMode(widget) ? base.darker() : base);
    setPalette(pal);
  }

public:
  Line(QWidget* parent)
  {
    setFrameShape(QFrame::HLine);
    updateColor(parent);
  }

  void changeEvent(QEvent* event) override
  {
    if(event->type() == QEvent::PaletteChange)
      updateColor(this);
    QFrame::changeEvent(event);
  }
};

/** A line editor to change the names of tabs. Mainly specializes leaving the editor. */
class LineEdit : public QLineEdit
{
  const QString originalText;

public:
  LineEdit(const QString& text) : QLineEdit(text), originalText(text)
  {
    installEventFilter(this);
  }

  bool eventFilter(QObject* source, QEvent* event)
  {
    if(event->type() == QEvent::KeyPress && source == this)
    {
      QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
      if(keyEvent->modifiers() == Qt::NoModifier
         && (keyEvent->key() == Qt::Key_Return
             || keyEvent->key() == Qt::Key_Enter
             || keyEvent->key() == Qt::Key_Escape))
      {
        if(keyEvent->key() == Qt::Key_Escape)
          setText(originalText);
        event->accept();
        emit editingFinished();
        return true;
      }
    }
    else if(event->type() == QEvent::FocusOut)
      emit editingFinished();
    return QLineEdit::eventFilter(source, event);
  }
};

namespace
{
QString readStrategyBehaviorControlValue(const std::string& scenario, const char* key)
{
  QFile file(QString("Scenarios/%1/strategyBehaviorControl.cfg").arg(scenario.c_str()));
  if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return {};

  const QString content = QString::fromUtf8(file.readAll());
  const QRegularExpression lineRegex(QString(R"(^\s*%1\s*=\s*(.+?)\s*;\s*$)").arg(QRegularExpression::escape(key)),
                                     QRegularExpression::MultilineOption);
  const QRegularExpressionMatch match = lineRegex.match(content);
  return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

std::vector<int> parsePlayerList(const QString& value)
{
  std::vector<int> players;
  const QString trimmed = value.trimmed();
  if(!trimmed.startsWith('[') || !trimmed.endsWith(']'))
    return players;

  const QString inner = trimmed.mid(1, trimmed.size() - 2).trimmed();
  if(inner.isEmpty())
    return players;

  for(const QString& token : inner.split(',', Qt::SkipEmptyParts))
  {
    bool ok = false;
    const int number = token.trimmed().toInt(&ok);
    if(ok)
      players.push_back(number);
  }
  return players;
}

QString normalizeEmbeddedModelPath(QString path)
{
  path = path.trimmed();
  if(path.startsWith('"') && path.endsWith('"') && path.size() >= 2)
    path = path.mid(1, path.size() - 2);
  return path;
}

QString displayNameForRLModelPath(const QString& path)
{
  const QString normalized = normalizeEmbeddedModelPath(path);
  if(normalized.isEmpty())
    return "Not used";

  const QString fileName = QFileInfo(normalized).fileName();
  if(fileName == "ppo_team_hsl2026_v4_2.onnx")
    return "Team attack v4.2";
  if(fileName == "ppo_striker_hsl2026.onnx")
    return "Attack legacy";
  if(fileName == "ppo_defender_hsl2026.onnx")
    return "Defense baseline";
  if(fileName == "ppo_defender_hsl2026_param_repair.onnx")
    return "Defense repaired";
  return fileName;
}

void ensureRLConfigInitialized(Presets::Preset* preset)
{
  if(preset->rlModes.size() == preset->players.size())
  {
    // already sized correctly
  }
  else
    preset->rlModes.assign(preset->players.size(), "off");

  const QString defaultStrikerModel = normalizeEmbeddedModelPath(readStrategyBehaviorControlValue(preset->scenario, "embeddedPPOStrikerModelPath"));
  const QString defaultTeamStrikerModel = normalizeEmbeddedModelPath(readStrategyBehaviorControlValue(preset->scenario, "embeddedPPOTeamStrikerModelPath"));
  const QString defaultDefenderModel = normalizeEmbeddedModelPath(readStrategyBehaviorControlValue(preset->scenario, "embeddedPPODefenderModelPath"));
  if(preset->rlStrikerModel.empty())
    preset->rlStrikerModel = defaultStrikerModel.toStdString();
  if(preset->rlTeamStrikerModel.empty() && !defaultTeamStrikerModel.isNull())
    preset->rlTeamStrikerModel = defaultTeamStrikerModel.toStdString();
  if(preset->rlDefenderModel.empty())
    preset->rlDefenderModel = defaultDefenderModel.toStdString();

  const QString enabled = readStrategyBehaviorControlValue(preset->scenario, "enableEmbeddedPPO");
  if(enabled == "true")
  {
    const auto strikerPlayers = parsePlayerList(readStrategyBehaviorControlValue(preset->scenario, "embeddedPPOPlayers"));
    const auto defenderPlayers = parsePlayerList(readStrategyBehaviorControlValue(preset->scenario, "embeddedPPODefenderPlayers"));
    for(const int player : strikerPlayers)
      if(player >= 1 && static_cast<size_t>(player) <= preset->rlModes.size())
        preset->rlModes[static_cast<size_t>(player - 1)] = "striker";
    for(const int player : defenderPlayers)
      if(player >= 1 && static_cast<size_t>(player) <= preset->rlModes.size())
        preset->rlModes[static_cast<size_t>(player - 1)] = "defender";

    if(!strikerPlayers.empty() || !defenderPlayers.empty())
      return;

    const QString dynamicPlayBall = readStrategyBehaviorControlValue(preset->scenario, "embeddedPPODynamicPlayBall");
    if(dynamicPlayBall == "true")
      return;

    const QString configuredRole = readStrategyBehaviorControlValue(preset->scenario, "embeddedPPORole").remove('"');
    if(configuredRole != "striker" && configuredRole != "defender")
      return;

    for(size_t i = 1; i < preset->players.size(); ++i)
      if(preset->players[i] != "_")
        preset->rlModes[i] = configuredRole.toStdString();
  }
}
}

SettingsArea::SettingsArea(Presets& presets, QDialog* dialog, RobotsTable* table, const QSettings& settings)
  : presets(presets), table(table)
{
  presetIndex = settings.value("presetIndex", 0).toInt();
  mode = static_cast<Mode>(settings.value("mode", robots).toInt());
  restart = settings.value("restart", true).toBool();
  deleteLogs = settings.value("deleteLogs", false).toBool();
  playerNumber = settings.value("playerNumber", 5).toInt();
  reboot = settings.value("reboot", false).toBool();
  usbCheck = settings.value("usbCheck", false).toBool();
  date = settings.value("date", false).toBool();
  logsMode = static_cast<LogsMode>(settings.value("logsMode", download).toInt());
  close = settings.value("close", true).toBool();

  // Read known teams.
  Teams teams;
  InMapFile teamsStream("teamList.cfg");
  if(teamsStream.exists())
    teamsStream >> teams;
  for(const Teams::Team& team : teams.teams)
    if(team.number)
      this->teams[team.name] = team.number;

  const QStringList rlModels = QDir("NeuralNets/RLPolicy").entryList(QStringList() << "*.onnx", QDir::Files, QDir::Name);
  for(const QString& rlModel : rlModels)
    rlModelPaths << QString("Config/NeuralNets/RLPolicy/%1").arg(rlModel);

  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->addWidget(createPresetTabs());

  QPushButton* deployButton = new QPushButton();
  deployButton->setFocusPolicy(Qt::StrongFocus);
  deployButton->setDefault(true);
  connect(deployButton, &QPushButton::clicked, dialog, &QDialog::accept);
  auto updateDeployButton = [=]
  {
    deployButton->setText(mode == robots ? "Deploy" : mode == image ? "Write" : logsMode == justDelete ? "Delete" : "Download");
    if(mode == image)
      deployButton->setEnabled(true);
    else
    {
      for(const std::string& player : selectedPreset->players)
        if(player != "_")
        {
          deployButton->setEnabled(true);
          return;
        }
      deployButton->setEnabled(false);
    }
  };
  updateDeployButton();
  connect(table, &RobotsTable::robotAssignmentChanged, updateDeployButton);

  QPushButton* cancelButton = new QPushButton("Cancel");
  cancelButton->setFocusPolicy(Qt::StrongFocus);
  deployButton->setDefault(false);
  connect(cancelButton, &QPushButton::clicked, dialog, &QDialog::reject);

  QTabWidget* modesWidget = new QTabWidget();
  layout->addWidget(modesWidget);
  modesWidget->addTab(createRobotsTab(), "Robots");
  modesWidget->addTab(createImageTab(), "Image");
  modesWidget->addTab(createLogsTab(updateDeployButton), "Logs");
  auto selectMode = [=](int index)
  {
    mode = static_cast<Mode>(index);
    updateDeployButton();
  };
  connect(modesWidget, &QTabWidget::currentChanged, selectMode);
  modesWidget->setCurrentIndex(mode);

  layout->addStretch();

  QGridLayout* buttonsLayout = new QGridLayout();
  layout->addLayout(buttonsLayout, Qt::AlignBottom);

  QCheckBox* closeButton = new QCheckBox("Close");
  closeButton->setChecked(close);
  closeButton->setFocusPolicy(Qt::StrongFocus);
  connect(closeButton, &QCheckBox::stateChanged, [&](int state) {close = state != Qt::Unchecked;});
  buttonsLayout->addWidget(closeButton);
  buttonsLayout->setColumnStretch(0, 2);

#ifdef MACOS
  constexpr int deployColumn = 2;
#else
  const int deployColumn = 1;
#endif
  buttonsLayout->addWidget(deployButton, 0, deployColumn);
  buttonsLayout->addWidget(cancelButton, 0, 3 - deployColumn);
  buttonsLayout->setColumnStretch(1, 3);
  buttonsLayout->setColumnStretch(2, 3);
}

QWidget* SettingsArea::createPresetTabs()
{
  QTabWidget* widget = new QTabWidget();
  widget->setUsesScrollButtons(true);

  for(Presets::Preset* preset : presets.teams)
    widget->addTab(createPresetTab(preset), preset->name.c_str());

  auto selectPreset = [=](int index)
  {
    presetIndex = index;
    selectedPreset = presets.teams[index];
    table->setSelectedPreset(selectedPreset, index);
  };
  QTabBar* tabBar = widget->tabBar();
  tabBar->setMovable(true);
  tabBar->setContextMenuPolicy(Qt::CustomContextMenu);

#ifdef MACOS
    tabBar->setStyleSheet("QToolButton {background-color: transparent; padding: 0 0 2 0; border 0; border-radius: 4}"
                          "QToolButton:hover {background-color: rgba(128, 128, 128, 64)}");
#endif

  connect(widget->tabBar(), &QTabBar::tabMoved, [=](int from, int to)
  {
    Presets::Preset* temp = presets.teams[from];
    presets.teams.erase(presets.teams.begin() + from);
    presets.teams.insert(presets.teams.begin() + to, temp);
    table->movePreset(from, to);
    selectPreset(widget->currentIndex());
  });

  connect(widget->tabBar(), &QTabBar::customContextMenuRequested, [=](const QPoint& point)
  {
    const int index = tabBar->tabAt(point);

    auto editName = [=](int index)
    {
      LineEdit* lineEdit = new LineEdit(tabBar->tabText(index));
      tabBar->setTabText(index, "");
      const QString prevStyleSheet = tabBar->styleSheet();
      tabBar->setStyleSheet(prevStyleSheet + "::tab:selected {padding: 0 0 0 0}");
      tabBar->setTabButton(index, QTabBar::RightSide, lineEdit);
      Qt::FocusPolicy policy = tabBar->focusPolicy();
      tabBar->setFocusPolicy(Qt::NoFocus);
      lineEdit->selectAll();
      lineEdit->setFocus(Qt::OtherFocusReason);
      connect(lineEdit, &QLineEdit::editingFinished, [=]
      {
        tabBar->setTabText(index, lineEdit->text());
        presets.teams[index]->name = lineEdit->text().toStdString();
        tabBar->setTabButton(index, QTabBar::RightSide, nullptr);
        tabBar->setStyleSheet(prevStyleSheet);
        tabBar->setFocusPolicy(policy);
      });
    };

    QMenu menu("Team", this);

    QAction* duplicate = new QAction("&Duplicate", this);
    connect(duplicate, &QAction::triggered, [=]
    {
      presets.teams.emplace_back(new Presets::Preset(*presets.teams[index]));
      table->addPreset(static_cast<int>(presets.teams.back()->players.size()));
      widget->addTab(createPresetTab(presets.teams.back()), presets.teams.back()->name.c_str());
      widget->setCurrentIndex(tabBar->count() - 1);
      editName(tabBar->count() - 1);
    });
    menu.addAction(duplicate);

    QAction* remove = new QAction("&Delete", this);
    remove->setEnabled(tabBar->count() > 1);
    connect(remove, &QAction::triggered, [=]
    {
      table->removePreset(index);
      delete presets.teams[index];
      presets.teams.erase(presets.teams.begin() + index);
      widget->setCurrentIndex(std::max(index, widget->count() - 2));
      widget->removeTab(index);
    });
    menu.addAction(remove);

    QAction* rename = new QAction("&Rename", this);
    connect(rename, &QAction::triggered, [=]
    {
      widget->setCurrentIndex(index);
      editName(index);
    });
    menu.addAction(rename);

    menu.exec(widget->tabBar()->mapToGlobal(point));
  });

  if(presetIndex >= static_cast<int>(presets.teams.size()))
    presetIndex = static_cast<int>(presets.teams.size()) - 1;
  widget->setCurrentIndex(presetIndex);
  selectPreset(presetIndex);
  connect(widget, &QTabWidget::currentChanged, selectPreset);

  return widget;
}

QWidget* SettingsArea::createPresetTab(Presets::Preset* preset)
{
  ensureRLConfigInitialized(preset);

  QWidget* widget = new QWidget();
  QFormLayout* layout = new QFormLayout(widget);

  QComboBox* teamSelector = new QComboBox();
  teamSelector->setFocusPolicy(Qt::StrongFocus);
  for(const auto& [team, number] : this->teams)
  {
    teamSelector->addItem(team.c_str());
    if(preset->number == number)
      teamSelector->setCurrentIndex(teamSelector->count() - 1);
  }
  teamSelector->setMaximumWidth(settingsFieldWidth);
  connect(teamSelector, &QComboBox::currentTextChanged, this, [=](const QString& team) {preset->number = this->teams[team.toStdString()];});
  layout->addRow("Team", teamSelector);

  const QStringList colors = {"black", "blue", "brown", "gray", "green", "orange", "purple", "red", "white", "yellow"};
  QComboBox* fieldPlayerColorSelector = new QComboBox();
  fieldPlayerColorSelector->setFocusPolicy(Qt::StrongFocus);
  fieldPlayerColorSelector->addItems(colors);
  fieldPlayerColorSelector->setCurrentText(preset->fieldPlayerColor.c_str());
  fieldPlayerColorSelector->setMaximumWidth(settingsFieldWidth);
  connect(fieldPlayerColorSelector, &QComboBox::currentTextChanged, this, [=](const QString& color) {preset->fieldPlayerColor = color.toStdString();});
  layout->addRow("Field player color", fieldPlayerColorSelector);

  QComboBox* goalkeeperColorSelector = new QComboBox();
  goalkeeperColorSelector->setFocusPolicy(Qt::StrongFocus);
  goalkeeperColorSelector->addItems(colors);
  goalkeeperColorSelector->setCurrentText(preset->goalkeeperColor.c_str());
  goalkeeperColorSelector->setMaximumWidth(settingsFieldWidth);
  connect(goalkeeperColorSelector, &QComboBox::currentTextChanged, this, [=](const QString& color) {preset->goalkeeperColor = color.toStdString();});
  layout->addRow("Goalkeeper color", goalkeeperColorSelector);

  layout->addRow(new Line(this));

  const QStringList scenarios = QDir("Scenarios").entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
  QComboBox* scenarioSelector = new QComboBox();
  scenarioSelector->setFocusPolicy(Qt::StrongFocus);
  scenarioSelector->addItems(scenarios);
  scenarioSelector->setCurrentText(preset->scenario.c_str());
  scenarioSelector->setMaximumWidth(settingsFieldWidth);
  connect(scenarioSelector, &QComboBox::currentTextChanged, this, [=](const QString& scenario) {preset->scenario = scenario.toStdString();});
  layout->addRow("Scenario", scenarioSelector);

  const QStringList locations = QDir("Locations").entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
  QComboBox* locationSelector = new QComboBox();
  locationSelector->setFocusPolicy(Qt::StrongFocus);
  locationSelector->addItems(locations);
  locationSelector->setCurrentText(preset->location.c_str());
  locationSelector->setMaximumWidth(settingsFieldWidth);
  connect(locationSelector, &QComboBox::currentTextChanged, this, [=](const QString& location) {preset->location = location.toStdString();});
  layout->addRow("Location", locationSelector);

  QSpinBox* magicNumberSelector = new QSpinBox();
  magicNumberSelector->setRange(-1, 255);
  magicNumberSelector->setSpecialValueText("auto");
  magicNumberSelector->setValue(preset->magicNumber);
#ifdef LINUX
  magicNumberSelector->setFixedWidth(55);
#else
  magicNumberSelector->setFixedWidth(50);
#endif
  connect(magicNumberSelector, &QSpinBox::valueChanged, this, [=](int magicNumber) {preset->magicNumber = magicNumber;});
  layout->addRow("Magic number", magicNumberSelector);

  layout->addRow(new Line(this));

  const QStringList profiles = QDir("../Install/Profiles").entryList(QDir::Files, QDir::Name);
  QComboBox* profileSelector = new QComboBox();
  profileSelector->setFocusPolicy(Qt::StrongFocus);
  profileSelector->addItems(profiles);
  profileSelector->setCurrentText(preset->wlanConfig.c_str());
  profileSelector->setMaximumWidth(settingsFieldWidth);
  connect(profileSelector, &QComboBox::currentTextChanged, this, [=](const QString& profile) {preset->wlanConfig = profile.toStdString();});
  layout->addRow("Wireless profile", profileSelector);

  QSlider* volumeSelector = new QSlider(Qt::Horizontal);
  volumeSelector->setFocusPolicy(Qt::StrongFocus);
  volumeSelector->setRange(0, 100);
  volumeSelector->setValue(preset->volume);
  volumeSelector->setFixedWidth(settingsFieldWidth);
  connect(volumeSelector, &QSlider::sliderMoved, [&](int volume) {QToolTip::showText(QCursor::pos(), QString("%1").arg(volume));});
  connect(volumeSelector, &QSlider::valueChanged, [=](int volume) {preset->volume = volume;});
  layout->addRow("Volume", volumeSelector);

  ensureRLConfigInitialized(preset);

  bool hasAssignedPlayers = false;
  for(const std::string& player : preset->players)
    if(player != "_")
    {
      hasAssignedPlayers = true;
      break;
    }

  if(hasAssignedPlayers)
  {
    layout->addRow(new Line(this));

    if(!rlModelPaths.isEmpty())
    {
      QComboBox* strikerModelSelector = new QComboBox();
      strikerModelSelector->setFocusPolicy(Qt::StrongFocus);
      for(const QString& rlModelPath : rlModelPaths)
        strikerModelSelector->addItem(displayNameForRLModelPath(rlModelPath), rlModelPath);
      strikerModelSelector->setEditable(true);
      strikerModelSelector->setCurrentText(displayNameForRLModelPath(preset->rlStrikerModel.c_str()));
      strikerModelSelector->setMaximumWidth(settingsFieldWidth * 2);
      connect(strikerModelSelector, &QComboBox::currentTextChanged, this, [=](const QString& model)
      {
        const int index = strikerModelSelector->currentIndex();
        preset->rlStrikerModel = (index >= 0 ? strikerModelSelector->itemData(index).toString() : model).toStdString();
      });
      layout->addRow("Attack style", strikerModelSelector);

      QComboBox* teamStrikerModelSelector = new QComboBox();
      teamStrikerModelSelector->setFocusPolicy(Qt::StrongFocus);
      teamStrikerModelSelector->addItem("Not used", "");
      for(const QString& rlModelPath : rlModelPaths)
        teamStrikerModelSelector->addItem(displayNameForRLModelPath(rlModelPath), rlModelPath);
      teamStrikerModelSelector->setEditable(true);
      const QString currentTeamStrikerModel = preset->rlTeamStrikerModel.empty() ? "Not used" : displayNameForRLModelPath(QString::fromStdString(preset->rlTeamStrikerModel));
      teamStrikerModelSelector->setCurrentText(currentTeamStrikerModel);
      teamStrikerModelSelector->setMaximumWidth(settingsFieldWidth * 2);
      connect(teamStrikerModelSelector, &QComboBox::currentTextChanged, this, [=](const QString& model)
      {
        const int index = teamStrikerModelSelector->currentIndex();
        preset->rlTeamStrikerModel = (index >= 0 ? teamStrikerModelSelector->itemData(index).toString() : model).toStdString();
      });
      layout->addRow("Team attack style", teamStrikerModelSelector);

      QComboBox* defenderModelSelector = new QComboBox();
      defenderModelSelector->setFocusPolicy(Qt::StrongFocus);
      for(const QString& rlModelPath : rlModelPaths)
        defenderModelSelector->addItem(displayNameForRLModelPath(rlModelPath), rlModelPath);
      defenderModelSelector->setEditable(true);
      defenderModelSelector->setCurrentText(displayNameForRLModelPath(preset->rlDefenderModel.c_str()));
      defenderModelSelector->setMaximumWidth(settingsFieldWidth * 2);
      connect(defenderModelSelector, &QComboBox::currentTextChanged, this, [=](const QString& model)
      {
        const int index = defenderModelSelector->currentIndex();
        preset->rlDefenderModel = (index >= 0 ? defenderModelSelector->itemData(index).toString() : model).toStdString();
      });
      layout->addRow("Defense style", defenderModelSelector);
    }

    const QStringList rlModes = {"Normal", "Attack", "Defense"};
    for(size_t i = 0; i < preset->players.size(); ++i)
    {
      if(preset->players[i] == "_")
        continue;

      QComboBox* rlModeSelector = new QComboBox();
      rlModeSelector->setFocusPolicy(Qt::StrongFocus);
      rlModeSelector->addItems(rlModes);
      rlModeSelector->setCurrentText(preset->rlModes[i] == "striker" ? "Attack" :
                                     preset->rlModes[i] == "defender" ? "Defense" : "Normal");
      rlModeSelector->setMaximumWidth(settingsFieldWidth);
      connect(rlModeSelector, &QComboBox::currentTextChanged, this, [=](const QString& mode)
      {
        preset->rlModes[i] = mode == "Attack" ? "striker" : mode == "Defense" ? "defender" : "off";
      });
      layout->addRow(QString("Player %1 role (%2)").arg(i + 1).arg(preset->players[i].c_str()), rlModeSelector);
    }
  }

  return widget;
}

QWidget* SettingsArea::createRobotsTab()
{
  QWidget* widget = new QWidget();
  QVBoxLayout* layout = new QVBoxLayout(widget);

  QCheckBox* restartSelector = new QCheckBox("Restart bhuman");
  restartSelector->setChecked(restart);
  restartSelector->setFocusPolicy(Qt::StrongFocus);
  connect(restartSelector, &QCheckBox::stateChanged, [&](int state) {restart = state != Qt::Unchecked;});
  layout->addWidget(restartSelector);

  QCheckBox* deleteLogsSelector = new QCheckBox("Delete logs on internal drive");
  deleteLogsSelector->setChecked(deleteLogs);
  deleteLogsSelector->setFocusPolicy(Qt::StrongFocus);
  connect(deleteLogsSelector, &QCheckBox::stateChanged, [&](int state) {deleteLogs = state != Qt::Unchecked;});
  layout->addWidget(deleteLogsSelector);

  return widget;
}

QWidget* SettingsArea::createImageTab()
{
  QWidget* widget = new QWidget();
  QGridLayout* layout = new QGridLayout(widget);

  QFormLayout* playerNumberLayout = new QFormLayout();
  playerNumberLayout->setFormAlignment(Qt::AlignLeft);
  layout->addLayout(playerNumberLayout, 0, 0);
  QSpinBox* playerNumberSelector = new QSpinBox();
  playerNumberSelector->setRange(1, presets.teams.empty() ? 7 : static_cast<int>(presets.teams[0]->players.size()));
  playerNumberSelector->setValue(playerNumber);
  connect(playerNumberSelector, &QSpinBox::valueChanged, this, [&](int number) {playerNumber = number;});
  playerNumberLayout->addRow("Player", playerNumberSelector);

  QCheckBox* usbCheckSelector = new QCheckBox("USB check");
  usbCheckSelector->setChecked(usbCheck);
  usbCheckSelector->setFocusPolicy(Qt::StrongFocus);
  connect(usbCheckSelector, &QCheckBox::stateChanged, [&](int state) {usbCheck = state != Qt::Unchecked;});
  layout->addWidget(usbCheckSelector, 0, 1);

  QCheckBox* dateSelector = new QCheckBox("Add date");
  dateSelector->setChecked(date);
  dateSelector->setFocusPolicy(Qt::StrongFocus);
  connect(dateSelector, &QCheckBox::stateChanged, [&](int state) {date = state != Qt::Unchecked;});
  layout->addWidget(dateSelector, 1, 0);

  QCheckBox* rebootSelector = new QCheckBox("Reboot");
  rebootSelector->setChecked(reboot);
  rebootSelector->setFocusPolicy(Qt::StrongFocus);
  connect(rebootSelector, &QCheckBox::stateChanged, [&](int state) {reboot = state != Qt::Unchecked;});
  layout->addWidget(rebootSelector, 1, 1);

  return widget;
}

QWidget* SettingsArea::createLogsTab(const std::function<void()>& updateDeployButton)
{
  QWidget* widget = new QWidget();
  QVBoxLayout* layout = new QVBoxLayout(widget);
  QHBoxLayout* rowLayout = new QHBoxLayout();
  layout->addLayout(rowLayout);

  QRadioButton* downloadSelector = new QRadioButton("Download");
  downloadSelector->setChecked(logsMode == download);
  downloadSelector->setFocusPolicy(Qt::StrongFocus);
  connect(downloadSelector, &QRadioButton::toggled, [=](bool checked) {if(checked) logsMode = download; updateDeployButton();});
  rowLayout->addWidget(downloadSelector);

  QRadioButton* deleteSelector = new QRadioButton("Delete");
  deleteSelector->setChecked(logsMode == justDelete);
  deleteSelector->setFocusPolicy(Qt::StrongFocus);
  connect(deleteSelector, &QRadioButton::toggled, [=](bool checked) {if(checked) logsMode = justDelete; updateDeployButton();});
  rowLayout->addWidget(deleteSelector);

  // Hack: Without this, the buttons are not aligned correctly
  rowLayout = new QHBoxLayout();
  layout->addLayout(rowLayout);

  QRadioButton* downloadAndDeleteSelector = new QRadioButton("Download and delete");
  downloadAndDeleteSelector->setChecked(logsMode == downloadAndDelete);
  downloadAndDeleteSelector->setFocusPolicy(Qt::StrongFocus);
  connect(downloadAndDeleteSelector, &QRadioButton::toggled, [=](bool checked) {if(checked) logsMode = downloadAndDelete; updateDeployButton();});
  rowLayout->addWidget(downloadAndDeleteSelector);

  return widget;
}

void SettingsArea::writeOutput(std::map<std::string, Robot>& robots, std::ostream& stream) const
{
  if(mode == logs)
  {
    stream << "logs ";
    table->writeOutput(robots, true, stream);
    if(logsMode == downloadAndDelete)
      stream << "-d";
    else if(logsMode == justDelete)
      stream << "-D";
    stream << std::endl;
    return;
  }
  else if(mode == image)
    stream << "-i -p " << playerNumber << " ";
  else
    table->writeOutput(robots, false, stream);

  std::vector<int> strikerPlayers;
  std::vector<int> defenderPlayers;
  ensureRLConfigInitialized(selectedPreset);
  for(size_t i = 0; i < selectedPreset->rlModes.size(); ++i)
  {
    if(selectedPreset->rlModes[i] == "striker")
      strikerPlayers.push_back(static_cast<int>(i + 1));
    else if(selectedPreset->rlModes[i] == "defender")
      defenderPlayers.push_back(static_cast<int>(i + 1));
  }

  stream <<  "-t " << selectedPreset->number
         << " -c " << selectedPreset->fieldPlayerColor
         << " -g " << selectedPreset->goalkeeperColor
         << " -s " << selectedPreset->scenario
         << " -l " << selectedPreset->location
         << " -m " << selectedPreset->magicNumber
         << " -w " << selectedPreset->wlanConfig
         << " -v " << selectedPreset->volume;

  if(strikerPlayers.empty() && defenderPlayers.empty())
    stream << " --rl-disable";
  else
  {
    auto writePlayerList = [&stream](const char* option, const std::vector<int>& players)
    {
      if(players.empty())
        return;
      stream << ' ' << option << ' ';
      for(size_t i = 0; i < players.size(); ++i)
      {
        if(i)
          stream << ',';
        stream << players[i];
      }
    };

    writePlayerList("--rl-striker", strikerPlayers);
    writePlayerList("--rl-defender", defenderPlayers);
  }

  if(!selectedPreset->rlStrikerModel.empty())
    stream << " --rl-striker-model " << selectedPreset->rlStrikerModel;
  stream << " --rl-team-striker-model " << (selectedPreset->rlTeamStrikerModel.empty() ? "\"\"" : selectedPreset->rlTeamStrikerModel);
  if(!selectedPreset->rlDefenderModel.empty())
    stream << " --rl-defender-model " << selectedPreset->rlDefenderModel;

  if(mode == image)
  {
    if(usbCheck)
      stream << " -u";
    if(date)
      stream << " -d";
    if(reboot)
      stream << " -b";
  }
  else
  {
    if(deleteLogs)
      stream << " -d";
    if(restart)
      stream << " -b";
  }
  stream << std::endl;
}

bool SettingsArea::modified(const QSettings& settings) const
{
  return presetIndex != std::min(static_cast<int>(presets.teams.size()) - 1, settings.value("presetIndex", 0).toInt())
         || mode != settings.value("mode", false).toInt()
         || restart != settings.value("restart", true).toBool()
         || deleteLogs != settings.value("deleteLogs", false).toBool()
         || playerNumber != settings.value("playerNumber", 5).toInt()
         || reboot != settings.value("reboot", false).toBool()
         || usbCheck != settings.value("usbCheck", false).toBool()
         || date != settings.value("date", false).toBool()
         || logsMode != settings.value("logsMode", download).toInt()
         || close != settings.value("close", true).toBool();
}

bool SettingsArea::save(QSettings& settings) const
{
  settings.setValue("presetIndex", presetIndex);
  settings.setValue("mode", mode);
  settings.setValue("restart", restart);
  settings.setValue("deleteLogs", deleteLogs);
  settings.setValue("playerNumber", playerNumber);
  settings.setValue("reboot", reboot);
  settings.setValue("usbCheck", usbCheck);
  settings.setValue("date", date);
  settings.setValue("logsMode", logsMode);
  settings.setValue("close", close);
  return close;
}
