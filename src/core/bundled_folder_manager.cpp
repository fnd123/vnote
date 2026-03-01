#include "bundled_folder_manager.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "metadata_store.h"
#include "bundled_notebook.h"
#include "utils/file_utils.h"
#include "utils/logger.h"
#include "utils/utils.h"

namespace vxcore {

namespace fs = std::filesystem;

namespace {

// Helper to convert FolderConfig to StoreFolderRecord
StoreFolderRecord ToStoreFolderRecord(const FolderConfig &config, const std::string &parent_id) {
  StoreFolderRecord record;
  record.id = config.id;
  record.parent_id = parent_id;
  record.name = config.name;
  record.created_utc = config.created_utc;
  record.modified_utc = config.modified_utc;
  record.metadata = config.metadata.dump();
  return record;
}

// Helper to convert FileRecord to StoreFileRecord
StoreFileRecord ToStoreFileRecord(const FileRecord &file, const std::string &folder_id) {
  StoreFileRecord record;
  record.id = file.id;
  record.folder_id = folder_id;
  record.name = file.name;
  record.created_utc = file.created_utc;
  record.modified_utc = file.modified_utc;
  record.metadata = file.metadata.dump();
  record.tags = file.tags;
  return record;
}

// Helper to sync files from config to store (add/update/delete orphans)
// - Deletes files in store that no longer exist in config
// - Creates new files that exist in config but not in store
// - Updates existing files if they differ
void SyncFilesToStore(MetadataStore *store, const std::string &folder_id,
                      const std::vector<FileRecord> &config_files) {
  if (!store) {
    return;
  }

  // Get existing files from store
  std::vector<StoreFileRecord> store_files = store->ListFiles(folder_id);

  // Build set of config file IDs for quick lookup
  std::unordered_set<std::string> config_file_ids;
  for (const auto &file : config_files) {
    config_file_ids.insert(file.id);
  }

  // Delete orphaned files (in store but not in config)
  for (const auto &store_file : store_files) {
    if (config_file_ids.find(store_file.id) == config_file_ids.end()) {
      VXCORE_LOG_DEBUG("SyncFilesToStore: Deleting orphaned file: id=%s, name=%s",
                       store_file.id.c_str(), store_file.name.c_str());
      if (!store->DeleteFile(store_file.id)) {
        VXCORE_LOG_WARN("SyncFilesToStore: Failed to delete orphaned file: id=%s",
                        store_file.id.c_str());
      }
    }
  }

  // Build map of store file IDs for quick lookup
  std::unordered_map<std::string, const StoreFileRecord *> store_file_map;
  for (const auto &store_file : store_files) {
    store_file_map[store_file.id] = &store_file;
  }

  // Add or update files from config
  for (const auto &file : config_files) {
    auto it = store_file_map.find(file.id);
    if (it != store_file_map.end()) {
      // File exists in store - update it
      if (!store->UpdateFile(file.id, file.name, file.modified_utc, file.metadata.dump())) {
        VXCORE_LOG_WARN("SyncFilesToStore: Failed to update file: id=%s", file.id.c_str());
      }
      // Also update tags
      if (!store->SetFileTags(file.id, file.tags)) {
        VXCORE_LOG_WARN("SyncFilesToStore: Failed to update file tags: id=%s", file.id.c_str());
      }
    } else {
      // File doesn't exist in store - create it
      StoreFileRecord file_record = ToStoreFileRecord(file, folder_id);
      if (!store->CreateFile(file_record)) {
        VXCORE_LOG_WARN("SyncFilesToStore: Failed to create file: id=%s", file.id.c_str());
      }
    }
  }
}

}  // namespace

BundledFolderManager::BundledFolderManager(Notebook *notebook) : FolderManager(notebook) {
  assert(notebook && notebook->GetType() == NotebookType::Bundled);
  // @notebook is not fully initialized yet.
}

BundledFolderManager::~BundledFolderManager() {}

VxCoreError BundledFolderManager::InitOnCreation() {
  const std::string folder_path(".");
  std::unique_ptr<FolderConfig> root_config;
  VxCoreError err = LoadFolderConfig(folder_path, root_config);
  assert(err == VXCORE_ERR_NOT_FOUND);
  root_config.reset(new FolderConfig(folder_path));
  err = SaveFolderConfig(folder_path, *root_config);
  if (err != VXCORE_OK) {
    VXCORE_LOG_ERROR("Failed to create root folder config: error=%d", err);
    return err;
  }

  // Write-through to MetadataStore: create root folder record
  // Root folder has empty parent_id
  if (auto *store = notebook_->GetMetadataStore()) {
    StoreFolderRecord root_record = ToStoreFolderRecord(*root_config, "");
    if (!store->CreateFolder(root_record)) {
      VXCORE_LOG_WARN("Failed to create root folder in MetadataStore: id=%s",
                      root_config->id.c_str());
    }
  }

  CacheConfig(folder_path, std::move(root_config));
  return VXCORE_OK;
}

std::string BundledFolderManager::GetConfigPath(const std::string &folder_path) const {
  fs::path metadata_folder = notebook_->GetMetadataFolder();
  fs::path config_path = metadata_folder / "contents";

  if (!folder_path.empty() && folder_path != ".") {
    config_path /= folder_path;
  }

  config_path /= "vx.json";
  return config_path.string();
}

std::string BundledFolderManager::GetContentPath(const std::string &folder_path) const {
  fs::path content_path = notebook_->GetRootFolder();

  if (!folder_path.empty() && folder_path != ".") {
    content_path /= folder_path;
  }

  return content_path.string();
}

FolderConfig *BundledFolderManager::GetCachedConfig(const std::string &folder_path) {
  auto it = config_cache_.find(folder_path);
  if (it != config_cache_.end()) {
    return it->second.get();
  }
  return nullptr;
}

void BundledFolderManager::CacheConfig(const std::string &folder_path,
                                       std::unique_ptr<FolderConfig> config) {
  config_cache_[folder_path] = std::move(config);
}

void BundledFolderManager::InvalidateCache(const std::string &folder_path) {
  config_cache_.erase(folder_path);
}

FileRecord *BundledFolderManager::FindFileRecord(FolderConfig &config,
                                                 const std::string &file_name) {
  for (auto &file : config.files) {
    if (file.name == file_name) {
      return &file;
    }
  }
  return nullptr;
}

VxCoreError BundledFolderManager::LoadFolderConfig(const std::string &folder_path,
                                                   std::unique_ptr<FolderConfig> &out_config) {
  out_config.reset();

  std::string config_path = GetConfigPath(folder_path);

  if (!fs::exists(config_path)) {
    return VXCORE_ERR_NOT_FOUND;
  }

  std::ifstream file(config_path);
  if (!file.is_open()) {
    return VXCORE_ERR_IO;
  }

  try {
    nlohmann::json json;
    file >> json;
    out_config = std::make_unique<FolderConfig>(FolderConfig::FromJson(json));
    return VXCORE_OK;
  } catch (const std::exception &) {
    return VXCORE_ERR_JSON_PARSE;
  }
}

VxCoreError BundledFolderManager::SaveFolderConfig(const std::string &folder_path,
                                                   const FolderConfig &config) {
  std::string config_path = GetConfigPath(folder_path);

  fs::path config_file_path(config_path);
  fs::path config_dir = config_file_path.parent_path();

  try {
    if (!fs::exists(config_dir)) {
      fs::create_directories(config_dir);
    }

    std::ofstream file(config_path);
    if (!file.is_open()) {
      return VXCORE_ERR_IO;
    }

    nlohmann::json json = config.ToJson();
    file << json.dump(2);
    file.close();
    return VXCORE_OK;
  } catch (const std::exception &) {
    return VXCORE_ERR_IO;
  }
}

VxCoreError BundledFolderManager::GetFolderConfig(const std::string &folder_path,
                                                  std::string &out_config_json) {
  out_config_json.clear();
  const auto clean_folder_path = GetCleanRelativePath(folder_path);
  FolderConfig *config = nullptr;
  VxCoreError error = GetFolderConfig(clean_folder_path, &config);
  if (error != VXCORE_OK) {
    return error;
  }
  out_config_json = config->ToJson().dump();
  return VXCORE_OK;
}

VxCoreError BundledFolderManager::GetFolderConfig(const std::string &folder_path,
                                                  FolderConfig **out_config,
                                                  const std::string *parent_id) {
  *out_config = nullptr;

  FolderConfig *cached = GetCachedConfig(folder_path);
  if (cached) {
    // Once it is in cache, we assume that it is up-to-date in store.
    *out_config = cached;
    return VXCORE_OK;
  }

  std::unique_ptr<FolderConfig> config;
  VxCoreError error = LoadFolderConfig(folder_path, config);
  if (error != VXCORE_OK) {
    return error;
  }

  *out_config = config.get();
  CacheConfig(folder_path, std::move(config));

  SyncFolderToStore(folder_path, **out_config,
                    parent_id ? *parent_id : GetParentFolderId(folder_path));
  return VXCORE_OK;
}

VxCoreError BundledFolderManager::CreateFolder(const std::string &parent_path,
                                               const std::string &folder_name,
                                               std::string &out_folder_id) {
  VXCORE_LOG_INFO("Creating folder: parent=%s, name=%s", parent_path.c_str(), folder_name.c_str());

  const auto clean_parent_path = GetCleanRelativePath(parent_path);

  std::string content_path = GetContentPath(clean_parent_path);
  fs::path folder_path = fs::path(content_path) / folder_name;

  if (fs::exists(folder_path)) {
    VXCORE_LOG_WARN("Folder already exists: %s", folder_path.string().c_str());
    return VXCORE_ERR_ALREADY_EXISTS;
  }

  FolderConfig *parent_config = nullptr;
  VxCoreError error = GetFolderConfig(clean_parent_path, &parent_config);
  if (error != VXCORE_OK) {
    VXCORE_LOG_ERROR("Failed to get parent folder config: parent=%s, error=%d",
                     clean_parent_path.c_str(), error);
    return error;
  }

  auto it = std::find(parent_config->folders.begin(), parent_config->folders.end(), folder_name);
  if (it != parent_config->folders.end()) {
    VXCORE_LOG_WARN("Folder already exists in config: %s", folder_name.c_str());
    return VXCORE_ERR_ALREADY_EXISTS;
  }

  try {
    fs::create_directories(folder_path);
    VXCORE_LOG_DEBUG("Created folder directory: %s", folder_path.string().c_str());
  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("Failed to create folder directory: %s", e.what());
    return VXCORE_ERR_IO;
  }

  parent_config->folders.push_back(folder_name);
  parent_config->modified_utc = GetCurrentTimestampMillis();
  error = SaveFolderConfig(clean_parent_path, *parent_config);
  if (error != VXCORE_OK) {
    VXCORE_LOG_ERROR("Failed to save parent folder config: error=%d", error);
    return error;
  }

  const auto folder_relative_path = ConcatenatePaths(clean_parent_path, folder_name);
  std::unique_ptr<FolderConfig> new_config(new FolderConfig(folder_name));
  error = SaveFolderConfig(folder_relative_path, *new_config);
  if (error != VXCORE_OK) {
    VXCORE_LOG_ERROR("Failed to save new folder config: error=%d", error);
    return error;
  }

  out_folder_id = new_config->id;

  // Write-through to MetadataStore
  if (auto *store = notebook_->GetMetadataStore()) {
    // Use parent folder's ID (root folder also has an ID now)
    StoreFolderRecord store_record = ToStoreFolderRecord(*new_config, parent_config->id);
    if (!store->CreateFolder(store_record)) {
      VXCORE_LOG_WARN("Failed to write folder to MetadataStore: id=%s", out_folder_id.c_str());
    }
  }

  CacheConfig(folder_relative_path, std::move(new_config));
  VXCORE_LOG_INFO("Folder created successfully: id=%s", out_folder_id.c_str());
  return VXCORE_OK;
}

VxCoreError BundledFolderManager::DeleteFolder(const std::string &folder_path) {
  VXCORE_LOG_INFO("Deleting folder: path=%s", folder_path.c_str());

  const auto clean_folder_path = GetCleanRelativePath(folder_path);

  const std::string content_path = GetContentPath(clean_folder_path);
  const std::string config_path = GetConfigPath(clean_folder_path);

  if (!fs::exists(content_path)) {
    return VXCORE_ERR_NOT_FOUND;
  }

  // Get the folder ID before we invalidate cache and delete
  std::string folder_id;
  FolderConfig *folder_config = nullptr;
  if (GetFolderConfig(clean_folder_path, &folder_config) == VXCORE_OK && folder_config) {
    folder_id = folder_config->id;
  }

  InvalidateCache(clean_folder_path);

  const auto [parent_path, folder_name] = SplitPath(clean_folder_path);
  FolderConfig *parent_config = nullptr;
  VxCoreError error = GetFolderConfig(parent_path, &parent_config);
  if (error != VXCORE_OK) {
    return error;
  }
  auto it = std::find(parent_config->folders.begin(), parent_config->folders.end(), folder_name);
  if (it != parent_config->folders.end()) {
    parent_config->folders.erase(it);
    parent_config->modified_utc = GetCurrentTimestampMillis();
    error = SaveFolderConfig(parent_path, *parent_config);
    if (error != VXCORE_OK) {
      return error;
    }
  }

  try {
    // Move content folder to recycle bin instead of permanent delete
    VxCoreError move_error = MoveToRecycleBin(fs::path(content_path));
    if (move_error != VXCORE_OK) {
      VXCORE_LOG_WARN("Failed to move folder to recycle bin, falling back to delete: %s",
                      content_path.c_str());
      fs::remove_all(content_path);
    }

    // Always permanently delete the config folder (vx.json metadata)
    if (fs::exists(config_path)) {
      fs::remove_all(fs::path(config_path).parent_path());
    }

    // Write-through to MetadataStore
    if (auto *store = notebook_->GetMetadataStore(); store && !folder_id.empty()) {
      if (!store->DeleteFolder(folder_id)) {
        VXCORE_LOG_WARN("Failed to delete folder from MetadataStore: id=%s", folder_id.c_str());
      }
    }

    VXCORE_LOG_INFO("Folder deleted successfully: path=%s", clean_folder_path.c_str());
    return VXCORE_OK;
  } catch (const std::exception &) {
    return VXCORE_ERR_IO;
  }
}

VxCoreError BundledFolderManager::UpdateFolderMetadata(const std::string &folder_path,
                                                       const std::string &metadata_json) {
  const auto clean_folder_path = GetCleanRelativePath(folder_path);

  FolderConfig *config = nullptr;
  VxCoreError error = GetFolderConfig(clean_folder_path, &config);
  if (error != VXCORE_OK) {
    return error;
  }

  try {
    nlohmann::json metadata = nlohmann::json::parse(metadata_json);
    if (!metadata.is_object()) {
      return VXCORE_ERR_JSON_PARSE;
    }

    config->metadata = metadata;
    config->modified_utc = GetCurrentTimestampMillis();

    error = SaveFolderConfig(clean_folder_path, *config);
    if (error != VXCORE_OK) {
      return error;
    }

    // Write-through to MetadataStore
    if (auto *store = notebook_->GetMetadataStore()) {
      if (!store->UpdateFolder(config->id, config->name, config->modified_utc,
                               config->metadata.dump())) {
        VXCORE_LOG_WARN("Failed to update folder in MetadataStore: id=%s", config->id.c_str());
      }
    }

    return VXCORE_OK;
  } catch (const std::exception &) {
    return VXCORE_ERR_JSON_PARSE;
  }
}

VxCoreError BundledFolderManager::GetFolderMetadata(const std::string &folder_path,
                                                    std::string &out_metadata_json) {
  const auto clean_folder_path = GetCleanRelativePath(folder_path);

  FolderConfig *config = nullptr;
  VxCoreError error = GetFolderConfig(clean_folder_path, &config);
  if (error != VXCORE_OK) {
    return error;
  }

  out_metadata_json = config->metadata.dump();
  return VXCORE_OK;
}

VxCoreError BundledFolderManager::RenameFolder(const std::string &folder_path,
                                               const std::string &new_name) {
  VXCORE_LOG_INFO("RenameFolder: folder_path=%s, new_name=%s", folder_path.c_str(),
                  new_name.c_str());

  const auto clean_folder_path = GetCleanRelativePath(folder_path);

  const auto [parent_path, old_name] = SplitPath(clean_folder_path);
  const std::string old_content_path = GetContentPath(clean_folder_path);
  const std::string old_config_path = GetConfigPath(clean_folder_path);

  if (!fs::exists(old_content_path)) {
    return VXCORE_ERR_NOT_FOUND;
  }

  const std::string new_folder_path = ConcatenatePaths(parent_path, new_name);
  const std::string new_content_path = GetContentPath(new_folder_path);
  const std::string new_config_path = GetConfigPath(new_folder_path);

  if (fs::exists(new_content_path)) {
    return VXCORE_ERR_ALREADY_EXISTS;
  }

  FolderConfig *parent_config = nullptr;
  VxCoreError error = GetFolderConfig(parent_path, &parent_config);
  if (error != VXCORE_OK) {
    return error;
  }

  auto it = std::find(parent_config->folders.begin(), parent_config->folders.end(), old_name);
  if (it == parent_config->folders.end()) {
    return VXCORE_ERR_NOT_FOUND;
  }

  FolderConfig *folder_config = nullptr;
  error = GetFolderConfig(clean_folder_path, &folder_config);
  if (error != VXCORE_OK) {
    return error;
  }

  try {
    fs::rename(old_content_path, new_content_path);

    if (fs::exists(old_config_path)) {
      fs::create_directories(fs::path(new_config_path).parent_path());
      fs::rename(old_config_path, new_config_path);
      fs::remove_all(fs::path(old_config_path).parent_path());
    }
  } catch (const std::exception &) {
    return VXCORE_ERR_IO;
  }

  folder_config->name = new_name;
  folder_config->modified_utc = GetCurrentTimestampMillis();
  error = SaveFolderConfig(new_folder_path, *folder_config);
  if (error != VXCORE_OK) {
    return error;
  }

  *it = new_name;
  parent_config->modified_utc = GetCurrentTimestampMillis();
  error = SaveFolderConfig(parent_path, *parent_config);
  if (error != VXCORE_OK) {
    return error;
  }

  // Write-through to MetadataStore (before cache invalidation to avoid dangling pointer)
  if (auto *store = notebook_->GetMetadataStore()) {
    if (!store->UpdateFolder(folder_config->id, folder_config->name, folder_config->modified_utc,
                             folder_config->metadata.dump())) {
      VXCORE_LOG_WARN("Failed to update folder in MetadataStore: id=%s", folder_config->id.c_str());
    }
  }

  InvalidateCache(clean_folder_path);
  InvalidateCache(new_folder_path);

  VXCORE_LOG_INFO("RenameFolder successful: folder renamed from %s to %s",
                  clean_folder_path.c_str(), new_folder_path.c_str());
  return VXCORE_OK;
}

VxCoreError BundledFolderManager::MoveFolder(const std::string &src_path,
                                             const std::string &dest_parent_path) {
  VXCORE_LOG_INFO("MoveFolder: src_path=%s, dest_parent_path=%s", src_path.c_str(),
                  dest_parent_path.c_str());

  const auto clean_src_path = GetCleanRelativePath(src_path);
  const auto clean_dest_parent_path = GetCleanRelativePath(dest_parent_path);

  const auto [src_parent_path, folder_name] = SplitPath(clean_src_path);
  const std::string src_content_path = GetContentPath(clean_src_path);
  const std::string src_config_path = GetConfigPath(clean_src_path);

  if (!fs::exists(src_content_path)) {
    return VXCORE_ERR_NOT_FOUND;
  }

  const std::string dest_path = ConcatenatePaths(clean_dest_parent_path, folder_name);
  const std::string dest_content_path = GetContentPath(dest_path);
  const std::string dest_config_path = GetConfigPath(dest_path);

  if (fs::exists(dest_content_path)) {
    return VXCORE_ERR_ALREADY_EXISTS;
  }

  FolderConfig *src_parent_config = nullptr;
  VxCoreError error = GetFolderConfig(src_parent_path, &src_parent_config);
  if (error != VXCORE_OK) {
    return error;
  }

  FolderConfig *dest_parent_config = nullptr;
  error = GetFolderConfig(clean_dest_parent_path, &dest_parent_config);
  if (error != VXCORE_OK) {
    return error;
  }

  // Get folder config for its ID (needed for MetadataStore)
  FolderConfig *folder_config = nullptr;
  error = GetFolderConfig(clean_src_path, &folder_config);
  if (error != VXCORE_OK) {
    return error;
  }

  auto it =
      std::find(src_parent_config->folders.begin(), src_parent_config->folders.end(), folder_name);
  if (it == src_parent_config->folders.end()) {
    return VXCORE_ERR_NOT_FOUND;
  }

  auto dest_it = std::find(dest_parent_config->folders.begin(), dest_parent_config->folders.end(),
                           folder_name);
  if (dest_it != dest_parent_config->folders.end()) {
    return VXCORE_ERR_ALREADY_EXISTS;
  }

  try {
    fs::create_directories(fs::path(dest_content_path).parent_path());
    fs::rename(src_content_path, dest_content_path);

    if (fs::exists(src_config_path)) {
      const fs::path src_config_dir = fs::path(src_config_path).parent_path();
      const fs::path dest_config_dir = fs::path(dest_config_path).parent_path();
      fs::create_directories(dest_config_dir.parent_path());
      fs::rename(src_config_dir, dest_config_dir);
    }
  } catch (const std::exception &) {
    return VXCORE_ERR_IO;
  }

  src_parent_config->folders.erase(it);
  src_parent_config->modified_utc = GetCurrentTimestampMillis();
  error = SaveFolderConfig(src_parent_path, *src_parent_config);
  if (error != VXCORE_OK) {
    return error;
  }

  dest_parent_config->folders.push_back(folder_name);
  dest_parent_config->modified_utc = GetCurrentTimestampMillis();
  error = SaveFolderConfig(clean_dest_parent_path, *dest_parent_config);
  if (error != VXCORE_OK) {
    return error;
  }

  // Write-through to MetadataStore (before cache invalidation to avoid dangling pointer)
  if (auto *store = notebook_->GetMetadataStore()) {
    // Use dest parent's ID (root folder also has an ID now)
    if (!store->MoveFolder(folder_config->id, dest_parent_config->id)) {
      VXCORE_LOG_WARN("Failed to move folder in MetadataStore: id=%s", folder_config->id.c_str());
    }
  }

  InvalidateCache(clean_src_path);
  InvalidateCache(dest_path);

  VXCORE_LOG_INFO("MoveFolder successful: folder moved from %s to %s", clean_src_path.c_str(),
                  dest_path.c_str());
  return VXCORE_OK;
}

VxCoreError BundledFolderManager::CopyFolder(const std::string &src_path,
                                             const std::string &dest_parent_path,
                                             const std::string &new_name,
                                             std::string &out_folder_id) {
  const auto clean_src_path = GetCleanRelativePath(src_path);
  const auto clean_dest_parent_path = GetCleanRelativePath(dest_parent_path);

  const auto [src_parent_path, src_folder_name] = SplitPath(clean_src_path);
  const std::string src_content_path = GetContentPath(clean_src_path);
  const std::string folder_name = new_name.empty() ? src_folder_name : new_name;

  VXCORE_LOG_INFO("Copying folder: src=%s, dest=%s, new_name=%s", clean_src_path.c_str(),
                  clean_dest_parent_path.c_str(), folder_name.c_str());

  if (!fs::exists(src_content_path)) {
    return VXCORE_ERR_NOT_FOUND;
  }

  const std::string dest_path = ConcatenatePaths(clean_dest_parent_path, folder_name);
  const std::string dest_content_path = GetContentPath(dest_path);

  if (fs::exists(dest_content_path)) {
    return VXCORE_ERR_ALREADY_EXISTS;
  }

  FolderConfig *dest_parent_config = nullptr;
  VxCoreError error = GetFolderConfig(clean_dest_parent_path, &dest_parent_config);
  if (error != VXCORE_OK) {
    return error;
  }

  auto dest_it = std::find(dest_parent_config->folders.begin(), dest_parent_config->folders.end(),
                           folder_name);
  if (dest_it != dest_parent_config->folders.end()) {
    return VXCORE_ERR_ALREADY_EXISTS;
  }

  FolderConfig *src_config = nullptr;
  error = GetFolderConfig(clean_src_path, &src_config);
  if (error != VXCORE_OK) {
    return error;
  }

  try {
    fs::copy(src_content_path, dest_content_path, fs::copy_options::recursive);
  } catch (const std::exception &) {
    return VXCORE_ERR_IO;
  }

  std::unique_ptr<FolderConfig> dest_config = std::make_unique<FolderConfig>(*src_config);
  dest_config->id = GenerateUUID();
  dest_config->name = folder_name;
  dest_config->created_utc = GetCurrentTimestampMillis();
  dest_config->modified_utc = dest_config->created_utc;

  for (auto &file : dest_config->files) {
    file.id = GenerateUUID();
    file.created_utc = dest_config->created_utc;
    file.modified_utc = dest_config->created_utc;
  }

  out_folder_id = dest_config->id;

  error = SaveFolderConfig(dest_path, *dest_config);
  if (error != VXCORE_OK) {
    return error;
  }

  dest_parent_config->folders.push_back(folder_name);
  dest_parent_config->modified_utc = GetCurrentTimestampMillis();
  error = SaveFolderConfig(clean_dest_parent_path, *dest_parent_config);
  if (error != VXCORE_OK) {
    return error;
  }

  // Write-through to MetadataStore
  if (auto *store = notebook_->GetMetadataStore()) {
    // Use dest parent's ID (root folder also has an ID now)
    StoreFolderRecord folder_record = ToStoreFolderRecord(*dest_config, dest_parent_config->id);
    if (!store->CreateFolder(folder_record)) {
      VXCORE_LOG_WARN("Failed to create folder in MetadataStore: id=%s", out_folder_id.c_str());
    }

    // Create all files in the folder
    for (const auto &file : dest_config->files) {
      StoreFileRecord file_record = ToStoreFileRecord(file, dest_config->id);
      if (!store->CreateFile(file_record)) {
        VXCORE_LOG_WARN("Failed to create file in MetadataStore: id=%s", file.id.c_str());
      }
    }
  }

  CacheConfig(dest_path, std::move(dest_config));

  VXCORE_LOG_INFO("Folder copied successfully: id=%s", out_folder_id.c_str());
  return VXCORE_OK;
}

VxCoreError BundledFolderManager::CreateFile(const std::string &folder_path,
                                             const std::string &file_name,
                                             std::string &out_file_id) {
  VXCORE_LOG_INFO("Creating file: folder=%s, name=%s", folder_path.c_str(), file_name.c_str());

  const auto clean_folder_path = GetCleanRelativePath(folder_path);

  std::string content_path = GetContentPath(clean_folder_path);
  fs::path file_path = fs::path(content_path) / file_name;

  if (fs::exists(file_path)) {
    VXCORE_LOG_WARN("File already exists: %s", file_path.string().c_str());
    return VXCORE_ERR_ALREADY_EXISTS;
  }

  FolderConfig *config = nullptr;
  VxCoreError error = GetFolderConfig(clean_folder_path, &config);
  if (error != VXCORE_OK) {
    VXCORE_LOG_ERROR("Failed to get folder config: folder=%s, error=%d", clean_folder_path.c_str(),
                     error);
    return error;
  }

  if (FindFileRecord(*config, file_name)) {
    VXCORE_LOG_WARN("File already exists in config: %s", file_name.c_str());
    return VXCORE_ERR_ALREADY_EXISTS;
  }

  try {
    std::ofstream(file_path).close();
    VXCORE_LOG_DEBUG("Created file: %s", file_path.string().c_str());
  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("Failed to create file: %s", e.what());
    return VXCORE_ERR_IO;
  }

  const auto ts = GetCurrentTimestampMillis();
  config->files.emplace_back(file_name);
  config->files.back().created_utc = ts;
  config->files.back().modified_utc = ts;
  config->modified_utc = ts;

  error = SaveFolderConfig(clean_folder_path, *config);
  if (error != VXCORE_OK) {
    return error;
  }

  out_file_id = config->files.back().id;

  // Write-through to MetadataStore
  if (auto *store = notebook_->GetMetadataStore()) {
    StoreFileRecord file_record = ToStoreFileRecord(config->files.back(), config->id);
    if (!store->CreateFile(file_record)) {
      VXCORE_LOG_WARN("Failed to create file in MetadataStore: id=%s", out_file_id.c_str());
    }
  }

  return VXCORE_OK;
}

VxCoreError BundledFolderManager::DeleteFile(const std::string &file_path) {
  const auto clean_file_path = GetCleanRelativePath(file_path);

  const auto [folder_path, file_name] = SplitPath(clean_file_path);
  VXCORE_LOG_INFO("DeleteFile: file_path=%s", clean_file_path.c_str());

  FolderConfig *config = nullptr;
  VxCoreError error = GetFolderConfig(folder_path, &config);
  if (error != VXCORE_OK) {
    return error;
  }

  auto it = std::find_if(config->files.begin(), config->files.end(),
                         [&file_name](const FileRecord &file) { return file.name == file_name; });

  if (it == config->files.end()) {
    return VXCORE_ERR_NOT_FOUND;
  }

  // Save the file ID before erasing
  std::string file_id = it->id;

  config->files.erase(it);
  config->modified_utc = GetCurrentTimestampMillis();
  error = SaveFolderConfig(folder_path, *config);
  if (error != VXCORE_OK) {
    return error;
  }

  try {
    std::string content_path = GetContentPath(folder_path);
    fs::path fs_file_path = fs::path(content_path) / file_name;

    // Move to recycle bin instead of permanent delete
    VxCoreError move_error = MoveToRecycleBin(fs_file_path);
    if (move_error != VXCORE_OK) {
      VXCORE_LOG_WARN("Failed to move file to recycle bin, falling back to delete: %s",
                      fs_file_path.string().c_str());
      fs::remove(fs_file_path);
    }

    // Write-through to MetadataStore
    if (auto *store = notebook_->GetMetadataStore(); !file_id.empty()) {
      if (!store->DeleteFile(file_id)) {
        VXCORE_LOG_WARN("Failed to delete file from MetadataStore: id=%s", file_id.c_str());
      }
    }

    VXCORE_LOG_INFO("DeleteFile successful: file %s deleted", clean_file_path.c_str());
    return VXCORE_OK;
  } catch (const std::exception &) {
    return VXCORE_ERR_IO;
  }
}

VxCoreError BundledFolderManager::UpdateFileMetadata(const std::string &file_path,
                                                     const std::string &metadata_json) {
  const auto clean_file_path = GetCleanRelativePath(file_path);

  const auto [folder_path, file_name] = SplitPath(clean_file_path);
  FolderConfig *config = nullptr;
  VxCoreError error = GetFolderConfig(folder_path, &config);
  if (error != VXCORE_OK) {
    return error;
  }

  FileRecord *file = FindFileRecord(*config, file_name);
  if (!file) {
    return VXCORE_ERR_NOT_FOUND;
  }

  try {
    nlohmann::json metadata = nlohmann::json::parse(metadata_json);
    if (!metadata.is_object()) {
      return VXCORE_ERR_JSON_PARSE;
    }

    file->metadata = metadata;
    file->modified_utc = GetCurrentTimestampMillis();

    config->modified_utc = file->modified_utc;

    error = SaveFolderConfig(folder_path, *config);
    if (error != VXCORE_OK) {
      return error;
    }

    // Write-through to MetadataStore
    if (auto *store = notebook_->GetMetadataStore()) {
      if (!store->UpdateFile(file->id, file->name, file->modified_utc, file->metadata.dump())) {
        VXCORE_LOG_WARN("Failed to update file in MetadataStore: id=%s", file->id.c_str());
      }
    }

    return VXCORE_OK;
  } catch (const std::exception &) {
    return VXCORE_ERR_JSON_PARSE;
  }
}

VxCoreError BundledFolderManager::UpdateFileTags(const std::string &file_path,
                                                 const std::string &tags_json) {
  const auto clean_file_path = GetCleanRelativePath(file_path);

  const auto [folder_path, file_name] = SplitPath(clean_file_path);
  FolderConfig *config = nullptr;
  VxCoreError error = GetFolderConfig(folder_path, &config);
  if (error != VXCORE_OK) {
    return error;
  }

  FileRecord *file = FindFileRecord(*config, file_name);
  if (!file) {
    return VXCORE_ERR_NOT_FOUND;
  }

  try {
    nlohmann::json tags = nlohmann::json::parse(tags_json);
    if (!tags.is_array()) {
      return VXCORE_ERR_JSON_PARSE;
    }

    std::vector<std::string> new_tags = tags.get<std::vector<std::string>>();

    for (const auto &tag_name : new_tags) {
      if (!notebook_->FindTag(tag_name)) {
        return VXCORE_ERR_INVALID_PARAM;
      }
    }

    file->tags = new_tags;
    file->modified_utc = GetCurrentTimestampMillis();

    config->modified_utc = file->modified_utc;

    error = SaveFolderConfig(folder_path, *config);
    if (error != VXCORE_OK) {
      return error;
    }

    // Write-through to MetadataStore
    if (auto *store = notebook_->GetMetadataStore()) {
      if (!store->SetFileTags(file->id, file->tags)) {
        VXCORE_LOG_WARN("Failed to set file tags in MetadataStore: id=%s", file->id.c_str());
      }
    }

    return VXCORE_OK;
  } catch (const std::exception &) {
    return VXCORE_ERR_JSON_PARSE;
  }
}

VxCoreError BundledFolderManager::TagFile(const std::string &file_path,
                                          const std::string &tag_name) {
  const auto clean_file_path = GetCleanRelativePath(file_path);

  const auto [folder_path, file_name] = SplitPath(clean_file_path);
  FolderConfig *config = nullptr;
  VxCoreError error = GetFolderConfig(folder_path, &config);
  if (error != VXCORE_OK) {
    return error;
  }

  FileRecord *file = FindFileRecord(*config, file_name);
  if (!file) {
    return VXCORE_ERR_NOT_FOUND;
  }

  if (!notebook_->FindTag(tag_name)) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  auto it = std::find(file->tags.begin(), file->tags.end(), tag_name);
  if (it != file->tags.end()) {
    return VXCORE_ERR_ALREADY_EXISTS;
  }

  file->tags.push_back(tag_name);
  file->modified_utc = GetCurrentTimestampMillis();
  config->modified_utc = file->modified_utc;

  error = SaveFolderConfig(folder_path, *config);
  if (error != VXCORE_OK) {
    return error;
  }

  // Write-through to MetadataStore
  if (auto *store = notebook_->GetMetadataStore()) {
    if (!store->AddTagToFile(file->id, tag_name)) {
      VXCORE_LOG_WARN("Failed to add tag to file in MetadataStore: id=%s, tag=%s", file->id.c_str(),
                      tag_name.c_str());
    }
  }

  return VXCORE_OK;
}

VxCoreError BundledFolderManager::UntagFile(const std::string &file_path,
                                            const std::string &tag_name) {
  const auto clean_file_path = GetCleanRelativePath(file_path);

  const auto [folder_path, file_name] = SplitPath(clean_file_path);
  FolderConfig *config = nullptr;
  VxCoreError error = GetFolderConfig(folder_path, &config);
  if (error != VXCORE_OK) {
    return error;
  }

  FileRecord *file = FindFileRecord(*config, file_name);
  if (!file) {
    return VXCORE_ERR_NOT_FOUND;
  }

  auto it = std::find(file->tags.begin(), file->tags.end(), tag_name);
  if (it == file->tags.end()) {
    return VXCORE_ERR_NOT_FOUND;
  }

  file->tags.erase(it);
  file->modified_utc = GetCurrentTimestampMillis();
  config->modified_utc = file->modified_utc;

  error = SaveFolderConfig(folder_path, *config);
  if (error != VXCORE_OK) {
    return error;
  }

  // Write-through to MetadataStore
  if (auto *store = notebook_->GetMetadataStore()) {
    if (!store->RemoveTagFromFile(file->id, tag_name)) {
      VXCORE_LOG_WARN("Failed to remove tag from file in MetadataStore: id=%s, tag=%s",
                      file->id.c_str(), tag_name.c_str());
    }
  }

  return VXCORE_OK;
}

VxCoreError BundledFolderManager::GetFileInfo(const std::string &file_path,
                                              std::string &out_file_info_json) {
  const FileRecord *record = nullptr;
  VxCoreError error = GetFileInfo(file_path, &record);
  if (error != VXCORE_OK) {
    return error;
  }
  out_file_info_json = record->ToJson().dump();
  return VXCORE_OK;
}

VxCoreError BundledFolderManager::GetFileInfo(const std::string &file_path,
                                              const FileRecord **out_record) {
  *out_record = nullptr;

  const auto clean_file_path = GetCleanRelativePath(file_path);
  const auto [folder_path, file_name] = SplitPath(clean_file_path);

  FolderConfig *config = nullptr;
  VxCoreError error = GetFolderConfig(folder_path, &config);
  if (error != VXCORE_OK) {
    return error;
  }

  FileRecord *file = FindFileRecord(*config, file_name);
  if (!file) {
    return VXCORE_ERR_NOT_FOUND;
  }

  *out_record = file;
  return VXCORE_OK;
}

VxCoreError BundledFolderManager::GetFileMetadata(const std::string &file_path,
                                                  std::string &out_metadata_json) {
  const auto clean_file_path = GetCleanRelativePath(file_path);

  const auto [folder_path, file_name] = SplitPath(clean_file_path);
  FolderConfig *config = nullptr;
  VxCoreError error = GetFolderConfig(folder_path, &config);
  if (error != VXCORE_OK) {
    return error;
  }

  FileRecord *file = FindFileRecord(*config, file_name);
  if (!file) {
    return VXCORE_ERR_NOT_FOUND;
  }

  out_metadata_json = file->metadata.dump();
  return VXCORE_OK;
}

VxCoreError BundledFolderManager::RenameFile(const std::string &file_path,
                                             const std::string &new_name) {
  const auto clean_file_path = GetCleanRelativePath(file_path);

  const auto [folder_path, old_name] = SplitPath(clean_file_path);
  VXCORE_LOG_INFO("RenameFile: file_path=%s, new_name=%s", clean_file_path.c_str(),
                  new_name.c_str());

  FolderConfig *config = nullptr;
  VxCoreError error = GetFolderConfig(folder_path, &config);
  if (error != VXCORE_OK) {
    return error;
  }

  FileRecord *file = FindFileRecord(*config, old_name);
  if (!file) {
    return VXCORE_ERR_NOT_FOUND;
  }

  FileRecord *existing_file = FindFileRecord(*config, new_name);
  if (existing_file) {
    return VXCORE_ERR_ALREADY_EXISTS;
  }

  const std::string content_path = GetContentPath(folder_path);
  fs::path old_file_path = fs::path(content_path) / old_name;
  fs::path new_file_path = fs::path(content_path) / new_name;

  if (!fs::exists(old_file_path)) {
    return VXCORE_ERR_NOT_FOUND;
  }

  if (fs::exists(new_file_path)) {
    return VXCORE_ERR_ALREADY_EXISTS;
  }

  try {
    fs::rename(old_file_path, new_file_path);
  } catch (const std::exception &) {
    return VXCORE_ERR_IO;
  }

  file->name = new_name;
  file->modified_utc = GetCurrentTimestampMillis();
  config->modified_utc = file->modified_utc;

  error = SaveFolderConfig(folder_path, *config);
  if (error != VXCORE_OK) {
    return error;
  }

  // Write-through to MetadataStore
  if (auto *store = notebook_->GetMetadataStore()) {
    if (!store->UpdateFile(file->id, file->name, file->modified_utc, file->metadata.dump())) {
      VXCORE_LOG_WARN("Failed to update file in MetadataStore: id=%s", file->id.c_str());
    }
  }

  VXCORE_LOG_INFO("RenameFile successful: file renamed from %s to %s", clean_file_path.c_str(),
                  ConcatenatePaths(folder_path, new_name).c_str());
  return VXCORE_OK;
}

VxCoreError BundledFolderManager::MoveFile(const std::string &src_file_path,
                                           const std::string &dest_folder_path) {
  const auto clean_src_file_path = GetCleanRelativePath(src_file_path);
  const auto clean_dest_folder_path = GetCleanRelativePath(dest_folder_path);

  const auto [src_folder_path, file_name] = SplitPath(clean_src_file_path);
  VXCORE_LOG_INFO("MoveFile: src_file_path=%s, dest_folder_path=%s", clean_src_file_path.c_str(),
                  clean_dest_folder_path.c_str());

  FolderConfig *src_config = nullptr;
  VxCoreError error = GetFolderConfig(src_folder_path, &src_config);
  if (error != VXCORE_OK) {
    return error;
  }

  FileRecord *file = FindFileRecord(*src_config, file_name);
  if (!file) {
    return VXCORE_ERR_NOT_FOUND;
  }

  FolderConfig *dest_config = nullptr;
  error = GetFolderConfig(clean_dest_folder_path, &dest_config);
  if (error != VXCORE_OK) {
    return error;
  }

  FileRecord *existing_file = FindFileRecord(*dest_config, file_name);
  if (existing_file) {
    return VXCORE_ERR_ALREADY_EXISTS;
  }

  const std::string src_content_path = GetContentPath(src_folder_path);
  const std::string dest_content_path = GetContentPath(dest_folder_path);
  fs::path src_fs_path = fs::path(src_content_path) / file_name;
  fs::path dest_fs_path = fs::path(dest_content_path) / file_name;

  if (!fs::exists(src_fs_path)) {
    return VXCORE_ERR_NOT_FOUND;
  }

  if (fs::exists(dest_fs_path)) {
    return VXCORE_ERR_ALREADY_EXISTS;
  }

  try {
    fs::rename(src_fs_path, dest_fs_path);
  } catch (const std::exception &) {
    return VXCORE_ERR_IO;
  }

  FileRecord file_copy = *file;
  file_copy.modified_utc = GetCurrentTimestampMillis();

  auto it = std::find_if(src_config->files.begin(), src_config->files.end(),
                         [&file_name](const FileRecord &f) { return f.name == file_name; });
  src_config->files.erase(it);
  src_config->modified_utc = file_copy.modified_utc;
  error = SaveFolderConfig(src_folder_path, *src_config);
  if (error != VXCORE_OK) {
    return error;
  }

  dest_config->files.push_back(file_copy);
  dest_config->modified_utc = file_copy.modified_utc;
  error = SaveFolderConfig(clean_dest_folder_path, *dest_config);
  if (error != VXCORE_OK) {
    return error;
  }

  // Write-through to MetadataStore
  if (auto *store = notebook_->GetMetadataStore()) {
    if (!store->MoveFile(file_copy.id, dest_config->id)) {
      VXCORE_LOG_WARN("Failed to move file in MetadataStore: id=%s", file_copy.id.c_str());
    }
  }

  VXCORE_LOG_INFO("MoveFile successful: file moved from %s to %s", clean_src_file_path.c_str(),
                  ConcatenatePaths(clean_dest_folder_path, file_name).c_str());
  return VXCORE_OK;
}

VxCoreError BundledFolderManager::CopyFile(const std::string &src_file_path,
                                           const std::string &dest_folder_path,
                                           const std::string &new_name, std::string &out_file_id) {
  const auto clean_src_file_path = GetCleanRelativePath(src_file_path);
  const auto clean_dest_folder_path = GetCleanRelativePath(dest_folder_path);

  const auto [src_folder_path, file_name] = SplitPath(clean_src_file_path);
  const std::string target_name = new_name.empty() ? file_name : new_name;
  VXCORE_LOG_INFO("Copying file: src=%s, dest=%s, new_name=%s", clean_src_file_path.c_str(),
                  clean_dest_folder_path.c_str(), target_name.c_str());

  FolderConfig *src_config = nullptr;
  VxCoreError error = GetFolderConfig(src_folder_path, &src_config);
  if (error != VXCORE_OK) {
    return error;
  }

  FileRecord *file = FindFileRecord(*src_config, file_name);
  if (!file) {
    return VXCORE_ERR_NOT_FOUND;
  }

  FolderConfig *dest_config = nullptr;
  error = GetFolderConfig(clean_dest_folder_path, &dest_config);
  if (error != VXCORE_OK) {
    return error;
  }

  FileRecord *existing_file = FindFileRecord(*dest_config, target_name);
  if (existing_file) {
    return VXCORE_ERR_ALREADY_EXISTS;
  }

  const std::string src_content_path = GetContentPath(src_folder_path);
  const std::string dest_content_path = GetContentPath(clean_dest_folder_path);
  fs::path src_fs_path = fs::path(src_content_path) / file_name;
  fs::path dest_fs_path = fs::path(dest_content_path) / target_name;

  if (!fs::exists(src_fs_path)) {
    return VXCORE_ERR_NOT_FOUND;
  }

  if (fs::exists(dest_fs_path)) {
    return VXCORE_ERR_ALREADY_EXISTS;
  }

  try {
    fs::copy_file(src_fs_path, dest_fs_path);
  } catch (const std::exception &) {
    return VXCORE_ERR_IO;
  }

  FileRecord new_file = *file;
  new_file.id = GenerateUUID();
  new_file.name = target_name;
  new_file.created_utc = GetCurrentTimestampMillis();
  new_file.modified_utc = new_file.created_utc;

  out_file_id = new_file.id;

  dest_config->files.push_back(new_file);
  dest_config->modified_utc = new_file.created_utc;
  error = SaveFolderConfig(clean_dest_folder_path, *dest_config);
  if (error != VXCORE_OK) {
    return error;
  }

  // Write-through to MetadataStore
  if (auto *store = notebook_->GetMetadataStore()) {
    StoreFileRecord file_record = ToStoreFileRecord(new_file, dest_config->id);
    if (!store->CreateFile(file_record)) {
      VXCORE_LOG_WARN("Failed to create file in MetadataStore: id=%s", out_file_id.c_str());
    }
  }

  VXCORE_LOG_INFO("File copied successfully: id=%s", out_file_id.c_str());
  return VXCORE_OK;
}

void BundledFolderManager::IterateAllFiles(
    std::function<bool(const std::string &, const FileRecord &)> callback) {
  std::function<bool(const std::string &)> iterate_folder = [&](const std::string &fp) {
    FolderConfig *config = nullptr;
    VxCoreError error = GetFolderConfig(fp, &config);
    if (error != VXCORE_OK) {
      return true;
    }

    for (const auto &file : config->files) {
      if (!callback(fp, file)) {
        return false;
      }
    }

    for (const auto &folder_name : config->folders) {
      std::string child_path = ConcatenatePaths(fp, folder_name);
      if (!iterate_folder(child_path)) {
        return false;
      }
    }

    return true;
  };

  iterate_folder(".");
}

VxCoreError BundledFolderManager::FindFilesByTag(const std::string &tag_name,
                                                 std::string &out_files_json) {
  nlohmann::json files_array = nlohmann::json::array();

  IterateAllFiles(
      [&tag_name, &files_array](const std::string &folder_path, const FileRecord &file) {
        (void)folder_path;
        if (std::find(file.tags.begin(), file.tags.end(), tag_name) != file.tags.end()) {
          files_array.push_back(file.ToJson());
        }
        return true;
      });

  out_files_json = files_array.dump();
  return VXCORE_OK;
}

VxCoreError BundledFolderManager::ListFolderContents(const std::string &folder_path,
                                                     bool include_folders_info,
                                                     FolderContents &out_contents) {
  FolderConfig *config = nullptr;
  VxCoreError error = GetFolderConfig(folder_path, &config);
  if (error != VXCORE_OK) {
    return error;
  }

  out_contents.files = config->files;
  out_contents.folders.clear();
  out_contents.folders.reserve(config->folders.size());
  for (const auto &subfolder_name : config->folders) {
    if (include_folders_info == false) {
      out_contents.folders.emplace_back(subfolder_name);
      continue;
    }
    std::string subfolder_path = ConcatenatePaths(folder_path, subfolder_name);
    FolderConfig *subfolder_config = nullptr;
    error = GetFolderConfig(subfolder_path, &subfolder_config);
    if (error == VXCORE_OK && subfolder_config) {
      out_contents.folders.emplace_back(subfolder_config->id, subfolder_config->name,
                                        subfolder_config->created_utc,
                                        subfolder_config->modified_utc, subfolder_config->metadata);
    }
  }

  return VXCORE_OK;
}

void BundledFolderManager::ClearCache() { config_cache_.clear(); }

VxCoreError BundledFolderManager::SyncMetadataStoreFromConfigs() {
  auto *store = notebook_->GetMetadataStore();
  if (!store) {
    VXCORE_LOG_ERROR("SyncMetadataStoreFromConfigs: MetadataStore not available");
    return VXCORE_ERR_INVALID_STATE;
  }

  VXCORE_LOG_INFO("SyncMetadataStoreFromConfigs: Starting sync from config files");

  // Rebuild the store (clears all data and re-initializes schema)
  if (!store->RebuildAll()) {
    VXCORE_LOG_ERROR("SyncMetadataStoreFromConfigs: Failed to rebuild store");
    return VXCORE_ERR_IO;
  }

  // Begin transaction for bulk inserts
  if (!store->BeginTransaction()) {
    VXCORE_LOG_WARN("SyncMetadataStoreFromConfigs: Failed to begin transaction");
  }

  // Helper to recursively sync folders
  // Returns true on success, false on failure
  std::function<bool(const std::string &, const std::string &)> sync_folder =
      [&](const std::string &folder_path, const std::string &parent_folder_id) -> bool {
    FolderConfig *config = nullptr;
    VxCoreError error = GetFolderConfig(folder_path, &config);
    if (error != VXCORE_OK) {
      VXCORE_LOG_WARN("SyncMetadataStoreFromConfigs: Failed to load config for folder: %s",
                      folder_path.c_str());
      return false;
    }

    // Create folder record in store
    StoreFolderRecord folder_record = ToStoreFolderRecord(*config, parent_folder_id);
    if (!store->CreateFolder(folder_record)) {
      VXCORE_LOG_WARN("SyncMetadataStoreFromConfigs: Failed to create folder in store: %s",
                      config->id.c_str());
      // Continue anyway - best effort
    }

    // Create file records in store
    for (const auto &file : config->files) {
      StoreFileRecord file_record = ToStoreFileRecord(file, config->id);
      if (!store->CreateFile(file_record)) {
        VXCORE_LOG_WARN("SyncMetadataStoreFromConfigs: Failed to create file in store: %s",
                        file.id.c_str());
        // Continue anyway - best effort
      }
    }

    // Recursively sync subfolders
    for (const auto &subfolder_name : config->folders) {
      std::string subfolder_path = ConcatenatePaths(folder_path, subfolder_name);
      if (!sync_folder(subfolder_path, config->id)) {
        // Log but continue with other subfolders
        VXCORE_LOG_WARN("SyncMetadataStoreFromConfigs: Failed to sync subfolder: %s",
                        subfolder_path.c_str());
      }
    }

    return true;
  };

  // Start from root folder with empty parent_id
  bool success = sync_folder(".", "");

  // Commit transaction
  if (!store->CommitTransaction()) {
    VXCORE_LOG_WARN("SyncMetadataStoreFromConfigs: Failed to commit transaction");
  }

  if (success) {
    VXCORE_LOG_INFO("SyncMetadataStoreFromConfigs: Sync completed successfully");
    return VXCORE_OK;
  } else {
    VXCORE_LOG_WARN("SyncMetadataStoreFromConfigs: Sync completed with warnings");
    return VXCORE_OK;  // Return OK since we did best-effort sync
  }
}

std::string BundledFolderManager::GetParentFolderId(const std::string &folder_path) {
  if (folder_path.empty() || folder_path == ".") {
    return "";  // Root folder has no parent
  }

  const auto [parent_path, folder_name] = SplitPath(folder_path);
  (void)folder_name;  // Unused

  FolderConfig *parent_config = nullptr;
  VxCoreError error = GetFolderConfig(parent_path, &parent_config);
  if (error == VXCORE_OK && parent_config) {
    return parent_config->id;
  }

  VXCORE_LOG_ERROR("GetParentFolderId: Parent folder not found: %s", parent_path.c_str());
  return "";  // Parent not found
}

void BundledFolderManager::SyncFolderToStore(const std::string &folder_path,
                                             const FolderConfig &config,
                                             const std::string &parent_folder_id) {
  auto *store = notebook_->GetMetadataStore();
  if (!store) {
    return;  // No store available
  }

  // Check staleness by comparing config.modified_utc against stored folder's modified_utc
  auto db_folder = store->GetFolder(config.id);
  if (db_folder.has_value() && config.modified_utc <= db_folder->modified_utc) {
    // Folder is up-to-date, no sync needed
    VXCORE_LOG_DEBUG("SyncFolderToStore: Folder up-to-date: id=%s, path=%s", config.id.c_str(),
                     folder_path.c_str());
    return;
  }

  VXCORE_LOG_INFO("SyncFolderToStore: Syncing folder: id=%s, path=%s", config.id.c_str(),
                  folder_path.c_str());

  // Reuse db_folder from staleness check (folder exists if db_folder has value)
  if (db_folder.has_value()) {
    // Update existing folder
    if (!store->UpdateFolder(config.id, config.name, config.modified_utc, config.metadata.dump())) {
      VXCORE_LOG_WARN("SyncFolderToStore: Failed to update folder: id=%s", config.id.c_str());
      return;  // Skip processing its children
    }
  } else {
    // Create new folder
    StoreFolderRecord folder_record = ToStoreFolderRecord(config, parent_folder_id);
    if (!store->CreateFolder(folder_record)) {
      VXCORE_LOG_WARN("SyncFolderToStore: Failed to create folder in store: id=%s",
                      config.id.c_str());
      return;  // Can't proceed without folder
    }
  }

  // Sync files (add/update/delete orphans)
  SyncFilesToStore(store, config.id, config.files);

  // Recursively sync subfolders
  // NOTE: Use LoadFolderConfig (not GetFolderConfig) to avoid infinite recursion.
  // GetFolderConfig triggers lazy sync which calls SyncFolderToStore again.
  for (const auto &subfolder_name : config.folders) {
    std::string subfolder_path = ConcatenatePaths(folder_path, subfolder_name);
    FolderConfig *subfolder_config;
    // Will call SyncFolderToStore recursively if necessary.
    VxCoreError error = GetFolderConfig(subfolder_path, &subfolder_config, &config.id);
    if (error != VXCORE_OK || !subfolder_config) {
      VXCORE_LOG_WARN("SyncFolderToStore: Failed to load subfolder config: %s",
                      subfolder_path.c_str());
      continue;  // Best-effort: skip this subfolder
    }
  }
  VXCORE_LOG_DEBUG("SyncFolderToStore: Completed sync for folder: id=%s, path=%s",
                   config.id.c_str(), folder_path.c_str());
}


std::string BundledFolderManager::GetRecycleBinPath() const {
  return ConcatenatePaths(notebook_->GetMetadataFolder(), "recycle_bin");
}

std::string BundledFolderManager::GenerateUniqueRecycleBinName(const std::string &name) const {
  fs::path recycle_bin_path(GetRecycleBinPath());
  fs::path dest_path = recycle_bin_path / name;

  if (!fs::exists(dest_path)) {
    return name;
  }

  // Extract base name and extension
  std::string base_name = name;
  std::string extension;
  size_t dot_pos = name.find_last_of('.');
  if (dot_pos != std::string::npos && dot_pos > 0) {
    base_name = name.substr(0, dot_pos);
    extension = name.substr(dot_pos);
  }

  // Try _1, _2, _3, etc. until we find a unique name
  int suffix = 1;
  while (true) {
    std::string new_name = base_name + "_" + std::to_string(suffix) + extension;
    dest_path = recycle_bin_path / new_name;
    if (!fs::exists(dest_path)) {
      return new_name;
    }
    ++suffix;
    if (suffix > 10000) {
      // Safety limit to prevent infinite loop
      VXCORE_LOG_ERROR("GenerateUniqueRecycleBinName: Too many conflicts for %s", name.c_str());
      return base_name + "_" + std::to_string(GetCurrentTimestampMillis()) + extension;
    }
  }
}

VxCoreError BundledFolderManager::MoveToRecycleBin(const std::filesystem::path &source_path) {
  if (!fs::exists(source_path)) {
    VXCORE_LOG_WARN("MoveToRecycleBin: Source does not exist: %s", source_path.string().c_str());
    return VXCORE_ERR_NOT_FOUND;
  }

  fs::path recycle_bin_path(GetRecycleBinPath());

  // Ensure recycle bin directory exists
  try {
    if (!fs::exists(recycle_bin_path)) {
      fs::create_directories(recycle_bin_path);
      VXCORE_LOG_INFO("MoveToRecycleBin: Created recycle bin at %s",
                      recycle_bin_path.string().c_str());
    }
  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("MoveToRecycleBin: Failed to create recycle bin: %s", e.what());
    return VXCORE_ERR_IO;
  }

  // Generate unique name in recycle bin
  std::string original_name = source_path.filename().string();
  std::string unique_name = GenerateUniqueRecycleBinName(original_name);
  fs::path dest_path = recycle_bin_path / unique_name;

  // Move the file/folder to recycle bin
  try {
    fs::rename(source_path, dest_path);
    VXCORE_LOG_INFO("MoveToRecycleBin: Moved %s to %s", source_path.string().c_str(),
                    dest_path.string().c_str());
    return VXCORE_OK;
  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("MoveToRecycleBin: Failed to move %s: %s", source_path.string().c_str(),
                     e.what());
    return VXCORE_ERR_IO;
  }
}

std::string BundledFolderManager::GenerateUniqueFileName(const std::string &folder_abs_path,
                                                         const std::string &desired_name) const {
  fs::path folder_path(folder_abs_path);
  fs::path dest_path = folder_path / desired_name;

  if (!fs::exists(dest_path)) {
    return desired_name;
  }

  // Extract base name and extension
  std::string base_name = desired_name;
  std::string extension;
  size_t dot_pos = desired_name.find_last_of('.');
  if (dot_pos != std::string::npos && dot_pos > 0) {
    base_name = desired_name.substr(0, dot_pos);
    extension = desired_name.substr(dot_pos);
  }

  // Try _1, _2, _3, etc. until we find a unique name
  int suffix = 1;
  while (true) {
    std::string new_name = base_name + "_" + std::to_string(suffix) + extension;
    dest_path = folder_path / new_name;
    if (!fs::exists(dest_path)) {
      return new_name;
    }
    ++suffix;
    if (suffix > 10000) {
      // Safety limit to prevent infinite loop
      VXCORE_LOG_ERROR("GenerateUniqueFileName: Too many conflicts for %s", desired_name.c_str());
      return base_name + "_" + std::to_string(GetCurrentTimestampMillis()) + extension;
    }
  }
}

VxCoreError BundledFolderManager::ImportFile(const std::string &folder_path,
                                             const std::string &external_file_path,
                                             std::string &out_file_id) {
  VXCORE_LOG_INFO("ImportFile: folder=%s, external_file=%s", folder_path.c_str(),
                  external_file_path.c_str());

  // Validate external file exists
  if (!fs::exists(external_file_path)) {
    VXCORE_LOG_ERROR("ImportFile: External file not found: %s", external_file_path.c_str());
    return VXCORE_ERR_NOT_FOUND;
  }

  if (!fs::is_regular_file(external_file_path)) {
    VXCORE_LOG_ERROR("ImportFile: Path is not a regular file: %s", external_file_path.c_str());
    return VXCORE_ERR_INVALID_PARAM;
  }

  const auto clean_folder_path = GetCleanRelativePath(folder_path);

  // Get folder config to ensure destination exists
  FolderConfig *config = nullptr;
  VxCoreError error = GetFolderConfig(clean_folder_path, &config);
  if (error != VXCORE_OK) {
    VXCORE_LOG_ERROR("ImportFile: Failed to get folder config: folder=%s, error=%d",
                     clean_folder_path.c_str(), error);
    return error;
  }

  // Get destination content path and determine unique file name
  std::string content_path = GetContentPath(clean_folder_path);
  fs::path source_path(external_file_path);
  std::string original_name = source_path.filename().string();
  std::string target_name = GenerateUniqueFileName(content_path, original_name);
  fs::path dest_path = fs::path(content_path) / target_name;

  // Check if file with this name already exists in config (shouldn't happen after unique name gen)
  if (FindFileRecord(*config, target_name)) {
    VXCORE_LOG_WARN("ImportFile: File already exists in config: %s", target_name.c_str());
    return VXCORE_ERR_ALREADY_EXISTS;
  }

  // Copy the external file to notebook folder
  try {
    fs::copy_file(external_file_path, dest_path);
    VXCORE_LOG_DEBUG("ImportFile: Copied file to: %s", dest_path.string().c_str());
  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("ImportFile: Failed to copy file: %s", e.what());
    return VXCORE_ERR_IO;
  }

  // Add file to folder config
  const auto ts = GetCurrentTimestampMillis();
  config->files.emplace_back(target_name);
  config->files.back().created_utc = ts;
  config->files.back().modified_utc = ts;
  config->modified_utc = ts;

  error = SaveFolderConfig(clean_folder_path, *config);
  if (error != VXCORE_OK) {
    // Rollback: delete the copied file
    try {
      fs::remove(dest_path);
    } catch (const std::exception &) {
      VXCORE_LOG_WARN("ImportFile: Failed to rollback copied file");
    }
    return error;
  }

  out_file_id = config->files.back().id;

  // Write-through to MetadataStore
  if (auto *store = notebook_->GetMetadataStore()) {
    StoreFileRecord file_record = ToStoreFileRecord(config->files.back(), config->id);
    if (!store->CreateFile(file_record)) {
      VXCORE_LOG_WARN("ImportFile: Failed to create file in MetadataStore: id=%s",
                      out_file_id.c_str());
    }
  }

  VXCORE_LOG_INFO("ImportFile: Successfully imported file: id=%s, name=%s", out_file_id.c_str(),
                  target_name.c_str());
  return VXCORE_OK;
}

VxCoreError BundledFolderManager::ImportFolder(const std::string &dest_folder_path,
                                               const std::string &external_folder_path,
                                               const std::string &suffix_allowlist,
                                               std::string &out_folder_id) {
  VXCORE_LOG_INFO("ImportFolder: dest=%s, external=%s, suffix_allowlist=%s", dest_folder_path.c_str(),
                  external_folder_path.c_str(), suffix_allowlist.c_str());

  // Require absolute path for external folder
  fs::path external_path(external_folder_path);
  if (!external_path.is_absolute()) {
    VXCORE_LOG_ERROR("ImportFolder: External folder path must be absolute: %s",
                     external_folder_path.c_str());
    return VXCORE_ERR_INVALID_PARAM;
  }

  // Validate external folder exists and is a directory
  if (!fs::exists(external_folder_path)) {
    VXCORE_LOG_ERROR("ImportFolder: External folder not found: %s", external_folder_path.c_str());
    return VXCORE_ERR_NOT_FOUND;
  }

  if (!fs::is_directory(external_folder_path)) {
    VXCORE_LOG_ERROR("ImportFolder: Path is not a directory: %s", external_folder_path.c_str());
    return VXCORE_ERR_INVALID_PARAM;
  }

  // Reject importing a folder from within the notebook root to prevent circular copies
  try {
    fs::path canonical_external = fs::canonical(external_folder_path);
    fs::path root_path(notebook_->GetRootFolder());
    // Check if external path starts with (is under) notebook root
    auto mismatch_pair = std::mismatch(root_path.begin(), root_path.end(),
                                       canonical_external.begin(), canonical_external.end());
    if (mismatch_pair.first == root_path.end()) {
      // root_path is a prefix of canonical_external (external is inside notebook)
      VXCORE_LOG_ERROR("ImportFolder: Cannot import folder from within notebook root: %s",
                       external_folder_path.c_str());
      return VXCORE_ERR_INVALID_PARAM;
    }
  } catch (const std::exception &e) {
    // If canonical fails, the path may not exist or be inaccessible
    VXCORE_LOG_WARN("ImportFolder: Failed to canonicalize paths: %s", e.what());
  }

  // Parse suffix allowlist into a set of lowercase extensions
  std::set<std::string> allowed_suffixes;
  if (!suffix_allowlist.empty()) {
    std::istringstream iss(suffix_allowlist);
    std::string suffix;
    while (std::getline(iss, suffix, ';')) {
      if (!suffix.empty()) {
        // Normalize: remove leading dot if present, convert to lowercase
        if (suffix[0] == '.') {
          suffix = suffix.substr(1);
        }
        std::transform(suffix.begin(), suffix.end(), suffix.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        allowed_suffixes.insert(suffix);
      }
    }
    VXCORE_LOG_DEBUG("ImportFolder: Parsed %zu allowed suffixes", allowed_suffixes.size());
  }
  const auto clean_dest_path = GetCleanRelativePath(dest_folder_path);

  // Get destination folder config to ensure it exists
  FolderConfig *dest_config = nullptr;
  VxCoreError error = GetFolderConfig(clean_dest_path, &dest_config);
  if (error != VXCORE_OK) {
    VXCORE_LOG_ERROR("ImportFolder: Failed to get dest folder config: folder=%s, error=%d",
                     clean_dest_path.c_str(), error);
    return error;
  }

  // Get destination content path and determine unique folder name
  std::string dest_content_path = GetContentPath(clean_dest_path);
  fs::path source_path(external_folder_path);
  std::string original_name = source_path.filename().string();
  std::string target_name = original_name;

  // Generate unique name if folder already exists
  fs::path target_path = fs::path(dest_content_path) / target_name;
  if (fs::exists(target_path)) {
    int suffix = 1;
    while (suffix <= 10000) {
      target_name = original_name + "_" + std::to_string(suffix);
      target_path = fs::path(dest_content_path) / target_name;
      if (!fs::exists(target_path)) {
        break;
      }
      ++suffix;
    }
    if (suffix > 10000) {
      target_name = original_name + "_" + std::to_string(GetCurrentTimestampMillis());
      target_path = fs::path(dest_content_path) / target_name;
    }
  }

  // Check if folder with this name already exists in config
  auto it = std::find(dest_config->folders.begin(), dest_config->folders.end(), target_name);
  if (it != dest_config->folders.end()) {
    VXCORE_LOG_WARN("ImportFolder: Folder already exists in config: %s", target_name.c_str());
    return VXCORE_ERR_ALREADY_EXISTS;
  }

  // Copy the external folder to notebook folder recursively (with suffix filtering)
  std::function<void(const fs::path &, const fs::path &)> copy_filtered =
      [&](const fs::path &src, const fs::path &dest) {
        fs::create_directories(dest);
        for (const auto &entry : fs::directory_iterator(src)) {
          const std::string entry_name = entry.path().filename().string();
          // Skip hidden files/folders
          if (entry_name.empty() || entry_name[0] == '.') {
            continue;
          }
          if (entry.is_directory()) {
            copy_filtered(entry.path(), dest / entry_name);
          } else if (entry.is_regular_file()) {
            // Apply suffix filter if allowlist is specified
            if (!allowed_suffixes.empty()) {
              std::string ext = entry.path().extension().string();
              if (!ext.empty() && ext[0] == '.') {
                ext = ext.substr(1);
              }
              std::transform(ext.begin(), ext.end(), ext.begin(),
                             [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
              if (allowed_suffixes.find(ext) == allowed_suffixes.end()) {
                continue;  // Skip files not in allowlist
              }
            }
            fs::copy_file(entry.path(), dest / entry_name, fs::copy_options::overwrite_existing);
          }
        }
      };

  try {
    copy_filtered(fs::path(external_folder_path), target_path);
    VXCORE_LOG_DEBUG("ImportFolder: Copied folder to: %s", target_path.string().c_str());
  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("ImportFolder: Failed to copy folder: %s", e.what());
    return VXCORE_ERR_IO;
  }

  // Add folder to destination's folder list
  dest_config->folders.push_back(target_name);
  dest_config->modified_utc = GetCurrentTimestampMillis();
  error = SaveFolderConfig(clean_dest_path, *dest_config);
  if (error != VXCORE_OK) {
    // Rollback: delete the copied folder
    try {
      fs::remove_all(target_path);
    } catch (const std::exception &) {
      VXCORE_LOG_WARN("ImportFolder: Failed to rollback copied folder");
    }
    return error;
  }

  // Create folder config for the imported folder (index it)
  const auto imported_folder_path = ConcatenatePaths(clean_dest_path, target_name);
  std::unique_ptr<FolderConfig> new_config(new FolderConfig(target_name));

  // Recursively scan and index the imported folder contents
  std::function<VxCoreError(const std::string &, FolderConfig &)> index_contents =
      [&](const std::string &rel_path, FolderConfig &config) -> VxCoreError {
    std::string abs_path = GetContentPath(rel_path);
    const bool is_root = (rel_path.empty() || rel_path == ".");

    try {
      for (const auto &entry : fs::directory_iterator(abs_path)) {
        std::string entry_name = entry.path().filename().string();

        if (entry.is_directory()) {
          if (entry_name.empty() || entry_name[0] == '.' ||
              (is_root && entry_name == BundledNotebook::kMetadataFolderName)) {
            continue;
          }

          // Add subfolder to config
          config.folders.push_back(entry_name);

          // Create and save config for subfolder
          std::string subfolder_rel_path = ConcatenatePaths(rel_path, entry_name);
          std::unique_ptr<FolderConfig> subfolder_config(new FolderConfig(entry_name));

          // Recursively index subfolder contents
          VxCoreError err = index_contents(subfolder_rel_path, *subfolder_config);
          if (err != VXCORE_OK) {
            VXCORE_LOG_WARN("ImportFolder: Failed to index subfolder: %s", subfolder_rel_path.c_str());
            // Continue with other entries
          }

          err = SaveFolderConfig(subfolder_rel_path, *subfolder_config);
          if (err != VXCORE_OK) {
            VXCORE_LOG_WARN("ImportFolder: Failed to save subfolder config: %s",
                            subfolder_rel_path.c_str());
          }

          // Write subfolder to MetadataStore
          if (auto *store = notebook_->GetMetadataStore()) {
            StoreFolderRecord store_record = ToStoreFolderRecord(*subfolder_config, config.id);
            if (!store->CreateFolder(store_record)) {
              VXCORE_LOG_WARN("ImportFolder: Failed to write subfolder to MetadataStore: id=%s",
                              subfolder_config->id.c_str());
            }

            // Write subfolder's files to MetadataStore
            for (const auto &file : subfolder_config->files) {
              StoreFileRecord file_record = ToStoreFileRecord(file, subfolder_config->id);
              if (!store->CreateFile(file_record)) {
                VXCORE_LOG_WARN("ImportFolder: Failed to write file to MetadataStore: id=%s",
                                file.id.c_str());
              }
            }
          }

          CacheConfig(subfolder_rel_path, std::move(subfolder_config));

        } else if (entry.is_regular_file()) {
          // Skip hidden files
          if (entry_name.empty() || entry_name[0] == '.') {
            continue;
          }

          // Add file to config
          const auto ts = GetCurrentTimestampMillis();
          config.files.emplace_back(entry_name);
          config.files.back().created_utc = ts;
          config.files.back().modified_utc = ts;
        }
      }
    } catch (const std::exception &e) {
      VXCORE_LOG_ERROR("ImportFolder: Failed to scan directory: %s, error: %s", abs_path.c_str(),
                       e.what());
      return VXCORE_ERR_IO;
    }

    return VXCORE_OK;
  };

  // Index contents of the imported folder
  error = index_contents(imported_folder_path, *new_config);
  if (error != VXCORE_OK) {
    VXCORE_LOG_WARN("ImportFolder: Some contents failed to index, continuing...");
    // Continue anyway - partial import is better than no import
  }

  // Save the imported folder's config
  error = SaveFolderConfig(imported_folder_path, *new_config);
  if (error != VXCORE_OK) {
    VXCORE_LOG_ERROR("ImportFolder: Failed to save imported folder config: error=%d", error);
    return error;
  }

  out_folder_id = new_config->id;

  // Write-through to MetadataStore
  if (auto *store = notebook_->GetMetadataStore()) {
    StoreFolderRecord folder_record = ToStoreFolderRecord(*new_config, dest_config->id);
    if (!store->CreateFolder(folder_record)) {
      VXCORE_LOG_WARN("ImportFolder: Failed to write folder to MetadataStore: id=%s",
                      out_folder_id.c_str());
    }

    // Write files of the imported folder to MetadataStore
    for (const auto &file : new_config->files) {
      StoreFileRecord file_record = ToStoreFileRecord(file, new_config->id);
      if (!store->CreateFile(file_record)) {
        VXCORE_LOG_WARN("ImportFolder: Failed to write file to MetadataStore: id=%s", file.id.c_str());
      }
    }
  }

  CacheConfig(imported_folder_path, std::move(new_config));

  VXCORE_LOG_INFO("ImportFolder: Successfully imported folder: id=%s, name=%s", out_folder_id.c_str(),
                  target_name.c_str());
  return VXCORE_OK;
}

VxCoreError BundledFolderManager::IndexNode(const std::string &node_path) {
  const auto clean_path = GetCleanRelativePath(node_path);
  VXCORE_LOG_INFO("IndexNode: path=%s", clean_path.c_str());

  // Get parent folder path and node name
  const auto [parent_path, node_name] = SplitPath(clean_path);

  // Get content path to check filesystem
  std::string content_path = GetContentPath(clean_path);

  // Check if node exists on filesystem
  if (!fs::exists(content_path)) {
    VXCORE_LOG_WARN("IndexNode: Node does not exist on filesystem: %s", content_path.c_str());
    return VXCORE_ERR_NOT_FOUND;
  }

  // Get parent folder config
  FolderConfig *parent_config = nullptr;
  VxCoreError error = GetFolderConfig(parent_path, &parent_config);
  if (error != VXCORE_OK) {
    VXCORE_LOG_ERROR("IndexNode: Failed to get parent folder config: path=%s, error=%d",
                     parent_path.c_str(), error);
    return error;
  }

  // Determine if it's a file or folder
  bool is_directory = fs::is_directory(content_path);

  if (is_directory) {
    // Check if folder is already indexed
    auto it = std::find(parent_config->folders.begin(), parent_config->folders.end(), node_name);
    if (it != parent_config->folders.end()) {
      VXCORE_LOG_WARN("IndexNode: Folder already indexed: %s", node_name.c_str());
      return VXCORE_ERR_ALREADY_EXISTS;
    }

    // Create a new FolderConfig for the folder
    std::unique_ptr<FolderConfig> new_config(new FolderConfig(node_name));
    error = SaveFolderConfig(clean_path, *new_config);
    if (error != VXCORE_OK) {
      VXCORE_LOG_ERROR("IndexNode: Failed to save folder config: error=%d", error);
      return error;
    }

    // Add folder to parent's folder list
    parent_config->folders.push_back(node_name);
    parent_config->modified_utc = GetCurrentTimestampMillis();
    error = SaveFolderConfig(parent_path, *parent_config);
    if (error != VXCORE_OK) {
      VXCORE_LOG_ERROR("IndexNode: Failed to save parent folder config: error=%d", error);
      return error;
    }

    // Write-through to MetadataStore
    if (auto *store = notebook_->GetMetadataStore()) {
      StoreFolderRecord store_record = ToStoreFolderRecord(*new_config, parent_config->id);
      if (!store->CreateFolder(store_record)) {
        VXCORE_LOG_WARN("IndexNode: Failed to write folder to MetadataStore: id=%s",
                        new_config->id.c_str());
      }
    }

    CacheConfig(clean_path, std::move(new_config));
    VXCORE_LOG_INFO("IndexNode: Folder indexed successfully: path=%s", clean_path.c_str());
  } else {
    // It's a file
    // Check if file is already indexed
    if (FindFileRecord(*parent_config, node_name)) {
      VXCORE_LOG_WARN("IndexNode: File already indexed: %s", node_name.c_str());
      return VXCORE_ERR_ALREADY_EXISTS;
    }

    // Add file to parent folder's file list
    const auto ts = GetCurrentTimestampMillis();
    parent_config->files.emplace_back(node_name);
    parent_config->files.back().created_utc = ts;
    parent_config->files.back().modified_utc = ts;
    parent_config->modified_utc = ts;

    error = SaveFolderConfig(parent_path, *parent_config);
    if (error != VXCORE_OK) {
      VXCORE_LOG_ERROR("IndexNode: Failed to save parent folder config: error=%d", error);
      return error;
    }

    // Write-through to MetadataStore
    if (auto *store = notebook_->GetMetadataStore()) {
      StoreFileRecord file_record =
          ToStoreFileRecord(parent_config->files.back(), parent_config->id);
      if (!store->CreateFile(file_record)) {
        VXCORE_LOG_WARN("IndexNode: Failed to create file in MetadataStore: id=%s",
                        parent_config->files.back().id.c_str());
      }
    }

    VXCORE_LOG_INFO("IndexNode: File indexed successfully: path=%s", clean_path.c_str());
  }

  return VXCORE_OK;
}

VxCoreError BundledFolderManager::UnindexNode(const std::string &node_path) {
  const auto clean_path = GetCleanRelativePath(node_path);
  VXCORE_LOG_INFO("UnindexNode: path=%s", clean_path.c_str());

  // Get parent folder path and node name
  const auto [parent_path, node_name] = SplitPath(clean_path);

  // Get parent folder config
  FolderConfig *parent_config = nullptr;
  VxCoreError error = GetFolderConfig(parent_path, &parent_config);
  if (error != VXCORE_OK) {
    VXCORE_LOG_ERROR("UnindexNode: Failed to get parent folder config: path=%s, error=%d",
                     parent_path.c_str(), error);
    return error;
  }

  // Check if it's a folder first
  auto folder_it =
      std::find(parent_config->folders.begin(), parent_config->folders.end(), node_name);
  if (folder_it != parent_config->folders.end()) {
    // It's an indexed folder - get its ID before removing
    FolderConfig *folder_config = nullptr;
    error = GetFolderConfig(clean_path, &folder_config);
    std::string folder_id;
    if (error == VXCORE_OK && folder_config) {
      folder_id = folder_config->id;
    }

    // Remove folder from parent's folder list
    parent_config->folders.erase(folder_it);
    parent_config->modified_utc = GetCurrentTimestampMillis();
    error = SaveFolderConfig(parent_path, *parent_config);
    if (error != VXCORE_OK) {
      VXCORE_LOG_ERROR("UnindexNode: Failed to save parent folder config: error=%d", error);
      return error;
    }

    // Delete the folder's vx.json config file (metadata only, not filesystem content)
    std::string config_path = GetConfigPath(clean_path);
    try {
      if (fs::exists(config_path)) {
        fs::remove_all(fs::path(config_path).parent_path());
      }
    } catch (const std::exception &e) {
      VXCORE_LOG_WARN("UnindexNode: Failed to delete folder config: %s", e.what());
    }

    // Write-through to MetadataStore
    if (auto *store = notebook_->GetMetadataStore(); !folder_id.empty()) {
      if (!store->DeleteFolder(folder_id)) {
        VXCORE_LOG_WARN("UnindexNode: Failed to delete folder from MetadataStore: id=%s",
                        folder_id.c_str());
      }
    }

    InvalidateCache(clean_path);
    VXCORE_LOG_INFO("UnindexNode: Folder unindexed successfully: path=%s", clean_path.c_str());
    return VXCORE_OK;
  }

  // Check if it's a file
  auto file_it = std::find_if(
      parent_config->files.begin(), parent_config->files.end(),
      [&node_name](const FileRecord &file) { return file.name == node_name; });
  if (file_it != parent_config->files.end()) {
    // Save the file ID before erasing
    std::string file_id = file_it->id;

    // Remove file from parent folder's file list
    parent_config->files.erase(file_it);
    parent_config->modified_utc = GetCurrentTimestampMillis();
    error = SaveFolderConfig(parent_path, *parent_config);
    if (error != VXCORE_OK) {
      VXCORE_LOG_ERROR("UnindexNode: Failed to save parent folder config: error=%d", error);
      return error;
    }

    // Write-through to MetadataStore
    if (auto *store = notebook_->GetMetadataStore(); !file_id.empty()) {
      if (!store->DeleteFile(file_id)) {
        VXCORE_LOG_WARN("UnindexNode: Failed to delete file from MetadataStore: id=%s",
                        file_id.c_str());
      }
    }

    VXCORE_LOG_INFO("UnindexNode: File unindexed successfully: path=%s", clean_path.c_str());
    return VXCORE_OK;
  }

  // Node not found in metadata
  VXCORE_LOG_WARN("UnindexNode: Node not found in metadata: %s", clean_path.c_str());
  return VXCORE_ERR_NOT_FOUND;
}

VxCoreError BundledFolderManager::ListExternalNodes(const std::string &folder_path,
                                                   FolderContents &out_contents) {
  const auto clean_path = GetCleanRelativePath(folder_path);

  // Clear output
  out_contents.files.clear();
  out_contents.folders.clear();

  // Get folder config (indexed items)
  FolderConfig *config = nullptr;
  VxCoreError error = GetFolderConfig(clean_path, &config);
  if (error != VXCORE_OK) {
    VXCORE_LOG_ERROR("ListExternalNodes: Failed to get folder config: path=%s, error=%d",
                     clean_path.c_str(), error);
    return error;
  }

  // Build sets of indexed items for fast lookup
  std::set<std::string> indexed_files;
  std::set<std::string> indexed_folders;
  for (const auto &file : config->files) {
    indexed_files.insert(file.name);
  }
  for (const auto &folder_name : config->folders) {
    indexed_folders.insert(folder_name);
  }

  // Get filesystem content path
  std::string content_path = GetContentPath(clean_path);
  if (!fs::exists(content_path)) {
    VXCORE_LOG_WARN("ListExternalNodes: Content path does not exist: %s", content_path.c_str());
    return VXCORE_ERR_NOT_FOUND;
  }

  // Check if assets/attachments folders should be skipped (only if single name)
  const NotebookConfig &nb_config = notebook_->GetConfig();
  const bool need_check_assets_folder = IsSingleName(nb_config.assets_folder);
  const bool need_check_attachments_folder = IsSingleName(nb_config.attachments_folder);

  const bool is_root = clean_path.empty() || clean_path == ".";

  // Scan filesystem and find unindexed items
  try {
    for (const auto &entry : fs::directory_iterator(content_path)) {
      std::string entry_name = entry.path().filename().string();

      // Skip hidden files/folders (starting with '.')
      if (entry_name.empty() || entry_name[0] == '.') {
        continue;
      }

      // Skip metadata folder (only at root)
      if (is_root && entry_name == BundledNotebook::kMetadataFolderName) {
        continue;
      }

      // Skip assets and attachments folders
      if ((need_check_assets_folder && entry_name == nb_config.assets_folder) ||
          (need_check_attachments_folder && entry_name == nb_config.attachments_folder)) {
        continue;
      }

      if (entry.is_directory()) {
        // Check if folder is NOT indexed
        if (indexed_folders.find(entry_name) == indexed_folders.end()) {
          // External folder - create a minimal FolderRecord
          FolderRecord record;
          record.name = entry_name;
          // No ID since it's not indexed
          out_contents.folders.push_back(record);
        }
      } else if (entry.is_regular_file()) {
        // Check if file is NOT indexed
        if (indexed_files.find(entry_name) == indexed_files.end()) {
          // External file - create a minimal FileRecord
          FileRecord record;
          record.name = entry_name;
          // No ID since it's not indexed
          out_contents.files.push_back(record);
        }
      }
    }
  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("ListExternalNodes: Failed to iterate directory: %s", e.what());
    return VXCORE_ERR_IO;
  }

  return VXCORE_OK;
}
}  // namespace vxcore
