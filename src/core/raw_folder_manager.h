#ifndef VXCORE_RAW_FOLDER_MANAGER_H_
#define VXCORE_RAW_FOLDER_MANAGER_H_

#include "folder_manager.h"

namespace vxcore {

class Notebook;

class RawFolderManager : public FolderManager {
 public:
  explicit RawFolderManager(Notebook *notebook);
  ~RawFolderManager() override;

  VxCoreError GetFolderConfig(const std::string &folder_path,
                              std::string &out_config_json) override;
  VxCoreError CreateFolder(const std::string &parent_path, const std::string &folder_name,
                           std::string &out_folder_id) override;
  VxCoreError DeleteFolder(const std::string &folder_path) override;
  VxCoreError UpdateFolderMetadata(const std::string &folder_path,
                                   const std::string &metadata_json) override;
  VxCoreError GetFolderMetadata(const std::string &folder_path,
                                std::string &out_metadata_json) override;
  VxCoreError RenameFolder(const std::string &folder_path, const std::string &new_name) override;
  VxCoreError MoveFolder(const std::string &src_path, const std::string &dest_parent_path) override;
  VxCoreError CopyFolder(const std::string &src_path, const std::string &dest_parent_path,
                         const std::string &new_name, std::string &out_folder_id) override;

  VxCoreError CreateFile(const std::string &folder_path, const std::string &file_name,
                         std::string &out_file_id) override;
  VxCoreError DeleteFile(const std::string &file_path) override;
  VxCoreError UpdateFileMetadata(const std::string &file_path,
                                 const std::string &metadata_json) override;
  VxCoreError UpdateFileTags(const std::string &file_path, const std::string &tags_json) override;
  VxCoreError TagFile(const std::string &file_path, const std::string &tag_name) override;
  VxCoreError UntagFile(const std::string &file_path, const std::string &tag_name) override;
  VxCoreError GetFileAttachments(const std::string &file_path,
                                 std::string &out_attachments_json) override;
  VxCoreError UpdateFileAttachments(const std::string &file_path,
                                    const std::string &attachments_json) override;
  VxCoreError AddFileAttachment(const std::string &file_path,
                                const std::string &attachment) override;
  VxCoreError DeleteFileAttachment(const std::string &file_path,
                                   const std::string &attachment) override;
  VxCoreError GetFileInfo(const std::string &file_path, std::string &out_file_info_json) override;
  VxCoreError GetFileInfo(const std::string &file_path, const FileRecord **out_record) override;
  VxCoreError GetFileMetadata(const std::string &file_path,
                              std::string &out_metadata_json) override;
  VxCoreError RenameFile(const std::string &file_path, const std::string &new_name) override;
  VxCoreError MoveFile(const std::string &src_file_path,
                       const std::string &dest_folder_path) override;
  VxCoreError CopyFile(const std::string &src_file_path, const std::string &dest_folder_path,
                       const std::string &new_name, std::string &out_file_id) override;
  VxCoreError ImportFile(const std::string &folder_path, const std::string &external_file_path,
                         std::string &out_file_id) override;
  VxCoreError ImportFolder(const std::string &dest_folder_path,
                           const std::string &external_folder_path,
                           const std::string &suffix_allowlist,
                           std::string &out_folder_id) override;
  void IterateAllFiles(
      std::function<bool(const std::string &, const FileRecord &)> callback) override;
  VxCoreError FindFilesByTag(const std::string &tag_name, std::string &out_files_json) override;
  VxCoreError ListFolderContents(const std::string &folder_path, bool include_folders_info,
                                 FolderContents &out_contents) override;

  VxCoreError ListExternalNodes(const std::string &folder_path,
                                FolderContents &out_contents) override;

  VxCoreError IndexNode(const std::string &node_path) override;
  VxCoreError UnindexNode(const std::string &node_path) override;

  void ClearCache() override;

 private:
  mutable FileRecord cached_file_record_;
};

}  // namespace vxcore

#endif  // VXCORE_RAW_FOLDER_MANAGER_H_
