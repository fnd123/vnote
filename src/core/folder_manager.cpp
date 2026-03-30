#include "folder_manager.h"

#include <filesystem>

namespace vxcore {

VxCoreError FolderManager::MoveToRecycleBin(const std::filesystem::path &source_path) {
  try {
    if (!std::filesystem::exists(source_path)) {
      return VXCORE_ERR_NOT_FOUND;
    }

    if (std::filesystem::is_directory(source_path)) {
      std::filesystem::remove_all(source_path);
    } else {
      std::filesystem::remove(source_path);
    }

    return VXCORE_OK;
  } catch (const std::exception &) {
    return VXCORE_ERR_IO;
  }
}

VxCoreError FolderManager::CreateFolderPath(const std::string &folder_path,
                                            std::string &out_folder_id) {
  if (folder_path.empty()) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  std::vector<std::string> path_components = SplitPathComponents(folder_path);
  if (path_components.empty()) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  std::string current_parent = ".";
  std::string folder_id;

  for (size_t i = 0; i < path_components.size(); ++i) {
    const std::string &folder_name = path_components[i];
    if (folder_name.empty()) {
      return VXCORE_ERR_INVALID_PARAM;
    }

    VxCoreError err = CreateFolder(current_parent, folder_name, folder_id);
    if (err != VXCORE_OK && err != VXCORE_ERR_ALREADY_EXISTS) {
      return err;
    }

    current_parent = ConcatenatePaths(current_parent, folder_name);
  }

  out_folder_id = folder_id;
  return VXCORE_OK;
}

std::string FolderManager::GetPublicAssetsFolder(const std::string &file_path) const {
  const NotebookConfig &config = notebook_->GetConfig();
  const std::string &assets_folder = config.assets_folder;

  // If absolute path, return as-is
  if (!IsRelativePath(assets_folder)) {
    return CleanPath(assets_folder);
  }

  // Get parent folder of the file
  auto [parent_path, _] = SplitPath(file_path);
  std::string abs_parent = notebook_->GetAbsolutePath(parent_path);

  // Resolve relative to file's parent folder
  return CleanFsPath(std::filesystem::path(abs_parent) / assets_folder);
}

std::string FolderManager::GetAssetsFolder(const std::string &file_path) {
  // Get file's UUID
  const FileRecord *record = nullptr;
  VxCoreError err = GetFileInfo(file_path, &record);
  if (err != VXCORE_OK || !record) {
    return "";
  }

  std::string public_folder = GetPublicAssetsFolder(file_path);
  return ConcatenatePaths(public_folder, record->id);
}

VxCoreError FolderManager::GetAvailableName(const std::string &folder_path,
                                            const std::string &new_name,
                                            std::string &out_available_name) {
  if (new_name.empty()) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  namespace fs = std::filesystem;

  const std::string clean_folder_path = GetCleanRelativePath(folder_path);
  std::string abs_folder_path = notebook_->GetAbsolutePath(clean_folder_path);
  fs::path folder_fs_path(abs_folder_path);

  // Check if the original name is available
  fs::path dest_path = folder_fs_path / new_name;
  if (!fs::exists(dest_path)) {
    out_available_name = new_name;
    return VXCORE_OK;
  }

  // Extract base name and extension
  std::string base_name = new_name;
  std::string extension;
  size_t dot_pos = new_name.find_last_of('.');
  if (dot_pos != std::string::npos && dot_pos > 0) {
    base_name = new_name.substr(0, dot_pos);
    extension = new_name.substr(dot_pos);
  }

  // Try _1, _2, _3, etc. until we find an available name
  int suffix = 1;
  while (true) {
    std::string candidate = base_name + "_" + std::to_string(suffix) + extension;
    dest_path = folder_fs_path / candidate;
    if (!fs::exists(dest_path)) {
      out_available_name = candidate;
      return VXCORE_OK;
    }
    ++suffix;
    if (suffix > 10000) {
      // Safety limit to prevent infinite loop
      return VXCORE_ERR_UNKNOWN;
    }
  }
}

}  // namespace vxcore
