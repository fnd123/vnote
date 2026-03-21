// Copyright (c) 2025 VNote
#include "standard_buffer_provider.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "folder.h"
#include "folder_manager.h"
#include "notebook.h"
#include "utils/file_utils.h"
#include "utils/logger.h"

namespace vxcore {

StandardBufferProvider::StandardBufferProvider(Notebook *notebook, const std::string &file_path)
    : notebook_(notebook), file_path_(file_path) {
  if (!notebook_) {
    throw std::invalid_argument("notebook cannot be null");
  }

  // Cache the file ID by retrieving file info
  auto folder_manager = notebook_->GetFolderManager();
  if (!folder_manager) {
    throw std::runtime_error("FolderManager is null");
  }

  const FileRecord *file_record = nullptr;
  VxCoreError err = folder_manager->GetFileInfo(file_path_, &file_record);
  if (err != VXCORE_OK || !file_record) {
    VXCORE_LOG_ERROR("Failed to get file info for %s: error %d", file_path_.c_str(), err);
    throw std::runtime_error("Failed to get file info");
  }

  file_id_ = file_record->id;
}

VxCoreError StandardBufferProvider::InsertAssetRaw(const std::string &name,
                                                   const std::vector<uint8_t> &data,
                                                   std::string &out_relative_path) {
  if (name.empty()) {
    VXCORE_LOG_ERROR("Asset name cannot be empty");
    return VXCORE_ERR_INVALID_PARAM;
  }

  // Ensure assets folder exists
  VxCoreError err = EnsureAssetsFolderExists();
  if (err != VXCORE_OK) {
    return err;
  }

  // Get the assets folder path
  std::string assets_folder_path = GetAssetsFolderPath();

  // Generate unique name if collision
  std::string unique_name = GetUniqueAssetName(name, assets_folder_path);

  // Construct absolute path for the asset
  std::string asset_abs_path = CleanPath(assets_folder_path + "/" + unique_name);

  // Write binary data to file
  try {
    std::ofstream ofs(asset_abs_path, std::ios::binary);
    if (!ofs) {
      VXCORE_LOG_ERROR("Failed to open file for writing: %s", asset_abs_path.c_str());
      return VXCORE_ERR_IO;
    }

    ofs.write(reinterpret_cast<const char *>(data.data()), data.size());
    if (!ofs) {
      VXCORE_LOG_ERROR("Failed to write data to file: %s", asset_abs_path.c_str());
      return VXCORE_ERR_IO;
    }

    ofs.close();
  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("Exception while writing asset: %s", e.what());
    return VXCORE_ERR_IO;
  }

  // Compute relative path from notebook root
  std::string notebook_root = notebook_->GetRootFolder();
  std::string relative_path;
  try {
    std::filesystem::path abs_path(asset_abs_path);
    std::filesystem::path root_path(notebook_root);
    relative_path = std::filesystem::relative(abs_path, root_path).string();
    relative_path = CleanPath(relative_path);
  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("Failed to compute relative path: %s", e.what());
    return VXCORE_ERR_UNKNOWN;
  }

  // NOTE: Do NOT update attachment metadata here (that's InsertAttachment's job)
  out_relative_path = relative_path;
  return VXCORE_OK;
}

VxCoreError StandardBufferProvider::InsertAsset(const std::string &source_path,
                                                std::string &out_relative_path) {
  if (source_path.empty()) {
    VXCORE_LOG_ERROR("Source path cannot be empty");
    return VXCORE_ERR_INVALID_PARAM;
  }

  // Check source file exists
  if (!std::filesystem::exists(source_path)) {
    VXCORE_LOG_ERROR("Source file does not exist: %s", source_path.c_str());
    return VXCORE_ERR_NOT_FOUND;
  }

  // Ensure assets folder exists
  VxCoreError err = EnsureAssetsFolderExists();
  if (err != VXCORE_OK) {
    return err;
  }

  // Get the assets folder path
  std::string assets_folder_path = GetAssetsFolderPath();

  // Extract filename from source path
  std::filesystem::path src_path(source_path);
  std::string filename = src_path.filename().string();

  // Generate unique name if collision
  std::string unique_name = GetUniqueAssetName(filename, assets_folder_path);

  // Construct destination path
  std::string dest_path = CleanPath(assets_folder_path + "/" + unique_name);

  // Copy file
  try {
    std::filesystem::copy_file(source_path, dest_path,
                               std::filesystem::copy_options::overwrite_existing);
  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("Failed to copy file: %s", e.what());
    return VXCORE_ERR_IO;
  }

  // Compute relative path from notebook root
  std::string notebook_root = notebook_->GetRootFolder();
  std::string relative_path;
  try {
    std::filesystem::path abs_path(dest_path);
    std::filesystem::path root_path(notebook_root);
    relative_path = std::filesystem::relative(abs_path, root_path).string();
    relative_path = CleanPath(relative_path);
  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("Failed to compute relative path: %s", e.what());
    return VXCORE_ERR_UNKNOWN;
  }

  out_relative_path = relative_path;
  return VXCORE_OK;
}

VxCoreError StandardBufferProvider::DeleteAsset(const std::string &relative_path) {
  if (relative_path.empty()) {
    VXCORE_LOG_ERROR("Relative path cannot be empty");
    return VXCORE_ERR_INVALID_PARAM;
  }

  // Convert relative path to absolute path
  std::string notebook_root = notebook_->GetRootFolder();
  std::string abs_path = CleanPath(notebook_root + "/" + relative_path);

  // Delete the file
  try {
    if (!std::filesystem::exists(abs_path)) {
      VXCORE_LOG_WARN("Asset file does not exist: %s", abs_path.c_str());
      return VXCORE_ERR_NOT_FOUND;
    }

    if (!std::filesystem::remove(abs_path)) {
      VXCORE_LOG_ERROR("Failed to delete asset file: %s", abs_path.c_str());
      return VXCORE_ERR_IO;
    }
  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("Exception while deleting asset: %s", e.what());
    return VXCORE_ERR_IO;
  }

  // NOTE: Do NOT update attachment metadata here (that's DeleteAttachment's job)
  return VXCORE_OK;
}

VxCoreError StandardBufferProvider::InsertAttachment(const std::string &source_path,
                                                     std::string &out_filename) {
  // First, insert the asset (copy file)
  std::string relative_path;
  VxCoreError err = InsertAsset(source_path, relative_path);
  if (err != VXCORE_OK) {
    return err;
  }

  // Add to attachment metadata
  auto folder_manager = notebook_->GetFolderManager();
  err = folder_manager->AddFileAttachment(file_path_, relative_path);
  if (err != VXCORE_OK) {
    VXCORE_LOG_ERROR("Failed to add file attachment: error %d", err);
    // Try to clean up the file we just created
    std::string notebook_root = notebook_->GetRootFolder();
    std::string abs_path = CleanPath(notebook_root + "/" + relative_path);
    try {
      std::filesystem::remove(abs_path);
    } catch (...) {
      // Ignore cleanup errors
    }
    return err;
  }

  // Extract just the filename from relative path
  std::filesystem::path rel(relative_path);
  out_filename = rel.filename().string();
  return VXCORE_OK;
}

VxCoreError StandardBufferProvider::DeleteAttachment(const std::string &filename) {
  if (filename.empty()) {
    VXCORE_LOG_ERROR("Filename cannot be empty");
    return VXCORE_ERR_INVALID_PARAM;
  }

  // Build relative path from filename
  std::string assets_folder_path = GetAssetsFolderPath();
  std::string notebook_root = notebook_->GetRootFolder();

  // Compute relative path for the attachment
  std::string abs_path = CleanPath(assets_folder_path + "/" + filename);
  std::string relative_path;
  try {
    std::filesystem::path abs(abs_path);
    std::filesystem::path root(notebook_root);
    relative_path = std::filesystem::relative(abs, root).string();
    relative_path = CleanPath(relative_path);
  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("Failed to compute relative path: %s", e.what());
    return VXCORE_ERR_UNKNOWN;
  }

  // Delete from metadata first
  auto folder_manager = notebook_->GetFolderManager();
  VxCoreError err = folder_manager->DeleteFileAttachment(file_path_, relative_path);
  if (err != VXCORE_OK && err != VXCORE_ERR_NOT_FOUND) {
    VXCORE_LOG_WARN("Failed to remove attachment metadata: error %d", err);
  }

  // Delete the file
  err = DeleteAsset(relative_path);
  return err;
}

VxCoreError StandardBufferProvider::RenameAttachment(const std::string &old_filename,
                                                     const std::string &new_filename,
                                                     std::string &out_new_filename) {
  if (old_filename.empty() || new_filename.empty()) {
    VXCORE_LOG_ERROR("Old filename or new filename cannot be empty");
    return VXCORE_ERR_INVALID_PARAM;
  }

  std::string assets_folder_path = GetAssetsFolderPath();
  std::string notebook_root = notebook_->GetRootFolder();

  // Build old absolute path
  std::string old_abs_path = CleanPath(assets_folder_path + "/" + old_filename);

  if (!std::filesystem::exists(old_abs_path)) {
    VXCORE_LOG_ERROR("Attachment does not exist: %s", old_abs_path.c_str());
    return VXCORE_ERR_NOT_FOUND;
  }

  // Generate unique name if collision
  std::string unique_name = GetUniqueAssetName(new_filename, assets_folder_path);

  // Build new absolute path
  std::string new_abs_path = CleanPath(assets_folder_path + "/" + unique_name);

  try {
    // Rename the file
    std::filesystem::rename(old_abs_path, new_abs_path);
  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("Failed to rename attachment: %s", e.what());
    return VXCORE_ERR_IO;
  }

  // Compute old and new relative paths
  std::string old_relative_path, new_relative_path;
  try {
    old_relative_path = CleanPath(std::filesystem::relative(old_abs_path, notebook_root).string());
    new_relative_path = CleanPath(std::filesystem::relative(new_abs_path, notebook_root).string());
  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("Failed to compute relative path: %s", e.what());
    return VXCORE_ERR_UNKNOWN;
  }

  // Update metadata: remove old attachment, add new
  auto folder_manager = notebook_->GetFolderManager();
  VxCoreError err = folder_manager->DeleteFileAttachment(file_path_, old_relative_path);
  if (err != VXCORE_OK && err != VXCORE_ERR_NOT_FOUND) {
    VXCORE_LOG_WARN("Failed to remove old attachment metadata: error %d", err);
  }

  err = folder_manager->AddFileAttachment(file_path_, new_relative_path);
  if (err != VXCORE_OK) {
    VXCORE_LOG_WARN("Failed to add new attachment metadata: error %d", err);
  }

  out_new_filename = unique_name;
  VXCORE_LOG_INFO("Renamed attachment: %s -> %s", old_filename.c_str(), unique_name.c_str());
  return VXCORE_OK;
}

VxCoreError StandardBufferProvider::ListAttachments(std::vector<std::string> &out_filenames) {
  out_filenames.clear();

  auto folder_manager = notebook_->GetFolderManager();
  if (!folder_manager) {
    VXCORE_LOG_ERROR("FolderManager is null");
    return VXCORE_ERR_UNKNOWN;
  }

  std::string attachments_json;
  VxCoreError err = folder_manager->GetFileAttachments(file_path_, attachments_json);
  if (err != VXCORE_OK) {
    return err;
  }

  try {
    nlohmann::json j = nlohmann::json::parse(attachments_json);
    if (j.is_array()) {
      // Extract filenames from relative paths
      for (const auto &rel_path : j) {
        std::string path = rel_path.get<std::string>();
        std::filesystem::path p(path);
        out_filenames.push_back(p.filename().string());
      }
    }
    return VXCORE_OK;
  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("Failed to parse attachments JSON: %s", e.what());
    return VXCORE_ERR_JSON_PARSE;
  }
}

std::string StandardBufferProvider::GetAssetsFolderPath() {
  auto folder_manager = notebook_->GetFolderManager();
  if (!folder_manager) {
    VXCORE_LOG_ERROR("FolderManager is null");
    return "";
  }

  std::string assets_folder = folder_manager->GetAssetsFolder(file_path_);
  return assets_folder;
}

VxCoreError StandardBufferProvider::GetAssetsFolder(std::string &out_path) {
  std::string path = GetAssetsFolderPath();
  if (path.empty()) {
    return VXCORE_ERR_UNKNOWN;
  }

  // Create folder lazily if it doesn't exist
  VxCoreError err = EnsureAssetsFolderExists();
  if (err != VXCORE_OK) {
    return err;
  }

  out_path = path;
  return VXCORE_OK;
}

VxCoreError StandardBufferProvider::GetAttachmentsFolder(std::string &out_path) {
  // Attachments and assets share the same folder
  return GetAssetsFolder(out_path);
}

VxCoreError StandardBufferProvider::GetAssetAbsolutePath(const std::string &relative_path,
                                                         std::string &out_abs_path) {
  if (relative_path.empty()) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  std::string notebook_root = notebook_->GetRootFolder();
  out_abs_path = CleanPath(notebook_root + "/" + relative_path);
  return VXCORE_OK;
}

VxCoreError StandardBufferProvider::GetResourceBasePath(std::string &out_path) {
  std::string abs_path = CleanPath(notebook_->GetRootFolder() + "/" + file_path_);
  std::filesystem::path p(abs_path);
  out_path = CleanFsPath(p.parent_path());
  return VXCORE_OK;
}

VxCoreError StandardBufferProvider::EnsureAssetsFolderExists() {
  std::string assets_folder = GetAssetsFolderPath();
  if (assets_folder.empty()) {
    VXCORE_LOG_ERROR("Failed to get assets folder path");
    return VXCORE_ERR_UNKNOWN;
  }

  try {
    if (!std::filesystem::exists(assets_folder)) {
      if (!std::filesystem::create_directories(assets_folder)) {
        VXCORE_LOG_ERROR("Failed to create assets folder: %s", assets_folder.c_str());
        return VXCORE_ERR_IO;
      }
    }
  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("Exception while creating assets folder: %s", e.what());
    return VXCORE_ERR_IO;
  }

  return VXCORE_OK;
}

std::string StandardBufferProvider::GetUniqueAssetName(const std::string &base_name,
                                                       const std::string &assets_folder_path) {
  std::string candidate = base_name;
  std::string full_path = CleanPath(assets_folder_path + "/" + candidate);

  int counter = 1;
  while (std::filesystem::exists(full_path)) {
    // Extract extension
    size_t dot_pos = base_name.find_last_of('.');
    std::string name_part;
    std::string ext_part;

    if (dot_pos != std::string::npos) {
      name_part = base_name.substr(0, dot_pos);
      ext_part = base_name.substr(dot_pos);
    } else {
      name_part = base_name;
      ext_part = "";
    }

    candidate = name_part + "_" + std::to_string(counter) + ext_part;
    full_path = CleanPath(assets_folder_path + "/" + candidate);
    counter++;
  }

  return candidate;
}

}  // namespace vxcore
