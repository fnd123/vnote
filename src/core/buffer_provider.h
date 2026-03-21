#ifndef VXCORE_BUFFER_PROVIDER_H
#define VXCORE_BUFFER_PROVIDER_H

#include <memory>
#include <string>
#include <vector>

#include "vxcore/vxcore_types.h"

namespace vxcore {

class Notebook;

// Abstract interface for buffer asset/attachment operations.
// Different implementations handle different file types/layouts:
// - StandardBufferProvider: regular notebook files (assets in vx_assets/<file_uuid>/)
// - ExternalBufferProvider: external files (assets in <filename>_assets/)
// - (Future) TextBundleProvider: text bundle folders (assets inside bundle)
//
// Terminology:
// - "Asset": Any file in the assets folder (filesystem only, no metadata tracking)
// - "Attachment": A file in the assets folder that is tracked in the attachment list
//
// Lifecycle: created when buffer opens, destroyed when buffer closes.
// Provider is owned by BufferManager, one per buffer.
class IBufferProvider {
 public:
  virtual ~IBufferProvider() = default;

  // Get the type identifier for this provider (for debugging/logging)
  virtual std::string GetType() const = 0;

  // ============ Asset Filesystem Operations (No Metadata) ============
  // These methods only operate on the filesystem, never touching metadata.

  // Insert binary data as an asset file.
  // Creates assets folder lazily if it doesn't exist.
  // Does NOT add to attachment list (use InsertAttachment for that).
  //
  // name: desired filename (e.g., "image.png"). If name already exists,
  //       a unique name is generated (e.g., "image_1.png").
  // data: binary content to write
  // out_relative_path: receives path relative to file's parent directory for embedding
  //                    (e.g., "vx_assets/<uuid>/image.png" for notebook files,
  //                     or "<filename>_assets/image.png" for external files)
  virtual VxCoreError InsertAssetRaw(const std::string &name, const std::vector<uint8_t> &data,
                                     std::string &out_relative_path) = 0;

  // Copy a file from source path to assets folder.
  // Does NOT add to attachment list (use InsertAttachment for that).
  //
  // source_path: absolute path to source file
  // out_relative_path: receives relative path for embedding
  virtual VxCoreError InsertAsset(const std::string &source_path,
                                  std::string &out_relative_path) = 0;

  // Delete an asset file from the assets folder.
  // Does NOT touch attachment metadata.
  //
  // relative_path: path as returned by InsertAsset/InsertAssetRaw
  virtual VxCoreError DeleteAsset(const std::string &relative_path) = 0;

  // Get absolute filesystem path to the assets folder.
  // Creates folder lazily if it doesn't exist.
  virtual VxCoreError GetAssetsFolder(std::string &out_path) = 0;

  // Get absolute filesystem path to a specific asset.
  // relative_path: path as returned by InsertAsset
  virtual VxCoreError GetAssetAbsolutePath(const std::string &relative_path,
                                           std::string &out_abs_path) = 0;

  // ============ Resource Resolution ============
  // Get the base path for resolving relative resource URLs in the file's content.
  // For standard files: parent directory of the file (e.g., notebook_root/folder/)
  // For external files: parent directory of the file
  // For TextBundle (future): the bundle directory itself
  virtual VxCoreError GetResourceBasePath(std::string &out_path) = 0;

  // ============ Attachment Operations (Filesystem + Metadata) ============
  // These methods operate on both filesystem and attachment metadata.
  // For ExternalBufferProvider, metadata operations return VXCORE_ERR_UNSUPPORTED.

  // Copy a file to attachments folder and add to attachment list.
  // For StandardBufferProvider: copies file + adds to FileRecord.attachments
  // For ExternalBufferProvider: copies file only (no metadata tracking)
  //
  // source_path: absolute path to source file
  // out_filename: receives just the filename (not full path) for the attachment
  virtual VxCoreError InsertAttachment(const std::string &source_path,
                                       std::string &out_filename) = 0;

  // Delete an attachment file and remove from attachment list.
  // filename: just the filename (not full path)
  virtual VxCoreError DeleteAttachment(const std::string &filename) = 0;

  // Rename an attachment file and update attachment list.
  // old_filename: current filename
  // new_filename: new filename. If exists, a unique name is generated.
  // out_new_filename: receives the actual new filename (may differ if collision)
  virtual VxCoreError RenameAttachment(const std::string &old_filename,
                                       const std::string &new_filename,
                                       std::string &out_new_filename) = 0;

  // List all attachments (from metadata, not filesystem scan).
  // For StandardBufferProvider: returns FileRecord.attachments
  // For ExternalBufferProvider: returns filesystem listing (no metadata)
  // out_filenames: receives list of filenames (not full paths)
  virtual VxCoreError ListAttachments(std::vector<std::string> &out_filenames) = 0;

  // Get absolute filesystem path to the attachments folder.
  // Creates folder lazily if it doesn't exist.
  // (Same as GetAssetsFolder - attachments and assets share the same folder)
  virtual VxCoreError GetAttachmentsFolder(std::string &out_path) = 0;
};

// Factory function to create appropriate provider for a notebook file.
// Returns nullptr for raw notebooks (unsupported).
// notebook: the notebook containing the file
// file_path: relative path within the notebook
std::unique_ptr<IBufferProvider> CreateBufferProvider(Notebook *notebook,
                                                      const std::string &file_path);

// Factory function to create provider for an external file (no notebook).
// absolute_file_path: absolute filesystem path to the file
std::unique_ptr<IBufferProvider> CreateBufferProviderForExternal(
    const std::string &absolute_file_path);

}  // namespace vxcore

#endif  // VXCORE_BUFFER_PROVIDER_H
