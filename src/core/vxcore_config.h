#ifndef VXCORE_VXCORE_CONFIG_H
#define VXCORE_VXCORE_CONFIG_H

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace vxcore {

struct SearchConfig {
  std::vector<std::string> backends;

  SearchConfig() : backends({"rg", "simple"}) {}

  static SearchConfig FromJson(const nlohmann::json &json);
  nlohmann::json ToJson() const;
};

struct FileTypeEntry {
  std::string name;
  std::vector<std::string> suffixes;
  bool is_newable;
  std::string display_name;
  std::string metadata;

  FileTypeEntry() : name(""), suffixes(), is_newable(true), display_name(""), metadata("") {}

  FileTypeEntry(std::string p_name, std::vector<std::string> p_suffixes, bool p_newable = true,
                std::string p_display_name = "")
      : name(std::move(p_name)),
        suffixes(std::move(p_suffixes)),
        is_newable(p_newable),
        display_name(p_display_name.empty() ? name : std::move(p_display_name)),
        metadata("") {}

  std::string GetDisplayName(const std::string &locale) const;

  static FileTypeEntry FromJson(const nlohmann::json &json);
  nlohmann::json ToJson() const;
};

struct FileTypesConfig {
  std::vector<FileTypeEntry> types;

  FileTypesConfig();

  const FileTypeEntry *GetBySuffix(const std::string &suffix) const;
  const FileTypeEntry *GetByName(const std::string &name) const;

  static FileTypesConfig FromJson(const nlohmann::json &json);
  nlohmann::json ToJson() const;
};

struct VxCoreConfig {
  std::string version;
  SearchConfig search;
  FileTypesConfig file_types;

  VxCoreConfig() : version("0.1.0"), search(), file_types() {}

  static VxCoreConfig FromJson(const nlohmann::json &json);
  nlohmann::json ToJson() const;
};

}  // namespace vxcore

#endif
