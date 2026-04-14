#include "raw_folder_manager.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

#include "bundled_notebook.h"
#include "notebook.h"
#include "utils/file_utils.h"
#include "utils/logger.h"

namespace vxcore {

namespace {

namespace fs = std::filesystem;

bool IsRootPath(const std::string &path) { return path.empty() || path == "."; }

bool IsValidNodeName(const std::string &name) {
  return IsSingleName(name) && name != "." && name != "..";
}

bool IsSafeRelativePath(const std::string &path) {
  if (path.empty() || path == ".") {
    return true;
  }

  if (!IsRelativePath(path)) {
    return false;
  }

  for (const auto &component : SplitPathComponents(path)) {
    if (component == "..") {
      return false;
    }
  }
  return true;
}

int64_t FileTimeToUnixMillis(const fs::file_time_type &time) {
  const auto system_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
      time - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
  return std::chrono::duration_cast<std::chrono::milliseconds>(system_time.time_since_epoch())
      .count();
}

int64_t EntryModifiedTime(const fs::directory_entry &entry) {
  std::error_code ec;
  const auto time = entry.last_write_time(ec);
  if (ec) {
    return 0;
  }
  return FileTimeToUnixMillis(time);
}

int64_t PathModifiedTime(const fs::path &path) {
  std::error_code ec;
  const auto time = fs::last_write_time(path, ec);
  if (ec) {
    return 0;
  }
  return FileTimeToUnixMillis(time);
}

std::string MakeNodeId(const char *prefix, const std::string &relative_path) {
  return std::string(prefix) + ":" + (IsRootPath(relative_path) ? "." : relative_path);
}

std::string DisplayNameForFolder(const Notebook *notebook, const std::string &relative_path) {
  if (IsRootPath(relative_path)) {
    if (!notebook->GetConfig().name.empty()) {
      return notebook->GetConfig().name;
    }
    return fs::path(notebook->GetRootFolder()).filename().string();
  }

  return SplitPath(relative_path).second;
}

bool ShouldSkipEntry(const Notebook *notebook, const std::string &folder_path,
                     const std::string &entry_name) {
  if (entry_name.empty()) {
    return true;
  }

  // Keep raw notebooks free from VNote bookkeeping files and editor backups.
  if (entry_name == "vx.json" || entry_name == BundledNotebook::kMetadataFolderName ||
      (entry_name.size() >= 5 && entry_name.compare(entry_name.size() - 5, 5, ".vswp") == 0)) {
    return true;
  }

  // Match the existing external-node behavior: hidden filesystem entries are not shown.
  if (entry_name[0] == '.') {
    return true;
  }

  const auto &assets_folder = notebook->GetConfig().assets_folder;
  if (IsSingleName(assets_folder) && entry_name == assets_folder) {
    return true;
  }

  (void)folder_path;
  return false;
}

bool CompareFileRecordByName(const FileRecord &a, const FileRecord &b) { return a.name < b.name; }

bool CompareFolderRecordByName(const FolderRecord &a, const FolderRecord &b) {
  return a.name < b.name;
}

std::string ToLowerAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

std::set<std::string> ParseSuffixAllowlist(const std::string &suffix_allowlist) {
  std::set<std::string> suffixes;
  std::istringstream stream(suffix_allowlist);
  std::string suffix;
  while (std::getline(stream, suffix, ';')) {
    if (suffix.empty()) {
      continue;
    }

    if (suffix[0] == '.') {
      suffix = suffix.substr(1);
    }
    suffixes.insert(ToLowerAscii(suffix));
  }
  return suffixes;
}

bool IsFileAllowedBySuffix(const fs::path &path, const std::set<std::string> &allowed_suffixes) {
  if (allowed_suffixes.empty()) {
    return true;
  }

  std::string suffix = path.extension().string();
  if (!suffix.empty() && suffix[0] == '.') {
    suffix = suffix.substr(1);
  }
  return allowed_suffixes.find(ToLowerAscii(suffix)) != allowed_suffixes.end();
}

std::string GenerateAvailableFileName(const fs::path &dest_folder_path,
                                      const std::string &original_name) {
  const fs::path original_path(original_name);
  const std::string stem = original_path.stem().string();
  const std::string extension = original_path.extension().string();

  std::string target_name = original_name;
  for (int suffix = 1; fs::exists(dest_folder_path / target_name) && suffix <= 10000; ++suffix) {
    target_name = stem + "_" + std::to_string(suffix) + extension;
  }
  if (fs::exists(dest_folder_path / target_name)) {
    const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
    target_name = stem + "_" + std::to_string(timestamp) + extension;
  }
  return target_name;
}

std::string GenerateAvailableFolderName(const fs::path &dest_parent_path,
                                        const std::string &original_name) {
  std::string target_name = original_name;
  for (int suffix = 1; fs::exists(dest_parent_path / target_name) && suffix <= 10000; ++suffix) {
    target_name = original_name + "_" + std::to_string(suffix);
  }
  if (fs::exists(dest_parent_path / target_name)) {
    const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
    target_name = original_name + "_" + std::to_string(timestamp);
  }
  return target_name;
}

bool IsSameOrInside(const fs::path &path, const fs::path &root_path) {
  std::error_code ec;
  const fs::path clean_path = fs::weakly_canonical(path, ec);
  if (ec) {
    return false;
  }

  const fs::path clean_root_path = fs::weakly_canonical(root_path, ec);
  if (ec) {
    return false;
  }

  const auto mismatch_pair = std::mismatch(clean_root_path.begin(), clean_root_path.end(),
                                           clean_path.begin(), clean_path.end());
  return mismatch_pair.first == clean_root_path.end();
}

VxCoreError CopyFolderFiltered(const Notebook *notebook, const fs::path &source_path,
                               const fs::path &dest_path,
                               const std::set<std::string> &allowed_suffixes) {
  std::error_code ec;
  fs::create_directories(dest_path, ec);
  if (ec) {
    return VXCORE_ERR_IO;
  }

  for (fs::directory_iterator it(source_path, ec), end; !ec && it != end; it.increment(ec)) {
    const std::string entry_name = it->path().filename().string();
    if (ShouldSkipEntry(notebook, std::string(), entry_name)) {
      continue;
    }

    const fs::path child_dest_path = dest_path / entry_name;
    if (it->is_directory(ec)) {
      VxCoreError err = CopyFolderFiltered(notebook, it->path(), child_dest_path, allowed_suffixes);
      if (err != VXCORE_OK) {
        return err;
      }
    } else if (it->is_regular_file(ec) && IsFileAllowedBySuffix(it->path(), allowed_suffixes)) {
      fs::copy_file(it->path(), child_dest_path, fs::copy_options::overwrite_existing, ec);
      if (ec) {
        return VXCORE_ERR_IO;
      }
    }
  }

  return ec ? VXCORE_ERR_IO : VXCORE_OK;
}

}  // namespace

RawFolderManager::RawFolderManager(Notebook *notebook) : FolderManager(notebook) {
  assert(notebook && notebook->GetType() == NotebookType::Raw);
}

RawFolderManager::~RawFolderManager() {}

VxCoreError RawFolderManager::GetFolderConfig(const std::string &folder_path,
                                              std::string &out_config_json) {
  const auto clean_path = GetCleanRelativePath(folder_path);
  if (!IsSafeRelativePath(clean_path)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  const fs::path abs_path = IsRootPath(clean_path)
                                ? fs::path(notebook_->GetRootFolder())
                                : fs::path(notebook_->GetAbsolutePath(clean_path));
  if (!fs::exists(abs_path) || !fs::is_directory(abs_path)) {
    return VXCORE_ERR_NOT_FOUND;
  }

  FolderConfig config;
  config.id = MakeNodeId("raw-folder", clean_path);
  config.name = DisplayNameForFolder(notebook_, clean_path);
  config.created_utc = PathModifiedTime(abs_path);
  config.modified_utc = config.created_utc;
  config.metadata = nlohmann::json::object();

  FolderContents contents;
  VxCoreError err = ListFolderContents(clean_path, false, contents);
  if (err == VXCORE_OK) {
    config.files = contents.files;
    config.folders.reserve(contents.folders.size());
    for (const auto &folder : contents.folders) {
      config.folders.push_back(folder.name);
    }
  }

  out_config_json = config.ToJson().dump();
  return VXCORE_OK;
}

VxCoreError RawFolderManager::CreateFolder(const std::string &parent_path,
                                           const std::string &folder_name,
                                           std::string &out_folder_id) {
  if (!IsValidNodeName(folder_name)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  const auto clean_parent_path = GetCleanRelativePath(parent_path);
  if (!IsSafeRelativePath(clean_parent_path)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  const fs::path parent_abs_path = IsRootPath(clean_parent_path)
                                       ? fs::path(notebook_->GetRootFolder())
                                       : fs::path(notebook_->GetAbsolutePath(clean_parent_path));
  if (!fs::exists(parent_abs_path) || !fs::is_directory(parent_abs_path)) {
    return VXCORE_ERR_NOT_FOUND;
  }

  const fs::path folder_abs_path = parent_abs_path / folder_name;
  if (fs::exists(folder_abs_path)) {
    return VXCORE_ERR_ALREADY_EXISTS;
  }

  std::error_code ec;
  if (!fs::create_directory(folder_abs_path, ec) || ec) {
    return VXCORE_ERR_IO;
  }

  const auto relative_path = ConcatenatePaths(clean_parent_path, folder_name);
  out_folder_id = MakeNodeId("raw-folder", relative_path);
  return VXCORE_OK;
}

VxCoreError RawFolderManager::DeleteFolder(const std::string &folder_path) {
  const auto clean_path = GetCleanRelativePath(folder_path);
  if (IsRootPath(clean_path) || !IsSafeRelativePath(clean_path)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  const fs::path abs_path = notebook_->GetAbsolutePath(clean_path);
  if (!fs::exists(abs_path) || !fs::is_directory(abs_path)) {
    return VXCORE_ERR_NOT_FOUND;
  }

  std::error_code ec;
  fs::remove_all(abs_path, ec);
  return ec ? VXCORE_ERR_IO : VXCORE_OK;
}

VxCoreError RawFolderManager::UpdateFolderMetadata(const std::string &folder_path,
                                                   const std::string &metadata_json) {
  (void)folder_path;
  (void)metadata_json;
  return VXCORE_ERR_UNSUPPORTED;
}

VxCoreError RawFolderManager::GetFolderMetadata(const std::string &folder_path,
                                                std::string &out_metadata_json) {
  const auto clean_path = GetCleanRelativePath(folder_path);
  const fs::path abs_path = IsRootPath(clean_path)
                                ? fs::path(notebook_->GetRootFolder())
                                : fs::path(notebook_->GetAbsolutePath(clean_path));
  if (!IsSafeRelativePath(clean_path) || !fs::exists(abs_path) || !fs::is_directory(abs_path)) {
    return VXCORE_ERR_NOT_FOUND;
  }

  out_metadata_json = nlohmann::json::object().dump();
  return VXCORE_OK;
}

VxCoreError RawFolderManager::RenameFolder(const std::string &folder_path,
                                           const std::string &new_name) {
  if (!IsValidNodeName(new_name)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  const auto clean_path = GetCleanRelativePath(folder_path);
  if (IsRootPath(clean_path) || !IsSafeRelativePath(clean_path)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  const auto [parent_path, old_name] = SplitPath(clean_path);
  const fs::path parent_abs_path = IsRootPath(parent_path)
                                       ? fs::path(notebook_->GetRootFolder())
                                       : fs::path(notebook_->GetAbsolutePath(parent_path));
  const fs::path old_abs_path = parent_abs_path / old_name;
  const fs::path new_abs_path = parent_abs_path / new_name;

  if (!fs::exists(old_abs_path) || !fs::is_directory(old_abs_path)) {
    return VXCORE_ERR_NOT_FOUND;
  }
  if (fs::exists(new_abs_path)) {
    return VXCORE_ERR_ALREADY_EXISTS;
  }

  std::error_code ec;
  fs::rename(old_abs_path, new_abs_path, ec);
  return ec ? VXCORE_ERR_IO : VXCORE_OK;
}

VxCoreError RawFolderManager::MoveFolder(const std::string &src_path,
                                         const std::string &dest_parent_path) {
  const auto clean_src_path = GetCleanRelativePath(src_path);
  const auto clean_dest_parent_path = GetCleanRelativePath(dest_parent_path);
  if (IsRootPath(clean_src_path) || !IsSafeRelativePath(clean_src_path) ||
      !IsSafeRelativePath(clean_dest_parent_path)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  const auto folder_name = SplitPath(clean_src_path).second;
  const fs::path src_abs_path = notebook_->GetAbsolutePath(clean_src_path);
  const fs::path dest_parent_abs_path =
      IsRootPath(clean_dest_parent_path)
          ? fs::path(notebook_->GetRootFolder())
          : fs::path(notebook_->GetAbsolutePath(clean_dest_parent_path));
  const fs::path dest_abs_path = dest_parent_abs_path / folder_name;

  if (!fs::exists(src_abs_path) || !fs::is_directory(src_abs_path) ||
      !fs::exists(dest_parent_abs_path) || !fs::is_directory(dest_parent_abs_path)) {
    return VXCORE_ERR_NOT_FOUND;
  }
  if (fs::exists(dest_abs_path)) {
    return VXCORE_ERR_ALREADY_EXISTS;
  }

  std::error_code ec;
  fs::rename(src_abs_path, dest_abs_path, ec);
  return ec ? VXCORE_ERR_IO : VXCORE_OK;
}

VxCoreError RawFolderManager::CopyFolder(const std::string &src_path,
                                         const std::string &dest_parent_path,
                                         const std::string &new_name, std::string &out_folder_id) {
  const auto clean_src_path = GetCleanRelativePath(src_path);
  const auto clean_dest_parent_path = GetCleanRelativePath(dest_parent_path);
  if (IsRootPath(clean_src_path) || !IsSafeRelativePath(clean_src_path) ||
      !IsSafeRelativePath(clean_dest_parent_path)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  const std::string target_name = new_name.empty() ? SplitPath(clean_src_path).second : new_name;
  if (!IsValidNodeName(target_name)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  const fs::path src_abs_path = notebook_->GetAbsolutePath(clean_src_path);
  const fs::path dest_parent_abs_path =
      IsRootPath(clean_dest_parent_path)
          ? fs::path(notebook_->GetRootFolder())
          : fs::path(notebook_->GetAbsolutePath(clean_dest_parent_path));
  const fs::path dest_abs_path = dest_parent_abs_path / target_name;

  if (!fs::exists(src_abs_path) || !fs::is_directory(src_abs_path) ||
      !fs::exists(dest_parent_abs_path) || !fs::is_directory(dest_parent_abs_path)) {
    return VXCORE_ERR_NOT_FOUND;
  }
  if (fs::exists(dest_abs_path)) {
    return VXCORE_ERR_ALREADY_EXISTS;
  }

  std::error_code ec;
  fs::copy(src_abs_path, dest_abs_path, fs::copy_options::recursive, ec);
  if (ec) {
    return VXCORE_ERR_IO;
  }

  out_folder_id = MakeNodeId("raw-folder", ConcatenatePaths(clean_dest_parent_path, target_name));
  return VXCORE_OK;
}

VxCoreError RawFolderManager::CreateFile(const std::string &folder_path,
                                         const std::string &file_name, std::string &out_file_id) {
  if (!IsValidNodeName(file_name)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  const auto clean_folder_path = GetCleanRelativePath(folder_path);
  if (!IsSafeRelativePath(clean_folder_path)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  const fs::path folder_abs_path = IsRootPath(clean_folder_path)
                                       ? fs::path(notebook_->GetRootFolder())
                                       : fs::path(notebook_->GetAbsolutePath(clean_folder_path));
  if (!fs::exists(folder_abs_path) || !fs::is_directory(folder_abs_path)) {
    return VXCORE_ERR_NOT_FOUND;
  }

  const fs::path file_abs_path = folder_abs_path / file_name;
  if (fs::exists(file_abs_path)) {
    return VXCORE_ERR_ALREADY_EXISTS;
  }

  std::ofstream file(file_abs_path, std::ios::binary);
  if (!file.is_open()) {
    return VXCORE_ERR_IO;
  }
  file.close();

  out_file_id = MakeNodeId("raw-file", ConcatenatePaths(clean_folder_path, file_name));
  return VXCORE_OK;
}

VxCoreError RawFolderManager::DeleteFile(const std::string &file_path) {
  const auto clean_file_path = GetCleanRelativePath(file_path);
  if (!IsSafeRelativePath(clean_file_path)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  const fs::path abs_path = notebook_->GetAbsolutePath(clean_file_path);
  if (!fs::exists(abs_path) || !fs::is_regular_file(abs_path)) {
    return VXCORE_ERR_NOT_FOUND;
  }

  std::error_code ec;
  fs::remove(abs_path, ec);
  return ec ? VXCORE_ERR_IO : VXCORE_OK;
}

VxCoreError RawFolderManager::UpdateFileMetadata(const std::string &file_path,
                                                 const std::string &metadata_json) {
  (void)file_path;
  (void)metadata_json;
  return VXCORE_ERR_UNSUPPORTED;
}

VxCoreError RawFolderManager::UpdateFileTags(const std::string &file_path,
                                             const std::string &tags_json) {
  (void)file_path;
  (void)tags_json;
  return VXCORE_ERR_UNSUPPORTED;
}

VxCoreError RawFolderManager::TagFile(const std::string &file_path, const std::string &tag_name) {
  (void)file_path;
  (void)tag_name;
  return VXCORE_ERR_UNSUPPORTED;
}

VxCoreError RawFolderManager::UntagFile(const std::string &file_path, const std::string &tag_name) {
  (void)file_path;
  (void)tag_name;
  return VXCORE_ERR_UNSUPPORTED;
}

VxCoreError RawFolderManager::GetFileInfo(const std::string &file_path,
                                          std::string &out_file_info_json) {
  const FileRecord *record = nullptr;
  VxCoreError err = GetFileInfo(file_path, &record);
  if (err != VXCORE_OK) {
    return err;
  }

  out_file_info_json = record->ToJson().dump();
  return VXCORE_OK;
}

VxCoreError RawFolderManager::GetFileInfo(const std::string &file_path,
                                          const FileRecord **out_record) {
  if (!out_record) {
    return VXCORE_ERR_NULL_POINTER;
  }
  *out_record = nullptr;

  const auto clean_file_path = GetCleanRelativePath(file_path);
  if (!IsSafeRelativePath(clean_file_path)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  const fs::path abs_path = notebook_->GetAbsolutePath(clean_file_path);
  if (!fs::exists(abs_path) || !fs::is_regular_file(abs_path)) {
    return VXCORE_ERR_NOT_FOUND;
  }

  cached_file_record_ = FileRecord();
  cached_file_record_.id = MakeNodeId("raw-file", clean_file_path);
  cached_file_record_.name = SplitPath(clean_file_path).second;
  cached_file_record_.created_utc = PathModifiedTime(abs_path);
  cached_file_record_.modified_utc = cached_file_record_.created_utc;
  cached_file_record_.metadata = nlohmann::json::object();
  *out_record = &cached_file_record_;
  return VXCORE_OK;
}

VxCoreError RawFolderManager::GetFileMetadata(const std::string &file_path,
                                              std::string &out_metadata_json) {
  const FileRecord *record = nullptr;
  VxCoreError err = GetFileInfo(file_path, &record);
  if (err != VXCORE_OK) {
    return err;
  }

  out_metadata_json = nlohmann::json::object().dump();
  return VXCORE_OK;
}

VxCoreError RawFolderManager::RenameFile(const std::string &file_path,
                                         const std::string &new_name) {
  if (!IsValidNodeName(new_name)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  const auto clean_file_path = GetCleanRelativePath(file_path);
  if (!IsSafeRelativePath(clean_file_path)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  const auto [folder_path, old_name] = SplitPath(clean_file_path);
  const fs::path folder_abs_path = IsRootPath(folder_path)
                                       ? fs::path(notebook_->GetRootFolder())
                                       : fs::path(notebook_->GetAbsolutePath(folder_path));
  const fs::path old_abs_path = folder_abs_path / old_name;
  const fs::path new_abs_path = folder_abs_path / new_name;

  if (!fs::exists(old_abs_path) || !fs::is_regular_file(old_abs_path)) {
    return VXCORE_ERR_NOT_FOUND;
  }
  if (fs::exists(new_abs_path)) {
    return VXCORE_ERR_ALREADY_EXISTS;
  }

  std::error_code ec;
  fs::rename(old_abs_path, new_abs_path, ec);
  return ec ? VXCORE_ERR_IO : VXCORE_OK;
}

VxCoreError RawFolderManager::MoveFile(const std::string &src_file_path,
                                       const std::string &dest_folder_path) {
  const auto clean_src_file_path = GetCleanRelativePath(src_file_path);
  const auto clean_dest_folder_path = GetCleanRelativePath(dest_folder_path);
  if (!IsSafeRelativePath(clean_src_file_path) || !IsSafeRelativePath(clean_dest_folder_path)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  const auto file_name = SplitPath(clean_src_file_path).second;
  const fs::path src_abs_path = notebook_->GetAbsolutePath(clean_src_file_path);
  const fs::path dest_folder_abs_path =
      IsRootPath(clean_dest_folder_path)
          ? fs::path(notebook_->GetRootFolder())
          : fs::path(notebook_->GetAbsolutePath(clean_dest_folder_path));
  const fs::path dest_abs_path = dest_folder_abs_path / file_name;

  if (!fs::exists(src_abs_path) || !fs::is_regular_file(src_abs_path) ||
      !fs::exists(dest_folder_abs_path) || !fs::is_directory(dest_folder_abs_path)) {
    return VXCORE_ERR_NOT_FOUND;
  }
  if (fs::exists(dest_abs_path)) {
    return VXCORE_ERR_ALREADY_EXISTS;
  }

  std::error_code ec;
  fs::rename(src_abs_path, dest_abs_path, ec);
  return ec ? VXCORE_ERR_IO : VXCORE_OK;
}

VxCoreError RawFolderManager::CopyFile(const std::string &src_file_path,
                                       const std::string &dest_folder_path,
                                       const std::string &new_name, std::string &out_file_id) {
  const auto clean_src_file_path = GetCleanRelativePath(src_file_path);
  const auto clean_dest_folder_path = GetCleanRelativePath(dest_folder_path);
  if (!IsSafeRelativePath(clean_src_file_path) || !IsSafeRelativePath(clean_dest_folder_path)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  const std::string target_name =
      new_name.empty() ? SplitPath(clean_src_file_path).second : new_name;
  if (!IsValidNodeName(target_name)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  const fs::path src_abs_path = notebook_->GetAbsolutePath(clean_src_file_path);
  const fs::path dest_folder_abs_path =
      IsRootPath(clean_dest_folder_path)
          ? fs::path(notebook_->GetRootFolder())
          : fs::path(notebook_->GetAbsolutePath(clean_dest_folder_path));
  const fs::path dest_abs_path = dest_folder_abs_path / target_name;

  if (!fs::exists(src_abs_path) || !fs::is_regular_file(src_abs_path) ||
      !fs::exists(dest_folder_abs_path) || !fs::is_directory(dest_folder_abs_path)) {
    return VXCORE_ERR_NOT_FOUND;
  }
  if (fs::exists(dest_abs_path)) {
    return VXCORE_ERR_ALREADY_EXISTS;
  }

  std::error_code ec;
  fs::copy_file(src_abs_path, dest_abs_path, ec);
  if (ec) {
    return VXCORE_ERR_IO;
  }

  out_file_id = MakeNodeId("raw-file", ConcatenatePaths(clean_dest_folder_path, target_name));
  return VXCORE_OK;
}

VxCoreError RawFolderManager::ImportFile(const std::string &folder_path,
                                         const std::string &external_file_path,
                                         std::string &out_file_id) {
  const fs::path source_path = external_file_path;
  if (!source_path.is_absolute()) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  if (!fs::exists(source_path)) {
    return VXCORE_ERR_NOT_FOUND;
  }
  if (!fs::is_regular_file(source_path)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  const auto clean_folder_path = GetCleanRelativePath(folder_path);
  if (!IsSafeRelativePath(clean_folder_path)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  const fs::path dest_folder_path = IsRootPath(clean_folder_path)
                                        ? fs::path(notebook_->GetRootFolder())
                                        : fs::path(notebook_->GetAbsolutePath(clean_folder_path));
  if (!fs::exists(dest_folder_path) || !fs::is_directory(dest_folder_path)) {
    return VXCORE_ERR_NOT_FOUND;
  }

  const std::string original_name = source_path.filename().string();
  if (!IsValidNodeName(original_name)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  const std::string target_name = GenerateAvailableFileName(dest_folder_path, original_name);
  if (!IsValidNodeName(target_name)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  const fs::path dest_path = dest_folder_path / target_name;
  std::error_code ec;
  fs::copy_file(source_path, dest_path, ec);
  if (ec) {
    return VXCORE_ERR_IO;
  }

  out_file_id = MakeNodeId("raw-file", ConcatenatePaths(clean_folder_path, target_name));
  return VXCORE_OK;
}

VxCoreError RawFolderManager::ImportFolder(const std::string &dest_folder_path,
                                           const std::string &external_folder_path,
                                           const std::string &suffix_allowlist,
                                           std::string &out_folder_id) {
  const fs::path source_path = external_folder_path;
  if (!source_path.is_absolute()) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  if (!fs::exists(source_path)) {
    return VXCORE_ERR_NOT_FOUND;
  }
  if (!fs::is_directory(source_path)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  if (IsSameOrInside(source_path, fs::path(notebook_->GetRootFolder()))) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  const auto clean_dest_path = GetCleanRelativePath(dest_folder_path);
  if (!IsSafeRelativePath(clean_dest_path)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  const fs::path dest_parent_path = IsRootPath(clean_dest_path)
                                        ? fs::path(notebook_->GetRootFolder())
                                        : fs::path(notebook_->GetAbsolutePath(clean_dest_path));
  if (!fs::exists(dest_parent_path) || !fs::is_directory(dest_parent_path)) {
    return VXCORE_ERR_NOT_FOUND;
  }

  const std::string original_name = source_path.filename().string();
  if (!IsValidNodeName(original_name)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  const std::string target_name = GenerateAvailableFolderName(dest_parent_path, original_name);
  if (!IsValidNodeName(target_name)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  const fs::path dest_path = dest_parent_path / target_name;
  VxCoreError err =
      CopyFolderFiltered(notebook_, source_path, dest_path, ParseSuffixAllowlist(suffix_allowlist));
  if (err != VXCORE_OK) {
    std::error_code ec;
    fs::remove_all(dest_path, ec);
    return err;
  }

  out_folder_id = MakeNodeId("raw-folder", ConcatenatePaths(clean_dest_path, target_name));
  return VXCORE_OK;
}

void RawFolderManager::IterateAllFiles(
    std::function<bool(const std::string &, const FileRecord &)> callback) {
  const fs::path root_path = notebook_->GetRootFolder();
  if (!fs::exists(root_path) || !fs::is_directory(root_path)) {
    return;
  }

  std::error_code ec;
  for (fs::recursive_directory_iterator it(root_path, ec), end; !ec && it != end;
       it.increment(ec)) {
    const std::string entry_name = it->path().filename().string();
    const auto parent_relative = CleanFsPath(fs::relative(it->path().parent_path(), root_path, ec));
    if (ShouldSkipEntry(notebook_, parent_relative, entry_name)) {
      if (it->is_directory()) {
        it.disable_recursion_pending();
      }
      continue;
    }

    if (!it->is_regular_file(ec)) {
      continue;
    }

    const auto relative_path = CleanFsPath(fs::relative(it->path(), root_path, ec));
    if (ec || !IsSafeRelativePath(relative_path)) {
      continue;
    }

    FileRecord record;
    record.id = MakeNodeId("raw-file", relative_path);
    record.name = entry_name;
    record.created_utc = EntryModifiedTime(*it);
    record.modified_utc = record.created_utc;
    record.metadata = nlohmann::json::object();

    const auto folder_path = SplitPath(relative_path).first;
    if (!callback(folder_path, record)) {
      return;
    }
  }
}

VxCoreError RawFolderManager::FindFilesByTag(const std::string &tag_name,
                                             std::string &out_files_json) {
  (void)tag_name;
  out_files_json = nlohmann::json::array().dump();
  return VXCORE_OK;
}

VxCoreError RawFolderManager::ListFolderContents(const std::string &folder_path,
                                                 bool include_folders_info,
                                                 FolderContents &out_contents) {
  (void)include_folders_info;

  out_contents.files.clear();
  out_contents.folders.clear();

  const auto clean_path = GetCleanRelativePath(folder_path);
  if (!IsSafeRelativePath(clean_path)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  const fs::path abs_path = IsRootPath(clean_path)
                                ? fs::path(notebook_->GetRootFolder())
                                : fs::path(notebook_->GetAbsolutePath(clean_path));
  if (!fs::exists(abs_path) || !fs::is_directory(abs_path)) {
    return VXCORE_ERR_NOT_FOUND;
  }

  try {
    for (const auto &entry : fs::directory_iterator(abs_path)) {
      const std::string entry_name = entry.path().filename().string();
      if (ShouldSkipEntry(notebook_, clean_path, entry_name)) {
        continue;
      }

      const std::string relative_path = ConcatenatePaths(clean_path, entry_name);
      if (entry.is_directory()) {
        FolderRecord record;
        record.id = MakeNodeId("raw-folder", relative_path);
        record.name = entry_name;
        record.created_utc = EntryModifiedTime(entry);
        record.modified_utc = record.created_utc;
        record.metadata = nlohmann::json::object();
        out_contents.folders.push_back(record);
      } else if (entry.is_regular_file()) {
        FileRecord record;
        record.id = MakeNodeId("raw-file", relative_path);
        record.name = entry_name;
        record.created_utc = EntryModifiedTime(entry);
        record.modified_utc = record.created_utc;
        record.metadata = nlohmann::json::object();
        out_contents.files.push_back(record);
      }
    }
  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("RawFolderManager: failed to list directory %s: %s", abs_path.string().c_str(),
                     e.what());
    return VXCORE_ERR_IO;
  }

  std::sort(out_contents.folders.begin(), out_contents.folders.end(), CompareFolderRecordByName);
  std::sort(out_contents.files.begin(), out_contents.files.end(), CompareFileRecordByName);
  return VXCORE_OK;
}

VxCoreError RawFolderManager::ListExternalNodes(const std::string &folder_path,
                                                FolderContents &out_contents) {
  (void)folder_path;
  out_contents.files.clear();
  out_contents.folders.clear();
  return VXCORE_OK;
}

void RawFolderManager::ClearCache() {}

VxCoreError RawFolderManager::IndexNode(const std::string &node_path) {
  const auto clean_path = GetCleanRelativePath(node_path);
  const fs::path abs_path = IsRootPath(clean_path)
                                ? fs::path(notebook_->GetRootFolder())
                                : fs::path(notebook_->GetAbsolutePath(clean_path));
  if (!IsSafeRelativePath(clean_path) || !fs::exists(abs_path)) {
    return VXCORE_ERR_NOT_FOUND;
  }
  return VXCORE_OK;
}

VxCoreError RawFolderManager::UnindexNode(const std::string &node_path) {
  (void)node_path;
  return VXCORE_ERR_UNSUPPORTED;
}

VxCoreError RawFolderManager::GetFileAttachments(const std::string &file_path,
                                                 std::string &out_attachments_json) {
  (void)file_path;
  out_attachments_json = nlohmann::json::array().dump();
  return VXCORE_OK;
}

VxCoreError RawFolderManager::UpdateFileAttachments(const std::string &file_path,
                                                    const std::string &attachments_json) {
  (void)file_path;
  (void)attachments_json;
  return VXCORE_ERR_UNSUPPORTED;
}

VxCoreError RawFolderManager::AddFileAttachment(const std::string &file_path,
                                                const std::string &attachment) {
  (void)file_path;
  (void)attachment;
  return VXCORE_ERR_UNSUPPORTED;
}

VxCoreError RawFolderManager::DeleteFileAttachment(const std::string &file_path,
                                                   const std::string &attachment) {
  (void)file_path;
  (void)attachment;
  return VXCORE_ERR_UNSUPPORTED;
}

}  // namespace vxcore
