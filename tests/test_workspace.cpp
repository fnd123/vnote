#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include "test_utils.h"
#include "vxcore/vxcore.h"

int test_workspace_create() {
  std::cout << "  Running test_workspace_create..." << std::endl;

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *workspace_id = nullptr;
  err = vxcore_workspace_create(ctx, "Test Workspace", &workspace_id);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(workspace_id);

  vxcore_string_free(workspace_id);
  vxcore_context_destroy(ctx);
  std::cout << "  ✓ test_workspace_create passed" << std::endl;
  return 0;
}

int test_workspace_get() {
  std::cout << "  Running test_workspace_get..." << std::endl;

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *workspace_id = nullptr;
  err = vxcore_workspace_create(ctx, "Get Test", &workspace_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *workspace_json = nullptr;
  err = vxcore_workspace_get(ctx, workspace_id, &workspace_json);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(workspace_json);

  // Verify JSON contains expected fields
  std::string json_str(workspace_json);
  ASSERT(json_str.find("\"id\"") != std::string::npos);
  ASSERT(json_str.find("\"name\"") != std::string::npos);
  ASSERT(json_str.find("Get Test") != std::string::npos);

  vxcore_string_free(workspace_id);
  vxcore_string_free(workspace_json);
  vxcore_context_destroy(ctx);
  std::cout << "  ✓ test_workspace_get passed" << std::endl;
  return 0;
}

int test_workspace_list() {
  std::cout << "  Running test_workspace_list..." << std::endl;

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a workspace first
  char *workspace_id = nullptr;
  err = vxcore_workspace_create(ctx, "List Test", &workspace_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *list_json = nullptr;
  err = vxcore_workspace_list(ctx, &list_json);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(list_json);

  // Should contain the created workspace
  std::string json_str(list_json);
  ASSERT(json_str.find("List Test") != std::string::npos);

  vxcore_string_free(workspace_id);
  vxcore_string_free(list_json);
  vxcore_context_destroy(ctx);
  std::cout << "  ✓ test_workspace_list passed" << std::endl;
  return 0;
}

int test_workspace_rename() {
  std::cout << "  Running test_workspace_rename..." << std::endl;

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *workspace_id = nullptr;
  err = vxcore_workspace_create(ctx, "Original Name", &workspace_id);
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_workspace_rename(ctx, workspace_id, "Renamed Workspace");
  ASSERT_EQ(err, VXCORE_OK);

  char *workspace_json = nullptr;
  err = vxcore_workspace_get(ctx, workspace_id, &workspace_json);
  ASSERT_EQ(err, VXCORE_OK);

  std::string json_str(workspace_json);
  ASSERT(json_str.find("Renamed Workspace") != std::string::npos);

  vxcore_string_free(workspace_id);
  vxcore_string_free(workspace_json);
  vxcore_context_destroy(ctx);
  std::cout << "  ✓ test_workspace_rename passed" << std::endl;
  return 0;
}

int test_workspace_delete() {
  std::cout << "  Running test_workspace_delete..." << std::endl;

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *workspace_id = nullptr;
  err = vxcore_workspace_create(ctx, "To Delete", &workspace_id);
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_workspace_delete(ctx, workspace_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Try to get deleted workspace - should fail
  char *workspace_json = nullptr;
  err = vxcore_workspace_get(ctx, workspace_id, &workspace_json);
  ASSERT_EQ(err, VXCORE_ERR_WORKSPACE_NOT_FOUND);

  vxcore_string_free(workspace_id);
  vxcore_context_destroy(ctx);
  std::cout << "  ✓ test_workspace_delete passed" << std::endl;
  return 0;
}

int test_workspace_current() {
  std::cout << "  Running test_workspace_current..." << std::endl;

  // Clear test directory to start fresh for this test
  vxcore_clear_test_directory();

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  // No workspaces at all - current should be empty initially
  char *current_id = nullptr;
  err = vxcore_workspace_get_current(ctx, &current_id);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(current_id);
  ASSERT_EQ(std::string(current_id), std::string(""));
  vxcore_string_free(current_id);

  // Create workspace and set as current
  char *workspace_id = nullptr;
  err = vxcore_workspace_create(ctx, "New Current", &workspace_id);
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_workspace_set_current(ctx, workspace_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *current_id2 = nullptr;
  err = vxcore_workspace_get_current(ctx, &current_id2);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_EQ(std::string(workspace_id), std::string(current_id2));

  vxcore_string_free(current_id2);
  vxcore_string_free(workspace_id);
  vxcore_context_destroy(ctx);
  std::cout << "  ✓ test_workspace_current passed" << std::endl;
  return 0;
}

int test_workspace_buffer_add_remove() {
  std::cout << "  Running test_workspace_buffer_add_remove..." << std::endl;

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *workspace_id = nullptr;
  err = vxcore_workspace_create(ctx, "Buffer Test", &workspace_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Add buffer to workspace (using fake buffer ID for now)
  const char *buffer_id = "test-buffer-id-123";
  err = vxcore_workspace_add_buffer(ctx, workspace_id, buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Try to add same buffer again - should succeed (idempotent operation)
  err = vxcore_workspace_add_buffer(ctx, workspace_id, buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Remove buffer
  err = vxcore_workspace_remove_buffer(ctx, workspace_id, buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  vxcore_string_free(workspace_id);
  vxcore_context_destroy(ctx);
  std::cout << "  ✓ test_workspace_buffer_add_remove passed" << std::endl;
  return 0;
}

int test_workspace_current_buffer() {
  std::cout << "  Running test_workspace_current_buffer..." << std::endl;

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *workspace_id = nullptr;
  err = vxcore_workspace_create(ctx, "Current Buffer Test", &workspace_id);
  ASSERT_EQ(err, VXCORE_OK);

  const char *buffer_id = "test-buffer-id-456";
  err = vxcore_workspace_add_buffer(ctx, workspace_id, buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Set current buffer
  err = vxcore_workspace_set_current_buffer(ctx, workspace_id, buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Verify current buffer in JSON
  char *workspace_json = nullptr;
  err = vxcore_workspace_get(ctx, workspace_id, &workspace_json);
  ASSERT_EQ(err, VXCORE_OK);

  std::string json_str(workspace_json);
  ASSERT(json_str.find(buffer_id) != std::string::npos);

  vxcore_string_free(workspace_id);
  vxcore_string_free(workspace_json);
  vxcore_context_destroy(ctx);
  std::cout << "  ✓ test_workspace_current_buffer passed" << std::endl;
  return 0;
}

int test_workspace_empty_initially() {
  std::cout << "  Running test_workspace_empty_initially..." << std::endl;

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  // No default workspace - list should be empty
  char *list_json = nullptr;
  err = vxcore_workspace_list(ctx, &list_json);
  ASSERT_EQ(err, VXCORE_OK);

  std::string json_str(list_json);
  ASSERT_EQ(json_str, std::string("[]"));

  vxcore_string_free(list_json);
  vxcore_context_destroy(ctx);
  std::cout << "  ✓ test_workspace_empty_initially passed" << std::endl;
  return 0;
}

int test_workspace_persistence() {
  std::cout << "  Running test_workspace_persistence..." << std::endl;

  std::string workspace_name = "Persistent Workspace";
  std::string workspace_id_str;

  // Create workspace and save
  {
    VxCoreContextHandle ctx = nullptr;
    VxCoreError err = vxcore_context_create(nullptr, &ctx);
    ASSERT_EQ(err, VXCORE_OK);

    char *workspace_id = nullptr;
    err = vxcore_workspace_create(ctx, workspace_name.c_str(), &workspace_id);
    ASSERT_EQ(err, VXCORE_OK);
    workspace_id_str = std::string(workspace_id);

    vxcore_string_free(workspace_id);
    vxcore_context_destroy(ctx);  // This should save session config
  }

  // Reload and verify workspace persisted
  {
    VxCoreContextHandle ctx = nullptr;
    VxCoreError err = vxcore_context_create(nullptr, &ctx);
    ASSERT_EQ(err, VXCORE_OK);

    char *workspace_json = nullptr;
    err = vxcore_workspace_get(ctx, workspace_id_str.c_str(), &workspace_json);
    ASSERT_EQ(err, VXCORE_OK);

    std::string json_str(workspace_json);
    ASSERT(json_str.find(workspace_name) != std::string::npos);

    vxcore_string_free(workspace_json);
    vxcore_context_destroy(ctx);
  }

  std::cout << "  ✓ test_workspace_persistence passed" << std::endl;
  return 0;
}

// Helper: create a temp file that can be opened as an external buffer.
static std::string create_ws_temp_file(const std::string &name, const std::string &content) {
  auto dir = std::filesystem::temp_directory_path() / "vxcore_test_data" / "workspace_test";
  std::filesystem::create_directories(dir);
  auto path = dir / name;
  std::ofstream file(path, std::ios::binary);
  file << content;
  file.close();
  std::string result = path.string();
  std::replace(result.begin(), result.end(), '\\', '/');
  return result;
}

static void cleanup_ws_temp_files() {
  std::error_code ec;
  std::filesystem::remove_all(
      std::filesystem::temp_directory_path() / "vxcore_test_data" / "workspace_test", ec);
}

// Regression test: vxcore_prepare_shutdown() correctly persists workspace buffers.
// Verifies the "save session before closing buffers" pattern works.
int test_workspace_shutdown_saves_buffers() {
  std::cout << "  Running test_workspace_shutdown_saves_buffers..." << std::endl;

  vxcore_clear_test_directory();
  cleanup_ws_temp_files();

  std::string file_a = create_ws_temp_file("buf-a.md", "# Buffer A");
  std::string file_b = create_ws_temp_file("buf-b.md", "# Buffer B");

  std::string ws_id_str;
  std::string buf_a_id_str;
  std::string buf_b_id_str;

  // Session 1: Create workspace with buffers, shutdown to persist.
  {
    VxCoreContextHandle ctx = nullptr;
    VxCoreError err = vxcore_context_create(nullptr, &ctx);
    ASSERT_EQ(err, VXCORE_OK);

    char *buf_a_id = nullptr;
    err = vxcore_buffer_open(ctx, nullptr, file_a.c_str(), &buf_a_id);
    ASSERT_EQ(err, VXCORE_OK);
    ASSERT_NOT_NULL(buf_a_id);
    buf_a_id_str = buf_a_id;

    char *buf_b_id = nullptr;
    err = vxcore_buffer_open(ctx, nullptr, file_b.c_str(), &buf_b_id);
    ASSERT_EQ(err, VXCORE_OK);
    ASSERT_NOT_NULL(buf_b_id);
    buf_b_id_str = buf_b_id;

    char *ws_id = nullptr;
    err = vxcore_workspace_create(ctx, "WS1", &ws_id);
    ASSERT_EQ(err, VXCORE_OK);
    ws_id_str = ws_id;

    err = vxcore_workspace_add_buffer(ctx, ws_id, buf_a_id);
    ASSERT_EQ(err, VXCORE_OK);
    err = vxcore_workspace_add_buffer(ctx, ws_id, buf_b_id);
    ASSERT_EQ(err, VXCORE_OK);

    err = vxcore_workspace_set_current(ctx, ws_id);
    ASSERT_EQ(err, VXCORE_OK);

    // Shutdown persists workspace state (including buffer list).
    err = vxcore_prepare_shutdown(ctx);
    ASSERT_EQ(err, VXCORE_OK);

    vxcore_string_free(buf_a_id);
    vxcore_string_free(buf_b_id);
    vxcore_string_free(ws_id);
    vxcore_context_destroy(ctx);
  }

  // Session 2: Reload and verify workspace + buffers survived.
  {
    VxCoreContextHandle ctx = nullptr;
    VxCoreError err = vxcore_context_create(nullptr, &ctx);
    ASSERT_EQ(err, VXCORE_OK);

    // Workspace should exist.
    char *ws_json = nullptr;
    err = vxcore_workspace_get(ctx, ws_id_str.c_str(), &ws_json);
    ASSERT_EQ(err, VXCORE_OK);
    ASSERT_NOT_NULL(ws_json);

    auto parsed = nlohmann::json::parse(ws_json);
    ASSERT_EQ(parsed["name"].get<std::string>(), std::string("WS1"));

    // bufferIds should contain both buffers.
    auto buffer_ids = parsed["bufferIds"];
    ASSERT_TRUE(buffer_ids.is_array());
    ASSERT_EQ(buffer_ids.size(), 2u);

    bool found_a = false;
    bool found_b = false;
    for (const auto &id : buffer_ids) {
      if (id.get<std::string>() == buf_a_id_str) found_a = true;
      if (id.get<std::string>() == buf_b_id_str) found_b = true;
    }
    ASSERT_TRUE(found_a);
    ASSERT_TRUE(found_b);

    // Current workspace should be WS1.
    char *current_ws = nullptr;
    err = vxcore_workspace_get_current(ctx, &current_ws);
    ASSERT_EQ(err, VXCORE_OK);
    ASSERT_EQ(std::string(current_ws), ws_id_str);

    vxcore_string_free(current_ws);
    vxcore_string_free(ws_json);
    vxcore_context_destroy(ctx);
  }

  cleanup_ws_temp_files();
  std::cout << "  ✓ test_workspace_shutdown_saves_buffers passed" << std::endl;
  return 0;
}

// Regression test: removing all buffers then shutting down loses buffers (expected).
// This documents the vxcore behavior that caused the Qt-layer bug: if closeAllBuffers()
// runs before saveSession()/vxcore_prepare_shutdown(), the persisted workspace has no buffers.
int test_workspace_buffer_removal_then_shutdown() {
  std::cout << "  Running test_workspace_buffer_removal_then_shutdown..." << std::endl;

  vxcore_clear_test_directory();
  cleanup_ws_temp_files();

  std::string file_a = create_ws_temp_file("rem-a.md", "# Remove A");
  std::string file_b = create_ws_temp_file("rem-b.md", "# Remove B");

  std::string ws_id_str;

  // Session 1: Create workspace with buffers and persist.
  {
    VxCoreContextHandle ctx = nullptr;
    VxCoreError err = vxcore_context_create(nullptr, &ctx);
    ASSERT_EQ(err, VXCORE_OK);

    char *buf_a_id = nullptr;
    err = vxcore_buffer_open(ctx, nullptr, file_a.c_str(), &buf_a_id);
    ASSERT_EQ(err, VXCORE_OK);

    char *buf_b_id = nullptr;
    err = vxcore_buffer_open(ctx, nullptr, file_b.c_str(), &buf_b_id);
    ASSERT_EQ(err, VXCORE_OK);

    char *ws_id = nullptr;
    err = vxcore_workspace_create(ctx, "WS1", &ws_id);
    ASSERT_EQ(err, VXCORE_OK);
    ws_id_str = ws_id;

    err = vxcore_workspace_add_buffer(ctx, ws_id, buf_a_id);
    ASSERT_EQ(err, VXCORE_OK);
    err = vxcore_workspace_add_buffer(ctx, ws_id, buf_b_id);
    ASSERT_EQ(err, VXCORE_OK);

    err = vxcore_workspace_set_current(ctx, ws_id);
    ASSERT_EQ(err, VXCORE_OK);

    err = vxcore_prepare_shutdown(ctx);
    ASSERT_EQ(err, VXCORE_OK);

    vxcore_string_free(buf_a_id);
    vxcore_string_free(buf_b_id);
    vxcore_string_free(ws_id);
    vxcore_context_destroy(ctx);
  }

  // Session 2: Reload, remove all buffers, then shutdown (the bug scenario).
  {
    VxCoreContextHandle ctx = nullptr;
    VxCoreError err = vxcore_context_create(nullptr, &ctx);
    ASSERT_EQ(err, VXCORE_OK);

    // Workspace should have 2 buffers from session 1.
    char *ws_json = nullptr;
    err = vxcore_workspace_get(ctx, ws_id_str.c_str(), &ws_json);
    ASSERT_EQ(err, VXCORE_OK);

    auto parsed = nlohmann::json::parse(ws_json);
    auto buffer_ids = parsed["bufferIds"];
    ASSERT_EQ(buffer_ids.size(), 2u);
    vxcore_string_free(ws_json);

    // Remove all buffers from workspace (simulates closeAllBuffers).
    for (const auto &id : buffer_ids) {
      std::string buf_id = id.get<std::string>();
      err = vxcore_workspace_remove_buffer(ctx, ws_id_str.c_str(), buf_id.c_str());
      ASSERT_EQ(err, VXCORE_OK);
    }

    // Shutdown persists the now-empty workspace.
    err = vxcore_prepare_shutdown(ctx);
    ASSERT_EQ(err, VXCORE_OK);

    vxcore_context_destroy(ctx);
  }

  // Session 3: Reload and verify workspace has 0 buffers (expected vxcore behavior).
  // This is the scenario that the Qt-layer fix prevents: the caller must save session
  // BEFORE removing buffers from workspaces.
  {
    VxCoreContextHandle ctx = nullptr;
    VxCoreError err = vxcore_context_create(nullptr, &ctx);
    ASSERT_EQ(err, VXCORE_OK);

    // Workspace should still exist but with no buffers.
    char *ws_json = nullptr;
    err = vxcore_workspace_get(ctx, ws_id_str.c_str(), &ws_json);
    ASSERT_EQ(err, VXCORE_OK);
    ASSERT_NOT_NULL(ws_json);

    auto parsed = nlohmann::json::parse(ws_json);
    ASSERT_EQ(parsed["name"].get<std::string>(), std::string("WS1"));

    auto buffer_ids = parsed["bufferIds"];
    ASSERT_TRUE(buffer_ids.is_array());
    ASSERT_EQ(buffer_ids.size(), 0u);

    vxcore_string_free(ws_json);
    vxcore_context_destroy(ctx);
  }

  cleanup_ws_temp_files();
  std::cout << "  ✓ test_workspace_buffer_removal_then_shutdown passed" << std::endl;
  return 0;
}

int main() {
  std::cout << "Running workspace tests..." << std::endl;

  vxcore_set_test_mode(1);
  vxcore_clear_test_directory();

  RUN_TEST(test_workspace_empty_initially);
  RUN_TEST(test_workspace_create);
  RUN_TEST(test_workspace_get);
  RUN_TEST(test_workspace_list);
  RUN_TEST(test_workspace_rename);
  RUN_TEST(test_workspace_delete);
  RUN_TEST(test_workspace_current);
  RUN_TEST(test_workspace_buffer_add_remove);
  RUN_TEST(test_workspace_current_buffer);
  RUN_TEST(test_workspace_persistence);
  RUN_TEST(test_workspace_shutdown_saves_buffers);
  RUN_TEST(test_workspace_buffer_removal_then_shutdown);

  std::cout << "All workspace tests passed!" << std::endl;
  return 0;
}
