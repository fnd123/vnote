#ifndef VXCORE_WORKSPACE_MANAGER_H
#define VXCORE_WORKSPACE_MANAGER_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "workspace.h"

namespace vxcore {

class BufferManager;
class ConfigManager;

class WorkspaceManager {
 public:
  explicit WorkspaceManager(ConfigManager *config_manager, BufferManager *buffer_manager);
  ~WorkspaceManager();

  // Create a new workspace with the given name, returns new workspace ID
  std::string CreateWorkspace(const std::string &name);

  // Delete a workspace by ID, returns true if successful
  bool DeleteWorkspace(const std::string &id);

  // Get workspace by ID, returns nullptr if not found
  WorkspaceConfig *GetWorkspace(const std::string &id);

  // List all workspaces
  std::vector<WorkspaceConfig> ListWorkspaces();

  // Rename a workspace, returns true if successful
  bool RenameWorkspace(const std::string &id, const std::string &name);

  // Get the current workspace ID
  std::string GetCurrentWorkspaceId() const;

  // Set the current workspace ID, returns true if workspace exists
  bool SetCurrentWorkspaceId(const std::string &id);

  // Add a buffer to a workspace, returns true if successful
  bool AddBufferToWorkspace(const std::string &ws_id, const std::string &buf_id);

  // Remove a buffer from a workspace, returns true if successful
  bool RemoveBufferFromWorkspace(const std::string &ws_id, const std::string &buf_id);

  // Set the current buffer in a workspace, returns true if successful
  bool SetCurrentBufferInWorkspace(const std::string &ws_id, const std::string &buf_id);

  // Mark that shutdown has been called (prevents destructor from saving)
  void SetShutdownCalled(bool called) { shutdown_called_ = called; }

  // Update workspace records in session config (in-memory only, no disk write)
  void UpdateSessionWorkspaces();

  // Save workspaces to session config and write to disk
  void SaveWorkspaces();

 private:
  void LoadWorkspaces();

  ConfigManager *config_manager_ = nullptr;
  BufferManager *buffer_manager_ = nullptr;
  std::map<std::string, std::unique_ptr<WorkspaceConfig>> workspaces_;
  std::string current_workspace_id_;
  bool shutdown_called_ = false;
};

}  // namespace vxcore

#endif  // VXCORE_WORKSPACE_MANAGER_H
