#ifndef VXCORE_BUFFER_H
#define VXCORE_BUFFER_H

#include <cstdint>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "vxcore/vxcore_types.h"

namespace vxcore {

class IBufferProvider;
class Notebook;

class BufferManager;

class Buffer {
 public:
  // Constructor for notebook files
  Buffer(Notebook *notebook, const std::string &file_path);

  // Constructor for external files (absolute path)
  explicit Buffer(const std::string &absolute_path);

  ~Buffer();

  // Accessors
  const std::string &GetId() const { return id_; }
  Notebook *GetNotebook() const { return notebook_; }
  std::string GetNotebookId() const;
  const std::string &GetFilePath() const { return file_path_; }
  int GetRevision() const { return revision_; }
  bool IsModified() const { return modified_; }
  VxCoreBufferState GetState() const { return state_; }
  const nlohmann::json &GetMetadata() const { return metadata_; }
  int64_t GetLastModifiedTime() const { return last_modified_time_; }
  bool IsContentLoaded() const { return content_loaded_; }
  IBufferProvider *GetProvider() const { return provider_.get(); }

  // Mutators
  void SetModified(bool modified) { modified_ = modified; }
  void SetState(VxCoreBufferState state) { state_ = state; }
  void SetMetadata(const nlohmann::json &metadata) { metadata_ = metadata; }

  // Content management methods
  void LoadContent(const std::string &full_path);
  void SaveContent(const std::string &full_path);
  const std::vector<uint8_t> &GetContent() const;
  void SetContent(const std::vector<uint8_t> &data);
  void CheckExternalChanges(const std::string &full_path);

  // Resolve full path (uses notebook root if available, else file_path as-is)
  std::string ResolveFullPath() const;

  // Backup file operations
  std::string GetBackupFilePath();
  VxCoreError WriteBackup();
  bool HasBackup();
  VxCoreError RecoverBackup();
  void DiscardBackup();

 private:
  friend class BufferManager;  // Allow BufferManager to set ID during reload

  // Private setter for ID (used by BufferManager when restoring from session)
  void SetId(const std::string &id) { id_ = id; }

  // Private setter for file path (used by BufferManager on rename)
  void SetFilePath(const std::string &file_path) { file_path_ = file_path; }

  // Clear cached backup file path (used after path changes)
  void ClearBackupPathCache() { backup_file_path_.clear(); }

  // Private default constructor for internal use
  Buffer();

  // Create appropriate provider based on context
  void CreateProvider();

  std::string id_;
  Notebook *notebook_;       // nullptr for external files (non-owning)
  std::string file_path_;    // Relative to notebook root, or absolute for external
  int revision_;             // Content revision number
  bool modified_;            // Unsaved changes flag
  VxCoreBufferState state_;  // Buffer state (NORMAL, FILE_MISSING, etc.)
  nlohmann::json metadata_;
  std::vector<uint8_t> content_;  // Cached file content as raw bytes
  int64_t last_modified_time_;    // File timestamp for change detection
  bool content_loaded_;           // True if content has been loaded from disk
  std::unique_ptr<IBufferProvider> provider_;
  std::string backup_file_path_;  // Cached backup file path (<full_path>.vswp)
};

struct BufferRecord {
  std::string id;
  std::string notebook_id;
  std::string file_path;

  BufferRecord();

  static BufferRecord FromJson(const nlohmann::json &json);
  nlohmann::json ToJson() const;
};

}  // namespace vxcore

#endif  // VXCORE_BUFFER_H
