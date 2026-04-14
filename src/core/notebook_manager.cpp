#include "notebook_manager.h"

#include <algorithm>
#include <filesystem>

#include "bundled_notebook.h"
#include "config_manager.h"
#include "folder_manager.h"
#include "metadata_store.h"
#include "raw_notebook.h"
#include "utils/file_utils.h"
#include "utils/logger.h"
#include "utils/utils.h"

namespace vxcore {

NotebookManager::NotebookManager(ConfigManager *config_manager) : config_manager_(config_manager) {
  LoadOpenNotebooks();
}

NotebookManager::~NotebookManager() {}

void NotebookManager::LoadOpenNotebooks() {
  auto &session_config = config_manager_->GetSessionConfig();
  const auto local_data_folder = config_manager_->GetLocalDataPath();
  notebooks_.clear();

  for (const auto &record : session_config.notebooks) {
    std::unique_ptr<Notebook> notebook;
    const std::string root_folder = CleanPath(record.root_folder);
    switch (record.type) {
      case NotebookType::Bundled: {
        auto error = BundledNotebook::Open(local_data_folder, root_folder, notebook);
        if (error != VXCORE_OK) {
          VXCORE_LOG_ERROR("Failed to load bundled notebook: root_folder=%s, error=%d",
                           record.root_folder.c_str(), error);
          continue;
        }
        break;
      }
      case NotebookType::Raw: {
        auto error = RawNotebook::Open(local_data_folder, root_folder, record.id, notebook);
        if (error != VXCORE_OK) {
          VXCORE_LOG_ERROR("Failed to load raw notebook: root_folder=%s, error=%d",
                           record.root_folder.c_str(), error);
          continue;
        }
        break;
      }
      default:
        VXCORE_LOG_ERROR("Skip invalid notebook type: %d", static_cast<int>(record.type));
        break;
    }

    if (notebook) {
      VXCORE_LOG_INFO("Loaded open notebook: id=%s, root_folder=%s", notebook->GetId().c_str(),
                      notebook->GetRootFolder().c_str());
      notebooks_[notebook->GetId()] = std::move(notebook);
    }
  }
}

VxCoreError NotebookManager::CreateNotebook(const std::string &root_folder, NotebookType type,
                                            const std::string &config_json,
                                            std::string &out_notebook_id) {
  VXCORE_LOG_INFO("Creating notebook: root_folder=%s, type=%d", root_folder.c_str(),
                  static_cast<int>(type));

  const auto root_folder_clean = CleanPath(root_folder);

  try {
    std::filesystem::path rootPath(root_folder_clean);
    if (!std::filesystem::exists(rootPath)) {
      std::filesystem::create_directories(rootPath);
      VXCORE_LOG_DEBUG("Created root directory: %s", root_folder_clean.c_str());
    } else {
      // TODO: check if @root_folder already has a notebook.
    }

    const auto local_data_folder = config_manager_->GetLocalDataPath();

    // At least there should be name.
    assert(!config_json.empty());
    nlohmann::json json = nlohmann::json::parse(config_json);
    auto config = NotebookConfig::FromJson(json);
    config.id.clear();

    std::unique_ptr<Notebook> notebook;
    switch (type) {
      case NotebookType::Bundled: {
        auto err = BundledNotebook::Create(local_data_folder, root_folder_clean, &config, notebook);
        if (err != VXCORE_OK) {
          VXCORE_LOG_ERROR("Failed to create bundled notebook: root_folder=%s, error=%d",
                           root_folder_clean.c_str(), err);
          return err;
        }
        break;
      }
      case NotebookType::Raw: {
        auto err = RawNotebook::Create(local_data_folder, root_folder_clean, &config, notebook);
        if (err != VXCORE_OK) {
          VXCORE_LOG_ERROR("Failed to create raw notebook: root_folder=%s, error=%d",
                           root_folder_clean.c_str(), err);
          return err;
        }
        break;
      }
      default:
        VXCORE_LOG_ERROR("Invalid notebook type: %d", static_cast<int>(type));
        return VXCORE_ERR_INVALID_PARAM;
    }

    auto err = UpdateNotebookRecord(*notebook);
    if (err != VXCORE_OK) {
      VXCORE_LOG_ERROR("Failed to save notebook record after creation: id=%s, error=%d",
                       notebook->GetId().c_str(), err);
      return err;
    }

    out_notebook_id = notebook->GetId();
    notebooks_[out_notebook_id] = std::move(notebook);

    VXCORE_LOG_INFO("Notebook created successfully: id=%s", out_notebook_id.c_str());
    return VXCORE_OK;
  } catch (const nlohmann::json::exception &e) {
    VXCORE_LOG_ERROR("JSON parse error while creating notebook: %s", e.what());
    return VXCORE_ERR_JSON_PARSE;
  } catch (const std::filesystem::filesystem_error &e) {
    VXCORE_LOG_ERROR("Filesystem error while creating notebook: %s", e.what());
    return VXCORE_ERR_IO;
  } catch (...) {
    VXCORE_LOG_ERROR("Unknown error while creating notebook");
    return VXCORE_ERR_UNKNOWN;
  }
}

VxCoreError NotebookManager::OpenNotebook(const std::string &root_folder,
                                          std::string &out_notebook_id) {
  VXCORE_LOG_INFO("Opening notebook: root_folder=%s", root_folder.c_str());
  const auto root_folder_clean = CleanPath(root_folder);
  if (auto *notebook = FindNotebookByRootFolder(root_folder_clean)) {
    out_notebook_id = notebook->GetId();
    VXCORE_LOG_DEBUG("Notebook already open: id=%s", out_notebook_id.c_str());
    return VXCORE_OK;
  }

  std::filesystem::path rootPath(root_folder_clean);
  if (!std::filesystem::exists(rootPath)) {
    VXCORE_LOG_WARN("Notebook root folder not found: %s", root_folder_clean.c_str());
    return VXCORE_ERR_NOT_FOUND;
  }

  const auto local_data_folder = config_manager_->GetLocalDataPath();
  const std::filesystem::path bundled_config_path =
      rootPath / BundledNotebook::kMetadataFolderName / "config.json";

  std::unique_ptr<Notebook> notebook;
  VxCoreError err = VXCORE_OK;
  std::error_code config_ec;
  const bool has_bundled_config = std::filesystem::exists(bundled_config_path, config_ec);
  if (config_ec) {
    VXCORE_LOG_ERROR("Failed to check bundled notebook config: path=%s, error=%s",
                     bundled_config_path.string().c_str(), config_ec.message().c_str());
    return VXCORE_ERR_IO;
  }

  if (has_bundled_config) {
    err = BundledNotebook::Open(local_data_folder, root_folder_clean, notebook);
    if (err != VXCORE_OK) {
      VXCORE_LOG_ERROR("Failed to load bundled notebook: root_folder=%s, error=%d",
                       root_folder_clean.c_str(), err);
      return err;
    }
  } else {
    VXCORE_LOG_INFO("No bundled notebook metadata found; opening as raw folder: root_folder=%s",
                    root_folder_clean.c_str());

    NotebookConfig config;
    config.name = rootPath.filename().string();
    if (config.name.empty()) {
      config.name = root_folder_clean;
    }
    config.description = "Plain folder notebook";

    err = RawNotebook::Create(local_data_folder, root_folder_clean, &config, notebook);
    if (err != VXCORE_OK) {
      VXCORE_LOG_ERROR("Failed to open raw notebook: root_folder=%s, error=%d",
                       root_folder_clean.c_str(), err);
      return err;
    }
  }

  err = UpdateNotebookRecord(*notebook);
  if (err != VXCORE_OK) {
    VXCORE_LOG_ERROR("Failed to save notebook record after open: id=%s, error=%d",
                     notebook->GetId().c_str(), err);
    return err;
  }

  out_notebook_id = notebook->GetId();
  notebooks_[out_notebook_id] = std::move(notebook);

  VXCORE_LOG_INFO("Notebook open successfully: id=%s", out_notebook_id.c_str());
  return VXCORE_OK;
}

VxCoreError NotebookManager::CloseNotebook(const std::string &notebook_id) {
  VXCORE_LOG_INFO("Closing notebook: id=%s", notebook_id.c_str());

  auto it = notebooks_.find(notebook_id);
  if (it == notebooks_.end()) {
    VXCORE_LOG_WARN("Notebook not found for closing: id=%s", notebook_id.c_str());
    return VXCORE_ERR_NOT_FOUND;
  }

  // Close notebook first to release DB file lock before deleting local data
  it->second->Close();

  DeleteNotebookLocalData(*it->second);

  notebooks_.erase(it);

  // Remove notebook record from session config
  auto &session_config = config_manager_->GetSessionConfig();
  auto rec_it =
      std::find_if(session_config.notebooks.begin(), session_config.notebooks.end(),
                   [&notebook_id](const NotebookRecord &r) { return r.id == notebook_id; });
  if (rec_it != session_config.notebooks.end()) {
    session_config.notebooks.erase(rec_it);
  }

  config_manager_->SaveSessionConfig();

  VXCORE_LOG_INFO("Notebook closed successfully: id=%s", notebook_id.c_str());
  return VXCORE_OK;
}

void NotebookManager::DeleteNotebookLocalData(const Notebook &notebook) {
  const auto local_data_folder = notebook.GetLocalDataFolder();

  if (std::filesystem::exists(local_data_folder)) {
    VXCORE_LOG_INFO("Deleting notebook local data: id=%s, path=%s", notebook.GetId().c_str(),
                    local_data_folder.c_str());
    std::error_code ec;
    std::filesystem::remove_all(local_data_folder, ec);
    if (ec) {
      VXCORE_LOG_ERROR("Failed to delete notebook local data: id=%s, path=%s, error=%s",
                       notebook.GetId().c_str(), local_data_folder.c_str(), ec.message().c_str());
    } else {
      VXCORE_LOG_DEBUG("Notebook local data deleted: id=%s, path=%s", notebook.GetId().c_str(),
                       local_data_folder.c_str());
    }
  }
}

VxCoreError NotebookManager::GetNotebookConfig(const std::string &notebook_id,
                                               std::string &out_config_json) {
  auto *notebook = GetNotebook(notebook_id);
  if (!notebook) {
    return VXCORE_ERR_NOT_FOUND;
  }

  try {
    nlohmann::json json = ToNotebookConfig(*notebook);
    out_config_json = json.dump();
    return VXCORE_OK;
  } catch (const nlohmann::json::exception &) {
    return VXCORE_ERR_JSON_SERIALIZE;
  } catch (...) {
    return VXCORE_ERR_UNKNOWN;
  }
}

nlohmann::json NotebookManager::ToNotebookConfig(const Notebook &notebook) const {
  nlohmann::json json = notebook.GetConfig().ToJson();
  json["rootFolder"] = notebook.GetRootFolder();
  json["type"] = notebook.GetTypeStr();
  return json;
}

VxCoreError NotebookManager::UpdateNotebookConfig(const std::string &notebook_id,
                                                  const std::string &config_json) {
  auto *notebook = GetNotebook(notebook_id);
  if (!notebook) {
    return VXCORE_ERR_NOT_FOUND;
  }

  try {
    nlohmann::json json = nlohmann::json::parse(config_json);
    NotebookConfig config = NotebookConfig::FromJson(json);
    config.id = notebook_id;

    VxCoreError err = notebook->UpdateConfig(config);
    if (err != VXCORE_OK) {
      VXCORE_LOG_ERROR("Failed to update notebook config: id=%s, error=%d", notebook_id.c_str(),
                       err);
      return err;
    }

    err = UpdateNotebookRecord(*notebook);
    if (err != VXCORE_OK) {
      VXCORE_LOG_ERROR("Failed to update notebook record after config update: id=%s, error=%d",
                       notebook_id.c_str(), err);
      return err;
    }

    return VXCORE_OK;
  } catch (const nlohmann::json::exception &) {
    return VXCORE_ERR_JSON_PARSE;
  } catch (...) {
    return VXCORE_ERR_UNKNOWN;
  }
}

VxCoreError NotebookManager::ListNotebooks(std::string &out_notebooks_json) {
  try {
    nlohmann::json jsonArray = nlohmann::json::array();

    for (const auto &pair : notebooks_) {
      nlohmann::json item = ToNotebookConfig(*pair.second);
      jsonArray.push_back(item);
    }

    out_notebooks_json = jsonArray.dump();
    return VXCORE_OK;
  } catch (const nlohmann::json::exception &) {
    return VXCORE_ERR_JSON_SERIALIZE;
  } catch (...) {
    return VXCORE_ERR_UNKNOWN;
  }
}

Notebook *NotebookManager::GetNotebook(const std::string &notebook_id) {
  auto it = notebooks_.find(notebook_id);
  if (it == notebooks_.end()) {
    return nullptr;
  }
  return it->second.get();
}

VxCoreError NotebookManager::UpdateNotebookRecord(const Notebook &notebook) {
  auto &session_config = config_manager_->GetSessionConfig();
  NotebookRecord *record = FindNotebookRecord(notebook.GetId());
  if (!record) {
    session_config.notebooks.emplace_back();
    record = &session_config.notebooks.back();
  }
  record->id = notebook.GetId();
  record->root_folder = notebook.GetRootFolder();
  record->type = notebook.GetType();

  config_manager_->SaveSessionConfig();
  return VXCORE_OK;
}

NotebookRecord *NotebookManager::FindNotebookRecord(const std::string &id) {
  auto &session_config = config_manager_->GetSessionConfig();
  auto it = std::find_if(session_config.notebooks.begin(), session_config.notebooks.end(),
                         [&id](const NotebookRecord &r) { return r.id == id; });

  if (it != session_config.notebooks.end()) {
    return &(*it);
  }

  return nullptr;
}

Notebook *NotebookManager::FindNotebookByRootFolder(const std::string &root_folder) {
  for (const auto &pair : notebooks_) {
    if (pair.second->GetRootFolder() == root_folder) {
      return pair.second.get();
    }
  }
  return nullptr;
}

VxCoreError NotebookManager::ResolvePathToNotebook(const std::string &absolute_path,
                                                   std::string &out_notebook_id,
                                                   std::string &out_relative_path) {
  const std::string clean_path = CleanPath(absolute_path);
  std::filesystem::path abs_path(clean_path);

  // Iterate through all open notebooks and find which one contains this path.
  for (const auto &pair : notebooks_) {
    const std::string &root_folder = pair.second->GetRootFolder();
    std::filesystem::path root_path(root_folder);

    // Check if absolute_path starts with root_folder.
    auto [root_end, path_pos] = std::mismatch(root_path.begin(), root_path.end(), abs_path.begin());

    if (root_end == root_path.end()) {
      // Path is within this notebook.
      out_notebook_id = pair.first;

      // Compute relative path.
      std::filesystem::path relative;
      for (; path_pos != abs_path.end(); ++path_pos) {
        relative /= *path_pos;
      }
      out_relative_path = CleanFsPath(relative);

      VXCORE_LOG_DEBUG("Resolved path %s to notebook %s, relative path %s", clean_path.c_str(),
                       out_notebook_id.c_str(), out_relative_path.c_str());
      return VXCORE_OK;
    }
  }

  VXCORE_LOG_DEBUG("Path %s not found in any open notebook", clean_path.c_str());
  return VXCORE_ERR_NOT_FOUND;
}

VxCoreError NotebookManager::ResolveNodeById(const std::string &node_id,
                                             std::string &out_notebook_id,
                                             std::string &out_relative_path) {
  for (const auto &pair : notebooks_) {
    auto *store = pair.second->GetMetadataStore();
    if (!store) {
      continue;
    }

    std::string path = store->GetNodePathById(node_id);
    if (!path.empty()) {
      out_notebook_id = pair.first;
      out_relative_path = path;
      VXCORE_LOG_DEBUG("Resolved node %s to notebook %s, relative path %s", node_id.c_str(),
                       out_notebook_id.c_str(), out_relative_path.c_str());
      return VXCORE_OK;
    }
  }

  VXCORE_LOG_DEBUG("Node %s not found in any open notebook", node_id.c_str());
  return VXCORE_ERR_NOT_FOUND;
}

}  // namespace vxcore
