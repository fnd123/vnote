#ifndef VXCORE_FILETYPE_CONFIG_H
#define VXCORE_FILETYPE_CONFIG_H

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace vxcore {

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

// Result of SetTypes validation
struct SetTypesResult {
  bool success;
  std::string error;  // Non-empty if success is false

  static SetTypesResult Ok() { return {true, ""}; }
  static SetTypesResult Error(const std::string &msg) { return {false, msg}; }
};

struct FileTypesConfig {
  std::vector<FileTypeEntry> types;

  FileTypesConfig();

  const FileTypeEntry *GetBySuffix(const std::string &suffix) const;
  const FileTypeEntry *GetByName(const std::string &name) const;

  // Replace all file types with new entries.
  // Validates: no empty names, no duplicate names (case-insensitive).
  // Returns error message if validation fails, empty string on success.
  SetTypesResult SetTypes(std::vector<FileTypeEntry> new_types);

  static FileTypesConfig FromJson(const nlohmann::json &json);
  nlohmann::json ToJson() const;
};

}  // namespace vxcore

#endif
