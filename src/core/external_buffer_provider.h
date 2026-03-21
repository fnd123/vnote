#ifndef VXCORE_EXTERNAL_BUFFER_PROVIDER_H
#define VXCORE_EXTERNAL_BUFFER_PROVIDER_H

#include <string>
#include <vector>

#include "buffer_provider.h"
#include "vxcore/vxcore_types.h"

namespace vxcore {

// Buffer provider for external files (not in any notebook).
// Assets are stored in <file_dir>/<filename_without_ext>_assets/
// Example: /path/to/notes.md -> assets in /path/to/notes_assets/
//
// Attachment operations work on filesystem only (no metadata tracking).
class ExternalBufferProvider : public IBufferProvider {
 public:
  explicit ExternalBufferProvider(const std::string &absolute_file_path);
  ~ExternalBufferProvider() override = default;

  // IBufferProvider implementation
  std::string GetType() const override { return "external"; }

  // Asset operations (filesystem only)
  VxCoreError InsertAssetRaw(const std::string &name, const std::vector<uint8_t> &data,
                             std::string &out_relative_path) override;

  VxCoreError InsertAsset(const std::string &source_path, std::string &out_relative_path) override;

  VxCoreError DeleteAsset(const std::string &relative_path) override;

  VxCoreError GetAssetsFolder(std::string &out_path) override;

  VxCoreError GetAssetAbsolutePath(const std::string &relative_path,
                                   std::string &out_abs_path) override;

  // Resource resolution
  VxCoreError GetResourceBasePath(std::string &out_path) override;

  // Attachment operations (filesystem only, no metadata for external files)
  VxCoreError InsertAttachment(const std::string &source_path, std::string &out_filename) override;

  VxCoreError DeleteAttachment(const std::string &filename) override;

  VxCoreError RenameAttachment(const std::string &old_filename, const std::string &new_filename,
                               std::string &out_new_filename) override;

  VxCoreError ListAttachments(std::vector<std::string> &out_filenames) override;

  VxCoreError GetAttachmentsFolder(std::string &out_path) override;

 private:
  // Get unique asset name if file exists (image.png -> image_1.png -> image_2.png)
  std::string GetUniqueAssetName(const std::string &name);

  std::string file_dir_;        // Parent directory of the file
  std::string file_name_stem_;  // Filename without extension
  std::string assets_folder_;   // Absolute path to assets folder
};

}  // namespace vxcore

#endif  // VXCORE_EXTERNAL_BUFFER_PROVIDER_H
