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

    if (!std::filesystem::exists(full_path)) {
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

  SaveBuffers();
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

void BufferManager::AutoSaveTick() {
  int64_t current_time = GetCurrentTimestampMillis();
  int saved_count = 0;

  for (const auto &pair : buffers_) {
    auto *buffer = pair.second.get();
    if (!buffer->IsModified()) {
      continue;
    }

    // Check if enough time has passed since last modification
    int64_t time_since_modification = current_time - buffer->GetLastModifiedTime();
    if (time_since_modification >= auto_save_interval_ms_) {
      VxCoreError err = SaveBuffer(buffer->GetId());
      if (err == VXCORE_OK) {
        saved_count++;
      } else {
        VXCORE_LOG_WARN("Auto-save failed for buffer: id=%s, error=%d", buffer->GetId().c_str(),
                        err);
      }
    }
  }

  if (saved_count > 0) {
    VXCORE_LOG_DEBUG("Auto-saved %d buffers", saved_count);
  }
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
