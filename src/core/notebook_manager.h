#ifndef VXCORE_NOTEBOOK_MANAGER_H
#define VXCORE_NOTEBOOK_MANAGER_H

#include <functional>
#include <map>
#include <memory>
#include <string>

#include "notebook.h"
#include "vxcore/vxcore_types.h"

namespace vxcore {

class ConfigManager;

class NotebookManager {
 public:
  NotebookManager(ConfigManager *config_manager);
  ~NotebookManager();

  VxCoreError CreateNotebook(const std::string &root_folder, NotebookType type,
                             const std::string &config_json, std::string &out_notebook_id);

  VxCoreError OpenNotebook(const std::string &root_folder, std::string &out_notebook_id);

  VxCoreError CloseNotebook(const std::string &notebook_id);

  VxCoreError GetNotebookConfig(const std::string &notebook_id, std::string &out_config_json);

  VxCoreError UpdateNotebookConfig(const std::string &notebook_id, const std::string &config_json);

  VxCoreError ListNotebooks(std::string &out_notebooks_json);

  Notebook *GetNotebook(const std::string &notebook_id);

  // Resolve an absolute path to its containing notebook.
  // Returns the notebook ID and relative path within that notebook.
  // Returns VXCORE_ERR_NOT_FOUND if path is not within any open notebook.
  VxCoreError ResolvePathToNotebook(const std::string &absolute_path, std::string &out_notebook_id,
                                    std::string &out_relative_path);

  // Resolve a node UUID to its containing notebook and relative path.
  // Searches all open notebooks' metadata stores for the given node ID.
  // Returns VXCORE_ERR_NOT_FOUND if no open notebook contains a node with this ID.
  VxCoreError ResolveNodeById(const std::string &node_id, std::string &out_notebook_id,
                              std::string &out_relative_path);

 private:
  void LoadOpenNotebooks();
  Notebook *FindNotebookByRootFolder(const std::string &root_folder);

  NotebookRecord *FindNotebookRecord(const std::string &id);
  VxCoreError UpdateNotebookRecord(const Notebook &notebook);

  nlohmann::json ToNotebookConfig(const Notebook &notebook) const;

  void DeleteNotebookLocalData(const Notebook &notebook);

  ConfigManager *config_manager_ = nullptr;
  std::map<std::string, std::unique_ptr<Notebook>> notebooks_;
};

}  // namespace vxcore

#endif
