#include "buffer_manager.h"

#include <algorithm>
#include <filesystem>

#include "buffer_provider.h"
#include "config_manager.h"
#include "notebook.h"
#include "notebook_manager.h"
#include "utils/file_utils.h"
#include "utils/logger.h"
#include "utils/utils.h"

#include "standard_buffer_provider.h"

namespace vxcore {

BufferManager::BufferManager(ConfigManager *config_manager, NotebookManager *notebook_manager)
    : config_manager_(config_manager), notebook_manager_(notebook_manager) {
  LoadBuffers();
}

BufferManager::~BufferManager() {
  if (!shutdown_called_) {
    SaveBuffers();
  }
}

void BufferManager::LoadBuffers() {
  auto &session_config = config_manager_->GetSessionConfig();
  buffers_.clear();

  // Check if session recovery is disabled
  if (!config_manager_->GetConfig().recover_last_session) {
    session_config.buffers.clear();
    VXCORE_LOG_INFO("Session recovery disabled, skipping buffer restore");
    return;
  }

  for (const auto &record : session_config.buffers) {
    VXCORE_LOG_INFO("LoadBuffers: processing record id=%s, notebook_id=%s, file_path=%s",
                    record.id.c_str(), record.notebook_id.c_str(), record.file_path.c_str());

    // Resolve notebook pointer if notebook_id is not empty
    Notebook *notebook = nullptr;
    if (!record.notebook_id.empty()) {
      notebook = notebook_manager_->GetNotebook(record.notebook_id);
      if (!notebook) {
        VXCORE_LOG_WARN("Skipping buffer: notebook not found: notebook_id=%s, file_path=%s",
                        record.notebook_id.c_str(), record.file_path.c_str());
        continue;
      }
    }

    // Check if file exists on disk before restoring
    std::string full_path;
    if (notebook) {
      full_path = ConcatenatePaths(notebook->GetRootFolder(), record.file_path);
    } else {
      full_path = record.file_path;
    }

    bool exists = std::filesystem::exists(full_path);
    VXCORE_LOG_INFO("LoadBuffers: id=%s, full_path=%s, exists=%d", record.id.c_str(),
                    full_path.c_str(), exists ? 1 : 0);

    if (!exists) {
      VXCORE_LOG_WARN("Skipping buffer: file not found on disk: %s", full_path.c_str());
      continue;
    }

    std::unique_ptr<Buffer> buffer;
    if (notebook) {
      buffer = std::make_unique<Buffer>(notebook, record.file_path);
    } else {
      // External file
      buffer = std::make_unique<Buffer>(record.file_path);
    }

    // Restore the original buffer ID from session config
    if (!record.id.empty()) {
      buffer->SetId(record.id);
    }

    std::string id = buffer->GetId();
    buffers_[id] = std::move(buffer);
    VXCORE_LOG_DEBUG("Loaded buffer: id=%s, notebook_id=%s, file_path=%s", id.c_str(),
                     record.notebook_id.c_str(), record.file_path.c_str());
  }

  VXCORE_LOG_INFO("Loaded %zu buffers from session", buffers_.size());
}

void BufferManager::UpdateSessionBuffers() {
  auto &session_config = config_manager_->GetSessionConfig();
  session_config.buffers.clear();

  for (const auto &pair : buffers_) {
    BufferRecord record;
    record.id = pair.second->GetId();
    record.notebook_id = pair.second->GetNotebookId();
    record.file_path = pair.second->GetFilePath();
    session_config.buffers.push_back(record);
  }

  VXCORE_LOG_DEBUG("Updated %zu buffer records in session config", buffers_.size());
}

void BufferManager::SaveBuffers() {
  UpdateSessionBuffers();
  config_manager_->SaveSessionConfig();
  VXCORE_LOG_DEBUG("Saved %zu buffers to session", buffers_.size());
}

std::string BufferManager::FindBufferByPath(const std::string &notebook_id,
                                            const std::string &file_path) {
  for (const auto &pair : buffers_) {
    const auto &buffer = pair.second;
    if (buffer->GetNotebookId() == notebook_id && buffer->GetFilePath() == file_path) {
      return buffer->GetId();
    }
  }
  return "";
}

void BufferManager::UpdatePathsAfterRename(const std::string &notebook_id,
                                           const std::string &old_path,
                                           const std::string &new_path, bool is_folder) {
  bool updated = false;
  std::string prefix = is_folder ? old_path + "/" : "";

  for (auto &pair : buffers_) {
    auto *buffer = pair.second.get();
    if (buffer->GetNotebookId() != notebook_id) {
      continue;
    }

    const std::string &buf_path = buffer->GetFilePath();
    std::string updated_path;

    if (!is_folder) {
      // File rename: exact match only.
      if (buf_path != old_path) {
        continue;
      }
      updated_path = new_path;
    } else {
      // Folder rename: match prefix (files inside the renamed folder).
      if (buf_path.size() > prefix.size() &&
          buf_path.compare(0, prefix.size(), prefix) == 0) {
        updated_path = new_path + "/" + buf_path.substr(prefix.size());
      } else {
        continue;
      }
    }

    std::string old_buf_path = buf_path;
    buffer->SetFilePath(updated_path);
    buffer->ClearBackupPathCache();
    buffer->DiscardBackup();

    // Update provider's cached path if it's a StandardBufferProvider.
    auto *provider = dynamic_cast<StandardBufferProvider *>(buffer->GetProvider());
    if (provider) {
      provider->SetFilePath(updated_path);
    }

    VXCORE_LOG_INFO("UpdatePathsAfterRename: buffer id=%s, old=%s, new=%s", buffer->GetId().c_str(),
                    old_buf_path.c_str(), updated_path.c_str());
    updated = true;
  }

  if (updated) {
    SaveBuffers();
  }
}

std::string BufferManager::OpenBuffer(const std::string &notebook_id,
                                      const std::string &file_path) {
  // Check if buffer already exists (de-duplication)
  std::string existing_id = FindBufferByPath(notebook_id, file_path);
  if (!existing_id.empty()) {
    VXCORE_LOG_DEBUG("Buffer already open: id=%s, path=%s", existing_id.c_str(), file_path.c_str());
    return existing_id;
  }

  // Resolve notebook pointer for validation
  Notebook *notebook = nullptr;
  if (!notebook_id.empty()) {
    notebook = notebook_manager_->GetNotebook(notebook_id);
    if (!notebook) {
      VXCORE_LOG_ERROR("Cannot open buffer: notebook not found: %s", notebook_id.c_str());
      return "";
    }
  }

  // Create new buffer (content loaded lazily on first access)
  std::unique_ptr<Buffer> buffer;
  if (notebook) {
    buffer = std::make_unique<Buffer>(notebook, file_path);
  } else {
    // External file - file_path is absolute
    buffer = std::make_unique<Buffer>(file_path);
  }

  std::string id = buffer->GetId();
  buffers_[id] = std::move(buffer);

  VXCORE_LOG_INFO("Opened buffer: id=%s, notebook_id=%s, file_path=%s", id.c_str(),
                  notebook_id.c_str(), file_path.c_str());
  return id;
}

bool BufferManager::CloseBuffer(const std::string &id) {
  auto it = buffers_.find(id);
  if (it == buffers_.end()) {
    VXCORE_LOG_WARN("Cannot close non-existent buffer: id=%s", id.c_str());
    return false;
  }

  // Clean up backup file before closing buffer
  it->second->DiscardBackup();

  buffers_.erase(it);
  VXCORE_LOG_INFO("Closed buffer: id=%s", id.c_str());
  return true;
}

Buffer *BufferManager::GetBuffer(const std::string &id) {
  auto it = buffers_.find(id);
  if (it == buffers_.end()) {
    return nullptr;
  }
  return it->second.get();
}

std::vector<Buffer *> BufferManager::ListBuffers() {
  std::vector<Buffer *> result;
  result.reserve(buffers_.size());
  for (const auto &pair : buffers_) {
    result.push_back(pair.second.get());
  }
  return result;
}

VxCoreError BufferManager::SaveBuffer(const std::string &id) {
  auto *buffer = GetBuffer(id);
  if (!buffer) {
    VXCORE_LOG_ERROR("Cannot save: buffer not found: id=%s", id.c_str());
    return VXCORE_ERR_BUFFER_NOT_FOUND;
  }

  // Resolve full path for saving
  std::string full_path = buffer->ResolveFullPath();
  if (full_path.empty()) {
    VXCORE_LOG_ERROR("Cannot save: failed to resolve path for buffer: id=%s", id.c_str());
    return VXCORE_ERR_IO;
  }
  buffer->SaveContent(full_path);

  // Check if save succeeded
  if (buffer->GetState() == VXCORE_BUFFER_SAVE_FAILED) {
    return VXCORE_ERR_IO;
  }

  VXCORE_LOG_INFO("Saved buffer: id=%s, revision=%d", id.c_str(), buffer->GetRevision());

  // Clean up backup file after successful save
  if (buffer->GetState() == VXCORE_BUFFER_NORMAL) {
    buffer->DiscardBackup();
  }

  return VXCORE_OK;
}

VxCoreError BufferManager::ReloadBuffer(const std::string &id) {
  auto *buffer = GetBuffer(id);
  if (!buffer) {
    VXCORE_LOG_ERROR("Cannot reload: buffer not found: id=%s", id.c_str());
    return VXCORE_ERR_BUFFER_NOT_FOUND;
  }

  // Resolve full path for loading
  std::string full_path = buffer->ResolveFullPath();
  if (full_path.empty()) {
    VXCORE_LOG_ERROR("Cannot reload: failed to resolve path for buffer: id=%s", id.c_str());
    return VXCORE_ERR_NOT_FOUND;
  }
  buffer->LoadContent(full_path);

  // Check if load succeeded
  if (buffer->GetState() == VXCORE_BUFFER_FILE_MISSING) {
    return VXCORE_ERR_NOT_FOUND;
  }

  VXCORE_LOG_INFO("Reloaded buffer: id=%s, %zu bytes", id.c_str(), buffer->GetContent().size());
  return VXCORE_OK;
}

VxCoreError BufferManager::GetBufferContent(const std::string &id, const void **out_data,
                                            size_t *out_size) {
  if (!out_data || !out_size) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *buffer = GetBuffer(id);
  if (!buffer) {
    VXCORE_LOG_ERROR("Cannot get content: buffer not found: id=%s", id.c_str());
    return VXCORE_ERR_BUFFER_NOT_FOUND;
  }

  // Lazy load content if not yet loaded
  if (!buffer->IsContentLoaded()) {
    std::string full_path = buffer->ResolveFullPath();
    if (full_path.empty()) {
      VXCORE_LOG_ERROR("Cannot load content: failed to resolve path for buffer: id=%s", id.c_str());
      return VXCORE_ERR_NOT_FOUND;
    }
    buffer->LoadContent(full_path);
    if (buffer->GetState() == VXCORE_BUFFER_FILE_MISSING) {
      return VXCORE_ERR_NOT_FOUND;
    }
  }

  *out_data = buffer->GetContent().data();
  *out_size = buffer->GetContent().size();
  return VXCORE_OK;
}

VxCoreError BufferManager::SetBufferContent(const std::string &id, const void *data, size_t size) {
  if (!data) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *buffer = GetBuffer(id);
  if (!buffer) {
    VXCORE_LOG_ERROR("Cannot set content: buffer not found: id=%s", id.c_str());
    return VXCORE_ERR_BUFFER_NOT_FOUND;
  }

  // Copy data into buffer's content vector
  std::vector<uint8_t> new_content(size);
  std::memcpy(new_content.data(), data, size);

  buffer->SetContent(new_content);
  VXCORE_LOG_DEBUG("Set buffer content: id=%s, %zu bytes", id.c_str(), size);
  return VXCORE_OK;
}

VxCoreError BufferManager::WriteBackup(const std::string &id) {
  auto *buffer = GetBuffer(id);
  if (!buffer) {
    VXCORE_LOG_ERROR("Cannot write backup: buffer not found: id=%s", id.c_str());
    return VXCORE_ERR_BUFFER_NOT_FOUND;
  }

  VxCoreError err = buffer->WriteBackup();
  VXCORE_LOG_INFO("Write backup for buffer: id=%s, err=%d", id.c_str(), static_cast<int>(err));
  return err;
}

VxCoreError BufferManager::HasBackup(const std::string &id, bool &out_has_backup) {
  auto *buffer = GetBuffer(id);
  if (!buffer) {
    VXCORE_LOG_ERROR("Cannot check backup: buffer not found: id=%s", id.c_str());
    return VXCORE_ERR_BUFFER_NOT_FOUND;
  }

  out_has_backup = buffer->HasBackup();
  VXCORE_LOG_DEBUG("Checked backup for buffer: id=%s, has_backup=%d", id.c_str(),
                   out_has_backup ? 1 : 0);
  return VXCORE_OK;
}

VxCoreError BufferManager::RecoverBackup(const std::string &id) {
  auto *buffer = GetBuffer(id);
  if (!buffer) {
    VXCORE_LOG_ERROR("Cannot recover backup: buffer not found: id=%s", id.c_str());
    return VXCORE_ERR_BUFFER_NOT_FOUND;
  }

  VxCoreError err = buffer->RecoverBackup();
  VXCORE_LOG_INFO("Recover backup for buffer: id=%s, err=%d", id.c_str(), static_cast<int>(err));
  return err;
}

VxCoreError BufferManager::DiscardBackup(const std::string &id) {
  auto *buffer = GetBuffer(id);
  if (!buffer) {
    VXCORE_LOG_ERROR("Cannot discard backup: buffer not found: id=%s", id.c_str());
    return VXCORE_ERR_BUFFER_NOT_FOUND;
  }

  buffer->DiscardBackup();
  VXCORE_LOG_INFO("Discarded backup for buffer: id=%s", id.c_str());
  return VXCORE_OK;
}

VxCoreError BufferManager::GetBackupPath(const std::string &id, std::string &out_path) {
  auto *buffer = GetBuffer(id);
  if (!buffer) {
    VXCORE_LOG_ERROR("Cannot get backup path: buffer not found: id=%s", id.c_str());
    return VXCORE_ERR_BUFFER_NOT_FOUND;
  }

  out_path = buffer->GetBackupFilePath();
  VXCORE_LOG_DEBUG("Got backup path for buffer: id=%s, path=%s", id.c_str(), out_path.c_str());
  return VXCORE_OK;
}

void BufferManager::CloseBuffersForNotebook(const std::string &notebook_id) {
  std::vector<std::string> to_close;

  // Collect buffer IDs to close
  for (const auto &pair : buffers_) {
    if (pair.second->GetNotebookId() == notebook_id) {
      to_close.push_back(pair.first);
    }
  }

  // Close collected buffers
  for (const auto &id : to_close) {
    CloseBuffer(id);
  }

  if (!to_close.empty()) {
    VXCORE_LOG_INFO("Closed %zu buffers for notebook: id=%s", to_close.size(), notebook_id.c_str());
  }
}

IBufferProvider *BufferManager::GetProvider(const std::string &buffer_id) {
  auto *buffer = GetBuffer(buffer_id);
  if (!buffer) {
    VXCORE_LOG_ERROR("Cannot get provider: buffer not found: id=%s", buffer_id.c_str());
    return nullptr;
  }

  return buffer->GetProvider();
}

}  // namespace vxcore
