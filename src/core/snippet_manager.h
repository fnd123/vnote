#ifndef VXCORE_SNIPPET_MANAGER_H
#define VXCORE_SNIPPET_MANAGER_H

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "vxcore/vxcore_types.h"

namespace vxcore {

class ConfigManager;

enum class SnippetType {
  kText,
  kDynamic,
};

struct SnippetData {
  std::string name;
  std::string description;
  SnippetType type = SnippetType::kText;
  std::string content;
  std::string cursor_mark = "@@";
  std::string selection_mark = "$$";
  bool indent_as_first_line = false;
  bool is_builtin = false;
};

struct ApplyResult {
  std::string text;
  int cursor_offset = -1;
};

using DynamicCallback = std::function<std::string()>;
using OverrideMap = std::unordered_map<std::string, std::string>;

class SnippetManager {
 public:
  explicit SnippetManager(ConfigManager *config_manager);

  VxCoreError GetSnippetFolderPath(std::string &out_path) const;
  VxCoreError ListSnippets(std::vector<SnippetData> &out_snippets) const;
  VxCoreError GetSnippet(const std::string &name, SnippetData &out_snippet) const;
  VxCoreError CreateSnippet(const std::string &name, const std::string &json_content);
  VxCoreError DeleteSnippet(const std::string &name);
  VxCoreError RenameSnippet(const std::string &old_name, const std::string &new_name);
  VxCoreError UpdateSnippet(const std::string &name, const std::string &json_content);

  ApplyResult ApplySnippet(const std::string &name, const std::string &selected_text,
                           const std::string &indentation, const OverrideMap &overrides);
  std::string ExpandSymbols(const std::string &content, const std::string &selected_text,
                            int &cursor_offset, const OverrideMap &overrides);

 private:
  VxCoreError EnsureSnippetFolderExists() const;
  bool IsValidSnippetName(const std::string &name) const;
  void LoadBuiltInSnippets();
  const SnippetData *FindSnippet(const std::string &name) const;
  std::string GetSnippetFilePath(const std::string &name) const;

  ConfigManager *config_manager_ = nullptr;
  std::string snippet_folder_path_;
  std::vector<SnippetData> builtin_snippets_;
  std::unordered_map<std::string, DynamicCallback> dynamic_callbacks_;
};

}  // namespace vxcore

#endif
