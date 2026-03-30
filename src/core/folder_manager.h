#ifndef VXCORE_FOLDER_MANAGER_H
#define VXCORE_FOLDER_MANAGER_H

#include <filesystem>
#include <functional>
#include <string>

#include "folder.h"
#include "notebook.h"
#include "utils/file_utils.h"
#include "vxcore/vxcore_types.h"

namespace vxcore {

class Notebook;

class FolderManager {
 public:
  explicit FolderManager(Notebook *notebook) : notebook_(notebook) {}

  virtual ~FolderManager() = default;

  virtual VxCoreError InitOnCreation() { return VXCORE_OK; }

  virtual VxCoreError GetFolderConfig(const std::string &folder_path,
                                      std::string &out_config_json) = 0;

  virtual VxCoreError CreateFolder(const std::string &parent_path, const std::string &folder_name,
                                   std::string &out_folder_id) = 0;

  // Create all folders in the path if they don't exist.
  // Returns the ID of the final (leaf) folder.
  virtual VxCoreError CreateFolderPath(const std::string &folder_path, std::string &out_folder_id);

  virtual VxCoreError DeleteFolder(const std::string &folder_path) = 0;

  virtual VxCoreError UpdateFolderMetadata(const std::string &folder_path,
                                           const std::string &metadata_json) = 0;

  virtual VxCoreError GetFolderMetadata(const std::string &folder_path,
                                        std::string &out_metadata_json) = 0;

  virtual VxCoreError RenameFolder(const std::string &folder_path, const std::string &new_name) = 0;

  virtual VxCoreError MoveFolder(const std::string &src_path,
                                 const std::string &dest_parent_path) = 0;

  virtual VxCoreError CopyFolder(const std::string &src_path, const std::string &dest_parent_path,
                                 const std::string &new_name, std::string &out_folder_id) = 0;

  virtual VxCoreError CreateFile(const std::string &folder_path, const std::string &file_name,
                                 std::string &out_file_id) = 0;

  virtual VxCoreError DeleteFile(const std::string &file_path) = 0;

  // Move a node to recycle bin when supported by notebook type.
  // Default implementation falls back to permanent deletion.
  virtual VxCoreError MoveToRecycleBin(const std::filesystem::path &source_path);

  virtual VxCoreError UpdateFileMetadata(const std::string &file_path,
                                         const std::string &metadata_json) = 0;

  virtual VxCoreError UpdateFileTags(const std::string &file_path,
                                     const std::string &tags_json) = 0;

  virtual VxCoreError TagFile(const std::string &file_path, const std::string &tag_name) = 0;

  virtual VxCoreError UntagFile(const std::string &file_path, const std::string &tag_name) = 0;

  // Get file attachments as JSON array
  virtual VxCoreError GetFileAttachments(const std::string &file_path,
                                         std::string &out_attachments_json) = 0;

  // Update file attachments from JSON array
  virtual VxCoreError UpdateFileAttachments(const std::string &file_path,
                                            const std::string &attachments_json) = 0;

  // Add a single attachment to a file (with deduplication)
  virtual VxCoreError AddFileAttachment(const std::string &file_path,
                                        const std::string &attachment) = 0;

  // Delete a single attachment from a file
  virtual VxCoreError DeleteFileAttachment(const std::string &file_path,
                                           const std::string &attachment) = 0;

  virtual VxCoreError GetFileInfo(const std::string &file_path,
                                  std::string &out_file_info_json) = 0;

  virtual VxCoreError GetFileInfo(const std::string &file_path, const FileRecord **out_record) = 0;

  virtual VxCoreError GetFileMetadata(const std::string &file_path,
                                      std::string &out_metadata_json) = 0;

  virtual VxCoreError RenameFile(const std::string &file_path, const std::string &new_name) = 0;

  virtual VxCoreError MoveFile(const std::string &src_file_path,
                               const std::string &dest_folder_path) = 0;

  virtual VxCoreError CopyFile(const std::string &src_file_path,
                               const std::string &dest_folder_path, const std::string &new_name,
                               std::string &out_file_id) = 0;

  virtual VxCoreError ImportFile(const std::string &folder_path,
                                 const std::string &external_file_path,
                                 std::string &out_file_id) = 0;

  virtual VxCoreError ImportFolder(const std::string &dest_folder_path,
                                   const std::string &external_folder_path,
                                   const std::string &suffix_allowlist,
                                   std::string &out_folder_id) = 0;

  virtual void IterateAllFiles(
      std::function<bool(const std::string &, const FileRecord &)> callback) = 0;

  virtual VxCoreError FindFilesByTag(const std::string &tag_name, std::string &out_files_json) = 0;

  // Index a filesystem node (file or folder) into metadata.
  // The node must exist on filesystem but not be tracked in metadata.
  virtual VxCoreError IndexNode(const std::string &node_path) = 0;

  // Remove a node from metadata without touching filesystem.
  virtual VxCoreError UnindexNode(const std::string &node_path) = 0;

  struct FolderContents {
    std::vector<FileRecord> files;
    std::vector<FolderRecord> folders;
  };

  virtual VxCoreError ListFolderContents(const std::string &folder_path, bool include_folders_info,
                                         FolderContents &out_contents) = 0;

  // List external (unindexed) filesystem nodes in a folder.
  // External nodes exist on filesystem but are not tracked in metadata.
  // Returns files and folders that are present on disk but not in FolderConfig.
  virtual VxCoreError ListExternalNodes(const std::string &folder_path,
                                        FolderContents &out_contents) = 0;

  virtual void ClearCache() = 0;

  // Get the public assets folder path for a file.
  // The path is resolved based on notebook's assetsFolder config and file's parent folder.
  // Config can be: simple folder name, relative path, or absolute path.
  std::string GetPublicAssetsFolder(const std::string &file_path) const;

  // Get the concrete assets folder path for a file (public folder + file's UUID).
  // Returns empty string if file not found.
  std::string GetAssetsFolder(const std::string &file_path);

  // Get an available name for a new node (file or folder) in a folder.
  // If new_name is available, returns it directly.
  // If new_name exists, tries new_name_1, new_name_2, etc. until an available name is found.
  // For files with extensions, the suffix is inserted before the extension (e.g., file_1.txt).
  // folder_path: Path relative to notebook root ("." for root)
  // new_name: Desired name for the new node
  // out_available_name: Receives the available name
  VxCoreError GetAvailableName(const std::string &folder_path, const std::string &new_name,
                               std::string &out_available_name);

 protected:
  inline std::string GetCleanRelativePath(const std::string &path) const {
    return notebook_->GetCleanRelativePath(path);
  }

  Notebook *notebook_ = nullptr;
};

}  // namespace vxcore

#endif
