// Stubs for symbols referenced by textviewwindowcontroller.cpp but not
// needed by the static methods under test.  Linking the real ConfigMgr2 /
// CoreConfig would pull in deep cascading dependencies that are not
// available in the test harness.

#include <core/configmgr2.h>
#include <core/coreconfig.h>
#include <core/editorconfig.h>
#include <core/mainconfig.h>
#include <core/sessionconfig.h>

using namespace vnotex;

// --- ConfigMgr2 stubs ---

CoreConfig &ConfigMgr2::getCoreConfig() {
  static CoreConfig *s = nullptr;
  Q_ASSERT_X(false, "stub", "ConfigMgr2::getCoreConfig() called in test - not expected");
  return *s; // unreachable
}

EditorConfig &ConfigMgr2::getEditorConfig() {
  static EditorConfig *s = nullptr;
  Q_ASSERT_X(false, "stub", "ConfigMgr2::getEditorConfig() called in test - not expected");
  return *s; // unreachable
}

ConfigMgr2::~ConfigMgr2() = default;

void ConfigMgr2::updateMainConfig(const QJsonObject &) {}

void ConfigMgr2::updateSessionConfig(const QJsonObject &) {}

void ConfigMgr2::doWriteMainConfig() {}

void ConfigMgr2::doWriteSessionConfig() {}

// --- CoreConfig stub ---

const QString &CoreConfig::getShortcutLeaderKey() const {
  Q_ASSERT_X(false, "stub", "CoreConfig::getShortcutLeaderKey() called in test - not expected");
  static QString s;
  return s; // unreachable
}
