#include "external_buffer_provider.h"

#include <filesystem>
#include <fstream>

#include "utils/file_utils.h"
#include "utils/logger.h"

namespace vxcore {

namespace fs = std::filesystem;

// Helper function to extract filename stem (without extension)
static std::string GetFileStem(const std::string &filename) {
  fs::path p(filename);
  return p.stem().string();
}

ExternalBufferProvider::ExternalBufferProvider(const std::string &absolute_file_path) {
  fs::path file_path = CleanPath(absolute_file_path);

  // Extract components
  file_dir_ = file_path.parent_path().string();
  file_name_stem_ = GetFileStem(file_path.filename().string());

  // Build assets folder path: <file_dir>/<file_name_stem>_assets
  assets_folder_ = (fs::path(file_dir_) / (file_name_stem_ + "_assets")).string();

  VXCORE_LOG_DEBUG("ExternalBufferProvider created for: %s", absolute_file_path.c_str());
  VXCORE_LOG_DEBUG("  file_dir: %s", file_dir_.c_str());
  VXCORE_LOG_DEBUG("  file_name_stem: %s", file_name_stem_.c_str());
  VXCORE_LOG_DEBUG("  assets_folder: %s", assets_folder_.c_str());
}

std::string ExternalBufferProvider::GetUniqueAssetName(const std::string &name) {
  fs::path base_path = fs::path(assets_folder_) / name;

  if (!fs::exists(base_path)) {
    return name;
  }

  // Extract stem and extension
  fs::path name_path(name);
  std::string stem = name_path.stem().string();
  std::string extension = name_path.extension().string();

  // Try suffixes: _1, _2, _3, ...
  for (int i = 1; i < 10000; ++i) {
    std::string candidate = stem + "_" + std::to_string(i) + extension;
    fs::path candidate_path = fs::path(assets_folder_) / candidate;
    if (!fs::exists(candidate_path)) {
      return candidate;
    }
  }

  // Fallback: append timestamp
  VXCORE_LOG_WARN("Too many collisions for asset name: %s", name.c_str());
  return stem + "_" + std::to_string(std::time(nullptr)) + extension;
}

VxCoreError ExternalBufferProvider::InsertAssetRaw(const std::string &name,
                                                   const std::vector<uint8_t> &data,
                                                   std::string &out_relative_path) {
  try {
    // Create assets folder if it doesn't exist
    if (!fs::exists(assets_folder_)) {
      fs::create_directories(assets_folder_);
      VXCORE_LOG_INFO("Created assets folder: %s", assets_folder_.c_str());
    }

    // Generate unique name if needed
    std::string unique_name = GetUniqueAssetName(name);

    // Write binary data
    fs::path asset_path = fs::path(assets_folder_) / unique_name;
    std::ofstream ofs(asset_path, std::ios::binary);
    if (!ofs) {
      VXCORE_LOG_ERROR("Failed to open file for writing: %s", asset_path.string().c_str());
      return VXCORE_ERR_IO;
    }

    ofs.write(reinterpret_cast<const char *>(data.data()), data.size());
    ofs.close();

    if (!ofs) {
      VXCORE_LOG_ERROR("Failed to write file: %s", asset_path.string().c_str());
      return VXCORE_ERR_IO;
    }

    // Build relative path: <file_name_stem>_assets/<unique_name>
    out_relative_path = file_name_stem_ + "_assets/" + unique_name;

    VXCORE_LOG_INFO("Inserted asset (raw): %s", out_relative_path.c_str());
    return VXCORE_OK;

  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("InsertAssetRaw failed: %s", e.what());
    return VXCORE_ERR_IO;
  }
}

VxCoreError ExternalBufferProvider::InsertAsset(const std::string &source_path,
                                                std::string &out_relative_path) {
  try {
    // Check source file exists
    if (!fs::exists(source_path)) {
      VXCORE_LOG_ERROR("Source file does not exist: %s", source_path.c_str());
      return VXCORE_ERR_NOT_FOUND;
    }

    // Create assets folder if it doesn't exist
    if (!fs::exists(assets_folder_)) {
      fs::create_directories(assets_folder_);
      VXCORE_LOG_INFO("Created assets folder: %s", assets_folder_.c_str());
    }

    // Extract filename from source path
    fs::path src_path(source_path);
    std::string filename = src_path.filename().string();

    // Generate unique name if needed
    std::string unique_name = GetUniqueAssetName(filename);

    // Copy file
    fs::path dest_path = fs::path(assets_folder_) / unique_name;
    fs::copy_file(source_path, dest_path, fs::copy_options::overwrite_existing);

    // Build relative path: <file_name_stem>_assets/<unique_name>
    out_relative_path = file_name_stem_ + "_assets/" + unique_name;

    VXCORE_LOG_INFO("Inserted asset (copy): %s -> %s", source_path.c_str(),
                    out_relative_path.c_str());
    return VXCORE_OK;

  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("InsertAsset failed: %s", e.what());
    return VXCORE_ERR_IO;
  }
}

VxCoreError ExternalBufferProvider::DeleteAsset(const std::string &relative_path) {
  try {
    // Convert relative path to absolute: <file_dir>/<relative_path>
    fs::path abs_path = fs::path(file_dir_) / relative_path;

    if (!fs::exists(abs_path)) {
      VXCORE_LOG_WARN("Asset does not exist: %s", abs_path.string().c_str());
      return VXCORE_ERR_NOT_FOUND;
    }

    fs::remove(abs_path);
    VXCORE_LOG_INFO("Deleted asset: %s", relative_path.c_str());

    return VXCORE_OK;

  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("DeleteAsset failed: %s", e.what());
    return VXCORE_ERR_IO;
  }
}

VxCoreError ExternalBufferProvider::GetAssetsFolder(std::string &out_path) {
  try {
    // Create directory if it doesn't exist
    if (!fs::exists(assets_folder_)) {
      fs::create_directories(assets_folder_);
      VXCORE_LOG_INFO("Created assets folder: %s", assets_folder_.c_str());
    }

    out_path = assets_folder_;
    return VXCORE_OK;

  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("GetAssetsFolder failed: %s", e.what());
    return VXCORE_ERR_IO;
  }
}

VxCoreError ExternalBufferProvider::GetAssetAbsolutePath(const std::string &relative_path,
                                                         std::string &out_abs_path) {
  // relative_path already includes assets folder name (e.g., "notes_assets/image.png")
  // so just concatenate with file_dir
  out_abs_path = (fs::path(file_dir_) / relative_path).string();
  return VXCORE_OK;
}

VxCoreError ExternalBufferProvider::GetResourceBasePath(std::string &out_path) {
  out_path = CleanPath(file_dir_);
  return VXCORE_OK;
}

// Attachment operations for external files (filesystem only, no metadata)

VxCoreError ExternalBufferProvider::InsertAttachment(const std::string &source_path,
                                                     std::string &out_filename) {
  // For external files, attachment = asset (filesystem only, no metadata)
  std::string relative_path;
  VxCoreError err = InsertAsset(source_path, relative_path);
  if (err != VXCORE_OK) {
    return err;
  }

  // Extract filename from relative path
  fs::path rel(relative_path);
  out_filename = rel.filename().string();
  VXCORE_LOG_INFO("Inserted attachment (external): %s", out_filename.c_str());
  return VXCORE_OK;
}

VxCoreError ExternalBufferProvider::DeleteAttachment(const std::string &filename) {
  // Build relative path from filename
  std::string relative_path = file_name_stem_ + "_assets/" + filename;
  VxCoreError err = DeleteAsset(relative_path);
  if (err == VXCORE_OK) {
    VXCORE_LOG_INFO("Deleted attachment (external): %s", filename.c_str());
  }
  return err;
}

VxCoreError ExternalBufferProvider::RenameAttachment(const std::string &old_filename,
                                                     const std::string &new_filename,
                                                     std::string &out_new_filename) {
  try {
    // Build old absolute path
    fs::path old_abs_path = fs::path(assets_folder_) / old_filename;

    if (!fs::exists(old_abs_path)) {
      VXCORE_LOG_ERROR("Attachment does not exist: %s", old_abs_path.string().c_str());
      return VXCORE_ERR_NOT_FOUND;
    }

    // Generate unique new name if needed
    std::string unique_new_name = GetUniqueAssetName(new_filename);

    // Build new absolute path
    fs::path new_abs_path = fs::path(assets_folder_) / unique_new_name;

    // Rename
    fs::rename(old_abs_path, new_abs_path);

    out_new_filename = unique_new_name;

    VXCORE_LOG_INFO("Renamed attachment (external): %s -> %s", old_filename.c_str(),
                    unique_new_name.c_str());

    return VXCORE_OK;

  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("RenameAttachment failed: %s", e.what());
    return VXCORE_ERR_IO;
  }
}

VxCoreError ExternalBufferProvider::ListAttachments(std::vector<std::string> &out_filenames) {
  out_filenames.clear();

  try {
    // Check if assets folder exists
    if (!fs::exists(assets_folder_)) {
      // No assets folder means no attachments
      return VXCORE_OK;
    }

    if (!fs::is_directory(assets_folder_)) {
      VXCORE_LOG_ERROR("Assets folder is not a directory: %s", assets_folder_.c_str());
      return VXCORE_ERR_IO;
    }

    // Iterate directory entries and return filenames (not full paths)
    for (const auto &entry : fs::directory_iterator(assets_folder_)) {
      if (entry.is_regular_file()) {
        out_filenames.push_back(entry.path().filename().string());
      }
    }

    VXCORE_LOG_DEBUG("Listed %zu attachments (external)", out_filenames.size());
    return VXCORE_OK;

  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("ListAttachments failed: %s", e.what());
    return VXCORE_ERR_IO;
  }
}

VxCoreError ExternalBufferProvider::GetAttachmentsFolder(std::string &out_path) {
  // Attachments and assets share the same folder
  return GetAssetsFolder(out_path);
}

}  // namespace vxcore
