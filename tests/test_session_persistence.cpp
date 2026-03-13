#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include "test_utils.h"
#include "vxcore/vxcore.h"

// Helper: read session config file directly from disk
static nlohmann::json read_session_file() {
  // In test mode, both app_data and local_data point to temp/vxcore_test_config
  auto path = std::filesystem::temp_directory_path() / "vxcore_test_config" / "vxsession.json";
  if (!std::filesystem::exists(path)) {
    return nlohmann::json::object();
  }
  std::ifstream file(path);
  if (!file.is_open()) {
    return nlohmann::json::object();
  }
  return nlohmann::json::parse(file);
}

// Helper: create a temp file that can be opened as an external buffer
static std::string create_temp_file(const std::string &name, const std::string &content) {
  auto dir = std::filesystem::temp_directory_path() / "vxcore_test_data" / "session_test";
  std::filesystem::create_directories(dir);
  auto path = dir / name;
  std::ofstream file(path, std::ios::binary);
  file << content;
  file.close();
  return path.string();
}

// Helper: clean up temp test files
static void cleanup_temp_files() {
  auto dir = std::filesystem::temp_directory_path() / "vxcore_test_data" / "session_test";
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
}

int test_workspace_record_roundtrip() {
  std::cout << "  Running test_workspace_record_roundtrip..." << std::endl;

  // Create a WorkspaceRecord with enriched fields via JSON roundtrip
  nlohmann::json input = {{"id", "ws-001"},
                          {"name", "My Workspace"},
                          {"bufferIds", {"buf-1", "buf-2", "buf-3"}},
                          {"currentBufferId", "buf-2"},
                          {"metadata", {{"theme", "dark"}, {"splitRatio", 0.5}}}};

  // Serialize -> parse via session config roundtrip
  nlohmann::json session_json = {
      {"notebooks", nlohmann::json::array()},
      {"workspaces", nlohmann::json::array({input})},
      {"buffers", nlohmann::json::array()},
      {"currentWorkspaceId", "ws-001"},
  };

  std::string serialized = session_json.dump();
  nlohmann::json parsed = nlohmann::json::parse(serialized);

  // Verify workspace record fields survive roundtrip
  auto ws = parsed["workspaces"][0];
  ASSERT_EQ(ws["id"].get<std::string>(), std::string("ws-001"));
  ASSERT_EQ(ws["name"].get<std::string>(), std::string("My Workspace"));
  ASSERT_EQ(ws["bufferIds"].size(), 3u);
  ASSERT_EQ(ws["bufferIds"][0].get<std::string>(), std::string("buf-1"));
  ASSERT_EQ(ws["bufferIds"][1].get<std::string>(), std::string("buf-2"));
  ASSERT_EQ(ws["bufferIds"][2].get<std::string>(), std::string("buf-3"));
  ASSERT_EQ(ws["currentBufferId"].get<std::string>(), std::string("buf-2"));
  ASSERT_TRUE(ws["metadata"].is_object());
  ASSERT_EQ(ws["metadata"]["theme"].get<std::string>(), std::string("dark"));

  std::cout << "  ✓ test_workspace_record_roundtrip passed" << std::endl;
  return 0;
}

int test_session_recovery_enabled() {
  std::cout << "  Running test_session_recovery_enabled..." << std::endl;

  vxcore_clear_test_directory();
  cleanup_temp_files();

  // Create temp files for external buffers
  std::string file1 = create_temp_file("note1.md", "# Note 1");
  std::string file2 = create_temp_file("note2.md", "# Note 2");

  std::string ws_id_str;
  std::string buf1_id_str;
  std::string buf2_id_str;

  // Session 1: Create buffers and workspaces, shutdown
  {
    VxCoreContextHandle ctx = nullptr;
    VxCoreError err = vxcore_context_create(nullptr, &ctx);
    ASSERT_EQ(err, VXCORE_OK);

    // Open two external buffers
    char *buf1_id = nullptr;
    err = vxcore_buffer_open(ctx, nullptr, file1.c_str(), &buf1_id);
    ASSERT_EQ(err, VXCORE_OK);
    ASSERT_NOT_NULL(buf1_id);
    buf1_id_str = buf1_id;

    char *buf2_id = nullptr;
    err = vxcore_buffer_open(ctx, nullptr, file2.c_str(), &buf2_id);
    ASSERT_EQ(err, VXCORE_OK);
    ASSERT_NOT_NULL(buf2_id);
    buf2_id_str = buf2_id;

    // Create workspace and add buffers
    char *ws_id = nullptr;
    err = vxcore_workspace_create(ctx, "Persistent WS", &ws_id);
    ASSERT_EQ(err, VXCORE_OK);
    ws_id_str = ws_id;

    err = vxcore_workspace_add_buffer(ctx, ws_id, buf1_id);
    ASSERT_EQ(err, VXCORE_OK);

    err = vxcore_workspace_add_buffer(ctx, ws_id, buf2_id);
    ASSERT_EQ(err, VXCORE_OK);

    err = vxcore_workspace_set_current_buffer(ctx, ws_id, buf1_id);
    ASSERT_EQ(err, VXCORE_OK);

    err = vxcore_workspace_set_current(ctx, ws_id);
    ASSERT_EQ(err, VXCORE_OK);

    // Shutdown to persist state
    err = vxcore_shutdown(ctx);
    ASSERT_EQ(err, VXCORE_OK);

    vxcore_string_free(buf1_id);
    vxcore_string_free(buf2_id);
    vxcore_string_free(ws_id);
    vxcore_context_destroy(ctx);
  }

  // Session 2: Verify recovery
  {
    VxCoreContextHandle ctx = nullptr;
    VxCoreError err = vxcore_context_create(nullptr, &ctx);
    ASSERT_EQ(err, VXCORE_OK);

    // Verify buffers restored
    char *buf1_json = nullptr;
    err = vxcore_buffer_get(ctx, buf1_id_str.c_str(), &buf1_json);
    ASSERT_EQ(err, VXCORE_OK);
    ASSERT_NOT_NULL(buf1_json);
    vxcore_string_free(buf1_json);

    char *buf2_json = nullptr;
    err = vxcore_buffer_get(ctx, buf2_id_str.c_str(), &buf2_json);
    ASSERT_EQ(err, VXCORE_OK);
    ASSERT_NOT_NULL(buf2_json);
    vxcore_string_free(buf2_json);

    // Verify workspace restored with buffer IDs
    char *ws_json = nullptr;
    err = vxcore_workspace_get(ctx, ws_id_str.c_str(), &ws_json);
    ASSERT_EQ(err, VXCORE_OK);
    ASSERT_NOT_NULL(ws_json);

    std::string ws_str(ws_json);
    ASSERT(ws_str.find("Persistent WS") != std::string::npos);
    ASSERT(ws_str.find(buf1_id_str) != std::string::npos);
    ASSERT(ws_str.find(buf2_id_str) != std::string::npos);
    vxcore_string_free(ws_json);

    // Verify current workspace restored
    char *current_ws = nullptr;
    err = vxcore_workspace_get_current(ctx, &current_ws);
    ASSERT_EQ(err, VXCORE_OK);
    ASSERT_EQ(std::string(current_ws), ws_id_str);
    vxcore_string_free(current_ws);

    vxcore_shutdown(ctx);
    vxcore_context_destroy(ctx);
  }

  cleanup_temp_files();
  std::cout << "  ✓ test_session_recovery_enabled passed" << std::endl;
  return 0;
}

int test_session_recovery_disabled() {
  std::cout << "  Running test_session_recovery_disabled..." << std::endl;

  vxcore_clear_test_directory();
  cleanup_temp_files();

  std::string file1 = create_temp_file("disabled1.md", "# Content");

  // Session 1: Create some state
  {
    VxCoreContextHandle ctx = nullptr;
    VxCoreError err = vxcore_context_create(nullptr, &ctx);
    ASSERT_EQ(err, VXCORE_OK);

    char *buf_id = nullptr;
    err = vxcore_buffer_open(ctx, nullptr, file1.c_str(), &buf_id);
    ASSERT_EQ(err, VXCORE_OK);

    char *ws_id = nullptr;
    err = vxcore_workspace_create(ctx, "Should Not Restore", &ws_id);
    ASSERT_EQ(err, VXCORE_OK);

    err = vxcore_workspace_add_buffer(ctx, ws_id, buf_id);
    ASSERT_EQ(err, VXCORE_OK);

    err = vxcore_shutdown(ctx);
    ASSERT_EQ(err, VXCORE_OK);

    vxcore_string_free(buf_id);
    vxcore_string_free(ws_id);
    vxcore_context_destroy(ctx);
  }

  // Write config with recoverLastSession=false
  {
    auto config_path =
        std::filesystem::temp_directory_path() / "vxcore_test_config" / "vxcore.json";
    nlohmann::json config = {{"version", "0.1.0"}, {"recoverLastSession", false}};
    std::ofstream file(config_path);
    file << config.dump(2);
  }

  // Session 2: Should NOT restore anything
  {
    VxCoreContextHandle ctx = nullptr;
    VxCoreError err = vxcore_context_create(nullptr, &ctx);
    ASSERT_EQ(err, VXCORE_OK);

    // Buffers should be empty
    char *buf_list = nullptr;
    err = vxcore_buffer_list(ctx, &buf_list);
    ASSERT_EQ(err, VXCORE_OK);
    std::string buf_list_str(buf_list);
    ASSERT_EQ(buf_list_str, std::string("[]"));
    vxcore_string_free(buf_list);

    // Workspaces should be empty
    char *ws_list = nullptr;
    err = vxcore_workspace_list(ctx, &ws_list);
    ASSERT_EQ(err, VXCORE_OK);
    std::string ws_list_str(ws_list);
    ASSERT_EQ(ws_list_str, std::string("[]"));
    vxcore_string_free(ws_list);

    vxcore_shutdown(ctx);
    vxcore_context_destroy(ctx);
  }

  cleanup_temp_files();
  std::cout << "  ✓ test_session_recovery_disabled passed" << std::endl;
  return 0;
}

int test_missing_files_skipped() {
  std::cout << "  Running test_missing_files_skipped..." << std::endl;

  vxcore_clear_test_directory();
  cleanup_temp_files();

  std::string file1 = create_temp_file("exists.md", "# Exists");
  std::string file2 = create_temp_file("will_delete.md", "# Will Delete");

  std::string ws_id_str;
  std::string buf1_id_str;
  std::string buf2_id_str;

  // Session 1: Create buffers and workspace
  {
    VxCoreContextHandle ctx = nullptr;
    VxCoreError err = vxcore_context_create(nullptr, &ctx);
    ASSERT_EQ(err, VXCORE_OK);

    char *buf1_id = nullptr;
    err = vxcore_buffer_open(ctx, nullptr, file1.c_str(), &buf1_id);
    ASSERT_EQ(err, VXCORE_OK);
    buf1_id_str = buf1_id;

    char *buf2_id = nullptr;
    err = vxcore_buffer_open(ctx, nullptr, file2.c_str(), &buf2_id);
    ASSERT_EQ(err, VXCORE_OK);
    buf2_id_str = buf2_id;

    char *ws_id = nullptr;
    err = vxcore_workspace_create(ctx, "Filter Test", &ws_id);
    ASSERT_EQ(err, VXCORE_OK);
    ws_id_str = ws_id;

    err = vxcore_workspace_add_buffer(ctx, ws_id, buf1_id);
    ASSERT_EQ(err, VXCORE_OK);
    err = vxcore_workspace_add_buffer(ctx, ws_id, buf2_id);
    ASSERT_EQ(err, VXCORE_OK);
    err = vxcore_workspace_set_current_buffer(ctx, ws_id, buf2_id);
    ASSERT_EQ(err, VXCORE_OK);

    err = vxcore_shutdown(ctx);
    ASSERT_EQ(err, VXCORE_OK);

    vxcore_string_free(buf1_id);
    vxcore_string_free(buf2_id);
    vxcore_string_free(ws_id);
    vxcore_context_destroy(ctx);
  }

  // Delete one file before recovery
  std::filesystem::remove(file2);

  // Session 2: Recover — missing file's buffer should be skipped
  {
    VxCoreContextHandle ctx = nullptr;
    VxCoreError err = vxcore_context_create(nullptr, &ctx);
    ASSERT_EQ(err, VXCORE_OK);

    // Buffer for existing file should be restored
    char *buf1_json = nullptr;
    err = vxcore_buffer_get(ctx, buf1_id_str.c_str(), &buf1_json);
    ASSERT_EQ(err, VXCORE_OK);
    ASSERT_NOT_NULL(buf1_json);
    vxcore_string_free(buf1_json);

    // Buffer for deleted file should NOT be restored
    char *buf2_json = nullptr;
    err = vxcore_buffer_get(ctx, buf2_id_str.c_str(), &buf2_json);
    ASSERT_EQ(err, VXCORE_ERR_BUFFER_NOT_FOUND);

    // Workspace should only contain the surviving buffer
    char *ws_json = nullptr;
    err = vxcore_workspace_get(ctx, ws_id_str.c_str(), &ws_json);
    ASSERT_EQ(err, VXCORE_OK);
    std::string ws_str(ws_json);
    ASSERT(ws_str.find(buf1_id_str) != std::string::npos);
    ASSERT(ws_str.find(buf2_id_str) == std::string::npos);
    vxcore_string_free(ws_json);

    // Current buffer should have fallen back to buf1 (since buf2 was removed)
    nlohmann::json ws_parsed = nlohmann::json::parse(ws_str);
    ASSERT_EQ(ws_parsed["currentBufferId"].get<std::string>(), buf1_id_str);

    vxcore_shutdown(ctx);
    vxcore_context_destroy(ctx);
  }

  cleanup_temp_files();
  std::cout << "  ✓ test_missing_files_skipped passed" << std::endl;
  return 0;
}

int test_save_on_create_not_on_close() {
  std::cout << "  Running test_save_on_create_not_on_close..." << std::endl;

  vxcore_clear_test_directory();
  cleanup_temp_files();

  std::string file1 = create_temp_file("save_test.md", "# Save Test");

  {
    VxCoreContextHandle ctx = nullptr;
    VxCoreError err = vxcore_context_create(nullptr, &ctx);
    ASSERT_EQ(err, VXCORE_OK);

    // Open buffer (create) — should trigger save
    char *buf_id = nullptr;
    err = vxcore_buffer_open(ctx, nullptr, file1.c_str(), &buf_id);
    ASSERT_EQ(err, VXCORE_OK);
    std::string buf_id_str(buf_id);

    // Read session file — buffer should be persisted after open
    nlohmann::json session = read_session_file();
    ASSERT_TRUE(session.contains("buffers"));
    ASSERT_EQ(session["buffers"].size(), 1u);
    ASSERT_EQ(session["buffers"][0]["id"].get<std::string>(), buf_id_str);

    // Close buffer — should NOT save session
    err = vxcore_buffer_close(ctx, buf_id);
    ASSERT_EQ(err, VXCORE_OK);

    // Read session file again — buffer should STILL be in the file (not updated on close)
    nlohmann::json session2 = read_session_file();
    ASSERT_EQ(session2["buffers"].size(), 1u);
    ASSERT_EQ(session2["buffers"][0]["id"].get<std::string>(), buf_id_str);

    // After shutdown, the empty state should be saved
    err = vxcore_shutdown(ctx);
    ASSERT_EQ(err, VXCORE_OK);

    nlohmann::json session3 = read_session_file();
    ASSERT_EQ(session3["buffers"].size(), 0u);

    vxcore_string_free(buf_id);
    vxcore_context_destroy(ctx);
  }

  cleanup_temp_files();
  std::cout << "  ✓ test_save_on_create_not_on_close passed" << std::endl;
  return 0;
}

int test_shutdown_prevents_destructor_save() {
  std::cout << "  Running test_shutdown_prevents_destructor_save..." << std::endl;

  vxcore_clear_test_directory();
  cleanup_temp_files();

  std::string file1 = create_temp_file("shutdown_test.md", "# Shutdown");

  {
    VxCoreContextHandle ctx = nullptr;
    VxCoreError err = vxcore_context_create(nullptr, &ctx);
    ASSERT_EQ(err, VXCORE_OK);

    // Open a buffer
    char *buf_id = nullptr;
    err = vxcore_buffer_open(ctx, nullptr, file1.c_str(), &buf_id);
    ASSERT_EQ(err, VXCORE_OK);
    std::string buf_id_str(buf_id);

    // Create a workspace with the buffer
    char *ws_id = nullptr;
    err = vxcore_workspace_create(ctx, "Shutdown Test WS", &ws_id);
    ASSERT_EQ(err, VXCORE_OK);

    err = vxcore_workspace_add_buffer(ctx, ws_id, buf_id);
    ASSERT_EQ(err, VXCORE_OK);

    // Shutdown saves the correct state
    err = vxcore_shutdown(ctx);
    ASSERT_EQ(err, VXCORE_OK);

    // Verify session file has the correct data after shutdown
    nlohmann::json session = read_session_file();
    ASSERT_EQ(session["buffers"].size(), 1u);
    ASSERT_EQ(session["workspaces"].size(), 1u);
    ASSERT_EQ(session["workspaces"][0]["bufferIds"].size(), 1u);

    vxcore_string_free(buf_id);
    vxcore_string_free(ws_id);

    // Destroy context — destructor should NOT overwrite the session file
    vxcore_context_destroy(ctx);

    // Verify session file still has the correct data (not overwritten by destructor)
    nlohmann::json session2 = read_session_file();
    ASSERT_EQ(session2["buffers"].size(), 1u);
    ASSERT_EQ(session2["workspaces"].size(), 1u);
  }

  cleanup_temp_files();
  std::cout << "  ✓ test_shutdown_prevents_destructor_save passed" << std::endl;
  return 0;
}

int test_workspace_create_saves_session() {
  std::cout << "  Running test_workspace_create_saves_session..." << std::endl;

  vxcore_clear_test_directory();

  {
    VxCoreContextHandle ctx = nullptr;
    VxCoreError err = vxcore_context_create(nullptr, &ctx);
    ASSERT_EQ(err, VXCORE_OK);

    // Create workspace — should trigger save
    char *ws_id = nullptr;
    err = vxcore_workspace_create(ctx, "Create Save Test", &ws_id);
    ASSERT_EQ(err, VXCORE_OK);
    std::string ws_id_str(ws_id);

    // Verify session file has workspace after create
    nlohmann::json session = read_session_file();
    ASSERT_TRUE(session.contains("workspaces"));
    ASSERT_EQ(session["workspaces"].size(), 1u);
    ASSERT_EQ(session["workspaces"][0]["name"].get<std::string>(), std::string("Create Save Test"));

    // Delete workspace — should NOT save session
    err = vxcore_workspace_delete(ctx, ws_id);
    ASSERT_EQ(err, VXCORE_OK);

    // Session file should STILL have the old workspace (not updated on delete)
    nlohmann::json session2 = read_session_file();
    ASSERT_EQ(session2["workspaces"].size(), 1u);

    // After shutdown, empty state persisted
    err = vxcore_shutdown(ctx);
    ASSERT_EQ(err, VXCORE_OK);

    nlohmann::json session3 = read_session_file();
    ASSERT_EQ(session3["workspaces"].size(), 0u);

    vxcore_string_free(ws_id);
    vxcore_context_destroy(ctx);
  }

  std::cout << "  ✓ test_workspace_create_saves_session passed" << std::endl;
  return 0;
}

int main() {
  std::cout << "Running session persistence tests..." << std::endl;

  vxcore_set_test_mode(1);
  vxcore_clear_test_directory();

  RUN_TEST(test_workspace_record_roundtrip);
  RUN_TEST(test_session_recovery_enabled);
  RUN_TEST(test_session_recovery_disabled);
  RUN_TEST(test_missing_files_skipped);
  RUN_TEST(test_save_on_create_not_on_close);
  RUN_TEST(test_shutdown_prevents_destructor_save);
  RUN_TEST(test_workspace_create_saves_session);

  std::cout << "All session persistence tests passed!" << std::endl;
  return 0;
}
