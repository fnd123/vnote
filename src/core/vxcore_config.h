#ifndef VXCORE_VXCORE_CONFIG_H
#define VXCORE_VXCORE_CONFIG_H

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "filetype_config.h"

namespace vxcore {

struct SearchConfig {
  std::vector<std::string> backends;

  SearchConfig() : backends({"rg", "simple"}) {}

  static SearchConfig FromJson(const nlohmann::json &json);
  nlohmann::json ToJson() const;
};

struct VxCoreConfig {
  std::string version;
  SearchConfig search;
  FileTypesConfig file_types;
  bool recover_last_session;

  VxCoreConfig() : version("0.1.0"), search(), file_types(), recover_last_session(true) {}

  static VxCoreConfig FromJson(const nlohmann::json &json);
  nlohmann::json ToJson() const;
};

}  // namespace vxcore

#endif
