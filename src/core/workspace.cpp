#include "workspace.h"

namespace vxcore {

WorkspaceConfig::WorkspaceConfig() : metadata(nlohmann::json::object()) {}

WorkspaceConfig WorkspaceConfig::FromJson(const nlohmann::json &json) {
  WorkspaceConfig config;
  if (json.contains("id") && json["id"].is_string()) {
    config.id = json["id"].get<std::string>();
  }
  if (json.contains("name") && json["name"].is_string()) {
    config.name = json["name"].get<std::string>();
  }
  if (json.contains("bufferIds") && json["bufferIds"].is_array()) {
    for (const auto &id : json["bufferIds"]) {
      if (id.is_string()) {
        config.buffer_ids.push_back(id.get<std::string>());
      }
    }
  }
  if (json.contains("currentBufferId") && json["currentBufferId"].is_string()) {
    config.current_buffer_id = json["currentBufferId"].get<std::string>();
  }
  if (json.contains("metadata") && json["metadata"].is_object()) {
    config.metadata = json["metadata"];
  }
  return config;
}

nlohmann::json WorkspaceConfig::ToJson() const {
  nlohmann::json json = nlohmann::json::object();
  json["id"] = id;
  json["name"] = name;
  nlohmann::json buffer_ids_array = nlohmann::json::array();
  for (const auto &buffer_id : buffer_ids) {
    buffer_ids_array.push_back(buffer_id);
  }
  json["bufferIds"] = std::move(buffer_ids_array);
  json["currentBufferId"] = current_buffer_id;
  json["metadata"] = metadata;
  return json;
}

WorkspaceRecord::WorkspaceRecord() : metadata(nlohmann::json::object()) {}

WorkspaceRecord WorkspaceRecord::FromJson(const nlohmann::json &json) {
  WorkspaceRecord record;
  if (json.contains("id") && json["id"].is_string()) {
    record.id = json["id"].get<std::string>();
  }
  if (json.contains("name") && json["name"].is_string()) {
    record.name = json["name"].get<std::string>();
  }
  if (json.contains("bufferIds") && json["bufferIds"].is_array()) {
    for (const auto &id : json["bufferIds"]) {
      if (id.is_string()) {
        record.buffer_ids.push_back(id.get<std::string>());
      }
    }
  }
  if (json.contains("currentBufferId") && json["currentBufferId"].is_string()) {
    record.current_buffer_id = json["currentBufferId"].get<std::string>();
  }
  if (json.contains("metadata") && json["metadata"].is_object()) {
    record.metadata = json["metadata"];
  }
  return record;
}

nlohmann::json WorkspaceRecord::ToJson() const {
  nlohmann::json json = nlohmann::json::object();
  json["id"] = id;
  json["name"] = name;
  nlohmann::json buffer_ids_array = nlohmann::json::array();
  for (const auto &buffer_id : buffer_ids) {
    buffer_ids_array.push_back(buffer_id);
  }
  json["bufferIds"] = std::move(buffer_ids_array);
  json["currentBufferId"] = current_buffer_id;
  json["metadata"] = metadata;
  return json;
}

}  // namespace vxcore
