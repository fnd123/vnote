#include "vxcore_config.h"

#include "utils/string_utils.h"

namespace vxcore {

SearchConfig SearchConfig::FromJson(const nlohmann::json &json) {
  SearchConfig config;
  if (json.contains("backends") && json["backends"].is_array()) {
    config.backends.clear();
    for (const auto &backend : json["backends"]) {
      if (backend.is_string()) {
        config.backends.push_back(backend.get<std::string>());
      }
    }
  }
  return config;
}

nlohmann::json SearchConfig::ToJson() const {
  nlohmann::json json = nlohmann::json::object();
  json["backends"] = backends;
  return json;
}

// FileTypeEntry implementation

std::string FileTypeEntry::GetDisplayName(const std::string &locale) const {
  if (locale.empty() || metadata.empty()) {
    return display_name;
  }

  try {
    auto metadata_json = nlohmann::json::parse(metadata);
    std::string key = "displayName_" + locale;
    if (metadata_json.contains(key) && metadata_json[key].is_string()) {
      return metadata_json[key].get<std::string>();
    }
  } catch (...) {
    // Parse error - return default
  }

  return display_name;
}

FileTypeEntry FileTypeEntry::FromJson(const nlohmann::json &json) {
  FileTypeEntry entry;

  if (json.contains("name") && json["name"].is_string()) {
    entry.name = json["name"].get<std::string>();
  }

  if (json.contains("suffixes") && json["suffixes"].is_array()) {
    entry.suffixes.clear();
    for (const auto &suffix : json["suffixes"]) {
      if (suffix.is_string()) {
        entry.suffixes.push_back(suffix.get<std::string>());
      }
    }
  }

  if (json.contains("isNewable") && json["isNewable"].is_boolean()) {
    entry.is_newable = json["isNewable"].get<bool>();
  }

  if (json.contains("displayName") && json["displayName"].is_string()) {
    entry.display_name = json["displayName"].get<std::string>();
  }

  if (json.contains("metadata") && json["metadata"].is_object()) {
    entry.metadata = json["metadata"].dump();
  }

  return entry;
}

nlohmann::json FileTypeEntry::ToJson() const {
  nlohmann::json json = nlohmann::json::object();
  json["name"] = name;
  json["suffixes"] = suffixes;
  json["isNewable"] = is_newable;
  json["displayName"] = display_name;

  if (!metadata.empty()) {
    try {
      json["metadata"] = nlohmann::json::parse(metadata);
    } catch (...) {
      // Invalid JSON in metadata - skip it
    }
  }

  return json;
}

// FileTypesConfig implementation

FileTypesConfig::FileTypesConfig() {
  types.push_back(FileTypeEntry("Markdown", {"md", "mkd", "rmd", "markdown"}, true, "Markdown"));
  types.push_back(FileTypeEntry("Text", {"txt", "text", "log"}, true, "Text"));
  types.push_back(FileTypeEntry("PDF", {"pdf"}, false, "Portable Document Format"));
  types.push_back(FileTypeEntry("MindMap", {"emind"}, true, "Mind Map"));
  types.push_back(FileTypeEntry("Others", {}, true, "Others"));
}

const FileTypeEntry *FileTypesConfig::GetBySuffix(const std::string &suffix) const {
  std::string lower_suffix = ToLowerString(suffix);

  for (const auto &type : types) {
    for (const auto &type_suffix : type.suffixes) {
      if (ToLowerString(type_suffix) == lower_suffix) {
        return &type;
      }
    }
  }

  // Return Others type if not found
  return GetByName("Others");
}

const FileTypeEntry *FileTypesConfig::GetByName(const std::string &name) const {
  std::string lower_name = ToLowerString(name);
  for (const auto &type : types) {
    if (ToLowerString(type.name) == lower_name) {
      return &type;
    }
  }
  return nullptr;
}

FileTypesConfig FileTypesConfig::FromJson(const nlohmann::json &json) {
  FileTypesConfig config;

  if (json.contains("fileTypes") && json["fileTypes"].is_array()) {
    config.types.clear();
    for (const auto &entry_json : json["fileTypes"]) {
      config.types.push_back(FileTypeEntry::FromJson(entry_json));
    }
  }

  return config;
}

nlohmann::json FileTypesConfig::ToJson() const {
  nlohmann::json json = nlohmann::json::object();
  nlohmann::json types_array = nlohmann::json::array();

  for (const auto &type : types) {
    types_array.push_back(type.ToJson());
  }

  json["fileTypes"] = types_array;
  return json;
}

VxCoreConfig VxCoreConfig::FromJson(const nlohmann::json &json) {
  VxCoreConfig config;
  if (json.contains("version") && json["version"].is_string()) {
    config.version = json["version"].get<std::string>();
  }
  if (json.contains("search") && json["search"].is_object()) {
    config.search = SearchConfig::FromJson(json["search"]);
  }
  if (json.contains("fileTypes") && json["fileTypes"].is_object()) {
    config.file_types = FileTypesConfig::FromJson(json["fileTypes"]);
  }
  return config;
}

nlohmann::json VxCoreConfig::ToJson() const {
  nlohmann::json json = nlohmann::json::object();
  json["version"] = version;
  json["search"] = search.ToJson();
  json["fileTypes"] = file_types.ToJson();
  return json;
}

}  // namespace vxcore
