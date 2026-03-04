#ifndef VXCORE_CONFIG_MANAGER_H
#define VXCORE_CONFIG_MANAGER_H

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

#include "vxcore/vxcore_types.h"
#include "vxcore_config.h"
#include "vxcore_session_config.h"

namespace vxcore {

class ConfigManager {
 public:
  ConfigManager();

  VxCoreError LoadConfigs();

  const VxCoreConfig &GetConfig() const { return config_; }
  VxCoreConfig &GetConfig() { return config_; }
  const VxCoreSessionConfig &GetSessionConfig() const { return session_config_; }
  VxCoreSessionConfig &GetSessionConfig() { return session_config_; }

  std::string GetDataPath(VxCoreDataLocation location) const;
  std::string GetLocalDataPath() const { return local_data_path_.string(); }
  std::string GetAppDataPath() const { return app_data_path_.string(); }
  std::string GetConfigPath() const { return (app_data_path_ / "vxcore.json").string(); }
  std::string GetSessionConfigPath() const { return (local_data_path_ / "session.json").string(); }

  VxCoreError SaveConfig();
  VxCoreError SaveSessionConfig();

  VxCoreError LoadConfigByName(VxCoreDataLocation location, const std::string &base_name,
                               std::string &out_content);
  VxCoreError SaveConfigByName(VxCoreDataLocation location, const std::string &base_name,
                               const std::string &content);

  VxCoreError LoadConfigByNameWithDefaults(VxCoreDataLocation location,
                                           const std::string &base_name,
                                           const std::string &defaults_json,
                                           std::string &out_merged);

  static void SetTestMode(bool enabled);
  static bool IsTestMode();

  static void SetAppInfo(const std::string &org_name, const std::string &app_name);
  static const std::string &GetOrgName() { return org_name_; }
  static const std::string &GetAppName() { return app_name_; }

 private:
  VxCoreError EnsureDataFolders();

  bool IsValidConfigBaseName(const std::string &base_name) const;

  static std::string GetDataPathInTestMode();

  VxCoreConfig config_;
  VxCoreSessionConfig session_config_;
  std::filesystem::path app_data_path_;
  std::filesystem::path local_data_path_;

  static bool test_mode_;
  static std::string org_name_;
  static std::string app_name_;
};

}  // namespace vxcore

#endif
