#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include "test_utils.h"
#include "vxcore/vxcore.h"

int test_folder_create() {
  std::cout << "  Running test_folder_create..." << std::endl;
  cleanup_test_dir(get_test_path("test_folder_create_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_folder_create_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(notebook_id);

  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "test_folder", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(folder_id);

  ASSERT(path_exists(get_test_path("test_folder_create_nb") + "/test_folder"));
  ASSERT(path_exists(get_test_path("test_folder_create_nb/vx_notebook/contents/test_folder") +
                     "/vx.json"));

  vxcore_string_free(folder_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_folder_create_nb"));
  std::cout << "  âœ“ test_folder_create passed" << std::endl;
  return 0;
}

int test_file_create() {
  std::cout << "  Running test_file_create..." << std::endl;
  cleanup_test_dir(get_test_path("test_file_create_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_file_create_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "note.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(file_id);

  char *config_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, ".", &config_json);
  ASSERT_EQ(err, VXCORE_OK);

  nlohmann::json config = nlohmann::json::parse(config_json);
  ASSERT(config.contains("files"));
  ASSERT(config["files"].size() == 1);
  ASSERT(config["files"][0]["name"] == "note.md");

  vxcore_string_free(config_json);
  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_file_create_nb"));
  std::cout << "  âœ“ test_file_create passed" << std::endl;
  return 0;
}

int test_file_metadata_and_tags() {
  std::cout << "  Running test_file_metadata_and_tags..." << std::endl;
  cleanup_test_dir(get_test_path("test_file_meta_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_file_meta_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "note.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  err = vxcore_node_update_metadata(ctx, notebook_id, "note.md",
                                    R"({"author": "John Doe", "priority": "high"})");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_tag_create(ctx, notebook_id, "work");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_tag_create(ctx, notebook_id, "urgent");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_update_tags(ctx, notebook_id, "note.md", R"(["work", "urgent"])");
  ASSERT_EQ(err, VXCORE_OK);

  char *config_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, ".", &config_json);
  ASSERT_EQ(err, VXCORE_OK);

  nlohmann::json config = nlohmann::json::parse(config_json);
  ASSERT(config["files"][0]["metadata"]["author"] == "John Doe");
  ASSERT(config["files"][0]["tags"].size() == 2);

  vxcore_string_free(config_json);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_file_meta_nb"));
  std::cout << "  âœ“ test_file_metadata_and_tags passed" << std::endl;
  return 0;
}

int test_folder_delete() {
  std::cout << "  Running test_folder_delete..." << std::endl;
  cleanup_test_dir(get_test_path("test_folder_delete_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_folder_delete_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "to_delete", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  ASSERT(path_exists(get_test_path("test_folder_delete_nb") + "/to_delete"));

  err = vxcore_node_delete(ctx, notebook_id, "to_delete");
  ASSERT_EQ(err, VXCORE_OK);

  ASSERT(!path_exists(get_test_path("test_folder_delete_nb") + "/to_delete"));

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_folder_delete_nb"));
  std::cout << "  âœ“ test_folder_delete passed" << std::endl;
  return 0;
}

int test_folder_create_duplicate() {
  std::cout << "  Running test_folder_create_duplicate..." << std::endl;
  cleanup_test_dir(get_test_path("test_folder_dup_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_folder_dup_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "duplicate", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  char *folder_id2 = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "duplicate", &folder_id2);
  ASSERT_EQ(err, VXCORE_ERR_ALREADY_EXISTS);
  ASSERT_NULL(folder_id2);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_folder_dup_nb"));
  std::cout << "  âœ“ test_folder_create_duplicate passed" << std::endl;
  return 0;
}

int test_file_create_duplicate() {
  std::cout << "  Running test_file_create_duplicate..." << std::endl;
  cleanup_test_dir(get_test_path("test_file_dup_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_file_dup_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "duplicate.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  char *file_id2 = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "duplicate.md", &file_id2);
  ASSERT_EQ(err, VXCORE_ERR_ALREADY_EXISTS);
  ASSERT_NULL(file_id2);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_file_dup_nb"));
  std::cout << "  âœ“ test_file_create_duplicate passed" << std::endl;
  return 0;
}

int test_folder_invalid_params() {
  std::cout << "  Running test_folder_invalid_params..." << std::endl;
  cleanup_test_dir(get_test_path("test_folder_invalid_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_folder_invalid_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *folder_id = nullptr;
  err = vxcore_folder_create(nullptr, notebook_id, ".", "test", &folder_id);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = vxcore_folder_create(ctx, nullptr, ".", "test", &folder_id);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = vxcore_folder_create(ctx, notebook_id, ".", nullptr, &folder_id);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = vxcore_folder_create(ctx, notebook_id, ".", "test", nullptr);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_folder_invalid_nb"));
  std::cout << "  âœ“ test_folder_invalid_params passed" << std::endl;
  return 0;
}

int test_file_invalid_params() {
  std::cout << "  Running test_file_invalid_params..." << std::endl;
  cleanup_test_dir(get_test_path("test_file_invalid_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_file_invalid_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(nullptr, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = vxcore_file_create(ctx, nullptr, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = vxcore_file_create(ctx, notebook_id, nullptr, "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = vxcore_file_create(ctx, notebook_id, ".", nullptr, &file_id);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", nullptr);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_file_invalid_nb"));
  std::cout << "  âœ“ test_file_invalid_params passed" << std::endl;
  return 0;
}

int test_folder_not_found() {
  std::cout << "  Running test_folder_not_found..." << std::endl;
  cleanup_test_dir(get_test_path("test_folder_notfound_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_folder_notfound_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_node_delete(ctx, notebook_id, "nonexistent");
  ASSERT_EQ(err, VXCORE_ERR_NOT_FOUND);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_folder_notfound_nb"));
  std::cout << "  âœ“ test_folder_not_found passed" << std::endl;
  return 0;
}

int test_file_not_found() {
  std::cout << "  Running test_file_not_found..." << std::endl;
  cleanup_test_dir(get_test_path("test_file_notfound_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_file_notfound_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_node_delete(ctx, notebook_id, "nonexistent.md");
  ASSERT_EQ(err, VXCORE_ERR_NOT_FOUND);

  err = vxcore_node_update_metadata(ctx, notebook_id, "nonexistent.md", "{}");
  ASSERT_EQ(err, VXCORE_ERR_NOT_FOUND);

  err = vxcore_file_update_tags(ctx, notebook_id, "nonexistent.md", "[]");
  ASSERT_EQ(err, VXCORE_ERR_NOT_FOUND);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_file_notfound_nb"));
  std::cout << "  âœ“ test_file_not_found passed" << std::endl;
  return 0;
}

int test_invalid_json() {
  std::cout << "  Running test_invalid_json..." << std::endl;
  cleanup_test_dir(get_test_path("test_invalid_json_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_invalid_json_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  err = vxcore_node_update_metadata(ctx, notebook_id, "test.md", "invalid json");
  ASSERT_EQ(err, VXCORE_ERR_JSON_PARSE);

  err = vxcore_node_update_metadata(ctx, notebook_id, "test.md", "[]");
  ASSERT_EQ(err, VXCORE_ERR_JSON_PARSE);

  err = vxcore_file_update_tags(ctx, notebook_id, "test.md", "not an array");
  ASSERT_EQ(err, VXCORE_ERR_JSON_PARSE);

  err = vxcore_file_update_tags(ctx, notebook_id, "test.md", "{}");
  ASSERT_EQ(err, VXCORE_ERR_JSON_PARSE);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_invalid_json_nb"));
  std::cout << "  âœ“ test_invalid_json passed" << std::endl;
  return 0;
}

int test_notebook_not_found() {
  std::cout << "  Running test_notebook_not_found..." << std::endl;
  cleanup_test_dir(get_test_path("test_nb_notfound"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, "nonexistent_nb", ".", "test", &folder_id);
  ASSERT_EQ(err, VXCORE_ERR_NOT_FOUND);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, "nonexistent_nb", ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_ERR_NOT_FOUND);

  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_nb_notfound"));
  std::cout << "  âœ“ test_notebook_not_found passed" << std::endl;
  return 0;
}

int test_file_delete() {
  std::cout << "  Running test_file_delete..." << std::endl;
  cleanup_test_dir(get_test_path("test_file_delete_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_file_delete_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "to_delete.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  ASSERT(path_exists(get_test_path("test_file_delete_nb") + "/to_delete.md"));

  err = vxcore_node_delete(ctx, notebook_id, "to_delete.md");
  ASSERT_EQ(err, VXCORE_OK);

  ASSERT(!path_exists(get_test_path("test_file_delete_nb") + "/to_delete.md"));

  char *config_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, ".", &config_json);
  ASSERT_EQ(err, VXCORE_OK);

  nlohmann::json config = nlohmann::json::parse(config_json);
  ASSERT(config["files"].size() == 0);

  vxcore_string_free(config_json);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_file_delete_nb"));
  std::cout << "  âœ“ test_file_delete passed" << std::endl;
  return 0;
}

int test_nested_folder_operations() {
  std::cout << "  Running test_nested_folder_operations..." << std::endl;
  cleanup_test_dir(get_test_path("test_nested_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_nested_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *folder1_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "parent", &folder1_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder1_id);

  char *folder2_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, "parent", "child", &folder2_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder2_id);

  ASSERT(path_exists(get_test_path("test_nested_nb") + "/parent"));
  ASSERT(path_exists(get_test_path("test_nested_nb/parent") + "/child"));

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, "parent/child", "nested.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  ASSERT(path_exists(get_test_path("test_nested_nb/parent/child") + "/nested.md"));

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_nested_nb"));
  std::cout << "  âœ“ test_nested_folder_operations passed" << std::endl;
  return 0;
}

int test_folder_rename() {
  std::cout << "  Running test_folder_rename..." << std::endl;
  cleanup_test_dir(get_test_path("test_folder_rename_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_folder_rename_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "old_name", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  ASSERT(path_exists(get_test_path("test_folder_rename_nb") + "/old_name"));

  err = vxcore_node_rename(ctx, notebook_id, "old_name", "new_name");
  ASSERT_EQ(err, VXCORE_OK);

  ASSERT(!path_exists(get_test_path("test_folder_rename_nb") + "/old_name"));
  ASSERT(path_exists(get_test_path("test_folder_rename_nb") + "/new_name"));

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_folder_rename_nb"));
  std::cout << "  âœ“ test_folder_rename passed" << std::endl;
  return 0;
}

int test_folder_move() {
  std::cout << "  Running test_folder_move..." << std::endl;
  cleanup_test_dir(get_test_path("test_folder_move_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_folder_move_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "source", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  err = vxcore_folder_create(ctx, notebook_id, ".", "dest", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  err = vxcore_node_move(ctx, notebook_id, "source", "dest");
  ASSERT_EQ(err, VXCORE_OK);

  ASSERT(!path_exists(get_test_path("test_folder_move_nb") + "/source"));
  ASSERT(path_exists(get_test_path("test_folder_move_nb/dest") + "/source"));

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_folder_move_nb"));
  std::cout << "  âœ“ test_folder_move passed" << std::endl;
  return 0;
}

int test_folder_copy() {
  std::cout << "  Running test_folder_copy..." << std::endl;
  cleanup_test_dir(get_test_path("test_folder_copy_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_folder_copy_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "original", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, "original", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  char *copied_folder_id = nullptr;
  err = vxcore_node_copy(ctx, notebook_id, "original", ".", "copy", &copied_folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT(copied_folder_id != nullptr);
  vxcore_string_free(copied_folder_id);

  ASSERT(path_exists(get_test_path("test_folder_copy_nb") + "/original"));
  ASSERT(path_exists(get_test_path("test_folder_copy_nb") + "/copy"));
  ASSERT(path_exists(get_test_path("test_folder_copy_nb/copy") + "/test.md"));

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_folder_copy_nb"));
  std::cout << "  âœ“ test_folder_copy passed" << std::endl;
  return 0;
}

int test_file_rename() {
  std::cout << "  Running test_file_rename..." << std::endl;
  cleanup_test_dir(get_test_path("test_file_rename_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_file_rename_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "old.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  err = vxcore_node_rename(ctx, notebook_id, "old.md", "new.md");
  ASSERT_EQ(err, VXCORE_OK);

  ASSERT(!path_exists(get_test_path("test_file_rename_nb") + "/old.md"));
  ASSERT(path_exists(get_test_path("test_file_rename_nb") + "/new.md"));

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_file_rename_nb"));
  std::cout << "  âœ“ test_file_rename passed" << std::endl;
  return 0;
}

int test_file_move() {
  std::cout << "  Running test_file_move..." << std::endl;
  cleanup_test_dir(get_test_path("test_file_move_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_file_move_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "file.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "dest", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  err = vxcore_node_move(ctx, notebook_id, "file.md", "dest");
  ASSERT_EQ(err, VXCORE_OK);

  ASSERT(!path_exists(get_test_path("test_file_move_nb") + "/file.md"));
  ASSERT(path_exists(get_test_path("test_file_move_nb/dest") + "/file.md"));

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_file_move_nb"));
  std::cout << "  âœ“ test_file_move passed" << std::endl;
  return 0;
}

int test_file_copy() {
  std::cout << "  Running test_file_copy..." << std::endl;
  cleanup_test_dir(get_test_path("test_file_copy_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_file_copy_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "original.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  char *copied_file_id = nullptr;
  err = vxcore_node_copy(ctx, notebook_id, "original.md", ".", "copy.md", &copied_file_id);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT(copied_file_id != nullptr);
  vxcore_string_free(copied_file_id);

  ASSERT(path_exists(get_test_path("test_file_copy_nb") + "/original.md"));
  ASSERT(path_exists(get_test_path("test_file_copy_nb") + "/copy.md"));

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_file_copy_nb"));
  std::cout << "  âœ“ test_file_copy passed" << std::endl;
  return 0;
}

int test_file_get_info() {
  std::cout << "  Running test_file_get_info..." << std::endl;
  cleanup_test_dir(get_test_path("test_file_info_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_file_info_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  err = vxcore_node_update_metadata(ctx, notebook_id, "test.md", R"({"key": "value"})");
  ASSERT_EQ(err, VXCORE_OK);

  char *info_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, "test.md", &info_json);
  ASSERT_EQ(err, VXCORE_OK);

  nlohmann::json info = nlohmann::json::parse(info_json);
  ASSERT(info["name"] == "test.md");
  ASSERT(info["metadata"]["key"] == "value");

  vxcore_string_free(info_json);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_file_info_nb"));
  std::cout << "  âœ“ test_file_get_info passed" << std::endl;
  return 0;
}

int test_folder_get_metadata() {
  std::cout << "  Running test_folder_get_metadata..." << std::endl;
  cleanup_test_dir(get_test_path("test_folder_meta_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_folder_meta_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "test", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  err = vxcore_node_update_metadata(ctx, notebook_id, "test", R"({"color": "blue"})");
  ASSERT_EQ(err, VXCORE_OK);

  char *metadata_json = nullptr;
  err = vxcore_node_get_metadata(ctx, notebook_id, "test", &metadata_json);
  ASSERT_EQ(err, VXCORE_OK);

  nlohmann::json metadata = nlohmann::json::parse(metadata_json);
  ASSERT(metadata["color"] == "blue");

  vxcore_string_free(metadata_json);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_folder_meta_nb"));
  std::cout << "  âœ“ test_folder_get_metadata passed" << std::endl;
  return 0;
}

int test_folder_copy_invalid_params() {
  std::cout << "  Running test_folder_copy_invalid_params..." << std::endl;
  cleanup_test_dir(get_test_path("test_fcopy_inv_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_fcopy_inv_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "original", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  err = vxcore_node_copy(ctx, notebook_id, "original", ".", "copy", nullptr);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_fcopy_inv_nb"));
  std::cout << "  âœ“ test_folder_copy_invalid_params passed" << std::endl;
  return 0;
}

int test_folder_copy_not_found() {
  std::cout << "  Running test_folder_copy_not_found..." << std::endl;
  cleanup_test_dir(get_test_path("test_fcopy_nf_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_fcopy_nf_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *copied_folder_id = nullptr;
  err = vxcore_node_copy(ctx, notebook_id, "nonexistent", ".", "copy", &copied_folder_id);
  ASSERT_EQ(err, VXCORE_ERR_NOT_FOUND);
  ASSERT(copied_folder_id == nullptr);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_fcopy_nf_nb"));
  std::cout << "  âœ“ test_folder_copy_not_found passed" << std::endl;
  return 0;
}

int test_folder_copy_already_exists() {
  std::cout << "  Running test_folder_copy_already_exists..." << std::endl;
  cleanup_test_dir(get_test_path("test_fcopy_exists_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_fcopy_exists_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "original", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  err = vxcore_folder_create(ctx, notebook_id, ".", "existing", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  char *copied_folder_id = nullptr;
  err = vxcore_node_copy(ctx, notebook_id, "original", ".", "existing", &copied_folder_id);
  ASSERT_EQ(err, VXCORE_ERR_ALREADY_EXISTS);
  ASSERT(copied_folder_id == nullptr);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_fcopy_exists_nb"));
  std::cout << "  âœ“ test_folder_copy_already_exists passed" << std::endl;
  return 0;
}

int test_file_copy_invalid_params() {
  std::cout << "  Running test_file_copy_invalid_params..." << std::endl;
  cleanup_test_dir(get_test_path("test_filecopy_inv_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_filecopy_inv_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "original.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  err = vxcore_node_copy(ctx, notebook_id, "original.md", ".", "copy.md", nullptr);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_filecopy_inv_nb"));
  std::cout << "  âœ“ test_file_copy_invalid_params passed" << std::endl;
  return 0;
}

int test_file_copy_not_found() {
  std::cout << "  Running test_file_copy_not_found..." << std::endl;
  cleanup_test_dir(get_test_path("test_filecopy_nf_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_filecopy_nf_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *copied_file_id = nullptr;
  err = vxcore_node_copy(ctx, notebook_id, "nonexistent.md", ".", "copy.md", &copied_file_id);
  ASSERT_EQ(err, VXCORE_ERR_NOT_FOUND);
  ASSERT(copied_file_id == nullptr);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_filecopy_nf_nb"));
  std::cout << "  âœ“ test_file_copy_not_found passed" << std::endl;
  return 0;
}

int test_file_copy_already_exists() {
  std::cout << "  Running test_file_copy_already_exists..." << std::endl;
  cleanup_test_dir(get_test_path("test_filecopy_exists_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_filecopy_exists_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "original.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  err = vxcore_file_create(ctx, notebook_id, ".", "existing.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  char *copied_file_id = nullptr;
  err = vxcore_node_copy(ctx, notebook_id, "original.md", ".", "existing.md", &copied_file_id);
  ASSERT_EQ(err, VXCORE_ERR_ALREADY_EXISTS);
  ASSERT(copied_file_id == nullptr);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_filecopy_exists_nb"));
  std::cout << "  âœ“ test_file_copy_already_exists passed" << std::endl;
  return 0;
}

int test_file_tag() {
  std::cout << "  Running test_file_tag..." << std::endl;
  cleanup_test_dir(get_test_path("test_file_tag_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_file_tag_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "note.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  err = vxcore_tag_create(ctx, notebook_id, "work");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_tag_create(ctx, notebook_id, "urgent");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "note.md", "work");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "note.md", "urgent");
  ASSERT_EQ(err, VXCORE_OK);

  char *config_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, ".", &config_json);
  ASSERT_EQ(err, VXCORE_OK);

  nlohmann::json config = nlohmann::json::parse(config_json);
  ASSERT(config["files"][0]["tags"].size() == 2);
  ASSERT(config["files"][0]["tags"][0] == "work");
  ASSERT(config["files"][0]["tags"][1] == "urgent");

  vxcore_string_free(config_json);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_file_tag_nb"));
  std::cout << "  âœ“ test_file_tag passed" << std::endl;
  return 0;
}

int test_file_untag() {
  std::cout << "  Running test_file_untag..." << std::endl;
  cleanup_test_dir(get_test_path("test_file_untag_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_file_untag_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "note.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  err = vxcore_tag_create(ctx, notebook_id, "work");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_tag_create(ctx, notebook_id, "urgent");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_tag_create(ctx, notebook_id, "personal");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "note.md", "work");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "note.md", "urgent");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "note.md", "personal");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_untag(ctx, notebook_id, "note.md", "urgent");
  ASSERT_EQ(err, VXCORE_OK);

  char *config_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, ".", &config_json);
  ASSERT_EQ(err, VXCORE_OK);

  nlohmann::json config = nlohmann::json::parse(config_json);
  ASSERT(config["files"][0]["tags"].size() == 2);
  ASSERT(config["files"][0]["tags"][0] == "work");
  ASSERT(config["files"][0]["tags"][1] == "personal");

  vxcore_string_free(config_json);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_file_untag_nb"));
  std::cout << "  âœ“ test_file_untag passed" << std::endl;
  return 0;
}

int test_file_tag_duplicate() {
  std::cout << "  Running test_file_tag_duplicate..." << std::endl;
  cleanup_test_dir(get_test_path("test_file_tag_dup_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_file_tag_dup_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "note.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  err = vxcore_tag_create(ctx, notebook_id, "work");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "note.md", "work");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "note.md", "work");
  ASSERT_EQ(err, VXCORE_ERR_ALREADY_EXISTS);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_file_tag_dup_nb"));
  std::cout << "  âœ“ test_file_tag_duplicate passed" << std::endl;
  return 0;
}

int test_file_untag_not_found() {
  std::cout << "  Running test_file_untag_not_found..." << std::endl;
  cleanup_test_dir(get_test_path("test_file_untag_nf_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_file_untag_nf_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "note.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  err = vxcore_file_untag(ctx, notebook_id, "note.md", "nonexistent");
  ASSERT_EQ(err, VXCORE_ERR_NOT_FOUND);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_file_untag_nf_nb"));
  std::cout << "  âœ“ test_file_untag_not_found passed" << std::endl;
  return 0;
}

int test_file_tag_invalid_params() {
  std::cout << "  Running test_file_tag_invalid_params..." << std::endl;
  cleanup_test_dir(get_test_path("test_file_tag_inv_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_file_tag_inv_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(nullptr, notebook_id, "note.md", "work");
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = vxcore_file_tag(ctx, nullptr, "note.md", "work");
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = vxcore_file_tag(ctx, notebook_id, nullptr, "work");
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = vxcore_file_tag(ctx, notebook_id, "note.md", nullptr);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = vxcore_file_untag(nullptr, notebook_id, "note.md", "work");
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = vxcore_file_untag(ctx, nullptr, "note.md", "work");
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = vxcore_file_untag(ctx, notebook_id, nullptr, "work");
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = vxcore_file_untag(ctx, notebook_id, "note.md", nullptr);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_file_tag_inv_nb"));
  std::cout << "  âœ“ test_file_tag_invalid_params passed" << std::endl;
  return 0;
}

int test_folder_create_path_basic() {
  std::cout << "  Running test_folder_create_path_basic..." << std::endl;
  cleanup_test_dir(get_test_path("test_folder_path_basic_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_folder_path_basic_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *folder_id = nullptr;
  err = vxcore_folder_create_path(ctx, notebook_id, "level1/level2/level3", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(folder_id);

  ASSERT(path_exists(get_test_path("test_folder_path_basic_nb") + "/level1"));
  ASSERT(path_exists(get_test_path("test_folder_path_basic_nb/level1") + "/level2"));
  ASSERT(path_exists(get_test_path("test_folder_path_basic_nb/level1/level2") + "/level3"));

  vxcore_string_free(folder_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_folder_path_basic_nb"));
  std::cout << "  âœ“ test_folder_create_path_basic passed" << std::endl;
  return 0;
}

int test_folder_create_path_single() {
  std::cout << "  Running test_folder_create_path_single..." << std::endl;
  cleanup_test_dir(get_test_path("test_folder_path_single_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_folder_path_single_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *folder_id = nullptr;
  err = vxcore_folder_create_path(ctx, notebook_id, "single_folder", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(folder_id);

  ASSERT(path_exists(get_test_path("test_folder_path_single_nb") + "/single_folder"));

  vxcore_string_free(folder_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_folder_path_single_nb"));
  std::cout << "  âœ“ test_folder_create_path_single passed" << std::endl;
  return 0;
}

int test_folder_create_path_partial_exists() {
  std::cout << "  Running test_folder_create_path_partial_exists..." << std::endl;
  cleanup_test_dir(get_test_path("test_folder_path_partial_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_folder_path_partial_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "existing", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  err = vxcore_folder_create_path(ctx, notebook_id, "existing/new1/new2", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(folder_id);

  ASSERT(path_exists(get_test_path("test_folder_path_partial_nb") + "/existing"));
  ASSERT(path_exists(get_test_path("test_folder_path_partial_nb/existing") + "/new1"));
  ASSERT(path_exists(get_test_path("test_folder_path_partial_nb/existing/new1") + "/new2"));

  vxcore_string_free(folder_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_folder_path_partial_nb"));
  std::cout << "  âœ“ test_folder_create_path_partial_exists passed" << std::endl;
  return 0;
}

int test_folder_create_path_empty() {
  std::cout << "  Running test_folder_create_path_empty..." << std::endl;
  cleanup_test_dir(get_test_path("test_folder_path_empty_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_folder_path_empty_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *folder_id = nullptr;
  err = vxcore_folder_create_path(ctx, notebook_id, "", &folder_id);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);
  ASSERT_NULL(folder_id);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_folder_path_empty_nb"));
  std::cout << "  âœ“ test_folder_create_path_empty passed" << std::endl;
  return 0;
}

int test_folder_create_path_trailing_slash() {
  std::cout << "  Running test_folder_create_path_trailing_slash..." << std::endl;
  cleanup_test_dir(get_test_path("test_folder_path_slash_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_folder_path_slash_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *folder_id = nullptr;
  err = vxcore_folder_create_path(ctx, notebook_id, "path1/path2/", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(folder_id);

  ASSERT(path_exists(get_test_path("test_folder_path_slash_nb") + "/path1"));
  ASSERT(path_exists(get_test_path("test_folder_path_slash_nb/path1") + "/path2"));

  vxcore_string_free(folder_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_folder_path_slash_nb"));
  std::cout << "  âœ“ test_folder_create_path_trailing_slash passed" << std::endl;
  return 0;
}

// ============================================================================
// MetadataStore Sync Integration Tests
// ============================================================================

// Test that metadata store syncs correctly when notebook is reopened.
// Note: With lazy sync architecture, data is synced to MetadataStore on-demand
// when folders/files are accessed, not eagerly on notebook open.
int test_metadata_store_sync_on_reopen() {
  std::cout << "  Running test_metadata_store_sync_on_reopen..." << std::endl;
  cleanup_test_dir(get_test_path("test_ms_sync_reopen_nb"));

  // Create notebook with folders and files
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_ms_sync_reopen_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create folder hierarchy
  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "docs", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  err = vxcore_folder_create(ctx, notebook_id, "docs", "guides", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  // Create files with tags
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "readme.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  err = vxcore_file_create(ctx, notebook_id, "docs", "intro.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  err = vxcore_file_create(ctx, notebook_id, "docs/guides", "setup.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  // Add tags
  err = vxcore_tag_create(ctx, notebook_id, "important");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "readme.md", "important");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "docs/intro.md", "important");
  ASSERT_EQ(err, VXCORE_OK);

  // Close notebook
  err = vxcore_notebook_close(ctx, notebook_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(notebook_id);
  notebook_id = nullptr;

  // Reopen notebook - lazy sync will populate MetadataStore when folders are accessed
  err = vxcore_notebook_open(ctx, get_test_path("test_ms_sync_reopen_nb").c_str(), &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(notebook_id);

  // Verify all data is accessible (indirectly tests that sync worked)
  char *config_json = nullptr;

  // Check root folder config
  err = vxcore_node_get_config(ctx, notebook_id, ".", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json root_config = nlohmann::json::parse(config_json);
  ASSERT(root_config["folders"].size() == 1);  // docs
  ASSERT(root_config["files"].size() == 1);    // readme.md
  vxcore_string_free(config_json);

  // Check nested folder config
  err = vxcore_node_get_config(ctx, notebook_id, "docs", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json docs_config = nlohmann::json::parse(config_json);
  ASSERT(docs_config["folders"].size() == 1);  // guides
  ASSERT(docs_config["files"].size() == 1);    // intro.md
  vxcore_string_free(config_json);

  // Check deeply nested folder config
  err = vxcore_node_get_config(ctx, notebook_id, "docs/guides", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json guides_config = nlohmann::json::parse(config_json);
  ASSERT(guides_config["files"].size() == 1);  // setup.md
  vxcore_string_free(config_json);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_ms_sync_reopen_nb"));
  std::cout << "  âœ“ test_metadata_store_sync_on_reopen passed" << std::endl;
  return 0;
}

// Test write-through behavior: operations update both config files and metadata store
int test_metadata_store_write_through() {
  std::cout << "  Running test_metadata_store_write_through..." << std::endl;
  cleanup_test_dir(get_test_path("test_ms_write_through_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_ms_write_through_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create folder
  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "notes", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  // Create file
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, "notes", "test.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  // Update file metadata
  err = vxcore_node_update_metadata(ctx, notebook_id, "notes/test.md",
                                    R"({"author": "test", "version": 1})");
  ASSERT_EQ(err, VXCORE_OK);

  // Create and apply tag
  err = vxcore_tag_create(ctx, notebook_id, "work");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "notes/test.md", "work");
  ASSERT_EQ(err, VXCORE_OK);

  // Rename file
  err = vxcore_node_rename(ctx, notebook_id, "notes/test.md", "renamed.md");
  ASSERT_EQ(err, VXCORE_OK);

  // Move file to root
  err = vxcore_node_move(ctx, notebook_id, "notes/renamed.md", ".");
  ASSERT_EQ(err, VXCORE_OK);

  // Verify file in root with all its attributes
  char *info_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, "renamed.md", &info_json);
  ASSERT_EQ(err, VXCORE_OK);

  nlohmann::json info = nlohmann::json::parse(info_json);
  ASSERT(info["name"] == "renamed.md");
  ASSERT(info["metadata"]["author"] == "test");
  ASSERT(info["tags"].size() == 1);
  ASSERT(info["tags"][0] == "work");
  vxcore_string_free(info_json);

  // Rename folder
  err = vxcore_node_rename(ctx, notebook_id, "notes", "documents");
  ASSERT_EQ(err, VXCORE_OK);

  // Verify folder exists with new name
  ASSERT(!path_exists(get_test_path("test_ms_write_through_nb") + "/notes"));
  ASSERT(path_exists(get_test_path("test_ms_write_through_nb") + "/documents"));

  // Delete file
  err = vxcore_node_delete(ctx, notebook_id, "renamed.md");
  ASSERT_EQ(err, VXCORE_OK);

  // Delete folder
  err = vxcore_node_delete(ctx, notebook_id, "documents");
  ASSERT_EQ(err, VXCORE_OK);

  // Verify root is empty (except for vx_notebook metadata folder)
  char *config_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, ".", &config_json);
  ASSERT_EQ(err, VXCORE_OK);

  nlohmann::json config = nlohmann::json::parse(config_json);
  ASSERT(config["folders"].empty());
  ASSERT(config["files"].empty());
  vxcore_string_free(config_json);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_ms_write_through_nb"));
  std::cout << "  âœ“ test_metadata_store_write_through passed" << std::endl;
  return 0;
}

// Test that tags are properly synced through copy operations
int test_metadata_store_copy_preserves_data() {
  std::cout << "  Running test_metadata_store_copy_preserves_data..." << std::endl;
  cleanup_test_dir(get_test_path("test_ms_copy_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_ms_copy_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create source folder with file
  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "source", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, "source", "original.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  // Set metadata and tags on original file
  err = vxcore_node_update_metadata(ctx, notebook_id, "source/original.md",
                                    R"({"priority": "high"})");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_tag_create(ctx, notebook_id, "important");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "source/original.md", "important");
  ASSERT_EQ(err, VXCORE_OK);

  // Copy file
  char *copied_file_id = nullptr;
  err = vxcore_node_copy(ctx, notebook_id, "source/original.md", ".", "copy.md", &copied_file_id);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(copied_file_id);
  vxcore_string_free(copied_file_id);

  // Verify copied file has same metadata and tags
  char *info_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, "copy.md", &info_json);
  ASSERT_EQ(err, VXCORE_OK);

  nlohmann::json info = nlohmann::json::parse(info_json);
  ASSERT(info["name"] == "copy.md");
  ASSERT(info["metadata"]["priority"] == "high");
  ASSERT(info["tags"].size() == 1);
  ASSERT(info["tags"][0] == "important");
  vxcore_string_free(info_json);

  // Copy folder
  char *copied_folder_id = nullptr;
  err = vxcore_node_copy(ctx, notebook_id, "source", ".", "source_copy", &copied_folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(copied_folder_id);
  vxcore_string_free(copied_folder_id);

  // Verify copied folder's file has metadata and tags
  err = vxcore_node_get_config(ctx, notebook_id, "source_copy/original.md", &info_json);
  ASSERT_EQ(err, VXCORE_OK);

  info = nlohmann::json::parse(info_json);
  ASSERT(info["metadata"]["priority"] == "high");
  ASSERT(info["tags"].size() == 1);
  ASSERT(info["tags"][0] == "important");
  vxcore_string_free(info_json);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_ms_copy_nb"));
  std::cout << "  âœ“ test_metadata_store_copy_preserves_data passed" << std::endl;
  return 0;
}

// Test that recursive folder sync works correctly.
// When accessing a parent folder, all subfolders should be synced to MetadataStore.
int test_sync_recursive_folders() {
  std::cout << "  Running test_sync_recursive_folders..." << std::endl;
  cleanup_test_dir(get_test_path("test_sync_recursive_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_sync_recursive_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create nested folder structure: root -> level1 -> level2 -> level3
  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "level1", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  err = vxcore_folder_create(ctx, notebook_id, "level1", "level2", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  err = vxcore_folder_create(ctx, notebook_id, "level1/level2", "level3", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  // Create files at each level
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, "level1", "file1.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  err = vxcore_file_create(ctx, notebook_id, "level1/level2", "file2.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  err = vxcore_file_create(ctx, notebook_id, "level1/level2/level3", "file3.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  // Close and reopen to clear in-memory state
  err = vxcore_notebook_close(ctx, notebook_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(notebook_id);
  notebook_id = nullptr;

  err = vxcore_notebook_open(ctx, get_test_path("test_sync_recursive_nb").c_str(), &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Access root folder - this triggers lazy sync which should recursively sync all subfolders
  char *config_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, ".", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(config_json);

  // Verify level1 is accessible and has correct content
  err = vxcore_node_get_config(ctx, notebook_id, "level1", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json level1_config = nlohmann::json::parse(config_json);
  ASSERT(level1_config["folders"].size() == 1);  // level2
  ASSERT(level1_config["files"].size() == 1);    // file1.md
  vxcore_string_free(config_json);

  // Verify level2 is accessible
  err = vxcore_node_get_config(ctx, notebook_id, "level1/level2", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json level2_config = nlohmann::json::parse(config_json);
  ASSERT(level2_config["folders"].size() == 1);  // level3
  ASSERT(level2_config["files"].size() == 1);    // file2.md
  vxcore_string_free(config_json);

  // Verify level3 is accessible
  err = vxcore_node_get_config(ctx, notebook_id, "level1/level2/level3", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json level3_config = nlohmann::json::parse(config_json);
  ASSERT(level3_config["files"].size() == 1);  // file3.md
  vxcore_string_free(config_json);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_sync_recursive_nb"));
  std::cout << "  ✓ test_sync_recursive_folders passed" << std::endl;
  return 0;
}

// Test that orphan files are deleted from store when they no longer exist in config.
// This simulates external modification of config files.
int test_sync_orphan_file_deletion() {
  std::cout << "  Running test_sync_orphan_file_deletion..." << std::endl;
  cleanup_test_dir(get_test_path("test_sync_orphan_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_sync_orphan_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create two files
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "keep.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  err = vxcore_file_create(ctx, notebook_id, ".", "delete.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  // Verify both files exist
  char *config_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, ".", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json config = nlohmann::json::parse(config_json);
  ASSERT(config["files"].size() == 2);
  vxcore_string_free(config_json);

  // Close notebook
  err = vxcore_notebook_close(ctx, notebook_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(notebook_id);
  notebook_id = nullptr;

  // Manually edit the vx.json to remove "delete.md" (simulating external modification)
  std::string vx_json_path = get_test_path("test_sync_orphan_nb") + "/vx_notebook/contents/vx.json";
  std::ifstream in_file(vx_json_path);
  nlohmann::json vx_json;
  in_file >> vx_json;
  in_file.close();

  // Remove delete.md from files array
  nlohmann::json new_files = nlohmann::json::array();
  for (const auto &file : vx_json["files"]) {
    if (file["name"] != "delete.md") {
      new_files.push_back(file);
    }
  }
  vx_json["files"] = new_files;
  // Update modified timestamp to trigger staleness check
  vx_json["modifiedUtc"] = vx_json["modifiedUtc"].get<int64_t>() + 1000;

  std::ofstream out_file(vx_json_path);
  out_file << vx_json.dump(2);
  out_file.close();

  // Reopen notebook - sync should detect staleness and remove orphan from store
  err = vxcore_notebook_open(ctx, get_test_path("test_sync_orphan_nb").c_str(), &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Access folder to trigger sync
  err = vxcore_node_get_config(ctx, notebook_id, ".", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  config = nlohmann::json::parse(config_json);

  // Verify only "keep.md" remains
  ASSERT(config["files"].size() == 1);
  ASSERT(config["files"][0]["name"] == "keep.md");
  vxcore_string_free(config_json);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_sync_orphan_nb"));
  std::cout << "  ✓ test_sync_orphan_file_deletion passed" << std::endl;
  return 0;
}

// Test that file metadata updates are synced correctly when config changes externally.
int test_sync_file_metadata_update() {
  std::cout << "  Running test_sync_file_metadata_update..." << std::endl;
  cleanup_test_dir(get_test_path("test_sync_file_update_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_sync_file_update_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a file with initial metadata
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "note.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  // Set initial metadata
  err = vxcore_node_update_metadata(ctx, notebook_id, "note.md", "{\"priority\":\"low\"}");
  ASSERT_EQ(err, VXCORE_OK);

  // Verify initial metadata
  char *info_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, "note.md", &info_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json info = nlohmann::json::parse(info_json);
  ASSERT(info["metadata"]["priority"] == "low");
  vxcore_string_free(info_json);

  // Close notebook
  err = vxcore_notebook_close(ctx, notebook_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(notebook_id);
  notebook_id = nullptr;

  // Manually edit the vx.json to change file metadata (simulating external modification)
  std::string vx_json_path =
      get_test_path("test_sync_file_update_nb") + "/vx_notebook/contents/vx.json";
  std::ifstream in_file(vx_json_path);
  nlohmann::json vx_json;
  in_file >> vx_json;
  in_file.close();

  // Update file metadata
  for (auto &file : vx_json["files"]) {
    if (file["name"] == "note.md") {
      file["metadata"]["priority"] = "high";
      file["metadata"]["author"] = "test";
    }
  }
  // Update modified timestamp to trigger staleness check
  vx_json["modifiedUtc"] = vx_json["modifiedUtc"].get<int64_t>() + 1000;

  std::ofstream out_file(vx_json_path);
  out_file << vx_json.dump(2);
  out_file.close();

  // Reopen notebook - sync should update file metadata in store
  err = vxcore_notebook_open(ctx, get_test_path("test_sync_file_update_nb").c_str(), &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Access folder to trigger sync
  char *config_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, ".", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(config_json);

  // Verify metadata was updated
  err = vxcore_node_get_config(ctx, notebook_id, "note.md", &info_json);
  ASSERT_EQ(err, VXCORE_OK);
  info = nlohmann::json::parse(info_json);
  ASSERT(info["metadata"]["priority"] == "high");
  ASSERT(info["metadata"]["author"] == "test");
  vxcore_string_free(info_json);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_sync_file_update_nb"));
  std::cout << "  ✓ test_sync_file_metadata_update passed" << std::endl;
  return 0;
}

// Test that new files added externally to config are synced to store.
int test_sync_new_file_creation() {
  std::cout << "  Running test_sync_new_file_creation..." << std::endl;
  cleanup_test_dir(get_test_path("test_sync_new_file_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_sync_new_file_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create one file initially
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "existing.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  // Verify only one file
  char *config_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, ".", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json config = nlohmann::json::parse(config_json);
  ASSERT(config["files"].size() == 1);
  vxcore_string_free(config_json);

  // Close notebook
  err = vxcore_notebook_close(ctx, notebook_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(notebook_id);
  notebook_id = nullptr;

  // Manually add a new file entry to vx.json (simulating external addition)
  std::string vx_json_path =
      get_test_path("test_sync_new_file_nb") + "/vx_notebook/contents/vx.json";
  std::ifstream in_file(vx_json_path);
  nlohmann::json vx_json;
  in_file >> vx_json;
  in_file.close();

  // Add new file entry
  nlohmann::json new_file;
  new_file["id"] = "new-file-uuid-12345";
  new_file["name"] = "newfile.md";
  new_file["createdUtc"] = vx_json["modifiedUtc"].get<int64_t>();
  new_file["modifiedUtc"] = vx_json["modifiedUtc"].get<int64_t>();
  new_file["metadata"] = nlohmann::json::object();
  new_file["tags"] = nlohmann::json::array();
  vx_json["files"].push_back(new_file);

  // Update modified timestamp to trigger staleness check
  vx_json["modifiedUtc"] = vx_json["modifiedUtc"].get<int64_t>() + 1000;

  std::ofstream out_file(vx_json_path);
  out_file << vx_json.dump(2);
  out_file.close();

  // Also create the actual file on disk
  write_file(get_test_path("test_sync_new_file_nb") + "/newfile.md", "# New File\n");

  // Reopen notebook - sync should add new file to store
  err = vxcore_notebook_open(ctx, get_test_path("test_sync_new_file_nb").c_str(), &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Access folder to trigger sync
  err = vxcore_node_get_config(ctx, notebook_id, ".", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  config = nlohmann::json::parse(config_json);

  // Verify both files exist
  ASSERT(config["files"].size() == 2);

  // Find the new file
  bool found_new_file = false;
  for (const auto &file : config["files"]) {
    if (file["name"] == "newfile.md") {
      found_new_file = true;
      ASSERT(file["id"] == "new-file-uuid-12345");
      break;
    }
  }
  ASSERT(found_new_file);
  vxcore_string_free(config_json);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_sync_new_file_nb"));
  std::cout << "  ✓ test_sync_new_file_creation passed" << std::endl;
  return 0;
}

// Test that folder metadata updates are synced correctly.
int test_sync_folder_metadata_update() {
  std::cout << "  Running test_sync_folder_metadata_update..." << std::endl;
  cleanup_test_dir(get_test_path("test_sync_folder_update_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_sync_folder_update_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a folder with metadata
  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "docs", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  // Set initial folder metadata
  err =
      vxcore_node_update_metadata(ctx, notebook_id, "docs", "{\"description\":\"Documentation\"}");
  ASSERT_EQ(err, VXCORE_OK);

  // Close notebook
  err = vxcore_notebook_close(ctx, notebook_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(notebook_id);
  notebook_id = nullptr;

  // Manually edit the docs folder's vx.json to change metadata
  std::string vx_json_path =
      get_test_path("test_sync_folder_update_nb") + "/vx_notebook/contents/docs/vx.json";
  std::ifstream in_file(vx_json_path);
  nlohmann::json vx_json;
  in_file >> vx_json;
  in_file.close();

  // Update folder metadata
  vx_json["metadata"]["description"] = "Updated docs";
  vx_json["metadata"]["version"] = "2.0";
  // Update modified timestamp
  vx_json["modifiedUtc"] = vx_json["modifiedUtc"].get<int64_t>() + 1000;

  std::ofstream out_file(vx_json_path);
  out_file << vx_json.dump(2);
  out_file.close();

  // Reopen notebook
  err =
      vxcore_notebook_open(ctx, get_test_path("test_sync_folder_update_nb").c_str(), &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Access folder to trigger sync
  char *config_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, "docs", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json config = nlohmann::json::parse(config_json);

  // Verify metadata was updated
  ASSERT(config["metadata"]["description"] == "Updated docs");
  ASSERT(config["metadata"]["version"] == "2.0");
  vxcore_string_free(config_json);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_sync_folder_update_nb"));
  std::cout << "  ✓ test_sync_folder_metadata_update passed" << std::endl;
  return 0;
}

// Test that sync is skipped when store is already up-to-date (staleness check).
// This validates performance optimization - no unnecessary DB operations.
int test_sync_staleness_check() {
  std::cout << "  Running test_sync_staleness_check..." << std::endl;
  cleanup_test_dir(get_test_path("test_sync_stale_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_sync_stale_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a folder with file
  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "docs", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, "docs", "readme.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  // Access folder multiple times - second access should use cached/synced data
  char *config_json = nullptr;
  for (int i = 0; i < 3; i++) {
    err = vxcore_node_get_config(ctx, notebook_id, "docs", &config_json);
    ASSERT_EQ(err, VXCORE_OK);
    nlohmann::json config = nlohmann::json::parse(config_json);
    ASSERT(config["files"].size() == 1);
    ASSERT(config["files"][0]["name"] == "readme.md");
    vxcore_string_free(config_json);
    config_json = nullptr;
  }

  // Close and reopen
  err = vxcore_notebook_close(ctx, notebook_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(notebook_id);
  notebook_id = nullptr;

  err = vxcore_notebook_open(ctx, get_test_path("test_sync_stale_nb").c_str(), &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Access again - sync state should be preserved, so only staleness check happens
  err = vxcore_node_get_config(ctx, notebook_id, "docs", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json config = nlohmann::json::parse(config_json);
  ASSERT(config["files"].size() == 1);
  ASSERT(config["files"][0]["name"] == "readme.md");
  vxcore_string_free(config_json);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_sync_stale_nb"));
  std::cout << "  ✓ test_sync_staleness_check passed" << std::endl;
  return 0;
}

// ============ Recycle Bin Tests ============

int test_recycle_bin_file_delete() {
  std::cout << "  Running test_recycle_bin_file_delete..." << std::endl;
  cleanup_test_dir(get_test_path("test_recycle_bin_file_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_recycle_bin_file_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a file
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "to_recycle.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  std::string file_path = get_test_path("test_recycle_bin_file_nb") + "/to_recycle.md";
  ASSERT(path_exists(file_path));

  // Delete the file - should move to recycle bin
  err = vxcore_node_delete(ctx, notebook_id, "to_recycle.md");
  ASSERT_EQ(err, VXCORE_OK);

  // Original file should no longer exist
  ASSERT(!path_exists(file_path));

  // File should be in recycle bin
  std::string recycle_bin_path =
      get_test_path("test_recycle_bin_file_nb") + "/vx_notebook/recycle_bin";
  ASSERT(path_exists(recycle_bin_path));
  ASSERT(path_exists(recycle_bin_path + "/to_recycle.md"));

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_recycle_bin_file_nb"));
  std::cout << "  ✓ test_recycle_bin_file_delete passed" << std::endl;
  return 0;
}

int test_recycle_bin_folder_delete() {
  std::cout << "  Running test_recycle_bin_folder_delete..." << std::endl;
  cleanup_test_dir(get_test_path("test_recycle_bin_folder_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_recycle_bin_folder_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a folder with a file inside
  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "to_recycle_folder", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, "to_recycle_folder", "inner.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  std::string folder_path = get_test_path("test_recycle_bin_folder_nb") + "/to_recycle_folder";
  ASSERT(path_exists(folder_path));
  ASSERT(path_exists(folder_path + "/inner.md"));

  // Delete the folder - should move entire folder to recycle bin
  err = vxcore_node_delete(ctx, notebook_id, "to_recycle_folder");
  ASSERT_EQ(err, VXCORE_OK);

  // Original folder should no longer exist
  ASSERT(!path_exists(folder_path));

  // Folder should be in recycle bin with contents
  std::string recycle_bin_path =
      get_test_path("test_recycle_bin_folder_nb") + "/vx_notebook/recycle_bin";
  ASSERT(path_exists(recycle_bin_path));
  ASSERT(path_exists(recycle_bin_path + "/to_recycle_folder"));
  ASSERT(path_exists(recycle_bin_path + "/to_recycle_folder/inner.md"));

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_recycle_bin_folder_nb"));
  std::cout << "  ✓ test_recycle_bin_folder_delete passed" << std::endl;
  return 0;
}

int test_recycle_bin_name_conflict() {
  std::cout << "  Running test_recycle_bin_name_conflict..." << std::endl;
  cleanup_test_dir(get_test_path("test_recycle_bin_conflict_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_recycle_bin_conflict_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create and delete first file
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "conflict.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  err = vxcore_node_delete(ctx, notebook_id, "conflict.md");
  ASSERT_EQ(err, VXCORE_OK);

  // Create and delete second file with same name
  err = vxcore_file_create(ctx, notebook_id, ".", "conflict.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  err = vxcore_node_delete(ctx, notebook_id, "conflict.md");
  ASSERT_EQ(err, VXCORE_OK);

  // Create and delete third file with same name
  err = vxcore_file_create(ctx, notebook_id, ".", "conflict.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  err = vxcore_node_delete(ctx, notebook_id, "conflict.md");
  ASSERT_EQ(err, VXCORE_OK);

  // All three should be in recycle bin with different names
  std::string recycle_bin_path =
      get_test_path("test_recycle_bin_conflict_nb") + "/vx_notebook/recycle_bin";
  ASSERT(path_exists(recycle_bin_path + "/conflict.md"));
  ASSERT(path_exists(recycle_bin_path + "/conflict_1.md"));
  ASSERT(path_exists(recycle_bin_path + "/conflict_2.md"));

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_recycle_bin_conflict_nb"));
  std::cout << "  ✓ test_recycle_bin_name_conflict passed" << std::endl;
  return 0;
}

int test_recycle_bin_get_path() {
  std::cout << "  Running test_recycle_bin_get_path..." << std::endl;
  cleanup_test_dir(get_test_path("test_recycle_bin_path_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_recycle_bin_path_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *recycle_bin_path = nullptr;
  err = vxcore_notebook_get_recycle_bin_path(ctx, notebook_id, &recycle_bin_path);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(recycle_bin_path);

  std::string expected_path =
      normalize_path(get_test_path("test_recycle_bin_path_nb") + "/vx_notebook/recycle_bin");
  ASSERT_EQ(std::string(recycle_bin_path), expected_path);

  vxcore_string_free(recycle_bin_path);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_recycle_bin_path_nb"));
  std::cout << "  ✓ test_recycle_bin_get_path passed" << std::endl;
  return 0;
}

int test_recycle_bin_empty() {
  std::cout << "  Running test_recycle_bin_empty..." << std::endl;
  cleanup_test_dir(get_test_path("test_recycle_bin_empty_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_recycle_bin_empty_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create and delete multiple files
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "file1.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);
  err = vxcore_node_delete(ctx, notebook_id, "file1.md");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_create(ctx, notebook_id, ".", "file2.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);
  err = vxcore_node_delete(ctx, notebook_id, "file2.md");
  ASSERT_EQ(err, VXCORE_OK);

  // Verify files are in recycle bin
  std::string recycle_bin_path =
      get_test_path("test_recycle_bin_empty_nb") + "/vx_notebook/recycle_bin";
  ASSERT(path_exists(recycle_bin_path + "/file1.md"));
  ASSERT(path_exists(recycle_bin_path + "/file2.md"));

  // Empty the recycle bin
  err = vxcore_notebook_empty_recycle_bin(ctx, notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Recycle bin folder should still exist but be empty
  ASSERT(path_exists(recycle_bin_path));
  ASSERT(!path_exists(recycle_bin_path + "/file1.md"));
  ASSERT(!path_exists(recycle_bin_path + "/file2.md"));

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_recycle_bin_empty_nb"));
  std::cout << "  ✓ test_recycle_bin_empty passed" << std::endl;
  return 0;
}

int test_recycle_bin_raw_notebook_unsupported() {
  std::cout << "  Running test_recycle_bin_raw_notebook_unsupported..." << std::endl;
  cleanup_test_dir(get_test_path("test_recycle_bin_raw_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a raw notebook
  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_recycle_bin_raw_nb").c_str(),
                               "{\"name\":\"Raw Notebook\"}", VXCORE_NOTEBOOK_RAW, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Get recycle bin path should return empty string for raw notebook
  char *recycle_bin_path = nullptr;
  err = vxcore_notebook_get_recycle_bin_path(ctx, notebook_id, &recycle_bin_path);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(recycle_bin_path);
  ASSERT_EQ(std::string(recycle_bin_path), std::string(""));
  vxcore_string_free(recycle_bin_path);

  // Empty recycle bin should return VXCORE_ERR_UNSUPPORTED for raw notebook
  err = vxcore_notebook_empty_recycle_bin(ctx, notebook_id);
  ASSERT_EQ(err, VXCORE_ERR_UNSUPPORTED);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_recycle_bin_raw_nb"));
  std::cout << "  ✓ test_recycle_bin_raw_notebook_unsupported passed" << std::endl;
  return 0;
}

// ============ File Import Tests ============

int test_file_import_basic() {
  std::cout << "  Running test_file_import_basic..." << std::endl;
  cleanup_test_dir(get_test_path("test_file_import_basic_nb"));
  cleanup_test_dir(get_test_path("test_file_import_source"));

  // Create external source file
  create_directory(get_test_path("test_file_import_source"));
  std::string source_file = get_test_path("test_file_import_source") + "/external.md";
  write_file(source_file, "# External File Content\n\nThis is test content.");
  ASSERT(path_exists(source_file));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_file_import_basic_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Import the file
  char *file_id = nullptr;
  err = vxcore_file_import(ctx, notebook_id, ".", source_file.c_str(), &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(file_id);

  // Verify file exists in notebook with correct name
  std::string imported_path = get_test_path("test_file_import_basic_nb") + "/external.md";
  ASSERT(path_exists(imported_path));

  // Verify file is in metadata
  char *config_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, ".", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json config = nlohmann::json::parse(config_json);
  ASSERT(config["files"].size() == 1);
  ASSERT(config["files"][0]["name"] == "external.md");
  vxcore_string_free(config_json);

  // Verify source file still exists (copy, not move)
  ASSERT(path_exists(source_file));

  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_file_import_basic_nb"));
  cleanup_test_dir(get_test_path("test_file_import_source"));
  std::cout << "  ✓ test_file_import_basic passed" << std::endl;
  return 0;
}

int test_file_import_name_conflict() {
  std::cout << "  Running test_file_import_name_conflict..." << std::endl;
  cleanup_test_dir(get_test_path("test_file_import_conflict_nb"));
  cleanup_test_dir(get_test_path("test_file_import_conflict_src"));

  // Create external source file
  create_directory(get_test_path("test_file_import_conflict_src"));
  std::string source_file = get_test_path("test_file_import_conflict_src") + "/conflict.md";
  write_file(source_file, "# Source content");

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_file_import_conflict_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create existing file with same name
  char *existing_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "conflict.md", &existing_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(existing_id);

  // Import file - should auto-rename to conflict_1.md
  char *file_id = nullptr;
  err = vxcore_file_import(ctx, notebook_id, ".", source_file.c_str(), &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(file_id);

  // Verify both files exist
  std::string original_path = get_test_path("test_file_import_conflict_nb") + "/conflict.md";
  std::string renamed_path = get_test_path("test_file_import_conflict_nb") + "/conflict_1.md";
  ASSERT(path_exists(original_path));
  ASSERT(path_exists(renamed_path));

  // Import again - should become conflict_2.md
  char *file_id2 = nullptr;
  err = vxcore_file_import(ctx, notebook_id, ".", source_file.c_str(), &file_id2);
  ASSERT_EQ(err, VXCORE_OK);
  std::string renamed_path2 = get_test_path("test_file_import_conflict_nb") + "/conflict_2.md";
  ASSERT(path_exists(renamed_path2));
  vxcore_string_free(file_id2);

  // Verify metadata has 3 files
  char *config_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, ".", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json config = nlohmann::json::parse(config_json);
  ASSERT(config["files"].size() == 3);
  vxcore_string_free(config_json);

  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_file_import_conflict_nb"));
  cleanup_test_dir(get_test_path("test_file_import_conflict_src"));
  std::cout << "  ✓ test_file_import_name_conflict passed" << std::endl;
  return 0;
}

int test_file_import_source_not_found() {
  std::cout << "  Running test_file_import_source_not_found..." << std::endl;
  cleanup_test_dir(get_test_path("test_file_import_notfound_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_file_import_notfound_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Try to import non-existent file
  char *file_id = nullptr;
  err = vxcore_file_import(ctx, notebook_id, ".", "/nonexistent/path/file.md", &file_id);
  ASSERT_EQ(err, VXCORE_ERR_NOT_FOUND);
  ASSERT_NULL(file_id);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_file_import_notfound_nb"));
  std::cout << "  ✓ test_file_import_source_not_found passed" << std::endl;
  return 0;
}

int test_file_import_invalid_params() {
  std::cout << "  Running test_file_import_invalid_params..." << std::endl;
  cleanup_test_dir(get_test_path("test_file_import_invalid_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_file_import_invalid_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;

  // Null context
  err = vxcore_file_import(nullptr, notebook_id, ".", "/some/path.md", &file_id);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  // Null notebook_id
  err = vxcore_file_import(ctx, nullptr, ".", "/some/path.md", &file_id);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  // Null folder_path
  err = vxcore_file_import(ctx, notebook_id, nullptr, "/some/path.md", &file_id);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  // Null external_file_path
  err = vxcore_file_import(ctx, notebook_id, ".", nullptr, &file_id);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  // Null out_file_id
  err = vxcore_file_import(ctx, notebook_id, ".", "/some/path.md", nullptr);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_file_import_invalid_nb"));
  std::cout << "  ✓ test_file_import_invalid_params passed" << std::endl;
  return 0;
}

int test_file_import_to_subfolder() {
  std::cout << "  Running test_file_import_to_subfolder..." << std::endl;
  cleanup_test_dir(get_test_path("test_file_import_subfolder_nb"));
  cleanup_test_dir(get_test_path("test_file_import_subfolder_src"));

  // Create external source file
  create_directory(get_test_path("test_file_import_subfolder_src"));
  std::string source_file = get_test_path("test_file_import_subfolder_src") + "/doc.md";
  write_file(source_file, "# Document");

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_file_import_subfolder_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create subfolder
  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "docs", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  // Import to subfolder
  char *file_id = nullptr;
  err = vxcore_file_import(ctx, notebook_id, "docs", source_file.c_str(), &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(file_id);

  // Verify file is in subfolder
  std::string imported_path = get_test_path("test_file_import_subfolder_nb") + "/docs/doc.md";
  ASSERT(path_exists(imported_path));

  // Verify metadata in subfolder
  char *config_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, "docs", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json config = nlohmann::json::parse(config_json);
  ASSERT(config["files"].size() == 1);
  ASSERT(config["files"][0]["name"] == "doc.md");
  vxcore_string_free(config_json);

  vxcore_string_free(file_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_file_import_subfolder_nb"));
  cleanup_test_dir(get_test_path("test_file_import_subfolder_src"));
  std::cout << "  ✓ test_file_import_to_subfolder passed" << std::endl;
  return 0;
}

// ============ Folder Import Tests ============

int test_folder_import_basic() {
  std::cout << "  Running test_folder_import_basic..." << std::endl;
  cleanup_test_dir(get_test_path("test_folder_import_basic_nb"));
  cleanup_test_dir(get_test_path("test_folder_import_source"));

  // Create external source folder with contents
  create_directory(get_test_path("test_folder_import_source"));
  create_directory(get_test_path("test_folder_import_source") + "/external_folder");
  create_directory(get_test_path("test_folder_import_source") + "/external_folder/subfolder");
  write_file(get_test_path("test_folder_import_source") + "/external_folder/file1.md", "# File 1");
  write_file(get_test_path("test_folder_import_source") + "/external_folder/subfolder/file2.md",
             "# File 2");
  std::string source_folder = get_test_path("test_folder_import_source") + "/external_folder";
  ASSERT(path_exists(source_folder));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_folder_import_basic_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Import the folder to root
  char *folder_id = nullptr;
  err = vxcore_folder_import(ctx, notebook_id, ".", source_folder.c_str(), nullptr, &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(folder_id);

  // Verify folder structure was copied
  std::string imported_folder = get_test_path("test_folder_import_basic_nb") + "/external_folder";
  ASSERT(path_exists(imported_folder));
  ASSERT(path_exists(imported_folder + "/file1.md"));
  ASSERT(path_exists(imported_folder + "/subfolder"));
  ASSERT(path_exists(imported_folder + "/subfolder/file2.md"));

  // Verify folder is in metadata
  char *config_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, ".", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json config = nlohmann::json::parse(config_json);
  ASSERT(config["folders"].size() == 1);
  ASSERT(config["folders"][0] == "external_folder");
  vxcore_string_free(config_json);

  // Verify files are indexed in the imported folder
  err = vxcore_node_get_config(ctx, notebook_id, "external_folder", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  config = nlohmann::json::parse(config_json);
  ASSERT(config["files"].size() == 1);
  ASSERT(config["files"][0]["name"] == "file1.md");
  ASSERT(config["folders"].size() == 1);
  ASSERT(config["folders"][0] == "subfolder");
  vxcore_string_free(config_json);

  // Verify subfolder contents are indexed
  err = vxcore_node_get_config(ctx, notebook_id, "external_folder/subfolder", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  config = nlohmann::json::parse(config_json);
  ASSERT(config["files"].size() == 1);
  ASSERT(config["files"][0]["name"] == "file2.md");
  vxcore_string_free(config_json);

  // Verify source folder still exists (copy, not move)
  ASSERT(path_exists(source_folder));

  vxcore_string_free(folder_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_folder_import_basic_nb"));
  cleanup_test_dir(get_test_path("test_folder_import_source"));
  std::cout << "  ✓ test_folder_import_basic passed" << std::endl;
  return 0;
}

int test_folder_import_name_conflict() {
  std::cout << "  Running test_folder_import_name_conflict..." << std::endl;
  cleanup_test_dir(get_test_path("test_folder_import_conflict_nb"));
  cleanup_test_dir(get_test_path("test_folder_import_conflict_src"));

  // Create external source folder
  create_directory(get_test_path("test_folder_import_conflict_src"));
  create_directory(get_test_path("test_folder_import_conflict_src") + "/conflict");
  write_file(get_test_path("test_folder_import_conflict_src") + "/conflict/file.md", "# File");
  std::string source_folder = get_test_path("test_folder_import_conflict_src") + "/conflict";

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_folder_import_conflict_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create existing folder with same name
  char *existing_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "conflict", &existing_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(existing_id);

  // Import folder - should auto-rename to conflict_1
  char *folder_id = nullptr;
  err = vxcore_folder_import(ctx, notebook_id, ".", source_folder.c_str(), nullptr, &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(folder_id);

  // Verify both folders exist
  std::string original_path = get_test_path("test_folder_import_conflict_nb") + "/conflict";
  std::string renamed_path = get_test_path("test_folder_import_conflict_nb") + "/conflict_1";
  ASSERT(path_exists(original_path));
  ASSERT(path_exists(renamed_path));
  ASSERT(path_exists(renamed_path + "/file.md"));

  vxcore_string_free(folder_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_folder_import_conflict_nb"));
  cleanup_test_dir(get_test_path("test_folder_import_conflict_src"));
  std::cout << "  ✓ test_folder_import_name_conflict passed" << std::endl;
  return 0;
}

int test_folder_import_source_not_found() {
  std::cout << "  Running test_folder_import_source_not_found..." << std::endl;
  cleanup_test_dir(get_test_path("test_folder_import_notfound_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_folder_import_notfound_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Try to import non-existent folder
  char *folder_id = nullptr;
#ifdef _WIN32
  err = vxcore_folder_import(ctx, notebook_id, ".", "C:\\nonexistent\\path\\folder", nullptr,
                             &folder_id);
#else
  err =
      vxcore_folder_import(ctx, notebook_id, ".", "/nonexistent/path/folder", nullptr, &folder_id);
#endif
  ASSERT_EQ(err, VXCORE_ERR_NOT_FOUND);
  ASSERT_NULL(folder_id);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_folder_import_notfound_nb"));
  std::cout << "  ✓ test_folder_import_source_not_found passed" << std::endl;
  return 0;
}

int test_folder_import_invalid_params() {
  std::cout << "  Running test_folder_import_invalid_params..." << std::endl;
  cleanup_test_dir(get_test_path("test_folder_import_invalid_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_folder_import_invalid_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *folder_id = nullptr;

  // Null context
  err = vxcore_folder_import(nullptr, notebook_id, ".", "/some/path", nullptr, &folder_id);
  ASSERT_EQ(err, VXCORE_ERR_NULL_POINTER);

  // Null notebook_id
  err = vxcore_folder_import(ctx, nullptr, ".", "/some/path", nullptr, &folder_id);
  ASSERT_EQ(err, VXCORE_ERR_NULL_POINTER);

  // Null dest_folder_path
  err = vxcore_folder_import(ctx, notebook_id, nullptr, "/some/path", nullptr, &folder_id);
  ASSERT_EQ(err, VXCORE_ERR_NULL_POINTER);

  // Null external_folder_path
  err = vxcore_folder_import(ctx, notebook_id, ".", nullptr, nullptr, &folder_id);
  ASSERT_EQ(err, VXCORE_ERR_NULL_POINTER);

  // Null out_folder_id
  err = vxcore_folder_import(ctx, notebook_id, ".", "/some/path", nullptr, nullptr);
  ASSERT_EQ(err, VXCORE_ERR_NULL_POINTER);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_folder_import_invalid_nb"));
  std::cout << "  ✓ test_folder_import_invalid_params passed" << std::endl;
  return 0;
}

int test_folder_import_to_subfolder() {
  std::cout << "  Running test_folder_import_to_subfolder..." << std::endl;
  cleanup_test_dir(get_test_path("test_folder_import_subfolder_nb"));
  cleanup_test_dir(get_test_path("test_folder_import_subfolder_src"));

  // Create external source folder
  create_directory(get_test_path("test_folder_import_subfolder_src"));
  create_directory(get_test_path("test_folder_import_subfolder_src") + "/imported");
  write_file(get_test_path("test_folder_import_subfolder_src") + "/imported/doc.md", "# Document");
  std::string source_folder = get_test_path("test_folder_import_subfolder_src") + "/imported";

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_folder_import_subfolder_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create target subfolder
  char *target_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "docs", &target_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(target_id);

  // Import to subfolder
  char *folder_id = nullptr;
  err = vxcore_folder_import(ctx, notebook_id, "docs", source_folder.c_str(), nullptr, &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(folder_id);

  // Verify folder is in subfolder
  std::string imported_path = get_test_path("test_folder_import_subfolder_nb") + "/docs/imported";
  ASSERT(path_exists(imported_path));
  ASSERT(path_exists(imported_path + "/doc.md"));

  // Verify metadata in subfolder
  char *config_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, "docs", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json config = nlohmann::json::parse(config_json);
  ASSERT(config["folders"].size() == 1);
  ASSERT(config["folders"][0] == "imported");
  vxcore_string_free(config_json);

  vxcore_string_free(folder_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_folder_import_subfolder_nb"));
  cleanup_test_dir(get_test_path("test_folder_import_subfolder_src"));
  std::cout << "  ✓ test_folder_import_to_subfolder passed" << std::endl;
  return 0;
}

int test_folder_import_within_notebook() {
  std::cout << "  Running test_folder_import_within_notebook..." << std::endl;
  cleanup_test_dir(get_test_path("test_folder_import_within_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_folder_import_within_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a folder inside the notebook
  char *internal_folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "internal_folder", &internal_folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(internal_folder_id);

  // Try to import a folder from within the notebook root - should fail
  std::string internal_folder_path =
      get_test_path("test_folder_import_within_nb") + "/internal_folder";
  char *folder_id = nullptr;
  err = vxcore_folder_import(ctx, notebook_id, ".", internal_folder_path.c_str(), nullptr,
                             &folder_id);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);
  ASSERT_NULL(folder_id);

  // Also try importing the notebook root itself - should fail
  err = vxcore_folder_import(ctx, notebook_id, ".",
                             get_test_path("test_folder_import_within_nb").c_str(), nullptr,
                             &folder_id);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);
  ASSERT_NULL(folder_id);

  // Try to import with a relative path - should fail
  err = vxcore_folder_import(ctx, notebook_id, ".", "../some_relative_path", nullptr, &folder_id);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);
  ASSERT_NULL(folder_id);

  err = vxcore_folder_import(ctx, notebook_id, ".", "relative/path", nullptr, &folder_id);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);
  ASSERT_NULL(folder_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_folder_import_within_nb"));
  std::cout << "  ✓ test_folder_import_within_notebook passed" << std::endl;
  return 0;
}

int test_folder_import_suffix_filter() {
  std::cout << "  Running test_folder_import_suffix_filter..." << std::endl;
  cleanup_test_dir(get_test_path("test_folder_import_filter_nb"));
  cleanup_test_dir(get_test_path("test_folder_import_filter_src"));

  // Create source folder with various file types
  std::string source_folder = get_test_path("test_folder_import_filter_src/mixed_folder");
  create_directory(source_folder);
  write_file(source_folder + "/doc1.md", "# Markdown 1");
  write_file(source_folder + "/doc2.MD", "# Markdown 2");  // Uppercase extension
  write_file(source_folder + "/notes.txt", "Some notes");
  write_file(source_folder + "/config.json", "{\"key\":\"value\"}");
  write_file(source_folder + "/script.py", "print('hello')");
  write_file(source_folder + "/image.png", "fake png data");
  create_directory(source_folder + "/subfolder");
  write_file(source_folder + "/subfolder/nested.md", "# Nested");
  write_file(source_folder + "/subfolder/nested.txt", "Nested text");
  write_file(source_folder + "/subfolder/nested.py", "# Python");

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_folder_import_filter_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Import with suffix filter: only md and txt files
  char *folder_id = nullptr;
  err = vxcore_folder_import(ctx, notebook_id, ".", source_folder.c_str(), "md;txt", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(folder_id);

  // Verify only filtered files were copied
  std::string imported_folder = get_test_path("test_folder_import_filter_nb") + "/mixed_folder";
  ASSERT(path_exists(imported_folder));
  ASSERT(path_exists(imported_folder + "/doc1.md"));
  ASSERT(path_exists(imported_folder + "/doc2.MD"));  // Case insensitive matching
  ASSERT(path_exists(imported_folder + "/notes.txt"));
  ASSERT(!path_exists(imported_folder + "/config.json"));  // Filtered out
  ASSERT(!path_exists(imported_folder + "/script.py"));    // Filtered out
  ASSERT(!path_exists(imported_folder + "/image.png"));    // Filtered out

  // Verify subfolder structure preserved but only filtered files copied
  ASSERT(path_exists(imported_folder + "/subfolder"));
  ASSERT(path_exists(imported_folder + "/subfolder/nested.md"));
  ASSERT(path_exists(imported_folder + "/subfolder/nested.txt"));
  ASSERT(!path_exists(imported_folder + "/subfolder/nested.py"));  // Filtered out

  // Verify files are indexed in metadata
  char *config_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, "mixed_folder", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json config = nlohmann::json::parse(config_json);
  ASSERT_EQ(config["files"].size(), 3);  // doc1.md, doc2.MD, notes.txt
  vxcore_string_free(config_json);

  vxcore_string_free(folder_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_folder_import_filter_nb"));
  cleanup_test_dir(get_test_path("test_folder_import_filter_src"));
  std::cout << "  ✓ test_folder_import_suffix_filter passed" << std::endl;
  return 0;
}

// ============ Node Index/Unindex Tests ============

int test_node_index_file() {
  std::cout << "  Running test_node_index_file..." << std::endl;
  cleanup_test_dir(get_test_path("test_node_index_file_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_node_index_file_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create an unindexed file directly on filesystem (not through API)
  std::string unindexed_file = get_test_path("test_node_index_file_nb") + "/unindexed.md";
  write_file(unindexed_file, "# Unindexed File\n\nThis file is not in metadata.");
  ASSERT(path_exists(unindexed_file));

  // Verify file is NOT in metadata yet
  char *config_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, ".", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json config = nlohmann::json::parse(config_json);
  ASSERT(config["files"].size() == 0);
  vxcore_string_free(config_json);

  // Index the file
  err = vxcore_node_index(ctx, notebook_id, "unindexed.md");
  ASSERT_EQ(err, VXCORE_OK);

  // Verify file is now in metadata
  err = vxcore_node_get_config(ctx, notebook_id, ".", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  config = nlohmann::json::parse(config_json);
  ASSERT(config["files"].size() == 1);
  ASSERT(config["files"][0]["name"] == "unindexed.md");
  vxcore_string_free(config_json);

  // Verify we can get file config directly
  char *file_config_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, "unindexed.md", &file_config_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json file_config = nlohmann::json::parse(file_config_json);
  ASSERT(file_config["name"] == "unindexed.md");
  ASSERT(file_config["type"] == "file");
  vxcore_string_free(file_config_json);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_node_index_file_nb"));
  std::cout << "  ✓ test_node_index_file passed" << std::endl;
  return 0;
}

int test_node_index_folder() {
  std::cout << "  Running test_node_index_folder..." << std::endl;
  cleanup_test_dir(get_test_path("test_node_index_folder_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_node_index_folder_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create an unindexed folder directly on filesystem
  std::string unindexed_folder = get_test_path("test_node_index_folder_nb") + "/unindexed_folder";
  create_directory(unindexed_folder);
  ASSERT(path_exists(unindexed_folder));

  // Verify folder is NOT in metadata yet
  char *config_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, ".", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json config = nlohmann::json::parse(config_json);
  ASSERT(config["folders"].size() == 0);
  vxcore_string_free(config_json);

  // Index the folder
  err = vxcore_node_index(ctx, notebook_id, "unindexed_folder");
  ASSERT_EQ(err, VXCORE_OK);

  // Verify folder is now in metadata
  err = vxcore_node_get_config(ctx, notebook_id, ".", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  config = nlohmann::json::parse(config_json);
  ASSERT(config["folders"].size() == 1);
  ASSERT(config["folders"][0] == "unindexed_folder");
  vxcore_string_free(config_json);

  // Verify we can get folder config directly
  char *folder_config_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, "unindexed_folder", &folder_config_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json folder_config = nlohmann::json::parse(folder_config_json);
  ASSERT(folder_config["name"] == "unindexed_folder");
  ASSERT(folder_config["type"] == "folder");
  vxcore_string_free(folder_config_json);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_node_index_folder_nb"));
  std::cout << "  ✓ test_node_index_folder passed" << std::endl;
  return 0;
}

int test_node_unindex_file() {
  std::cout << "  Running test_node_unindex_file..." << std::endl;
  cleanup_test_dir(get_test_path("test_node_unindex_file_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_node_unindex_file_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a file through API (so it's indexed)
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "indexed.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  // Verify file is in metadata
  char *config_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, ".", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json config = nlohmann::json::parse(config_json);
  ASSERT(config["files"].size() == 1);
  vxcore_string_free(config_json);

  // Verify file exists on filesystem
  std::string file_path = get_test_path("test_node_unindex_file_nb") + "/indexed.md";
  ASSERT(path_exists(file_path));

  // Unindex the file (removes from metadata but keeps filesystem)
  err = vxcore_node_unindex(ctx, notebook_id, "indexed.md");
  ASSERT_EQ(err, VXCORE_OK);

  // Verify file is NO LONGER in metadata
  err = vxcore_node_get_config(ctx, notebook_id, ".", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  config = nlohmann::json::parse(config_json);
  ASSERT(config["files"].size() == 0);
  vxcore_string_free(config_json);

  // Verify file STILL exists on filesystem
  ASSERT(path_exists(file_path));

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_node_unindex_file_nb"));
  std::cout << "  ✓ test_node_unindex_file passed" << std::endl;
  return 0;
}

int test_node_unindex_folder() {
  std::cout << "  Running test_node_unindex_folder..." << std::endl;
  cleanup_test_dir(get_test_path("test_node_unindex_folder_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_node_unindex_folder_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a folder through API (so it's indexed)
  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "indexed_folder", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  // Verify folder is in metadata
  char *config_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, ".", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json config = nlohmann::json::parse(config_json);
  ASSERT(config["folders"].size() == 1);
  vxcore_string_free(config_json);

  // Verify folder exists on filesystem
  std::string folder_path = get_test_path("test_node_unindex_folder_nb") + "/indexed_folder";
  ASSERT(path_exists(folder_path));

  // Unindex the folder (removes from metadata but keeps filesystem)
  err = vxcore_node_unindex(ctx, notebook_id, "indexed_folder");
  ASSERT_EQ(err, VXCORE_OK);

  // Verify folder is NO LONGER in metadata
  err = vxcore_node_get_config(ctx, notebook_id, ".", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  config = nlohmann::json::parse(config_json);
  ASSERT(config["folders"].size() == 0);
  vxcore_string_free(config_json);

  // Verify folder STILL exists on filesystem
  ASSERT(path_exists(folder_path));

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_node_unindex_folder_nb"));
  std::cout << "  ✓ test_node_unindex_folder passed" << std::endl;
  return 0;
}

int test_node_index_not_found() {
  std::cout << "  Running test_node_index_not_found..." << std::endl;
  cleanup_test_dir(get_test_path("test_node_index_notfound_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_node_index_notfound_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Try to index a non-existent file
  err = vxcore_node_index(ctx, notebook_id, "nonexistent.md");
  ASSERT_EQ(err, VXCORE_ERR_NOT_FOUND);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_node_index_notfound_nb"));
  std::cout << "  ✓ test_node_index_not_found passed" << std::endl;
  return 0;
}

int test_node_index_already_exists() {
  std::cout << "  Running test_node_index_already_exists..." << std::endl;
  cleanup_test_dir(get_test_path("test_node_index_exists_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_node_index_exists_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a file through API (already indexed)
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "existing.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  // Try to index the already-indexed file
  err = vxcore_node_index(ctx, notebook_id, "existing.md");
  ASSERT_EQ(err, VXCORE_ERR_ALREADY_EXISTS);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_node_index_exists_nb"));
  std::cout << "  ✓ test_node_index_already_exists passed" << std::endl;
  return 0;
}

int test_node_unindex_not_found() {
  std::cout << "  Running test_node_unindex_not_found..." << std::endl;
  cleanup_test_dir(get_test_path("test_node_unindex_notfound_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_node_unindex_notfound_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Try to unindex a non-existent file
  err = vxcore_node_unindex(ctx, notebook_id, "nonexistent.md");
  ASSERT_EQ(err, VXCORE_ERR_NOT_FOUND);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_node_unindex_notfound_nb"));
  std::cout << "  ✓ test_node_unindex_not_found passed" << std::endl;
  return 0;
}

int test_node_index_invalid_params() {
  std::cout << "  Running test_node_index_invalid_params..." << std::endl;
  cleanup_test_dir(get_test_path("test_node_index_inv_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_node_index_inv_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Null context
  err = vxcore_node_index(nullptr, notebook_id, "test.md");
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  // Null notebook_id
  err = vxcore_node_index(ctx, nullptr, "test.md");
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  // Null node_path
  err = vxcore_node_index(ctx, notebook_id, nullptr);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  // Same for unindex
  err = vxcore_node_unindex(nullptr, notebook_id, "test.md");
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = vxcore_node_unindex(ctx, nullptr, "test.md");
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = vxcore_node_unindex(ctx, notebook_id, nullptr);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_node_index_inv_nb"));
  std::cout << "  ✓ test_node_index_invalid_params passed" << std::endl;
  return 0;
}

int test_node_index_nested() {
  std::cout << "  Running test_node_index_nested..." << std::endl;
  cleanup_test_dir(get_test_path("test_node_index_nested_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_node_index_nested_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a folder through API
  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "docs", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  // Create an unindexed file in the nested folder
  std::string nested_file = get_test_path("test_node_index_nested_nb") + "/docs/nested.md";
  write_file(nested_file, "# Nested file");
  ASSERT(path_exists(nested_file));

  // Verify file is NOT in folder's metadata yet
  char *config_json = nullptr;
  err = vxcore_node_get_config(ctx, notebook_id, "docs", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  nlohmann::json config = nlohmann::json::parse(config_json);
  ASSERT(config["files"].size() == 0);
  vxcore_string_free(config_json);

  // Index the nested file
  err = vxcore_node_index(ctx, notebook_id, "docs/nested.md");
  ASSERT_EQ(err, VXCORE_OK);

  // Verify file is now in folder's metadata
  err = vxcore_node_get_config(ctx, notebook_id, "docs", &config_json);
  ASSERT_EQ(err, VXCORE_OK);
  config = nlohmann::json::parse(config_json);
  ASSERT(config["files"].size() == 1);
  ASSERT(config["files"][0]["name"] == "nested.md");
  vxcore_string_free(config_json);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_node_index_nested_nb"));
  std::cout << "  ✓ test_node_index_nested passed" << std::endl;
  return 0;
}

int test_node_get_path_by_id() {
  std::cout << "  Running test_node_get_path_by_id..." << std::endl;
  cleanup_test_dir(get_test_path("test_node_get_path_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_node_get_path_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create folder structure: docs/guides
  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "docs", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  std::string docs_id(folder_id);
  vxcore_string_free(folder_id);

  err = vxcore_folder_create(ctx, notebook_id, "docs", "guides", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  std::string guides_id(folder_id);
  vxcore_string_free(folder_id);

  // Create files at different levels
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "readme.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  std::string readme_id(file_id);
  vxcore_string_free(file_id);

  err = vxcore_file_create(ctx, notebook_id, "docs", "intro.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  std::string intro_id(file_id);
  vxcore_string_free(file_id);

  err = vxcore_file_create(ctx, notebook_id, "docs/guides", "setup.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  std::string setup_id(file_id);
  vxcore_string_free(file_id);

  // Test getting path for folder at root level
  char *path = nullptr;
  err = vxcore_node_get_path_by_id(ctx, notebook_id, docs_id.c_str(), &path);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(path);
  ASSERT_EQ(std::string(path), std::string("docs"));
  vxcore_string_free(path);

  // Test getting path for nested folder
  err = vxcore_node_get_path_by_id(ctx, notebook_id, guides_id.c_str(), &path);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(path);
  ASSERT_EQ(std::string(path), std::string("docs/guides"));
  vxcore_string_free(path);

  // Test getting path for file at root level
  err = vxcore_node_get_path_by_id(ctx, notebook_id, readme_id.c_str(), &path);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(path);
  ASSERT_EQ(std::string(path), std::string("readme.md"));
  vxcore_string_free(path);

  // Test getting path for file in subfolder
  err = vxcore_node_get_path_by_id(ctx, notebook_id, intro_id.c_str(), &path);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(path);
  ASSERT_EQ(std::string(path), std::string("docs/intro.md"));
  vxcore_string_free(path);

  // Test getting path for deeply nested file
  err = vxcore_node_get_path_by_id(ctx, notebook_id, setup_id.c_str(), &path);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(path);
  ASSERT_EQ(std::string(path), std::string("docs/guides/setup.md"));
  vxcore_string_free(path);

  // Test with non-existent ID
  err = vxcore_node_get_path_by_id(ctx, notebook_id, "nonexistent-uuid", &path);
  ASSERT_EQ(err, VXCORE_ERR_NOT_FOUND);
  ASSERT_NULL(path);

  // Test invalid params
  err = vxcore_node_get_path_by_id(nullptr, notebook_id, docs_id.c_str(), &path);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = vxcore_node_get_path_by_id(ctx, nullptr, docs_id.c_str(), &path);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = vxcore_node_get_path_by_id(ctx, notebook_id, nullptr, &path);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = vxcore_node_get_path_by_id(ctx, notebook_id, docs_id.c_str(), nullptr);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_node_get_path_nb"));
  std::cout << "  ✓ test_node_get_path_by_id passed" << std::endl;
  return 0;
}

int test_node_resolve_by_id() {
  std::cout << "  Running test_node_resolve_by_id..." << std::endl;
  cleanup_test_dir(get_test_path("test_node_resolve_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_node_resolve_nb").c_str(),
                             "{\"name\":\"Resolve Test\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);
  std::string nb_id(notebook_id);
  vxcore_string_free(notebook_id);

  // Create a folder
  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, nb_id.c_str(), ".", "docs", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  std::string docs_id(folder_id);
  vxcore_string_free(folder_id);

  // Create a file in the folder
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, nb_id.c_str(), "docs", "readme.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  std::string readme_id(file_id);
  vxcore_string_free(file_id);

  // === Test 1: Resolve file UUID ===
  char *out_nb = nullptr;
  char *out_path = nullptr;
  err = vxcore_node_resolve_by_id(ctx, readme_id.c_str(), &out_nb, &out_path);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(out_nb);
  ASSERT_NOT_NULL(out_path);
  ASSERT_EQ(std::string(out_nb), nb_id);
  ASSERT_EQ(std::string(out_path), std::string("docs/readme.md"));
  vxcore_string_free(out_nb);
  vxcore_string_free(out_path);

  // === Test 2: Resolve folder UUID ===
  out_nb = nullptr;
  out_path = nullptr;
  err = vxcore_node_resolve_by_id(ctx, docs_id.c_str(), &out_nb, &out_path);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(out_nb);
  ASSERT_NOT_NULL(out_path);
  ASSERT_EQ(std::string(out_nb), nb_id);
  ASSERT_EQ(std::string(out_path), std::string("docs"));
  vxcore_string_free(out_nb);
  vxcore_string_free(out_path);

  // === Test 3: Not found ===
  out_nb = nullptr;
  out_path = nullptr;
  err = vxcore_node_resolve_by_id(ctx, "nonexistent-uuid", &out_nb, &out_path);
  ASSERT_EQ(err, VXCORE_ERR_NOT_FOUND);
  ASSERT_NULL(out_nb);
  ASSERT_NULL(out_path);

  // === Test 4: Null pointer args ===
  err = vxcore_node_resolve_by_id(nullptr, readme_id.c_str(), &out_nb, &out_path);
  ASSERT_EQ(err, VXCORE_ERR_NULL_POINTER);

  err = vxcore_node_resolve_by_id(ctx, nullptr, &out_nb, &out_path);
  ASSERT_EQ(err, VXCORE_ERR_NULL_POINTER);

  err = vxcore_node_resolve_by_id(ctx, readme_id.c_str(), nullptr, &out_path);
  ASSERT_EQ(err, VXCORE_ERR_NULL_POINTER);

  err = vxcore_node_resolve_by_id(ctx, readme_id.c_str(), &out_nb, nullptr);
  ASSERT_EQ(err, VXCORE_ERR_NULL_POINTER);

  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_node_resolve_nb"));
  std::cout << "  ✓ test_node_resolve_by_id passed" << std::endl;
  return 0;
}

// ============================================================================
// External nodes (list unindexed files/folders) tests
// ============================================================================

int test_folder_list_external_basic() {
  std::cout << "  Running test_folder_list_external_basic..." << std::endl;
  cleanup_test_dir(get_test_path("test_list_external_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_list_external_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create an indexed file through API
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "indexed.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  // Create an indexed folder through API
  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "indexed_folder", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  // Create unindexed file directly on filesystem
  std::string unindexed_file = get_test_path("test_list_external_nb") + "/external.txt";
  write_file(unindexed_file, "This is an external file.");
  ASSERT(path_exists(unindexed_file));

  // Create unindexed folder directly on filesystem
  std::string unindexed_folder = get_test_path("test_list_external_nb") + "/external_folder";
  create_directory(unindexed_folder);
  ASSERT(path_exists(unindexed_folder));

  // List external nodes
  char *external_json = nullptr;
  err = vxcore_folder_list_external(ctx, notebook_id, ".", &external_json);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(external_json);

  nlohmann::json result = nlohmann::json::parse(external_json);
  ASSERT(result.contains("files"));
  ASSERT(result.contains("folders"));

  // Should have 1 external file (external.txt)
  ASSERT_EQ(result["files"].size(), 1);
  ASSERT_EQ(result["files"][0]["name"].get<std::string>(), "external.txt");

  // Should have 1 external folder (external_folder)
  ASSERT_EQ(result["folders"].size(), 1);
  ASSERT_EQ(result["folders"][0]["name"].get<std::string>(), "external_folder");

  vxcore_string_free(external_json);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_list_external_nb"));
  std::cout << "  ✓ test_folder_list_external_basic passed" << std::endl;
  return 0;
}

int test_folder_list_external_empty() {
  std::cout << "  Running test_folder_list_external_empty..." << std::endl;
  cleanup_test_dir(get_test_path("test_list_external_empty_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_list_external_empty_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create only indexed items (no external files)
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "indexed.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  // List external nodes - should be empty
  char *external_json = nullptr;
  err = vxcore_folder_list_external(ctx, notebook_id, ".", &external_json);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(external_json);

  nlohmann::json result = nlohmann::json::parse(external_json);
  ASSERT_EQ(result["files"].size(), 0);
  ASSERT_EQ(result["folders"].size(), 0);

  vxcore_string_free(external_json);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_list_external_empty_nb"));
  std::cout << "  ✓ test_folder_list_external_empty passed" << std::endl;
  return 0;
}

int test_folder_list_external_nested() {
  std::cout << "  Running test_folder_list_external_nested..." << std::endl;
  cleanup_test_dir(get_test_path("test_list_external_nested_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_list_external_nested_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create indexed folder through API
  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "docs", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  // Create indexed file inside docs through API
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, "docs", "indexed.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  // Create external file in nested folder directly on filesystem
  std::string unindexed_file = get_test_path("test_list_external_nested_nb") + "/docs/external.txt";
  write_file(unindexed_file, "External file in nested folder.");
  ASSERT(path_exists(unindexed_file));

  // List external nodes in nested folder
  char *external_json = nullptr;
  err = vxcore_folder_list_external(ctx, notebook_id, "docs", &external_json);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(external_json);

  nlohmann::json result = nlohmann::json::parse(external_json);
  ASSERT_EQ(result["files"].size(), 1);
  ASSERT_EQ(result["files"][0]["name"].get<std::string>(), "external.txt");
  ASSERT_EQ(result["folders"].size(), 0);

  vxcore_string_free(external_json);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_list_external_nested_nb"));
  std::cout << "  ✓ test_folder_list_external_nested passed" << std::endl;
  return 0;
}

int test_folder_list_external_skips_hidden() {
  std::cout << "  Running test_folder_list_external_skips_hidden..." << std::endl;
  cleanup_test_dir(get_test_path("test_list_external_hidden_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_list_external_hidden_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create hidden file (should be skipped)
  std::string hidden_file = get_test_path("test_list_external_hidden_nb") + "/.hidden_file";
  write_file(hidden_file, "Hidden file.");
  ASSERT(path_exists(hidden_file));

  // Create hidden folder (should be skipped)
  std::string hidden_folder = get_test_path("test_list_external_hidden_nb") + "/.hidden_folder";
  create_directory(hidden_folder);
  ASSERT(path_exists(hidden_folder));

  // Create visible external file (should be listed)
  std::string visible_file = get_test_path("test_list_external_hidden_nb") + "/visible.txt";
  write_file(visible_file, "Visible file.");
  ASSERT(path_exists(visible_file));

  // List external nodes - should only have visible.txt
  char *external_json = nullptr;
  err = vxcore_folder_list_external(ctx, notebook_id, ".", &external_json);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(external_json);

  nlohmann::json result = nlohmann::json::parse(external_json);
  ASSERT_EQ(result["files"].size(), 1);
  ASSERT_EQ(result["files"][0]["name"].get<std::string>(), "visible.txt");
  ASSERT_EQ(result["folders"].size(), 0);  // hidden folder excluded

  vxcore_string_free(external_json);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_list_external_hidden_nb"));
  std::cout << "  ✓ test_folder_list_external_skips_hidden passed" << std::endl;
  return 0;
}

int test_folder_list_external_invalid_params() {
  std::cout << "  Running test_folder_list_external_invalid_params..." << std::endl;
  cleanup_test_dir(get_test_path("test_list_external_invalid_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_list_external_invalid_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *external_json = nullptr;

  // Null context
  err = vxcore_folder_list_external(nullptr, notebook_id, ".", &external_json);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  // Null notebook_id
  err = vxcore_folder_list_external(ctx, nullptr, ".", &external_json);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  // Null folder_path - allowed, defaults to root
  err = vxcore_folder_list_external(ctx, notebook_id, nullptr, &external_json);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(external_json);

  // Null output pointer
  err = vxcore_folder_list_external(ctx, notebook_id, ".", nullptr);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_list_external_invalid_nb"));
  std::cout << "  ✓ test_folder_list_external_invalid_params passed" << std::endl;
  return 0;
}

// ============================================================================
// vxcore_folder_get_available_name tests
// ============================================================================

int test_folder_get_available_name_basic() {
  std::cout << "  Running test_folder_get_available_name_basic..." << std::endl;
  cleanup_test_dir(get_test_path("test_avail_name_basic_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_avail_name_basic_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Name doesn't exist - should return original name
  char *available_name = nullptr;
  err = vxcore_folder_get_available_name(ctx, notebook_id, ".", "new_folder", &available_name);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(available_name);
  ASSERT_EQ(std::string(available_name), "new_folder");
  vxcore_string_free(available_name);

  // Same for file name
  available_name = nullptr;
  err = vxcore_folder_get_available_name(ctx, notebook_id, ".", "document.md", &available_name);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(available_name);
  ASSERT_EQ(std::string(available_name), "document.md");
  vxcore_string_free(available_name);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_avail_name_basic_nb"));
  std::cout << "  ✓ test_folder_get_available_name_basic passed" << std::endl;
  return 0;
}

int test_folder_get_available_name_conflict() {
  std::cout << "  Running test_folder_get_available_name_conflict..." << std::endl;
  cleanup_test_dir(get_test_path("test_avail_name_conflict_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_avail_name_conflict_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create existing folder
  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "existing_folder", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  // Now get available name for same name - should return existing_folder_1
  char *available_name = nullptr;
  err = vxcore_folder_get_available_name(ctx, notebook_id, ".", "existing_folder", &available_name);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(available_name);
  ASSERT_EQ(std::string(available_name), "existing_folder_1");
  vxcore_string_free(available_name);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_avail_name_conflict_nb"));
  std::cout << "  ✓ test_folder_get_available_name_conflict passed" << std::endl;
  return 0;
}

int test_folder_get_available_name_multiple_conflicts() {
  std::cout << "  Running test_folder_get_available_name_multiple_conflicts..." << std::endl;
  cleanup_test_dir(get_test_path("test_avail_name_multi_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_avail_name_multi_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create folder, folder_1, folder_2
  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "folder", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  err = vxcore_folder_create(ctx, notebook_id, ".", "folder_1", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  err = vxcore_folder_create(ctx, notebook_id, ".", "folder_2", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  // Get available name - should skip to folder_3
  char *available_name = nullptr;
  err = vxcore_folder_get_available_name(ctx, notebook_id, ".", "folder", &available_name);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(available_name);
  ASSERT_EQ(std::string(available_name), "folder_3");
  vxcore_string_free(available_name);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_avail_name_multi_nb"));
  std::cout << "  ✓ test_folder_get_available_name_multiple_conflicts passed" << std::endl;
  return 0;
}

int test_folder_get_available_name_with_extension() {
  std::cout << "  Running test_folder_get_available_name_with_extension..." << std::endl;
  cleanup_test_dir(get_test_path("test_avail_name_ext_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_avail_name_ext_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create existing file with extension
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "document.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  // Get available name - should return document_1.md (suffix before extension)
  char *available_name = nullptr;
  err = vxcore_folder_get_available_name(ctx, notebook_id, ".", "document.md", &available_name);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(available_name);
  ASSERT_EQ(std::string(available_name), "document_1.md");
  vxcore_string_free(available_name);

  // Create document_1.md
  err = vxcore_file_create(ctx, notebook_id, ".", "document_1.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  // Get available name again - should return document_2.md
  available_name = nullptr;
  err = vxcore_folder_get_available_name(ctx, notebook_id, ".", "document.md", &available_name);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(available_name);
  ASSERT_EQ(std::string(available_name), "document_2.md");
  vxcore_string_free(available_name);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_avail_name_ext_nb"));
  std::cout << "  ✓ test_folder_get_available_name_with_extension passed" << std::endl;
  return 0;
}

int test_folder_get_available_name_in_subfolder() {
  std::cout << "  Running test_folder_get_available_name_in_subfolder..." << std::endl;
  cleanup_test_dir(get_test_path("test_avail_name_subfolder_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_avail_name_subfolder_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create parent folder
  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "parent", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  // Create child in parent
  err = vxcore_folder_create(ctx, notebook_id, "parent", "child", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  // Get available name in parent - should return child_1
  char *available_name = nullptr;
  err = vxcore_folder_get_available_name(ctx, notebook_id, "parent", "child", &available_name);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(available_name);
  ASSERT_EQ(std::string(available_name), "child_1");
  vxcore_string_free(available_name);

  // Same name in root should be available (no conflict)
  available_name = nullptr;
  err = vxcore_folder_get_available_name(ctx, notebook_id, ".", "child", &available_name);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(available_name);
  ASSERT_EQ(std::string(available_name), "child");
  vxcore_string_free(available_name);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_avail_name_subfolder_nb"));
  std::cout << "  ✓ test_folder_get_available_name_in_subfolder passed" << std::endl;
  return 0;
}

int test_folder_get_available_name_invalid_params() {
  std::cout << "  Running test_folder_get_available_name_invalid_params..." << std::endl;
  cleanup_test_dir(get_test_path("test_avail_name_invalid_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_avail_name_invalid_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *available_name = nullptr;

  // Null context
  err = vxcore_folder_get_available_name(nullptr, notebook_id, ".", "name", &available_name);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  // Null notebook_id
  err = vxcore_folder_get_available_name(ctx, nullptr, ".", "name", &available_name);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  // Null new_name
  err = vxcore_folder_get_available_name(ctx, notebook_id, ".", nullptr, &available_name);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  // Null output pointer
  err = vxcore_folder_get_available_name(ctx, notebook_id, ".", "name", nullptr);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  // Null folder_path - should be allowed (defaults to root)
  err = vxcore_folder_get_available_name(ctx, notebook_id, nullptr, "name", &available_name);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(available_name);
  ASSERT_EQ(std::string(available_name), "name");
  vxcore_string_free(available_name);

  // Non-existent notebook
  err = vxcore_folder_get_available_name(ctx, "non_existent_id", ".", "name", &available_name);
  ASSERT_EQ(err, VXCORE_ERR_NOT_FOUND);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_avail_name_invalid_nb"));
  std::cout << "  ✓ test_folder_get_available_name_invalid_params passed" << std::endl;
  return 0;
}

int test_node_attachments_basic() {
  std::cout << "  Running test_node_attachments_basic..." << std::endl;
  cleanup_test_dir(get_test_path("test_node_attachments_nb"));
  create_directory(get_test_path("test_node_attachments_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_node_attachments_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "note.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  // Insert attachment via buffer API to set up metadata + filesystem.
  char *buffer_id = nullptr;
  err = vxcore_buffer_open(ctx, notebook_id, "note.md", &buffer_id);
  ASSERT_EQ(err, VXCORE_OK);

  std::string source_path = get_test_path("test_node_attachments_nb") + "/doc.pdf";
  write_file(source_path, "PDF content");

  char *filename = nullptr;
  err = vxcore_buffer_insert_attachment(ctx, buffer_id, source_path.c_str(), &filename);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(filename);
  ASSERT_EQ(std::string(filename), "doc.pdf");

  // Verify node-level get attachments folder.
  char *attachments_folder = nullptr;
  err = vxcore_node_get_attachments_folder(ctx, notebook_id, "note.md", &attachments_folder);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(attachments_folder);
  ASSERT_TRUE(path_exists(attachments_folder));
  ASSERT_TRUE(path_exists(std::string(attachments_folder) + "/doc.pdf"));

  // Verify node-level list attachments.
  char *attachments_json = nullptr;
  err = vxcore_node_list_attachments(ctx, notebook_id, "note.md", &attachments_json);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(attachments_json);
  auto json = nlohmann::json::parse(attachments_json);
  ASSERT_TRUE(json.is_array());
  ASSERT_EQ(json.size(), 1u);
  ASSERT_EQ(json[0].get<std::string>(), "doc.pdf");

  vxcore_string_free(attachments_json);
  vxcore_string_free(attachments_folder);
  vxcore_string_free(filename);
  vxcore_string_free(buffer_id);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_node_attachments_nb"));
  std::cout << "  ✓ test_node_attachments_basic passed" << std::endl;
  return 0;
}

int test_node_attachments_invalid_params() {
  std::cout << "  Running test_node_attachments_invalid_params..." << std::endl;
  cleanup_test_dir(get_test_path("test_node_attachments_invalid_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_node_attachments_invalid_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "note.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  char *path = nullptr;
  err = vxcore_node_get_attachments_folder(nullptr, notebook_id, "note.md", &path);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = vxcore_node_get_attachments_folder(ctx, nullptr, "note.md", &path);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = vxcore_node_get_attachments_folder(ctx, notebook_id, nullptr, &path);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = vxcore_node_get_attachments_folder(ctx, notebook_id, "note.md", nullptr);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = vxcore_node_get_attachments_folder(ctx, "non_existent_id", "note.md", &path);
  ASSERT_EQ(err, VXCORE_ERR_NOT_FOUND);
  ASSERT_NULL(path);

  char *attachments_json = nullptr;
  err = vxcore_node_list_attachments(nullptr, notebook_id, "note.md", &attachments_json);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = vxcore_node_list_attachments(ctx, nullptr, "note.md", &attachments_json);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = vxcore_node_list_attachments(ctx, notebook_id, nullptr, &attachments_json);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = vxcore_node_list_attachments(ctx, notebook_id, "note.md", nullptr);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = vxcore_node_list_attachments(ctx, "non_existent_id", "note.md", &attachments_json);
  ASSERT_EQ(err, VXCORE_ERR_NOT_FOUND);
  ASSERT_NULL(attachments_json);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_node_attachments_invalid_nb"));
  std::cout << "  ✓ test_node_attachments_invalid_params passed" << std::endl;
  return 0;
}

int test_file_peek() {
  std::cout << "  Running test_file_peek..." << std::endl;
  cleanup_test_dir(get_test_path("test_file_peek_nb"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err =
      vxcore_notebook_create(ctx, get_test_path("test_file_peek_nb").c_str(),
                             "{\"name\":\"Test Notebook\"}", VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a file and write content
  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "note.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  // Write content: 80 chars
  std::string long_content =
      "0123456789012345678901234567890123456789012345678901234567890123456789ABCDEFGHIJ";
  std::string file_path = get_test_path("test_file_peek_nb") + "/note.md";
  std::ofstream ofs(file_path);
  ofs << long_content;
  ofs.close();

  // Test 1: Peek with max_bytes=64 (truncates 80 char content)
  char *content = nullptr;
  err = vxcore_file_peek(ctx, notebook_id, "note.md", 64, &content);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(content);
  ASSERT_EQ(strlen(content), 64);
  ASSERT_EQ(std::string(content), long_content.substr(0, 64));
  vxcore_string_free(content);

  // Test 2: Peek with max_bytes=32 (smaller than content)
  char *content1b = nullptr;
  err = vxcore_file_peek(ctx, notebook_id, "note.md", 32, &content1b);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(content1b);
  ASSERT_EQ(strlen(content1b), 32);
  ASSERT_EQ(std::string(content1b), long_content.substr(0, 32));
  vxcore_string_free(content1b);

  // Test 3: Peek with max_bytes=128 (larger than content)
  char *content1c = nullptr;
  err = vxcore_file_peek(ctx, notebook_id, "note.md", 128, &content1c);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(content1c);
  ASSERT_EQ(strlen(content1c), 80);  // Returns full content
  ASSERT_EQ(std::string(content1c), long_content);
  vxcore_string_free(content1c);

  // Test 4: Peek file with content shorter than requested
  char *file_id2 = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "short.md", &file_id2);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id2);

  std::string short_content = "Hello World";
  std::string short_file_path = get_test_path("test_file_peek_nb") + "/short.md";
  std::ofstream ofs2(short_file_path);
  ofs2 << short_content;
  ofs2.close();

  char *content2 = nullptr;
  err = vxcore_file_peek(ctx, notebook_id, "short.md", 64, &content2);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(content2);
  ASSERT_EQ(std::string(content2), short_content);
  vxcore_string_free(content2);

  // Test 5: Peek non-existent file
  char *content3 = nullptr;
  err = vxcore_file_peek(ctx, notebook_id, "nonexistent.md", 64, &content3);
  ASSERT_EQ(err, VXCORE_ERR_NOT_FOUND);
  ASSERT_NULL(content3);

  // Test 6: Invalid params - null context
  char *content4 = nullptr;
  err = vxcore_file_peek(nullptr, notebook_id, "note.md", 64, &content4);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  // Test 7: Invalid params - null notebook_id
  char *content5 = nullptr;
  err = vxcore_file_peek(ctx, nullptr, "note.md", 64, &content5);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  // Test 8: Invalid params - null file_path
  char *content6 = nullptr;
  err = vxcore_file_peek(ctx, notebook_id, nullptr, 64, &content6);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  // Test 9: Invalid params - null out_content
  err = vxcore_file_peek(ctx, notebook_id, "note.md", 64, nullptr);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_file_peek_nb"));
  std::cout << "  \xe2\x9c\x93 test_file_peek passed" << std::endl;
  return 0;
}

int main() {
  std::cout << "Running folder tests..." << std::endl;

  vxcore_set_test_mode(1);
  vxcore_clear_test_directory();

  RUN_TEST(test_folder_create);
  RUN_TEST(test_file_create);
  RUN_TEST(test_file_metadata_and_tags);
  RUN_TEST(test_folder_delete);
  RUN_TEST(test_folder_create_duplicate);
  RUN_TEST(test_file_create_duplicate);
  RUN_TEST(test_folder_invalid_params);
  RUN_TEST(test_file_invalid_params);
  RUN_TEST(test_folder_not_found);
  RUN_TEST(test_file_not_found);
  RUN_TEST(test_invalid_json);
  RUN_TEST(test_notebook_not_found);
  RUN_TEST(test_file_delete);
  RUN_TEST(test_nested_folder_operations);
  RUN_TEST(test_folder_rename);
  RUN_TEST(test_folder_move);
  RUN_TEST(test_folder_copy);
  RUN_TEST(test_file_rename);
  RUN_TEST(test_file_move);
  RUN_TEST(test_file_copy);
  RUN_TEST(test_file_get_info);
  RUN_TEST(test_folder_get_metadata);
  RUN_TEST(test_folder_copy_invalid_params);
  RUN_TEST(test_folder_copy_not_found);
  RUN_TEST(test_folder_copy_already_exists);
  RUN_TEST(test_file_copy_invalid_params);
  RUN_TEST(test_file_copy_not_found);
  RUN_TEST(test_file_copy_already_exists);
  RUN_TEST(test_file_tag);
  RUN_TEST(test_file_untag);
  RUN_TEST(test_file_tag_duplicate);
  RUN_TEST(test_file_untag_not_found);
  RUN_TEST(test_file_tag_invalid_params);
  RUN_TEST(test_folder_create_path_basic);
  RUN_TEST(test_folder_create_path_single);
  RUN_TEST(test_folder_create_path_partial_exists);
  RUN_TEST(test_folder_create_path_empty);
  RUN_TEST(test_folder_create_path_trailing_slash);

  // MetadataStore sync integration tests
  RUN_TEST(test_metadata_store_sync_on_reopen);
  RUN_TEST(test_metadata_store_write_through);
  RUN_TEST(test_metadata_store_copy_preserves_data);

  // SyncFolderToStore enhancement tests
  RUN_TEST(test_sync_recursive_folders);
  RUN_TEST(test_sync_orphan_file_deletion);
  RUN_TEST(test_sync_file_metadata_update);
  RUN_TEST(test_sync_new_file_creation);
  RUN_TEST(test_sync_folder_metadata_update);
  RUN_TEST(test_sync_staleness_check);

  // Recycle bin tests
  RUN_TEST(test_recycle_bin_file_delete);
  RUN_TEST(test_recycle_bin_folder_delete);
  RUN_TEST(test_recycle_bin_name_conflict);
  RUN_TEST(test_recycle_bin_get_path);
  RUN_TEST(test_recycle_bin_empty);
  RUN_TEST(test_recycle_bin_raw_notebook_unsupported);

  // Node index/unindex tests
  RUN_TEST(test_node_index_file);
  RUN_TEST(test_node_index_folder);
  RUN_TEST(test_node_unindex_file);
  RUN_TEST(test_node_unindex_folder);
  RUN_TEST(test_node_index_not_found);
  RUN_TEST(test_node_index_already_exists);
  RUN_TEST(test_node_unindex_not_found);
  RUN_TEST(test_node_index_invalid_params);
  RUN_TEST(test_node_index_nested);

  // Node path lookup tests
  RUN_TEST(test_node_get_path_by_id);
  RUN_TEST(test_node_resolve_by_id);
  // File import tests
  RUN_TEST(test_file_import_basic);
  RUN_TEST(test_file_import_name_conflict);
  RUN_TEST(test_file_import_source_not_found);
  RUN_TEST(test_file_import_invalid_params);
  RUN_TEST(test_file_import_to_subfolder);

  // Folder import tests
  RUN_TEST(test_folder_import_basic);
  RUN_TEST(test_folder_import_name_conflict);
  RUN_TEST(test_folder_import_source_not_found);
  RUN_TEST(test_folder_import_invalid_params);
  RUN_TEST(test_folder_import_to_subfolder);
  RUN_TEST(test_folder_import_within_notebook);
  RUN_TEST(test_folder_import_suffix_filter);

  // External nodes (list unindexed) tests
  RUN_TEST(test_folder_list_external_basic);
  RUN_TEST(test_folder_list_external_empty);
  RUN_TEST(test_folder_list_external_nested);
  RUN_TEST(test_folder_list_external_skips_hidden);
  RUN_TEST(test_folder_list_external_invalid_params);
  // Get available name tests
  RUN_TEST(test_folder_get_available_name_basic);
  RUN_TEST(test_folder_get_available_name_conflict);
  RUN_TEST(test_folder_get_available_name_multiple_conflicts);
  RUN_TEST(test_folder_get_available_name_with_extension);
  RUN_TEST(test_folder_get_available_name_in_subfolder);
  RUN_TEST(test_folder_get_available_name_invalid_params);

  // Node attachments tests
  RUN_TEST(test_node_attachments_basic);
  RUN_TEST(test_node_attachments_invalid_params);

  // File peek tests
  RUN_TEST(test_file_peek);

  std::cout << "✓ All folder tests passed" << std::endl;
  return 0;
}
