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
    err = vxcore_prepare_shutdown(ctx);
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

    vxcore_prepare_shutdown(ctx);
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

    err = vxcore_prepare_shutdown(ctx);
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

    vxcore_prepare_shutdown(ctx);
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

    err = vxcore_prepare_shutdown(ctx);
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

    vxcore_prepare_shutdown(ctx);
    vxcore_context_destroy(ctx);
  }

  cleanup_temp_files();
  std::cout << "  ✓ test_missing_files_skipped passed" << std::endl;
  return 0;
}

int test_save_on_create_and_close() {
  std::cout << "  Running test_save_on_create_and_close..." << std::endl;

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

    // Close buffer — with always-in-sync, close now DOES save session
    err = vxcore_buffer_close(ctx, buf_id);
    ASSERT_EQ(err, VXCORE_OK);

    // Read session file again — buffer should be gone (close persists now)
    nlohmann::json session2 = read_session_file();
    ASSERT_EQ(session2["buffers"].size(), 0u);

    vxcore_string_free(buf_id);
    vxcore_prepare_shutdown(ctx);
    vxcore_context_destroy(ctx);
  }

  cleanup_temp_files();
  std::cout << "  ✓ test_save_on_create_and_close passed" << std::endl;
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
    err = vxcore_prepare_shutdown(ctx);
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

int test_workspace_create_and_delete_saves_session() {
  std::cout << "  Running test_workspace_create_and_delete_saves_session..." << std::endl;

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

    // Delete workspace — with always-in-sync, delete now DOES save session
    err = vxcore_workspace_delete(ctx, ws_id);
    ASSERT_EQ(err, VXCORE_OK);

    // Session file should now reflect 0 workspaces (delete persists now)
    nlohmann::json session2 = read_session_file();
    ASSERT_EQ(session2["workspaces"].size(), 0u);

    vxcore_string_free(ws_id);
    vxcore_prepare_shutdown(ctx);
    vxcore_context_destroy(ctx);
  }

  std::cout << "  ✓ test_workspace_create_and_delete_saves_session passed" << std::endl;
  return 0;
}

int test_all_mutations_persist() {
  std::cout << "  Running test_all_mutations_persist..." << std::endl;

  vxcore_clear_test_directory();
  cleanup_temp_files();

  std::string file1 = create_temp_file("mut1.md", "# Mutation 1");
  std::string file2 = create_temp_file("mut2.md", "# Mutation 2");

  {
    VxCoreContextHandle ctx = nullptr;
    VxCoreError err = vxcore_context_create(nullptr, &ctx);
    ASSERT_EQ(err, VXCORE_OK);

    // 1. Open buffer — should persist
    char *buf1_id = nullptr;
    err = vxcore_buffer_open(ctx, nullptr, file1.c_str(), &buf1_id);
    ASSERT_EQ(err, VXCORE_OK);
    {
      nlohmann::json s = read_session_file();
      ASSERT_EQ(s["buffers"].size(), 1u);
    }

    // 2. Open second buffer
    char *buf2_id = nullptr;
    err = vxcore_buffer_open(ctx, nullptr, file2.c_str(), &buf2_id);
    ASSERT_EQ(err, VXCORE_OK);
    {
      nlohmann::json s = read_session_file();
      ASSERT_EQ(s["buffers"].size(), 2u);
    }

    // 3. Create workspace — should persist
    char *ws_id = nullptr;
    err = vxcore_workspace_create(ctx, "Mut Test WS", &ws_id);
    ASSERT_EQ(err, VXCORE_OK);
    {
      nlohmann::json s = read_session_file();
      ASSERT_EQ(s["workspaces"].size(), 1u);
    }

    // 4. Add buffer to workspace — should persist
    err = vxcore_workspace_add_buffer(ctx, ws_id, buf1_id);
    ASSERT_EQ(err, VXCORE_OK);
    {
      nlohmann::json s = read_session_file();
      ASSERT_EQ(s["workspaces"][0]["bufferIds"].size(), 1u);
    }

    // 5. Add second buffer
    err = vxcore_workspace_add_buffer(ctx, ws_id, buf2_id);
    ASSERT_EQ(err, VXCORE_OK);
    {
      nlohmann::json s = read_session_file();
      ASSERT_EQ(s["workspaces"][0]["bufferIds"].size(), 2u);
    }

    // 6. Set current buffer — should persist
    err = vxcore_workspace_set_current_buffer(ctx, ws_id, buf1_id);
    ASSERT_EQ(err, VXCORE_OK);
    {
      nlohmann::json s = read_session_file();
      ASSERT_EQ(s["workspaces"][0]["currentBufferId"].get<std::string>(),
                std::string(buf1_id));
    }

    // 7. Set current workspace — should persist
    err = vxcore_workspace_set_current(ctx, ws_id);
    ASSERT_EQ(err, VXCORE_OK);
    {
      nlohmann::json s = read_session_file();
      ASSERT_EQ(s["currentWorkspaceId"].get<std::string>(), std::string(ws_id));
    }

    // 8. Set buffer order — should persist
    std::string order_json = std::string("[\"") + buf2_id + "\",\"" + buf1_id + "\"]";
    err = vxcore_workspace_set_buffer_order(ctx, ws_id, order_json.c_str());
    ASSERT_EQ(err, VXCORE_OK);
    {
      nlohmann::json s = read_session_file();
      ASSERT_EQ(s["workspaces"][0]["bufferIds"][0].get<std::string>(),
                std::string(buf2_id));
      ASSERT_EQ(s["workspaces"][0]["bufferIds"][1].get<std::string>(),
                std::string(buf1_id));
    }

    // 9. Remove buffer from workspace — should persist
    // Note: buf1 is also in no other workspace, so it will be auto-closed
    err = vxcore_workspace_remove_buffer(ctx, ws_id, buf1_id);
    ASSERT_EQ(err, VXCORE_OK);
    {
      nlohmann::json s = read_session_file();
      ASSERT_EQ(s["workspaces"][0]["bufferIds"].size(), 1u);
      // buf1 was orphaned and auto-closed
      ASSERT_EQ(s["buffers"].size(), 1u);
    }

    // 10. Close buffer — should persist
    err = vxcore_buffer_close(ctx, buf2_id);
    ASSERT_EQ(err, VXCORE_OK);
    {
      nlohmann::json s = read_session_file();
      ASSERT_EQ(s["buffers"].size(), 0u);
    }

    // 11. Delete workspace — should persist
    err = vxcore_workspace_delete(ctx, ws_id);
    ASSERT_EQ(err, VXCORE_OK);
    {
      nlohmann::json s = read_session_file();
      ASSERT_EQ(s["workspaces"].size(), 0u);
    }

    vxcore_string_free(buf1_id);
    vxcore_string_free(buf2_id);
    vxcore_string_free(ws_id);
    vxcore_prepare_shutdown(ctx);
    vxcore_context_destroy(ctx);
  }

  cleanup_temp_files();
  std::cout << "  ✓ test_all_mutations_persist passed" << std::endl;
  return 0;
}

int test_remove_buffer_orphan_auto_close() {
  std::cout << "  Running test_remove_buffer_orphan_auto_close..." << std::endl;

  vxcore_clear_test_directory();
  cleanup_temp_files();

  std::string file1 = create_temp_file("orphan.md", "# Orphan Test");

  {
    VxCoreContextHandle ctx = nullptr;
    VxCoreError err = vxcore_context_create(nullptr, &ctx);
    ASSERT_EQ(err, VXCORE_OK);

    // Open buffer
    char *buf_id = nullptr;
    err = vxcore_buffer_open(ctx, nullptr, file1.c_str(), &buf_id);
    ASSERT_EQ(err, VXCORE_OK);
    std::string buf_id_str(buf_id);

    // Create workspace and add buffer
    char *ws_id = nullptr;
    err = vxcore_workspace_create(ctx, "Orphan WS", &ws_id);
    ASSERT_EQ(err, VXCORE_OK);

    err = vxcore_workspace_add_buffer(ctx, ws_id, buf_id);
    ASSERT_EQ(err, VXCORE_OK);

    // Verify buffer exists
    char *buf_json = nullptr;
    err = vxcore_buffer_get(ctx, buf_id, &buf_json);
    ASSERT_EQ(err, VXCORE_OK);
    vxcore_string_free(buf_json);

    // Remove buffer from its only workspace — should auto-close
    err = vxcore_workspace_remove_buffer(ctx, ws_id, buf_id);
    ASSERT_EQ(err, VXCORE_OK);

    // Buffer should be closed (orphaned)
    char *buf_json2 = nullptr;
    err = vxcore_buffer_get(ctx, buf_id_str.c_str(), &buf_json2);
    ASSERT_EQ(err, VXCORE_ERR_BUFFER_NOT_FOUND);

    vxcore_string_free(buf_id);
    vxcore_string_free(ws_id);
    vxcore_prepare_shutdown(ctx);
    vxcore_context_destroy(ctx);
  }

  cleanup_temp_files();
  std::cout << "  ✓ test_remove_buffer_orphan_auto_close passed" << std::endl;
  return 0;
}

int test_remove_buffer_not_orphaned() {
  std::cout << "  Running test_remove_buffer_not_orphaned..." << std::endl;

  vxcore_clear_test_directory();
  cleanup_temp_files();

  std::string file1 = create_temp_file("shared.md", "# Shared Buffer");

  {
    VxCoreContextHandle ctx = nullptr;
    VxCoreError err = vxcore_context_create(nullptr, &ctx);
    ASSERT_EQ(err, VXCORE_OK);

    // Open buffer
    char *buf_id = nullptr;
    err = vxcore_buffer_open(ctx, nullptr, file1.c_str(), &buf_id);
    ASSERT_EQ(err, VXCORE_OK);
    std::string buf_id_str(buf_id);

    // Create two workspaces and add buffer to both
    char *ws1_id = nullptr;
    err = vxcore_workspace_create(ctx, "WS1", &ws1_id);
    ASSERT_EQ(err, VXCORE_OK);

    char *ws2_id = nullptr;
    err = vxcore_workspace_create(ctx, "WS2", &ws2_id);
    ASSERT_EQ(err, VXCORE_OK);

    err = vxcore_workspace_add_buffer(ctx, ws1_id, buf_id);
    ASSERT_EQ(err, VXCORE_OK);
    err = vxcore_workspace_add_buffer(ctx, ws2_id, buf_id);
    ASSERT_EQ(err, VXCORE_OK);

    // Remove buffer from first workspace — still in second, should NOT close
    err = vxcore_workspace_remove_buffer(ctx, ws1_id, buf_id);
    ASSERT_EQ(err, VXCORE_OK);

    // Buffer should still be open
    char *buf_json = nullptr;
    err = vxcore_buffer_get(ctx, buf_id_str.c_str(), &buf_json);
    ASSERT_EQ(err, VXCORE_OK);
    ASSERT_NOT_NULL(buf_json);
    vxcore_string_free(buf_json);

    // Verify it's still in workspace 2
    char *ws2_json = nullptr;
    err = vxcore_workspace_get(ctx, ws2_id, &ws2_json);
    ASSERT_EQ(err, VXCORE_OK);
    std::string ws2_str(ws2_json);
    ASSERT(ws2_str.find(buf_id_str) != std::string::npos);
    vxcore_string_free(ws2_json);

    vxcore_string_free(buf_id);
    vxcore_string_free(ws1_id);
    vxcore_string_free(ws2_id);
    vxcore_prepare_shutdown(ctx);
    vxcore_context_destroy(ctx);
  }

  cleanup_temp_files();
  std::cout << "  ✓ test_remove_buffer_not_orphaned passed" << std::endl;
  return 0;
}

int test_cancel_shutdown_resumes_mutations() {
  std::cout << "  Running test_cancel_shutdown_resumes_mutations..." << std::endl;

  vxcore_clear_test_directory();
  cleanup_temp_files();

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a workspace with a buffer.
  char *ws_id = nullptr;
  err = vxcore_workspace_create(ctx, "Cancel Test WS", &ws_id);
  ASSERT_EQ(err, VXCORE_OK);
  std::string ws_id_str(ws_id);

  // Open a real buffer (create temp file first).
  auto temp_path = create_temp_file("cancel_test.md", "# Cancel test");
  char *buf_id = nullptr;
  err = vxcore_buffer_open(ctx, nullptr, temp_path.c_str(), &buf_id);
  ASSERT_EQ(err, VXCORE_OK);
  std::string buf_id_str(buf_id);

  err = vxcore_workspace_add_buffer(ctx, ws_id, buf_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Prepare shutdown — snapshots session to disk.
  err = vxcore_prepare_shutdown(ctx);
  ASSERT_EQ(err, VXCORE_OK);

  // Cancel shutdown — resume normal operation.
  err = vxcore_cancel_shutdown(ctx);
  ASSERT_EQ(err, VXCORE_OK);

  // After cancel, mutations should sync to session again.
  // Open another buffer.
  auto temp_path2 = create_temp_file("cancel_test2.md", "# Cancel test 2");
  char *buf2_id = nullptr;
  err = vxcore_buffer_open(ctx, nullptr, temp_path2.c_str(), &buf2_id);
  ASSERT_EQ(err, VXCORE_OK);
  std::string buf2_id_str(buf2_id);

  err = vxcore_workspace_add_buffer(ctx, ws_id, buf2_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Read session file — should have 2 buffers in workspace (mutation synced).
  nlohmann::json session = read_session_file();
  ASSERT_EQ(session["workspaces"].size(), 1u);
  auto ws_buffers = session["workspaces"][0]["bufferIds"];
  ASSERT_EQ(ws_buffers.size(), 2u);

  vxcore_string_free(ws_id);
  vxcore_string_free(buf_id);
  vxcore_string_free(buf2_id);
  vxcore_prepare_shutdown(ctx);
  vxcore_context_destroy(ctx);

  cleanup_temp_files();
  std::cout << "  ✓ test_cancel_shutdown_resumes_mutations passed" << std::endl;
  return 0;
}

int test_prepare_shutdown_snapshots_before_buffer_close() {
  std::cout << "  Running test_prepare_shutdown_snapshots_before_buffer_close..." << std::endl;

  vxcore_clear_test_directory();
  cleanup_temp_files();

  std::string ws_id_str;
  std::string buf_a_str, buf_b_str;

  // Phase 1: Set up workspace with 2 buffers, prepare shutdown, then close buffers.
  {
    VxCoreContextHandle ctx = nullptr;
    VxCoreError err = vxcore_context_create(nullptr, &ctx);
    ASSERT_EQ(err, VXCORE_OK);

    char *ws_id = nullptr;
    err = vxcore_workspace_create(ctx, "Snapshot Test", &ws_id);
    ASSERT_EQ(err, VXCORE_OK);
    ws_id_str = std::string(ws_id);

    err = vxcore_workspace_set_current(ctx, ws_id);
    ASSERT_EQ(err, VXCORE_OK);

    auto path_a = create_temp_file("snap_a.md", "# A");
    auto path_b = create_temp_file("snap_b.md", "# B");

    char *buf_a = nullptr;
    err = vxcore_buffer_open(ctx, nullptr, path_a.c_str(), &buf_a);
    ASSERT_EQ(err, VXCORE_OK);
    buf_a_str = std::string(buf_a);

    char *buf_b = nullptr;
    err = vxcore_buffer_open(ctx, nullptr, path_b.c_str(), &buf_b);
    ASSERT_EQ(err, VXCORE_OK);
    buf_b_str = std::string(buf_b);

    err = vxcore_workspace_add_buffer(ctx, ws_id, buf_a);
    ASSERT_EQ(err, VXCORE_OK);
    err = vxcore_workspace_add_buffer(ctx, ws_id, buf_b);
    ASSERT_EQ(err, VXCORE_OK);

    // THIS IS THE KEY: prepare_shutdown snapshots BEFORE buffer close.
    err = vxcore_prepare_shutdown(ctx);
    ASSERT_EQ(err, VXCORE_OK);

    // Now close all buffers (simulating closeAllBuffers in Qt layer).
    // Since shutdown_called is set, destructor won't overwrite session.
    err = vxcore_workspace_remove_buffer(ctx, ws_id, buf_a);
    // In-memory mutation happens but PersistSession is blocked by shutdown_called.

    err = vxcore_workspace_remove_buffer(ctx, ws_id, buf_b);

    vxcore_string_free(ws_id);
    vxcore_string_free(buf_a);
    vxcore_string_free(buf_b);
    vxcore_context_destroy(ctx);
  }

  // Phase 2: Reload and verify the saved session still has workspace with buffers.
  {
    VxCoreContextHandle ctx = nullptr;
    VxCoreError err = vxcore_context_create(nullptr, &ctx);
    ASSERT_EQ(err, VXCORE_OK);

    // The workspace should exist because prepare_shutdown saved it before removal.
    char *ws_json = nullptr;
    err = vxcore_workspace_get(ctx, ws_id_str.c_str(), &ws_json);
    ASSERT_EQ(err, VXCORE_OK);
    ASSERT_NOT_NULL(ws_json);

    nlohmann::json ws = nlohmann::json::parse(ws_json);
    ASSERT_EQ(ws["name"].get<std::string>(), std::string("Snapshot Test"));

    // The workspace should have 2 buffers (snapshot was taken before removal).
    auto buf_ids = ws["bufferIds"];
    ASSERT_EQ(buf_ids.size(), 2u);

    // Verify current workspace was saved.
    char *current_ws = nullptr;
    err = vxcore_workspace_get_current(ctx, &current_ws);
    ASSERT_EQ(err, VXCORE_OK);
    ASSERT_EQ(std::string(current_ws), ws_id_str);

    vxcore_string_free(ws_json);
    vxcore_string_free(current_ws);
    vxcore_prepare_shutdown(ctx);
    vxcore_context_destroy(ctx);
  }

  cleanup_temp_files();
  std::cout << "  ✓ test_prepare_shutdown_snapshots_before_buffer_close passed" << std::endl;
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
  RUN_TEST(test_save_on_create_and_close);
  RUN_TEST(test_shutdown_prevents_destructor_save);
  RUN_TEST(test_workspace_create_and_delete_saves_session);
  RUN_TEST(test_all_mutations_persist);
  RUN_TEST(test_remove_buffer_orphan_auto_close);
  RUN_TEST(test_remove_buffer_not_orphaned);
  RUN_TEST(test_cancel_shutdown_resumes_mutations);
  RUN_TEST(test_prepare_shutdown_snapshots_before_buffer_close);

  std::cout << "All session persistence tests passed!" << std::endl;
  return 0;
}
