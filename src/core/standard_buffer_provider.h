// Copyright (c) 2025 VNote
#ifndef VXCORE_STANDARD_BUFFER_PROVIDER_H
#define VXCORE_STANDARD_BUFFER_PROVIDER_H

#include <cstdint>
#include <string>
#include <vector>

#include "buffer_provider.h"
#include "vxcore/vxcore_types.h"

namespace vxcore {

class Notebook;

// StandardBufferProvider implements IBufferProvider for bundled notebook files.
// Assets are stored in <notebook_root>/<assets_folder>/<file_uuid>/ directories.
class StandardBufferProvider : public IBufferProvider {
 public:
  // Constructor.
  // @param notebook: The notebook instance (not owned).
  // @param file_path: The relative path of the file within the notebook.
  StandardBufferProvider(Notebook *notebook, const std::string &file_path);

  ~StandardBufferProvider() override = default;

  // IBufferProvider interface implementation
  std::string GetType() const override { return "standard"; }

  // Asset operations (filesystem only, no metadata)
  VxCoreError InsertAssetRaw(const std::string &name, const std::vector<uint8_t> &data,
                             std::string &out_relative_path) override;

  VxCoreError InsertAsset(const std::string &source_path, std::string &out_relative_path) override;

  VxCoreError DeleteAsset(const std::string &relative_path) override;

  VxCoreError GetAssetsFolder(std::string &out_path) override;

  VxCoreError GetAssetAbsolutePath(const std::string &relative_path,
                                   std::string &out_abs_path) override;

  // Attachment operations (filesystem + metadata)
  VxCoreError InsertAttachment(const std::string &source_path, std::string &out_filename) override;

  VxCoreError DeleteAttachment(const std::string &filename) override;

  VxCoreError RenameAttachment(const std::string &old_filename, const std::string &new_filename,
                               std::string &out_new_filename) override;

  VxCoreError ListAttachments(std::vector<std::string> &out_filenames) override;

  VxCoreError GetAttachmentsFolder(std::string &out_path) override;

 private:
  // Ensures the assets folder exists, creating it if necessary.
  VxCoreError EnsureAssetsFolderExists();

  // Gets the assets folder path (internal helper, doesn't create folder)
  std::string GetAssetsFolderPath();

  // Generates a unique asset name if there's a collision.
  // @param base_name: The original filename.
  // @param assets_folder_path: The absolute path to the assets folder.
  // @return A unique filename.
  std::string GetUniqueAssetName(const std::string &base_name,
                                 const std::string &assets_folder_path);

  Notebook *notebook_;     // Not owned
  std::string file_path_;  // Relative path within notebook
  std::string file_id_;    // Cached file UUID

  // Allow BufferManager to update cached path after rename.
  friend class BufferManager;
  void SetFilePath(const std::string &path) { file_path_ = path; }
};

}  // namespace vxcore

#endif  // VXCORE_STANDARD_BUFFER_PROVIDER_H
