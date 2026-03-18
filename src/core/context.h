#ifndef VXCORE_CONTEXT_H
#define VXCORE_CONTEXT_H

#include <memory>
#include <string>

namespace vxcore {

class ConfigManager;
class NotebookManager;
class WorkspaceManager;
class BufferManager;

struct VxCoreContext {
  // IMPORTANT: Member order determines destruction order (reverse of declaration).
  // workspace_manager depends on buffer_manager and config_manager.
  // buffer_manager depends on notebook_manager and config_manager.
  // Destruction: workspace -> buffer -> notebook -> config (safe).
  std::unique_ptr<ConfigManager> config_manager;
  std::unique_ptr<NotebookManager> notebook_manager;
  std::unique_ptr<BufferManager> buffer_manager;
  std::unique_ptr<WorkspaceManager> workspace_manager;
  std::string last_error;
  bool shutdown_called = false;
};

}  // namespace vxcore

#endif
