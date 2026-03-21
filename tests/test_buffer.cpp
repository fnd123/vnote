#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

#include "test_utils.h"
#include "vxcore/vxcore.h"

// Simple base64 encoder for test purposes (RFC 4648)
static std::string test_base64_encode(const uint8_t *data, size_t size) {
  static const char *chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string result;
  result.reserve(((size + 2) / 3) * 4);

  for (size_t i = 0; i < size; i += 3) {
    uint32_t triple = static_cast<uint32_t>(data[i]) << 16;
    if (i + 1 < size) triple |= static_cast<uint32_t>(data[i + 1]) << 8;
    if (i + 2 < size) triple |= static_cast<uint32_t>(data[i + 2]);

    result += chars[(triple >> 18) & 0x3F];
    result += chars[(triple >> 12) & 0x3F];
    result += (i + 1 < size) ? chars[(triple >> 6) & 0x3F] : '=';
    result += (i + 2 < size) ? chars[triple & 0x3F] : '=';
  }
  return result;
}

// Simple base64 decoder for test purposes
static std::vector<uint8_t> test_base64_decode(const std::string &encoded) {
  static const int8_t table[256] = {
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62,
      -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -2, -1, -1, -1, 0,
      1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
      23, 24, 25, -1, -1, -1, -1, -1, -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
      39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
  };
  std::vector<uint8_t> result;
  uint32_t buffer = 0;
  int bits = 0;
  for (char c : encoded) {
    int8_t val = table[static_cast<uint8_t>(c)];
    if (val >= 0) {
      buffer = (buffer << 6) | static_cast<uint32_t>(val);
      bits += 6;
      if (bits >= 8) {
        bits -= 8;
        result.push_back(static_cast<uint8_t>((buffer >> bits) & 0xFF));
      }
    }
  }
  return result;
}

// Helper to create a test file
void create_test_file(const std::string &path, const std::string &content) {
  std::ofstream file(path, std::ios::binary);
  if (file.is_open()) {
    file.write(content.c_str(), content.length());
    file.close();
  }
}

// Helper to read file content
std::string read_file_content(const std::string &path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) return "";

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::string buffer(size, '\0');
  file.read(&buffer[0], size);
  return buffer;
}

int test_buffer_open_close() {
  std::cout << "  Running test_buffer_open_close..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_open"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_open").c_str(),
                               "{\"name\":\"Buffer Test\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a test file
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Open buffer
  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(buffer_id);

  // Close buffer
  err = vxcore_buffer_close(ctx, buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  vxcore_string_free(buffer_id);
  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_open"));
  std::cout << "  ✓ test_buffer_open_close passed" << std::endl;
  return 0;
}

int test_buffer_get() {
  std::cout << "  Running test_buffer_get..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_get"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_get").c_str(),
                               "{\"name\":\"Buffer Test\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Get buffer info
  char *buffer_json = nullptr;
  err = vxcore_buffer_get(ctx, buffer_id, &buffer_json);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(buffer_json);

  // Verify JSON contains expected fields
  nlohmann::json buffer_data = nlohmann::json::parse(buffer_json);
  ASSERT(buffer_data.contains("id"));
  ASSERT(buffer_data.contains("filePath"));
  ASSERT(buffer_data["filePath"] == "test.md");

  vxcore_string_free(buffer_json);
  vxcore_string_free(buffer_id);
  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_get"));
  std::cout << "  ✓ test_buffer_get passed" << std::endl;
  return 0;
}

int test_buffer_list() {
  std::cout << "  Running test_buffer_list..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_list"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_list").c_str(),
                               "{\"name\":\"Buffer Test\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id1 = nullptr, *file_id2 = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test1.md", &file_id1);
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_file_create(ctx, notebook_id, ".", "test2.md", &file_id2);
  ASSERT_EQ(err, VXCORE_OK);

  char *buffer_id1 = nullptr, *buffer_id2 = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test1.md", &buffer_id1);
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_buffer_open(ctx, notebook_id, "test2.md", &buffer_id2);
  ASSERT_EQ(err, VXCORE_OK);

  // List buffers
  char *list_json = nullptr;
  err = vxcore_buffer_list(ctx, &list_json);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(list_json);

  nlohmann::json buffer_list = nlohmann::json::parse(list_json);
  ASSERT(buffer_list.is_array());
  // Note: May have buffers from previous tests due to session persistence
  // We just check that our two buffers are present
  bool found_buffer1 = false, found_buffer2 = false;
  for (const auto &buf : buffer_list) {
    std::string id = buf["id"];
    if (id == buffer_id1) found_buffer1 = true;
    if (id == buffer_id2) found_buffer2 = true;
  }
  ASSERT(found_buffer1);
  ASSERT(found_buffer2);

  vxcore_string_free(list_json);
  vxcore_string_free(buffer_id1);
  vxcore_string_free(buffer_id2);
  vxcore_string_free(file_id1);
  vxcore_string_free(file_id2);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_list"));
  std::cout << "  ✓ test_buffer_list passed" << std::endl;
  return 0;
}

int test_buffer_content_raw() {
  std::cout << "  Running test_buffer_content_raw..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_content_raw"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_content_raw").c_str(),
                               "{\"name\":\"Buffer Test\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Set content using raw API
  const char *test_content = "Hello, VxCore!";
  err = vxcore_buffer_set_content_raw(ctx, buffer_id, test_content, strlen(test_content));
  ASSERT_EQ(err, VXCORE_OK);

  // Get content using raw API
  const void *content_ptr = nullptr;
  size_t content_size = 0;
  err = vxcore_buffer_get_content_raw(ctx, buffer_id, &content_ptr, &content_size);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(content_ptr);
  ASSERT_EQ(content_size, strlen(test_content));

  std::string retrieved(static_cast<const char *>(content_ptr), content_size);
  ASSERT_EQ(retrieved, std::string(test_content));

  vxcore_string_free(buffer_id);
  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_content_raw"));
  std::cout << "  ✓ test_buffer_content_raw passed" << std::endl;
  return 0;
}

int test_buffer_content_json() {
  std::cout << "  Running test_buffer_content_json..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_content_json"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_content_json").c_str(),
                               "{\"name\":\"Buffer Test\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Set content using JSON API (base64-encoded)
  const char *test_content = "JSON Content Test";
  std::string base64_content =
      test_base64_encode(reinterpret_cast<const uint8_t *>(test_content), strlen(test_content));

  nlohmann::json content_json;
  content_json["content"] = base64_content;
  std::string content_json_str = content_json.dump();

  err = vxcore_buffer_set_content(ctx, buffer_id, content_json_str.c_str());
  ASSERT_EQ(err, VXCORE_OK);

  // Get content using JSON API
  char *retrieved_json = nullptr;
  err = vxcore_buffer_get_content(ctx, buffer_id, &retrieved_json);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(retrieved_json);

  nlohmann::json retrieved_data = nlohmann::json::parse(retrieved_json);
  ASSERT(retrieved_data.contains("content"));
  ASSERT_EQ(retrieved_data["content"].get<std::string>(), base64_content);

  // Verify decoding back to original content
  std::vector<uint8_t> decoded = test_base64_decode(retrieved_data["content"].get<std::string>());
  std::string decoded_str(decoded.begin(), decoded.end());
  ASSERT_EQ(decoded_str, std::string(test_content));

  vxcore_string_free(retrieved_json);
  vxcore_string_free(buffer_id);
  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_content_json"));
  std::cout << "  ✓ test_buffer_content_json passed" << std::endl;
  return 0;
}

int test_buffer_save_reload() {
  std::cout << "  Running test_buffer_save_reload..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_save_reload"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  std::string notebook_path = get_test_path("test_buffer_save_reload");
  err = vxcore_notebook_create(ctx, notebook_path.c_str(), "{\"name\":\"Buffer Test\"}",
                               VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Set content
  const char *original_content = "Original content";
  err = vxcore_buffer_set_content_raw(ctx, buffer_id, original_content, strlen(original_content));
  ASSERT_EQ(err, VXCORE_OK);

  // Save to disk
  err = vxcore_buffer_save(ctx, buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Verify file on disk
  std::string file_path = notebook_path + "/test.md";
  std::string disk_content = read_file_content(file_path);
  ASSERT_EQ(disk_content, std::string(original_content));

  // Modify file on disk externally
  const char *modified_content = "Modified externally";
  create_test_file(file_path, modified_content);

  // Reload buffer
  err = vxcore_buffer_reload(ctx, buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Verify reloaded content
  const void *content_ptr = nullptr;
  size_t content_size = 0;
  err = vxcore_buffer_get_content_raw(ctx, buffer_id, &content_ptr, &content_size);
  ASSERT_EQ(err, VXCORE_OK);

  std::string reloaded(static_cast<const char *>(content_ptr), content_size);
  ASSERT_EQ(reloaded, std::string(modified_content));

  vxcore_string_free(buffer_id);
  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_save_reload"));
  std::cout << "  ✓ test_buffer_save_reload passed" << std::endl;
  return 0;
}

int test_buffer_deduplication() {
  std::cout << "  Running test_buffer_deduplication..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_dedup"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_dedup").c_str(),
                               "{\"name\":\"Buffer Test\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Open buffer first time
  char *buffer_id1 = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id1);
  ASSERT_EQ(err, VXCORE_OK);

  // Open same buffer again - should return same ID
  char *buffer_id2 = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id2);
  ASSERT_EQ(err, VXCORE_OK);

  ASSERT_EQ(std::string(buffer_id1), std::string(buffer_id2));

  vxcore_string_free(buffer_id1);
  vxcore_string_free(buffer_id2);
  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_dedup"));
  std::cout << "  ✓ test_buffer_deduplication passed" << std::endl;
  return 0;
}

int test_buffer_external_file() {
  std::cout << "  Running test_buffer_external_file..." << std::endl;

  // Create external file outside any notebook
  std::string external_path = get_test_path("external_test_file.txt");
  create_test_file(external_path, "External file content");

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  // Open external file (notebook_id = NULL)
  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, nullptr, external_path.c_str(), &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(buffer_id);

  // Verify content
  const void *content_ptr = nullptr;
  size_t content_size = 0;
  err = vxcore_buffer_get_content_raw(ctx, buffer_id, &content_ptr, &content_size);
  ASSERT_EQ(err, VXCORE_OK);

  std::string content(static_cast<const char *>(content_ptr), content_size);
  ASSERT_EQ(content, "External file content");

  vxcore_string_free(buffer_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(external_path);
  std::cout << "  ✓ test_buffer_external_file passed" << std::endl;
  return 0;
}

int test_buffer_state() {
  std::cout << "  Running test_buffer_state..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_state"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  std::string notebook_path = get_test_path("test_buffer_state");
  err = vxcore_notebook_create(ctx, notebook_path.c_str(), "{\"name\":\"Buffer Test\"}",
                               VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Write initial content to disk
  std::string file_path = notebook_path + "/test.md";
  create_test_file(file_path, "Initial content");

  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Initial state should be NORMAL
  VxCoreBufferState state;
  err = vxcore_buffer_get_state(ctx, buffer_id, &state);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_EQ(state, VXCORE_BUFFER_NORMAL);

  // Modify file externally and wait a bit to ensure timestamp changes
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  create_test_file(file_path, "Externally modified");

  // Check for external changes (state should change to FILE_CHANGED)
  err = vxcore_buffer_reload(ctx, buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  vxcore_string_free(buffer_id);
  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_state"));
  std::cout << "  ✓ test_buffer_state passed" << std::endl;
  return 0;
}

int test_buffer_is_modified() {
  std::cout << "  Running test_buffer_is_modified..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_modified"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  std::string notebook_path = get_test_path("test_buffer_modified");
  err = vxcore_notebook_create(ctx, notebook_path.c_str(), "{\"name\":\"Modified Test\"}",
                               VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Write initial content to disk
  std::string file_path = notebook_path + "/test.md";
  create_test_file(file_path, "Initial content");

  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Load content first (triggers lazy loading)
  const void *content_ptr = nullptr;
  size_t content_size = 0;
  err = vxcore_buffer_get_content_raw(ctx, buffer_id, &content_ptr, &content_size);
  ASSERT_EQ(err, VXCORE_OK);

  // Initially should not be modified
  int is_modified = -1;
  err = vxcore_buffer_is_modified(ctx, buffer_id, &is_modified);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_EQ(is_modified, 0);

  // Modify content
  const char *new_content = "Modified content";
  err = vxcore_buffer_set_content_raw(ctx, buffer_id, new_content, strlen(new_content));
  ASSERT_EQ(err, VXCORE_OK);

  // Should now be modified
  err = vxcore_buffer_is_modified(ctx, buffer_id, &is_modified);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_EQ(is_modified, 1);

  // Save buffer
  err = vxcore_buffer_save(ctx, buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // After save, should not be modified
  err = vxcore_buffer_is_modified(ctx, buffer_id, &is_modified);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_EQ(is_modified, 0);

  vxcore_string_free(buffer_id);
  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_modified"));
  std::cout << "  ✓ test_buffer_is_modified passed" << std::endl;
  return 0;
}

int test_buffer_get_revision() {
  std::cout << "  Running test_buffer_get_revision..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_revision"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  std::string notebook_path = get_test_path("test_buffer_revision");
  err = vxcore_notebook_create(ctx, notebook_path.c_str(), "{\"name\":\"Revision Test\"}",
                               VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Write initial content to disk
  std::string file_path = notebook_path + "/test.md";
  create_test_file(file_path, "Initial content");

  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Get initial revision (before content loaded)
  int revision = -1;
  err = vxcore_buffer_get_revision(ctx, buffer_id, &revision);
  ASSERT_EQ(err, VXCORE_OK);
  int initial_rev = revision;

  // Load content (triggers lazy loading)
  const void *content_ptr = nullptr;
  size_t content_size = 0;
  err = vxcore_buffer_get_content_raw(ctx, buffer_id, &content_ptr, &content_size);
  ASSERT_EQ(err, VXCORE_OK);

  // Set content — revision should increment
  const char *new_content = "Modified content";
  err = vxcore_buffer_set_content_raw(ctx, buffer_id, new_content, strlen(new_content));
  ASSERT_EQ(err, VXCORE_OK);

  int after_set_rev = -1;
  err = vxcore_buffer_get_revision(ctx, buffer_id, &after_set_rev);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_TRUE(after_set_rev > initial_rev);

  // Save — revision should increment again
  err = vxcore_buffer_save(ctx, buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  int after_save_rev = -1;
  err = vxcore_buffer_get_revision(ctx, buffer_id, &after_save_rev);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_TRUE(after_save_rev > after_set_rev);

  // Verify get_revision matches the JSON "revision" field from buffer_get
  char *buffer_json = nullptr;
  err = vxcore_buffer_get(ctx, buffer_id, &buffer_json);
  ASSERT_EQ(err, VXCORE_OK);
  auto json = nlohmann::json::parse(buffer_json);
  ASSERT_EQ(json["revision"].get<int>(), after_save_rev);
  vxcore_string_free(buffer_json);

  // Error cases
  int dummy = -1;
  err = vxcore_buffer_get_revision(ctx, "nonexistent_id", &dummy);
  ASSERT_EQ(err, VXCORE_ERR_BUFFER_NOT_FOUND);

  err = vxcore_buffer_get_revision(nullptr, buffer_id, &dummy);
  ASSERT_EQ(err, VXCORE_ERR_NULL_POINTER);

  err = vxcore_buffer_get_revision(ctx, buffer_id, nullptr);
  ASSERT_EQ(err, VXCORE_ERR_NULL_POINTER);

  vxcore_string_free(buffer_id);
  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_revision"));
  std::cout << "  ✓ test_buffer_get_revision passed" << std::endl;
  return 0;
}

int test_buffer_write_backup() {
  std::cout << "  Running test_buffer_write_backup..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_write_backup"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  std::string notebook_path = get_test_path("test_buffer_write_backup");
  err = vxcore_notebook_create(ctx, notebook_path.c_str(), "{\"name\":\"Backup Test\"}",
                               VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  const char *content = "backup test content";
  err = vxcore_buffer_set_content_raw(ctx, buffer_id, content, strlen(content));
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_buffer_write_backup(ctx, buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  std::string backup_path = notebook_path + "/test.md.vswp";
  ASSERT_TRUE(std::filesystem::exists(backup_path));

  vxcore_string_free(buffer_id);
  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_write_backup"));
  std::cout << "  ✓ test_buffer_write_backup passed" << std::endl;
  return 0;
}

int test_buffer_has_backup() {
  std::cout << "  Running test_buffer_has_backup..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_has_backup"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_has_backup").c_str(),
                               "{\"name\":\"Backup Test\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  const char *content = "has backup content";
  err = vxcore_buffer_set_content_raw(ctx, buffer_id, content, strlen(content));
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_buffer_write_backup(ctx, buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  int has_backup = 0;
  err = vxcore_buffer_has_backup(ctx, buffer_id, &has_backup);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_EQ(has_backup, 1);

  err = vxcore_buffer_discard_backup(ctx, buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  has_backup = 1;
  err = vxcore_buffer_has_backup(ctx, buffer_id, &has_backup);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_EQ(has_backup, 0);

  vxcore_string_free(buffer_id);
  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_has_backup"));
  std::cout << "  ✓ test_buffer_has_backup passed" << std::endl;
  return 0;
}

int test_buffer_recover_backup() {
  std::cout << "  Running test_buffer_recover_backup..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_recover_backup"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  std::string notebook_path = get_test_path("test_buffer_recover_backup");
  err = vxcore_notebook_create(ctx, notebook_path.c_str(), "{\"name\":\"Backup Test\"}",
                               VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  const char *original = "original";
  err = vxcore_buffer_set_content_raw(ctx, buffer_id, original, strlen(original));
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_buffer_save(ctx, buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  const char *backup_content = "modified for backup";
  err = vxcore_buffer_set_content_raw(ctx, buffer_id, backup_content, strlen(backup_content));
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_buffer_write_backup(ctx, buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  const char *other_content = "some other content";
  err = vxcore_buffer_set_content_raw(ctx, buffer_id, other_content, strlen(other_content));
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_buffer_recover_backup(ctx, buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  std::string file_path = notebook_path + "/test.md";
  std::string disk_content = read_file_content(file_path);
  ASSERT_EQ(disk_content, std::string(backup_content));

  std::string backup_path = file_path + ".vswp";
  ASSERT_FALSE(std::filesystem::exists(backup_path));

  vxcore_string_free(buffer_id);
  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_recover_backup"));
  std::cout << "  ✓ test_buffer_recover_backup passed" << std::endl;
  return 0;
}

int test_buffer_discard_backup() {
  std::cout << "  Running test_buffer_discard_backup..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_discard_backup"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_discard_backup").c_str(),
                               "{\"name\":\"Backup Test\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  const char *content = "discard backup content";
  err = vxcore_buffer_set_content_raw(ctx, buffer_id, content, strlen(content));
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_buffer_write_backup(ctx, buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  int has_backup = 0;
  err = vxcore_buffer_has_backup(ctx, buffer_id, &has_backup);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_EQ(has_backup, 1);

  err = vxcore_buffer_discard_backup(ctx, buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  has_backup = 1;
  err = vxcore_buffer_has_backup(ctx, buffer_id, &has_backup);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_EQ(has_backup, 0);

  vxcore_string_free(buffer_id);
  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_discard_backup"));
  std::cout << "  ✓ test_buffer_discard_backup passed" << std::endl;
  return 0;
}

int test_buffer_get_backup_path() {
  std::cout << "  Running test_buffer_get_backup_path..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_get_backup_path"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  std::string notebook_path = get_test_path("test_buffer_get_backup_path");
  err = vxcore_notebook_create(ctx, notebook_path.c_str(), "{\"name\":\"Backup Test\"}",
                               VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *backup_path = nullptr;
  err = vxcore_buffer_get_backup_path(ctx, buffer_id, &backup_path);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(backup_path);

  std::string backup_path_str = normalize_path(backup_path);
  std::string notebook_path_str = normalize_path(notebook_path);
  ASSERT_TRUE(backup_path_str.size() >= 5 &&
              backup_path_str.substr(backup_path_str.size() - 5) == ".vswp");
  ASSERT_TRUE(backup_path_str.rfind(notebook_path_str, 0) == 0);

  vxcore_string_free(backup_path);
  vxcore_string_free(buffer_id);
  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_get_backup_path"));
  std::cout << "  ✓ test_buffer_get_backup_path passed" << std::endl;
  return 0;
}

int test_buffer_backup_no_content() {
  std::cout << "  Running test_buffer_backup_no_content..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_backup_no_content"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_backup_no_content").c_str(),
                               "{\"name\":\"Backup Test\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_buffer_write_backup(ctx, buffer_id);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_STATE);

  vxcore_string_free(buffer_id);
  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_backup_no_content"));
  std::cout << "  ✓ test_buffer_backup_no_content passed" << std::endl;
  return 0;
}

int test_buffer_recover_no_backup() {
  std::cout << "  Running test_buffer_recover_no_backup..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_recover_no_backup"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_recover_no_backup").c_str(),
                               "{\"name\":\"Backup Test\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_buffer_recover_backup(ctx, buffer_id);
  ASSERT_EQ(err, VXCORE_ERR_NOT_FOUND);

  vxcore_string_free(buffer_id);
  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_recover_no_backup"));
  std::cout << "  ✓ test_buffer_recover_no_backup passed" << std::endl;
  return 0;
}

int test_buffer_discard_no_backup() {
  std::cout << "  Running test_buffer_discard_no_backup..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_discard_no_backup"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_discard_no_backup").c_str(),
                               "{\"name\":\"Backup Test\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_buffer_discard_backup(ctx, buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  vxcore_string_free(buffer_id);
  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_discard_no_backup"));
  std::cout << "  ✓ test_buffer_discard_no_backup passed" << std::endl;
  return 0;
}

int test_buffer_backup_format() {
  std::cout << "  Running test_buffer_backup_format..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_backup_format"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_backup_format").c_str(),
                               "{\"name\":\"Backup Test\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  const char *content = "format test content";
  err = vxcore_buffer_set_content_raw(ctx, buffer_id, content, strlen(content));
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_buffer_write_backup(ctx, buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *backup_path = nullptr;
  err = vxcore_buffer_get_backup_path(ctx, buffer_id, &backup_path);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(backup_path);

  std::string backup_raw = read_file_content(backup_path);
  ASSERT_TRUE(backup_raw.rfind("vnotex_backup_file ", 0) == 0);
  size_t sep_pos = backup_raw.find('|');
  ASSERT_NE(sep_pos, std::string::npos);
  ASSERT_EQ(backup_raw.substr(sep_pos + 1), std::string(content));

  vxcore_string_free(backup_path);
  vxcore_string_free(buffer_id);
  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_backup_format"));
  std::cout << "  ✓ test_buffer_backup_format passed" << std::endl;
  return 0;
}

int test_buffer_backup_overwrite() {
  std::cout << "  Running test_buffer_backup_overwrite..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_backup_overwrite"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  std::string notebook_path = get_test_path("test_buffer_backup_overwrite");
  err = vxcore_notebook_create(ctx, notebook_path.c_str(), "{\"name\":\"Backup Test\"}",
                               VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  const char *first_content = "first backup";
  err = vxcore_buffer_set_content_raw(ctx, buffer_id, first_content, strlen(first_content));
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_buffer_write_backup(ctx, buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  const char *second_content = "second backup";
  err = vxcore_buffer_set_content_raw(ctx, buffer_id, second_content, strlen(second_content));
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_buffer_write_backup(ctx, buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *backup_path = nullptr;
  err = vxcore_buffer_get_backup_path(ctx, buffer_id, &backup_path);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(backup_path);

  std::string backup_raw = read_file_content(backup_path);
  size_t sep_pos = backup_raw.find('|');
  ASSERT_NE(sep_pos, std::string::npos);
  ASSERT_EQ(backup_raw.substr(sep_pos + 1), std::string(second_content));

  size_t vswp_count = 0;
  for (const auto &entry : std::filesystem::directory_iterator(notebook_path)) {
    if (entry.is_regular_file() && entry.path().extension() == ".vswp") {
      ++vswp_count;
    }
  }
  ASSERT_EQ(vswp_count, 1u);

  vxcore_string_free(backup_path);
  vxcore_string_free(buffer_id);
  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_backup_overwrite"));
  std::cout << "  ✓ test_buffer_backup_overwrite passed" << std::endl;
  return 0;
}

int test_buffer_notebook_close() {
  std::cout << "  Running test_buffer_notebook_close..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_nb_close"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_nb_close").c_str(),
                               "{\"name\":\"Buffer Test\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Close notebook - should close all buffers
  err = vxcore_notebook_close(ctx, notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Try to get closed buffer - should fail
  char *buffer_json = nullptr;
  err = vxcore_buffer_get(ctx, buffer_id, &buffer_json);
  ASSERT_EQ(err, VXCORE_ERR_BUFFER_NOT_FOUND);

  vxcore_string_free(buffer_id);
  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_nb_close"));
  std::cout << "  ✓ test_buffer_notebook_close passed" << std::endl;
  return 0;
}

int test_buffer_persistence() {
  std::cout << "  Running test_buffer_persistence..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_persist"));

  std::string notebook_id_str;
  std::string buffer_id_str;

  // Create notebook, file, and buffer
  {
    VxCoreContextHandle ctx = nullptr;
    VxCoreError err = vxcore_context_create(nullptr, &ctx);
    ASSERT_EQ(err, VXCORE_OK);

    char *notebook_id = nullptr;
    err =
        vxcore_notebook_create(ctx, get_test_path("test_buffer_persist").c_str(),
                               "{\"name\":\"Buffer Test\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
    ASSERT_EQ(err, VXCORE_OK);
    notebook_id_str = std::string(notebook_id);

    char *file_id = nullptr;
    err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
    ASSERT_EQ(err, VXCORE_OK);

    char *buffer_id = nullptr;
    err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
    ASSERT_EQ(err, VXCORE_OK);
    buffer_id_str = std::string(buffer_id);

    vxcore_string_free(buffer_id);
    vxcore_string_free(file_id);
    vxcore_string_free(notebook_id);
    vxcore_context_destroy(ctx);  // Should save session config
  }

  // Reload and verify buffer persisted with same ID
  {
    VxCoreContextHandle ctx = nullptr;
    VxCoreError err = vxcore_context_create(nullptr, &ctx);
    ASSERT_EQ(err, VXCORE_OK);

    // Notebook is auto-loaded from session config during context creation.
    // Buffer ID should be preserved across sessions.
    char *buffer_json = nullptr;
    err = vxcore_buffer_get(ctx, buffer_id_str.c_str(), &buffer_json);
    ASSERT_EQ(err, VXCORE_OK);

    nlohmann::json buffer_data = nlohmann::json::parse(buffer_json);
    ASSERT(buffer_data.contains("id"));
    ASSERT_EQ(buffer_data["id"].get<std::string>(), buffer_id_str);
    ASSERT(buffer_data.contains("filePath"));
    ASSERT_EQ(buffer_data["filePath"].get<std::string>(), "test.md");
    ASSERT(buffer_data.contains("notebookId"));
    ASSERT_EQ(buffer_data["notebookId"].get<std::string>(), notebook_id_str);

    vxcore_string_free(buffer_json);
    vxcore_context_destroy(ctx);
  }

  cleanup_test_dir(get_test_path("test_buffer_persist"));
  std::cout << "  ✓ test_buffer_persistence passed" << std::endl;
  return 0;
}

int test_buffer_lazy_loading() {
  std::cout << "  Running test_buffer_lazy_loading..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_lazy"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  std::string notebook_path = get_test_path("test_buffer_lazy");
  err = vxcore_notebook_create(ctx, notebook_path.c_str(), "{\"name\":\"Lazy Test\"}",
                               VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "lazy.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Write content to the file on disk
  std::string file_path = notebook_path + "/lazy.md";
  const char *initial_content = "Lazy loaded content";
  create_test_file(file_path, initial_content);

  // Open buffer - content should NOT be loaded yet
  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "lazy.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Check buffer info - contentLoaded should be false
  char *buffer_json = nullptr;
  err = vxcore_buffer_get(ctx, buffer_id, &buffer_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json buffer_data = nlohmann::json::parse(buffer_json);
  ASSERT_EQ(buffer_data["contentLoaded"].get<bool>(), false);
  vxcore_string_free(buffer_json);

  // Get content - should trigger lazy loading
  const void *content_ptr = nullptr;
  size_t content_size = 0;
  err = vxcore_buffer_get_content_raw(ctx, buffer_id, &content_ptr, &content_size);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(content_ptr);
  ASSERT_EQ(content_size, strlen(initial_content));

  std::string loaded_content(static_cast<const char *>(content_ptr), content_size);
  ASSERT_EQ(loaded_content, std::string(initial_content));

  // Check buffer info again - contentLoaded should now be true
  err = vxcore_buffer_get(ctx, buffer_id, &buffer_json);
  ASSERT_EQ(err, VXCORE_OK);
  buffer_data = nlohmann::json::parse(buffer_json);
  ASSERT_EQ(buffer_data["contentLoaded"].get<bool>(), true);
  vxcore_string_free(buffer_json);

  vxcore_string_free(buffer_id);
  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_lazy"));
  std::cout << "  ✓ test_buffer_lazy_loading passed" << std::endl;
  return 0;
}

// ============ Buffer Asset Tests (Filesystem Only) ============

int test_buffer_insert_asset_raw() {
  std::cout << "  Running test_buffer_insert_asset_raw..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_insert_asset_raw"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_insert_asset_raw").c_str(),
                               "{\"name\":\"Asset Test\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a test file
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Open buffer
  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Insert asset with raw binary data (does NOT add to attachment list)
  const uint8_t image_data[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  char *relative_path = nullptr;
  err = vxcore_buffer_insert_asset_raw(ctx, buffer_id, "screenshot.png", image_data,
                                       sizeof(image_data), &relative_path);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(relative_path);

  // Verify relative path format (should contain vx_assets and file uuid)
  std::string rel_path(relative_path);
  ASSERT_TRUE(rel_path.find("vx_assets") != std::string::npos);
  ASSERT_TRUE(rel_path.find("screenshot.png") != std::string::npos);

  // Verify file was created on disk
  std::string notebook_root = get_test_path("test_buffer_insert_asset_raw");
  std::string asset_abs_path = notebook_root + "/" + rel_path;
  ASSERT_TRUE(path_exists(asset_abs_path));

  // Verify NOT added to attachment list (insert_asset_raw doesn't touch metadata)
  char *attachments_json = nullptr;
  err = vxcore_buffer_list_attachments(ctx, buffer_id, &attachments_json);
  ASSERT_EQ(err, VXCORE_OK);
  auto json = nlohmann::json::parse(attachments_json);
  ASSERT_EQ(json.size(), 0u);  // Should be empty
  vxcore_string_free(attachments_json);

  vxcore_string_free(relative_path);
  vxcore_string_free(file_id);
  vxcore_string_free(buffer_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_insert_asset_raw"));
  std::cout << "  ✓ test_buffer_insert_asset_raw passed" << std::endl;
  return 0;
}

int test_buffer_insert_asset() {
  std::cout << "  Running test_buffer_insert_asset..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_insert_asset"));
  create_directory(get_test_path("test_buffer_insert_asset"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_insert_asset").c_str(),
                               "{\"name\":\"Asset Test\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a test file
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Open buffer
  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a source file to copy
  std::string source_path = get_test_path("test_buffer_insert_asset") + "/source_image.png";
  const uint8_t image_data[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  std::ofstream ofs(source_path, std::ios::binary);
  ofs.write(reinterpret_cast<const char *>(image_data), sizeof(image_data));
  ofs.close();

  // Insert asset by copying file (does NOT add to attachment list)
  char *relative_path = nullptr;
  err = vxcore_buffer_insert_asset(ctx, buffer_id, source_path.c_str(), &relative_path);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(relative_path);

  // Verify relative path format
  std::string rel_path(relative_path);
  ASSERT_TRUE(rel_path.find("vx_assets") != std::string::npos);
  ASSERT_TRUE(rel_path.find("source_image.png") != std::string::npos);

  // Verify file was created on disk
  std::string notebook_root = get_test_path("test_buffer_insert_asset");
  std::string asset_abs_path = notebook_root + "/" + rel_path;
  ASSERT_TRUE(path_exists(asset_abs_path));

  vxcore_string_free(relative_path);
  vxcore_string_free(file_id);
  vxcore_string_free(buffer_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_insert_asset"));
  std::cout << "  ✓ test_buffer_insert_asset passed" << std::endl;
  return 0;
}

int test_buffer_delete_asset() {
  std::cout << "  Running test_buffer_delete_asset..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_delete_asset"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_delete_asset").c_str(),
                               "{\"name\":\"Asset Test\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a test file
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Open buffer
  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Insert asset (raw - doesn't touch metadata)
  const uint8_t data[] = {0x89, 'P', 'N', 'G'};
  char *relative_path = nullptr;
  err = vxcore_buffer_insert_asset_raw(ctx, buffer_id, "to_delete.png", data, sizeof(data),
                                       &relative_path);
  ASSERT_EQ(err, VXCORE_OK);

  // Verify file exists
  std::string notebook_root = get_test_path("test_buffer_delete_asset");
  std::string asset_abs_path = notebook_root + "/" + relative_path;
  ASSERT_TRUE(path_exists(asset_abs_path));

  // Delete asset (doesn't touch metadata)
  err = vxcore_buffer_delete_asset(ctx, buffer_id, relative_path);
  ASSERT_EQ(err, VXCORE_OK);

  // Verify file is gone
  ASSERT_FALSE(path_exists(asset_abs_path));

  vxcore_string_free(relative_path);
  vxcore_string_free(file_id);
  vxcore_string_free(buffer_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_delete_asset"));
  std::cout << "  ✓ test_buffer_delete_asset passed" << std::endl;
  return 0;
}

int test_buffer_get_assets_folder() {
  std::cout << "  Running test_buffer_get_assets_folder..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_assets_folder"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_assets_folder").c_str(),
                               "{\"name\":\"Asset Test\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a test file
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Open buffer
  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Get assets folder (should create it lazily)
  char *assets_folder = nullptr;
  err = vxcore_buffer_get_assets_folder(ctx, buffer_id, &assets_folder);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(assets_folder);

  // Verify folder exists
  ASSERT_TRUE(path_exists(assets_folder));
  ASSERT_TRUE(std::string(assets_folder).find("vx_assets") != std::string::npos);

  vxcore_string_free(assets_folder);
  vxcore_string_free(file_id);
  vxcore_string_free(buffer_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_assets_folder"));
  std::cout << "  ✓ test_buffer_get_assets_folder passed" << std::endl;
  return 0;
}

// ============ Buffer Attachment Tests (Filesystem + Metadata) ============

int test_buffer_insert_attachment() {
  std::cout << "  Running test_buffer_insert_attachment..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_insert_attachment"));
  create_directory(get_test_path("test_buffer_insert_attachment"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_insert_attachment").c_str(),
                               "{\"name\":\"Attachment Test\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a test file
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Open buffer
  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create source file
  std::string source_path = get_test_path("test_buffer_insert_attachment") + "/document.pdf";
  write_file(source_path, "PDF content");

  // Insert attachment (copies file + adds to metadata)
  char *filename = nullptr;
  err = vxcore_buffer_insert_attachment(ctx, buffer_id, source_path.c_str(), &filename);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(filename);
  ASSERT_EQ(std::string(filename), "document.pdf");

  // Verify attachment is in the list
  char *attachments_json = nullptr;
  err = vxcore_buffer_list_attachments(ctx, buffer_id, &attachments_json);
  ASSERT_EQ(err, VXCORE_OK);
  auto json = nlohmann::json::parse(attachments_json);
  ASSERT_EQ(json.size(), 1u);
  ASSERT_EQ(json[0].get<std::string>(), "document.pdf");
  vxcore_string_free(attachments_json);

  vxcore_string_free(filename);
  vxcore_string_free(file_id);
  vxcore_string_free(buffer_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_insert_attachment"));
  std::cout << "  ✓ test_buffer_insert_attachment passed" << std::endl;
  return 0;
}

int test_buffer_delete_attachment() {
  std::cout << "  Running test_buffer_delete_attachment..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_delete_attachment"));
  create_directory(get_test_path("test_buffer_delete_attachment"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_delete_attachment").c_str(),
                               "{\"name\":\"Attachment Test\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a test file
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Open buffer
  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create and insert attachment
  std::string source_path = get_test_path("test_buffer_delete_attachment") + "/to_delete.zip";
  write_file(source_path, "ZIP content");

  char *filename = nullptr;
  err = vxcore_buffer_insert_attachment(ctx, buffer_id, source_path.c_str(), &filename);
  ASSERT_EQ(err, VXCORE_OK);

  // Verify in list
  char *attachments_json = nullptr;
  err = vxcore_buffer_list_attachments(ctx, buffer_id, &attachments_json);
  ASSERT_EQ(err, VXCORE_OK);
  auto json = nlohmann::json::parse(attachments_json);
  ASSERT_EQ(json.size(), 1u);
  vxcore_string_free(attachments_json);

  // Delete attachment
  err = vxcore_buffer_delete_attachment(ctx, buffer_id, filename);
  ASSERT_EQ(err, VXCORE_OK);

  // Verify removed from list
  err = vxcore_buffer_list_attachments(ctx, buffer_id, &attachments_json);
  ASSERT_EQ(err, VXCORE_OK);
  json = nlohmann::json::parse(attachments_json);
  ASSERT_EQ(json.size(), 0u);
  vxcore_string_free(attachments_json);

  vxcore_string_free(filename);
  vxcore_string_free(file_id);
  vxcore_string_free(buffer_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_delete_attachment"));
  std::cout << "  ✓ test_buffer_delete_attachment passed" << std::endl;
  return 0;
}

int test_buffer_rename_attachment() {
  std::cout << "  Running test_buffer_rename_attachment..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_rename_attachment"));
  create_directory(get_test_path("test_buffer_rename_attachment"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_rename_attachment").c_str(),
                               "{\"name\":\"Attachment Test\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a test file
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Open buffer
  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create and insert attachment
  std::string source_path = get_test_path("test_buffer_rename_attachment") + "/old_name.pdf";
  write_file(source_path, "PDF content");

  char *filename = nullptr;
  err = vxcore_buffer_insert_attachment(ctx, buffer_id, source_path.c_str(), &filename);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_EQ(std::string(filename), "old_name.pdf");

  // Rename attachment
  char *new_filename = nullptr;
  err = vxcore_buffer_rename_attachment(ctx, buffer_id, filename, "new_name.pdf", &new_filename);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(new_filename);
  ASSERT_EQ(std::string(new_filename), "new_name.pdf");

  // Verify updated in list
  char *attachments_json = nullptr;
  err = vxcore_buffer_list_attachments(ctx, buffer_id, &attachments_json);
  ASSERT_EQ(err, VXCORE_OK);
  auto json = nlohmann::json::parse(attachments_json);
  ASSERT_EQ(json.size(), 1u);
  ASSERT_EQ(json[0].get<std::string>(), "new_name.pdf");
  vxcore_string_free(attachments_json);

  vxcore_string_free(new_filename);
  vxcore_string_free(filename);
  vxcore_string_free(file_id);
  vxcore_string_free(buffer_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_rename_attachment"));
  std::cout << "  ✓ test_buffer_rename_attachment passed" << std::endl;
  return 0;
}

int test_buffer_list_attachments() {
  std::cout << "  Running test_buffer_list_attachments..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_list_attachments"));
  create_directory(get_test_path("test_buffer_list_attachments"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_list_attachments").c_str(),
                               "{\"name\":\"Attachment Test\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a test file
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Open buffer
  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create and insert multiple attachments
  std::string source1 = get_test_path("test_buffer_list_attachments") + "/doc1.pdf";
  std::string source2 = get_test_path("test_buffer_list_attachments") + "/doc2.zip";
  write_file(source1, "PDF content");
  write_file(source2, "ZIP content");

  char *filename1 = nullptr;
  char *filename2 = nullptr;
  err = vxcore_buffer_insert_attachment(ctx, buffer_id, source1.c_str(), &filename1);
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_buffer_insert_attachment(ctx, buffer_id, source2.c_str(), &filename2);
  ASSERT_EQ(err, VXCORE_OK);

  // List attachments
  char *attachments_json = nullptr;
  err = vxcore_buffer_list_attachments(ctx, buffer_id, &attachments_json);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(attachments_json);

  auto json = nlohmann::json::parse(attachments_json);
  ASSERT_TRUE(json.is_array());
  ASSERT_EQ(json.size(), 2u);

  vxcore_string_free(attachments_json);
  vxcore_string_free(filename1);
  vxcore_string_free(filename2);
  vxcore_string_free(file_id);
  vxcore_string_free(buffer_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_list_attachments"));
  std::cout << "  ✓ test_buffer_list_attachments passed" << std::endl;
  return 0;
}

int test_buffer_get_attachments_folder() {
  std::cout << "  Running test_buffer_get_attachments_folder..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_attachments_folder"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_attachments_folder").c_str(),
                               "{\"name\":\"Attachment Test\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a test file
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Open buffer
  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Get attachments folder (should create it lazily)
  char *attachments_folder = nullptr;
  err = vxcore_buffer_get_attachments_folder(ctx, buffer_id, &attachments_folder);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(attachments_folder);

  // Verify folder exists and is same as assets folder
  ASSERT_TRUE(path_exists(attachments_folder));
  ASSERT_TRUE(std::string(attachments_folder).find("vx_assets") != std::string::npos);

  vxcore_string_free(attachments_folder);
  vxcore_string_free(file_id);
  vxcore_string_free(buffer_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_attachments_folder"));
  std::cout << "  ✓ test_buffer_get_attachments_folder passed" << std::endl;
  return 0;
}

int test_buffer_external_asset() {
  std::cout << "  Running test_buffer_external_asset..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_external_asset"));
  create_directory(get_test_path("test_buffer_external_asset"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  // Create an external file
  std::string ext_file_path = get_test_path("test_buffer_external_asset") + "/notes.md";
  write_file(ext_file_path, "# External Notes\n");

  // Open buffer for external file (notebook_id = NULL)
  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, nullptr, ext_file_path.c_str(), &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Insert asset (raw binary data)
  const uint8_t data[] = {0x89, 'P', 'N', 'G'};
  char *relative_path = nullptr;
  err = vxcore_buffer_insert_asset_raw(ctx, buffer_id, "image.png", data, sizeof(data),
                                       &relative_path);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(relative_path);

  // Verify relative path format (should be notes_assets/image.png)
  std::string rel_path(relative_path);
  ASSERT_TRUE(rel_path.find("notes_assets") != std::string::npos);
  ASSERT_TRUE(rel_path.find("image.png") != std::string::npos);

  // Verify file was created
  std::string asset_abs_path = get_test_path("test_buffer_external_asset") + "/" + rel_path;
  ASSERT_TRUE(path_exists(asset_abs_path));

  vxcore_string_free(relative_path);
  vxcore_string_free(buffer_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_external_asset"));
  std::cout << "  ✓ test_buffer_external_asset passed" << std::endl;
  return 0;
}

int test_buffer_asset_unique_name() {
  std::cout << "  Running test_buffer_asset_unique_name..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_asset_unique"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_asset_unique").c_str(),
                               "{\"name\":\"Asset Test\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a test file
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Open buffer
  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Insert same-named assets multiple times (using raw)
  const uint8_t data[] = {0x89, 'P', 'N', 'G'};
  char *path1 = nullptr;
  char *path2 = nullptr;
  char *path3 = nullptr;

  err = vxcore_buffer_insert_asset_raw(ctx, buffer_id, "image.png", data, sizeof(data), &path1);
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_buffer_insert_asset_raw(ctx, buffer_id, "image.png", data, sizeof(data), &path2);
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_buffer_insert_asset_raw(ctx, buffer_id, "image.png", data, sizeof(data), &path3);
  ASSERT_EQ(err, VXCORE_OK);

  // All paths should be different
  ASSERT_NE(std::string(path1), std::string(path2));
  ASSERT_NE(std::string(path2), std::string(path3));
  ASSERT_NE(std::string(path1), std::string(path3));

  // Should have image.png, image_1.png, image_2.png
  ASSERT_TRUE(std::string(path1).find("image.png") != std::string::npos);
  ASSERT_TRUE(std::string(path2).find("image_1.png") != std::string::npos);
  ASSERT_TRUE(std::string(path3).find("image_2.png") != std::string::npos);

  vxcore_string_free(path1);
  vxcore_string_free(path2);
  vxcore_string_free(path3);
  vxcore_string_free(file_id);
  vxcore_string_free(buffer_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_asset_unique"));
  std::cout << "  ✓ test_buffer_asset_unique_name passed" << std::endl;
  return 0;
}

int test_buffer_backup_cleanup_on_save() {
  std::cout << "  Running test_buffer_backup_cleanup_on_save..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_backup_cleanup_on_save"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  std::string notebook_path = get_test_path("test_buffer_backup_cleanup_on_save");
  err = vxcore_notebook_create(ctx, notebook_path.c_str(), "{\"name\":\"Backup Cleanup Test\"}",
                               VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  const char *content = "cleanup save test";
  err = vxcore_buffer_set_content_raw(ctx, buffer_id, content, strlen(content));
  ASSERT_EQ(err, VXCORE_OK);

  // Write backup and verify it exists
  err = vxcore_buffer_write_backup(ctx, buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  std::string backup_path = notebook_path + "/test.md.vswp";
  ASSERT_TRUE(std::filesystem::exists(backup_path));

  // Save buffer — should auto-delete backup
  err = vxcore_buffer_save(ctx, buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Verify backup was auto-deleted
  ASSERT_FALSE(std::filesystem::exists(backup_path));

  // Verify original file has the saved content
  std::string file_path = notebook_path + "/test.md";
  std::string disk_content = read_file_content(file_path);
  ASSERT_EQ(disk_content, std::string(content));

  vxcore_string_free(buffer_id);
  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_backup_cleanup_on_save"));
  std::cout << "  ✓ test_buffer_backup_cleanup_on_save passed" << std::endl;
  return 0;
}

int test_buffer_backup_cleanup_on_close() {
  std::cout << "  Running test_buffer_backup_cleanup_on_close..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_backup_cleanup_on_close"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  std::string notebook_path = get_test_path("test_buffer_backup_cleanup_on_close");
  err = vxcore_notebook_create(ctx, notebook_path.c_str(), "{\"name\":\"Backup Cleanup Test\"}",
                               VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  const char *content = "cleanup close test";
  err = vxcore_buffer_set_content_raw(ctx, buffer_id, content, strlen(content));
  ASSERT_EQ(err, VXCORE_OK);

  // Write backup and verify it exists
  err = vxcore_buffer_write_backup(ctx, buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  std::string backup_path = notebook_path + "/test.md.vswp";
  ASSERT_TRUE(std::filesystem::exists(backup_path));

  // Close buffer — should auto-delete backup
  err = vxcore_buffer_close(ctx, buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Verify backup was auto-deleted
  ASSERT_FALSE(std::filesystem::exists(backup_path));

  vxcore_string_free(buffer_id);
  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_backup_cleanup_on_close"));
  std::cout << "  ✓ test_buffer_backup_cleanup_on_close passed" << std::endl;
  return 0;
}

int test_buffer_rename_file_updates_path() {
  std::cout << "  Running test_buffer_rename_file_updates_path..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_rename_file_updates_path"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_rename_file_updates_path").c_str(),
                               "{\"name\":\"Rename Test\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a file and open its buffer
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(buffer_id);

  // Verify initial filePath is "test.md"
  char *buffer_json = nullptr;
  err = vxcore_buffer_get(ctx, buffer_id, &buffer_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json buf_data = nlohmann::json::parse(buffer_json);
  ASSERT_EQ(buf_data["filePath"].get<std::string>(), std::string("test.md"));
  vxcore_string_free(buffer_json);

  // Save the buffer ID for later comparison
  std::string original_buffer_id(buffer_id);

  // Rename the file
  err = vxcore_node_rename(ctx, notebook_id, "test.md", "renamed.md");
  ASSERT_EQ(err, VXCORE_OK);

  // Verify buffer's filePath is now "renamed.md"
  buffer_json = nullptr;
  err = vxcore_buffer_get(ctx, buffer_id, &buffer_json);
  ASSERT_EQ(err, VXCORE_OK);
  buf_data = nlohmann::json::parse(buffer_json);
  ASSERT_EQ(buf_data["filePath"].get<std::string>(), std::string("renamed.md"));
  vxcore_string_free(buffer_json);

  // Verify buffer ID is unchanged
  ASSERT_EQ(buf_data["id"].get<std::string>(), original_buffer_id);

  vxcore_string_free(buffer_id);
  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_rename_file_updates_path"));
  std::cout << "  ✓ test_buffer_rename_file_updates_path passed" << std::endl;
  return 0;
}

int test_buffer_rename_folder_updates_paths() {
  std::cout << "  Running test_buffer_rename_folder_updates_paths..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_rename_folder_updates_paths"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(
      ctx, get_test_path("test_buffer_rename_folder_updates_paths").c_str(),
      "{\"name\":\"Rename Folder Test\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create folder "docs" and two files inside it
  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "docs", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id1 = nullptr;
  err = vxcore_file_create(ctx, notebook_id, "docs", "note.md", &file_id1);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id2 = nullptr;
  err = vxcore_file_create(ctx, notebook_id, "docs", "readme.md", &file_id2);
  ASSERT_EQ(err, VXCORE_OK);

  // Open buffers for both files
  char *buffer_id1 = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "docs/note.md", &buffer_id1);
  ASSERT_EQ(err, VXCORE_OK);

  char *buffer_id2 = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "docs/readme.md", &buffer_id2);
  ASSERT_EQ(err, VXCORE_OK);

  // Save buffer IDs for later comparison
  std::string original_buffer_id1(buffer_id1);
  std::string original_buffer_id2(buffer_id2);

  // Rename folder "docs" to "documentation"
  err = vxcore_node_rename(ctx, notebook_id, "docs", "documentation");
  ASSERT_EQ(err, VXCORE_OK);

  // Verify first buffer's filePath is "documentation/note.md"
  char *buffer_json = nullptr;
  err = vxcore_buffer_get(ctx, buffer_id1, &buffer_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json buf_data1 = nlohmann::json::parse(buffer_json);
  ASSERT_EQ(buf_data1["filePath"].get<std::string>(), std::string("documentation/note.md"));
  vxcore_string_free(buffer_json);

  // Verify second buffer's filePath is "documentation/readme.md"
  buffer_json = nullptr;
  err = vxcore_buffer_get(ctx, buffer_id2, &buffer_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json buf_data2 = nlohmann::json::parse(buffer_json);
  ASSERT_EQ(buf_data2["filePath"].get<std::string>(), std::string("documentation/readme.md"));
  vxcore_string_free(buffer_json);

  // Verify buffer IDs are unchanged
  ASSERT_EQ(buf_data1["id"].get<std::string>(), original_buffer_id1);
  ASSERT_EQ(buf_data2["id"].get<std::string>(), original_buffer_id2);

  // Verify provider is functional after rename: insert asset into buffer1
  // This proves the StandardBufferProvider's cached file_path_ was updated
  const uint8_t asset_data[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  char *relative_path = nullptr;
  err = vxcore_buffer_insert_asset_raw(ctx, buffer_id1, "post_rename.png", asset_data,
                                       sizeof(asset_data), &relative_path);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(relative_path);

  // Verify asset was created under the renamed folder path
  std::string rel_path(relative_path);
  ASSERT_TRUE(rel_path.find("vx_assets") != std::string::npos);
  ASSERT_TRUE(rel_path.find("post_rename.png") != std::string::npos);

  // Verify the file actually exists on disk
  std::string notebook_root = get_test_path("test_buffer_rename_folder_updates_paths");
  std::string asset_abs_path = notebook_root + "/" + rel_path;
  ASSERT_TRUE(path_exists(asset_abs_path));

  vxcore_string_free(relative_path);
  vxcore_string_free(buffer_id1);
  vxcore_string_free(buffer_id2);
  vxcore_string_free(file_id1);
  vxcore_string_free(file_id2);
  vxcore_string_free(folder_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_rename_folder_updates_paths"));
  std::cout << "  ✓ test_buffer_rename_folder_updates_paths passed" << std::endl;
  return 0;
}

int test_buffer_rename_no_affect_other_buffers() {
  std::cout << "  Running test_buffer_rename_no_affect_other_buffers..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_rename_no_affect_other"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_rename_no_affect_other").c_str(),
                               "{\"name\":\"Rename Isolation Test\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create two files
  char *file_id_a = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "a.md", &file_id_a);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id_b = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "b.md", &file_id_b);
  ASSERT_EQ(err, VXCORE_OK);

  // Open buffers for both
  char *buffer_id_a = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "a.md", &buffer_id_a);
  ASSERT_EQ(err, VXCORE_OK);

  char *buffer_id_b = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "b.md", &buffer_id_b);
  ASSERT_EQ(err, VXCORE_OK);

  // Rename "a.md" to "c.md"
  err = vxcore_node_rename(ctx, notebook_id, "a.md", "c.md");
  ASSERT_EQ(err, VXCORE_OK);

  // Verify buffer A now has filePath "c.md"
  char *buffer_json = nullptr;
  err = vxcore_buffer_get(ctx, buffer_id_a, &buffer_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json buf_data_a = nlohmann::json::parse(buffer_json);
  ASSERT_EQ(buf_data_a["filePath"].get<std::string>(), std::string("c.md"));
  vxcore_string_free(buffer_json);

  // Verify buffer B still has filePath "b.md" (unaffected)
  buffer_json = nullptr;
  err = vxcore_buffer_get(ctx, buffer_id_b, &buffer_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json buf_data_b = nlohmann::json::parse(buffer_json);
  ASSERT_EQ(buf_data_b["filePath"].get<std::string>(), std::string("b.md"));
  vxcore_string_free(buffer_json);

  vxcore_string_free(buffer_id_a);
  vxcore_string_free(buffer_id_b);
  vxcore_string_free(file_id_a);
  vxcore_string_free(file_id_b);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_rename_no_affect_other"));
  std::cout << "  ✓ test_buffer_rename_no_affect_other_buffers passed" << std::endl;
  return 0;
}

int test_buffer_rename_provider_functional() {
  std::cout << "  Running test_buffer_rename_provider_functional..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_rename_provider"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_rename_provider").c_str(),
                               "{\"name\":\"Provider Rename Test\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create folder "notes" and a file inside it
  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "notes", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, "notes", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Open buffer for "notes/test.md"
  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "notes/test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(buffer_id);

  // Verify initial filePath
  char *buffer_json = nullptr;
  err = vxcore_buffer_get(ctx, buffer_id, &buffer_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json buf_data = nlohmann::json::parse(buffer_json);
  ASSERT_EQ(buf_data["filePath"].get<std::string>(), std::string("notes/test.md"));
  vxcore_string_free(buffer_json);

  // Step 3: Insert asset BEFORE rename — verify it works and path relates to "notes/test.md"
  const uint8_t image_data1[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  char *pre_rename_asset_path = nullptr;
  err = vxcore_buffer_insert_asset_raw(ctx, buffer_id, "before_rename.png", image_data1,
                                       sizeof(image_data1), &pre_rename_asset_path);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(pre_rename_asset_path);

  // Verify pre-rename asset path format
  std::string pre_rel_path(pre_rename_asset_path);
  ASSERT_TRUE(pre_rel_path.find("vx_assets") != std::string::npos);
  ASSERT_TRUE(pre_rel_path.find("before_rename.png") != std::string::npos);

  // Verify pre-rename asset exists on disk
  std::string notebook_root = get_test_path("test_buffer_rename_provider");
  std::string pre_asset_abs = notebook_root + "/" + pre_rel_path;
  ASSERT_TRUE(path_exists(pre_asset_abs));

  // Get assets folder BEFORE rename for comparison
  char *pre_assets_folder = nullptr;
  err = vxcore_buffer_get_assets_folder(ctx, buffer_id, &pre_assets_folder);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(pre_assets_folder);
  std::string pre_assets_folder_str(pre_assets_folder);
  vxcore_string_free(pre_assets_folder);

  // Step 4: Rename the file from "notes/test.md" to "renamed.md"
  err = vxcore_node_rename(ctx, notebook_id, "notes/test.md", "renamed.md");
  ASSERT_EQ(err, VXCORE_OK);

  // Step 5: Verify buffer filePath updated
  buffer_json = nullptr;
  err = vxcore_buffer_get(ctx, buffer_id, &buffer_json);
  ASSERT_EQ(err, VXCORE_OK);
  buf_data = nlohmann::json::parse(buffer_json);
  ASSERT_EQ(buf_data["filePath"].get<std::string>(), std::string("notes/renamed.md"));
  vxcore_string_free(buffer_json);

  // Step 6: Insert asset AFTER rename — proves provider uses updated path
  const uint8_t image_data2[] = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46};
  char *post_rename_asset_path = nullptr;
  err = vxcore_buffer_insert_asset_raw(ctx, buffer_id, "after_rename.jpg", image_data2,
                                       sizeof(image_data2), &post_rename_asset_path);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(post_rename_asset_path);

  // Verify post-rename asset path format
  std::string post_rel_path(post_rename_asset_path);
  ASSERT_TRUE(post_rel_path.find("vx_assets") != std::string::npos);
  ASSERT_TRUE(post_rel_path.find("after_rename.jpg") != std::string::npos);

  // Verify post-rename asset exists on disk
  std::string post_asset_abs = notebook_root + "/" + post_rel_path;
  ASSERT_TRUE(path_exists(post_asset_abs));

  // Step 7: Get assets folder AFTER rename — verify it resolves to new location
  char *post_assets_folder = nullptr;
  err = vxcore_buffer_get_assets_folder(ctx, buffer_id, &post_assets_folder);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(post_assets_folder);
  std::string post_assets_folder_str(post_assets_folder);

  // The assets folder should reference the renamed file's location
  ASSERT_TRUE(path_exists(post_assets_folder_str));
  ASSERT_TRUE(post_assets_folder_str.find("vx_assets") != std::string::npos);

  // Assets folder uses UUID-based path, so it should NOT change on file rename
  ASSERT_EQ(pre_assets_folder_str, post_assets_folder_str);

  vxcore_string_free(post_assets_folder);

  // Step 8: Cleanup
  vxcore_string_free(pre_rename_asset_path);
  vxcore_string_free(post_rename_asset_path);
  vxcore_string_free(buffer_id);
  vxcore_string_free(file_id);
  vxcore_string_free(folder_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_rename_provider"));
  std::cout << "  ✓ test_buffer_rename_provider_functional passed" << std::endl;
  return 0;
}

// ============ Buffer Resource Base Path Tests ============

int test_buffer_get_resource_base_path() {
  std::cout << "  Running test_buffer_get_resource_base_path..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_resource_base"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_resource_base").c_str(),
                               "{\"name\":\"Resource Base Test\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a test file at root level
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Open buffer
  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "test.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Get resource base path
  char *base_path = nullptr;
  err = vxcore_buffer_get_resource_base_path(ctx, buffer_id, &base_path);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(base_path);

  // For a root-level file, the resource base path should be the notebook root
  std::string base_str = normalize_path(base_path);
  std::string notebook_root = normalize_path(get_test_path("test_buffer_resource_base"));
  ASSERT_EQ(base_str, notebook_root);

  vxcore_string_free(base_path);
  vxcore_string_free(buffer_id);
  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_resource_base"));
  std::cout << "  ✓ test_buffer_get_resource_base_path passed" << std::endl;
  return 0;
}

int test_buffer_get_resource_base_path_subfolder() {
  std::cout << "  Running test_buffer_get_resource_base_path_subfolder..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_resource_sub"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_buffer_resource_sub").c_str(),
                               "{\"name\":\"Resource Sub Test\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a subfolder and file
  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "docs", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, "docs", "note.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Open buffer
  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "docs/note.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Get resource base path
  char *base_path = nullptr;
  err = vxcore_buffer_get_resource_base_path(ctx, buffer_id, &base_path);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(base_path);

  // For a file in docs/, the resource base path should be notebook_root/docs
  std::string base_str = normalize_path(base_path);
  std::string expected = normalize_path(get_test_path("test_buffer_resource_sub") + "/docs");
  ASSERT_EQ(base_str, expected);

  vxcore_string_free(base_path);
  vxcore_string_free(buffer_id);
  vxcore_string_free(file_id);
  vxcore_string_free(folder_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_resource_sub"));
  std::cout << "  ✓ test_buffer_get_resource_base_path_subfolder passed" << std::endl;
  return 0;
}

int test_buffer_get_resource_base_path_external() {
  std::cout << "  Running test_buffer_get_resource_base_path_external..." << std::endl;
  cleanup_test_dir(get_test_path("test_buffer_resource_ext"));
  create_directory(get_test_path("test_buffer_resource_ext"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  // Create an external file
  std::string ext_file_path = get_test_path("test_buffer_resource_ext") + "/notes.md";
  write_file(ext_file_path, "# External Notes\n");

  // Open buffer for external file (notebook_id = NULL)
  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, nullptr, ext_file_path.c_str(), &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Get resource base path
  char *base_path = nullptr;
  err = vxcore_buffer_get_resource_base_path(ctx, buffer_id, &base_path);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(base_path);

  // For an external file, the resource base path should be the file's parent directory
  std::string base_str = normalize_path(base_path);
  std::string expected = normalize_path(get_test_path("test_buffer_resource_ext"));
  ASSERT_EQ(base_str, expected);

  vxcore_string_free(base_path);
  vxcore_string_free(buffer_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_buffer_resource_ext"));
  std::cout << "  ✓ test_buffer_get_resource_base_path_external passed" << std::endl;
  return 0;
}

int main() {
  std::cout << "Running buffer tests..." << std::endl;

  vxcore_set_test_mode(1);
  vxcore_clear_test_directory();

  RUN_TEST(test_buffer_open_close);
  RUN_TEST(test_buffer_get);
  RUN_TEST(test_buffer_list);
  RUN_TEST(test_buffer_content_raw);
  RUN_TEST(test_buffer_content_json);
  RUN_TEST(test_buffer_save_reload);
  RUN_TEST(test_buffer_deduplication);
  RUN_TEST(test_buffer_external_file);
  RUN_TEST(test_buffer_state);
  RUN_TEST(test_buffer_is_modified);
  RUN_TEST(test_buffer_get_revision);

  // Buffer Backup Tests
  RUN_TEST(test_buffer_write_backup);
  RUN_TEST(test_buffer_has_backup);
  RUN_TEST(test_buffer_recover_backup);
  RUN_TEST(test_buffer_discard_backup);
  RUN_TEST(test_buffer_get_backup_path);
  RUN_TEST(test_buffer_backup_no_content);
  RUN_TEST(test_buffer_recover_no_backup);
  RUN_TEST(test_buffer_discard_no_backup);
  RUN_TEST(test_buffer_backup_format);
  RUN_TEST(test_buffer_backup_overwrite);
  RUN_TEST(test_buffer_backup_cleanup_on_save);
  RUN_TEST(test_buffer_backup_cleanup_on_close);

  RUN_TEST(test_buffer_notebook_close);
  RUN_TEST(test_buffer_lazy_loading);
  RUN_TEST(test_buffer_persistence);

  // Buffer Asset Tests (Filesystem Only)
  RUN_TEST(test_buffer_insert_asset_raw);
  RUN_TEST(test_buffer_insert_asset);
  RUN_TEST(test_buffer_delete_asset);
  RUN_TEST(test_buffer_get_assets_folder);

  // Buffer Attachment Tests (Filesystem + Metadata)
  RUN_TEST(test_buffer_insert_attachment);
  RUN_TEST(test_buffer_delete_attachment);
  RUN_TEST(test_buffer_rename_attachment);
  RUN_TEST(test_buffer_list_attachments);
  RUN_TEST(test_buffer_get_attachments_folder);

  // External File Asset Tests
  RUN_TEST(test_buffer_external_asset);
  RUN_TEST(test_buffer_asset_unique_name);

  // Buffer Path Update on Rename Tests
  RUN_TEST(test_buffer_rename_file_updates_path);
  RUN_TEST(test_buffer_rename_folder_updates_paths);
  RUN_TEST(test_buffer_rename_no_affect_other_buffers);
  RUN_TEST(test_buffer_rename_provider_functional);

  // Buffer Resource Base Path Tests
  RUN_TEST(test_buffer_get_resource_base_path);
  RUN_TEST(test_buffer_get_resource_base_path_subfolder);
  RUN_TEST(test_buffer_get_resource_base_path_external);

  std::cout << "All buffer tests passed!" << std::endl;
  return 0;
}
