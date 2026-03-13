#ifndef VXCORE_WORKSPACE_H
#define VXCORE_WORKSPACE_H

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace vxcore {

struct WorkspaceConfig {
  std::string id;
  std::string name;
  std::vector<std::string> buffer_ids;
  std::string current_buffer_id;
  nlohmann::json metadata;

  WorkspaceConfig();

  static WorkspaceConfig FromJson(const nlohmann::json &json);
  nlohmann::json ToJson() const;
};

struct WorkspaceRecord {
  std::string id;
  std::string name;
  std::vector<std::string> buffer_ids;
  std::string current_buffer_id;
  nlohmann::json metadata;

  WorkspaceRecord();

  static WorkspaceRecord FromJson(const nlohmann::json &json);
  nlohmann::json ToJson() const;
};

}  // namespace vxcore

#endif  // VXCORE_WORKSPACE_H
