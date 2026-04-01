#include <iostream>
#include <map>
#include <string>

#include "nlohmann/json.hpp"
#include "test_utils.h"
#include "vxcore/vxcore.h"

int test_search_files_basic() {
  std::cout << "  Running test_search_files_basic..." << std::endl;
  cleanup_test_dir(get_test_path("test_search_basic"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_search_basic").c_str(),
                               "{\"name\":\"Test Search Basic\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file1_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "file1.md", &file1_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file1_id);

  char *file2_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "file2.txt", &file2_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file2_id);

  char *file3_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "notes.md", &file3_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file3_id);

  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "subfolder", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  const char *query_json = R"({
    "pattern": "*.md",
    "includeFiles": true,
    "includeFolders": false,
    "maxResults": 100,
    "scope": {
      "folderPath": ".",
      "recursive": false
    }
  })";

  char *results = nullptr;
  err = vxcore_search_files(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT(json_results["matchCount"].get<int>() == 2);
  ASSERT(json_results["matches"].is_array());
  ASSERT(json_results["matches"].size() == 2);

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_search_basic"));
  std::cout << "  âœ“ test_search_files_basic passed" << std::endl;
  return 0;
}

int test_search_files_include_folders() {
  std::cout << "  Running test_search_files_include_folders..." << std::endl;
  cleanup_test_dir(get_test_path("test_search_folders"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_search_folders").c_str(),
                               "{\"name\":\"Test Search Folders\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file1_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "file1.md", &file1_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file1_id);

  char *docs_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "docs", &docs_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(docs_id);

  char *images_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "images", &images_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(images_id);

  const char *query_json = R"({
    "pattern": "*",
    "includeFiles": true,
    "includeFolders": true,
    "maxResults": 100,
    "scope": {
      "folderPath": ".",
      "recursive": false
    }
  })";

  char *results = nullptr;
  err = vxcore_search_files(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT(json_results["matchCount"].get<int>() == 3);

  int file_count = 0;
  int folder_count = 0;
  for (const auto &item : json_results["matches"]) {
    if (item["type"].get<std::string>() == "folder") {
      folder_count++;
    } else {
      file_count++;
    }
  }
  ASSERT(file_count == 1);
  ASSERT(folder_count == 2);

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_search_folders"));
  std::cout << "  âœ“ test_search_files_include_folders passed" << std::endl;
  return 0;
}

int test_search_files_pattern_matching() {
  std::cout << "  Running test_search_files_pattern_matching..." << std::endl;
  cleanup_test_dir(get_test_path("test_search_pattern"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_search_pattern").c_str(),
                               "{\"name\":\"Test Search Pattern\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *readme_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "readme.md", &readme_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(readme_id);

  char *todo_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "todo.txt", &todo_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(todo_id);

  char *notes_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "notes.md", &notes_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(notes_id);

  char *subfolder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "subfolder", &subfolder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(subfolder_id);

  char *deep_readme_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, "subfolder", "deep_readme.md", &deep_readme_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(deep_readme_id);

  const char *query_json = R"({
    "pattern": "readme",
    "includeFiles": true,
    "includeFolders": false,
    "maxResults": 100,
    "scope": {
      "folderPath": ".",
      "recursive": true
    }
  })";

  char *results = nullptr;
  err = vxcore_search_files(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT(json_results["matchCount"].get<int>() == 2);

  bool found_readme = false;
  bool found_deep_readme = false;
  for (const auto &item : json_results["matches"]) {
    std::string path = item["path"].get<std::string>();
    if (path.find("readme.md") != std::string::npos) {
      found_readme = true;
    }
    if (path.find("deep_readme.md") != std::string::npos) {
      found_deep_readme = true;
    }
  }
  ASSERT(found_readme);
  ASSERT(found_deep_readme);

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_search_pattern"));
  std::cout << "  âœ“ test_search_files_pattern_matching passed" << std::endl;
  return 0;
}

int test_search_files_name_vs_path_ranking() {
  std::cout << "  Running test_search_files_name_vs_path_ranking..." << std::endl;
  cleanup_test_dir(get_test_path("test_search_ranking"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_search_ranking").c_str(),
                               "{\"name\":\"Test Search Ranking\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *other_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "other.md", &other_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(other_id);

  char *readme_folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "readme", &readme_folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(readme_folder_id);

  char *file_in_readme_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, "readme", "file.md", &file_in_readme_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_in_readme_id);

  char *readme_file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "readme.md", &readme_file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(readme_file_id);

  const char *query_json = R"({
    "pattern": "readme",
    "includeFiles": true,
    "includeFolders": true,
    "maxResults": 100,
    "scope": {
      "folderPath": ".",
      "recursive": true
    }
  })";

  char *results = nullptr;
  err = vxcore_search_files(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT(json_results["matchCount"].get<int>() >= 2);

  int first_name_match_index = -1;
  int first_path_match_index = -1;

  for (size_t i = 0; i < json_results["matches"].size(); i++) {
    std::string path = json_results["matches"][i]["path"].get<std::string>();
    size_t last_slash = path.find_last_of('/');
    std::string name = (last_slash == std::string::npos) ? path : path.substr(last_slash + 1);

    bool name_has_readme = (name.find("readme") != std::string::npos);
    bool path_has_readme = (path.find("readme") != std::string::npos);

    if (name_has_readme && first_name_match_index == -1) {
      first_name_match_index = static_cast<int>(i);
    }
    if (!name_has_readme && path_has_readme && first_path_match_index == -1) {
      first_path_match_index = static_cast<int>(i);
    }
  }

  if (first_name_match_index != -1 && first_path_match_index != -1) {
    ASSERT(first_name_match_index < first_path_match_index);
  }

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_search_ranking"));
  std::cout << "  âœ“ test_search_files_name_vs_path_ranking passed" << std::endl;
  return 0;
}

int test_search_files_exclude_patterns() {
  std::cout << "  Running test_search_files_exclude_patterns..." << std::endl;
  cleanup_test_dir(get_test_path("test_search_exclude"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_search_exclude").c_str(),
                               "{\"name\":\"Test Search Exclude\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file1_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "file1.md", &file1_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file1_id);

  char *file2_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "file2.md", &file2_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file2_id);

  char *node_modules_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "node_modules", &node_modules_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(node_modules_id);

  char *lib_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, "node_modules", "lib.js", &lib_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(lib_id);

  const char *query_json = R"({
    "pattern": "*",
    "includeFiles": true,
    "includeFolders": false,
    "maxResults": 100,
    "scope": {
      "folderPath": ".",
      "recursive": true,
      "excludePatterns": ["node_modules"]
    }
  })";

  char *results = nullptr;
  err = vxcore_search_files(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT(json_results["matchCount"].get<int>() == 2);

  for (const auto &item : json_results["matches"]) {
    std::string path = item["path"].get<std::string>();
    ASSERT(path.find("node_modules") == std::string::npos);
  }

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_search_exclude"));
  std::cout << "  âœ“ test_search_files_exclude_patterns passed" << std::endl;
  return 0;
}

int test_search_files_max_results() {
  std::cout << "  Running test_search_files_max_results..." << std::endl;
  cleanup_test_dir(get_test_path("test_search_max"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_search_max").c_str(),
                               "{\"name\":\"Test Search Max\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  for (int i = 1; i <= 10; i++) {
    char *file_id = nullptr;
    std::string filename = "file" + std::to_string(i) + ".md";
    err = vxcore_file_create(ctx, notebook_id, ".", filename.c_str(), &file_id);
    ASSERT_EQ(err, VXCORE_OK);
    vxcore_string_free(file_id);
  }

  const char *query_json = R"({
    "pattern": "*.md",
    "includeFiles": true,
    "includeFolders": false,
    "maxResults": 5,
    "scope": {
      "folderPath": ".",
      "recursive": false
    }
  })";

  char *results = nullptr;
  err = vxcore_search_files(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT(json_results["matchCount"].get<int>() == 5);
  ASSERT(json_results["matches"].size() == 5);
  ASSERT(json_results["truncated"].get<bool>() == true);

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_search_max"));
  std::cout << "  âœ“ test_search_files_max_results passed" << std::endl;
  return 0;
}

int test_search_by_tags_single_tag() {
  std::cout << "  Running test_search_by_tags_single_tag..." << std::endl;
  cleanup_test_dir(get_test_path("test_search_tags_single"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_search_tags_single").c_str(),
                               "{\"name\":\"Test Search Tags Single\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file1_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "file1.md", &file1_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file1_id);

  char *file2_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "file2.md", &file2_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file2_id);

  char *file3_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "file3.md", &file3_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file3_id);

  err = vxcore_tag_create(ctx, notebook_id, "important");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_tag_create(ctx, notebook_id, "todo");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "file1.md", "important");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "file2.md", "important");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "file3.md", "todo");
  ASSERT_EQ(err, VXCORE_OK);

  const char *query_json = R"({
    "tags": ["important"],
    "operator": "AND",
    "maxResults": 100,
    "scope": {
      "folderPath": ".",
      "recursive": false
    }
  })";

  char *results = nullptr;
  err = vxcore_search_by_tags(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT(json_results["matchCount"].get<int>() == 2);
  ASSERT(json_results["matches"].size() == 2);

  for (const auto &item : json_results["matches"]) {
    ASSERT(item["type"].get<std::string>() == "file");
    auto tags = item["tags"];
    ASSERT(tags.is_array());
    ASSERT(std::find(tags.begin(), tags.end(), "important") != tags.end());
  }

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_search_tags_single"));
  std::cout << "  âœ“ test_search_by_tags_single_tag passed" << std::endl;
  return 0;
}

int test_search_by_tags_and_operator() {
  std::cout << "  Running test_search_by_tags_and_operator..." << std::endl;
  cleanup_test_dir(get_test_path("test_search_tags_and"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_search_tags_and").c_str(),
                               "{\"name\":\"Test Search Tags AND\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file1_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "file1.md", &file1_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file1_id);

  char *file2_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "file2.md", &file2_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file2_id);

  char *file3_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "file3.md", &file3_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file3_id);

  err = vxcore_tag_create(ctx, notebook_id, "important");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_tag_create(ctx, notebook_id, "urgent");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "file1.md", "important");
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_file_tag(ctx, notebook_id, "file1.md", "urgent");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "file2.md", "important");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "file3.md", "urgent");
  ASSERT_EQ(err, VXCORE_OK);

  const char *query_json = R"({
    "tags": ["important", "urgent"],
    "operator": "AND",
    "maxResults": 100,
    "scope": {
      "folderPath": ".",
      "recursive": false
    }
  })";

  char *results = nullptr;
  err = vxcore_search_by_tags(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT(json_results["matchCount"].get<int>() == 1);
  ASSERT(json_results["matches"].size() == 1);

  auto &item = json_results["matches"][0];
  std::string path = item["path"].get<std::string>();
  ASSERT(path.find("file1.md") != std::string::npos);

  auto tags = item["tags"];
  ASSERT(std::find(tags.begin(), tags.end(), "important") != tags.end());
  ASSERT(std::find(tags.begin(), tags.end(), "urgent") != tags.end());

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_search_tags_and"));
  std::cout << "  âœ“ test_search_by_tags_and_operator passed" << std::endl;
  return 0;
}

int test_search_by_tags_or_operator() {
  std::cout << "  Running test_search_by_tags_or_operator..." << std::endl;
  cleanup_test_dir(get_test_path("test_search_tags_or"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_search_tags_or").c_str(),
                               "{\"name\":\"Test Search Tags OR\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file1_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "file1.md", &file1_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file1_id);

  char *file2_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "file2.md", &file2_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file2_id);

  char *file3_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "file3.md", &file3_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file3_id);

  err = vxcore_tag_create(ctx, notebook_id, "important");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_tag_create(ctx, notebook_id, "urgent");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_tag_create(ctx, notebook_id, "todo");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "file1.md", "important");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "file2.md", "urgent");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "file3.md", "todo");
  ASSERT_EQ(err, VXCORE_OK);

  const char *query_json = R"({
    "tags": ["important", "urgent"],
    "operator": "OR",
    "maxResults": 100,
    "scope": {
      "folderPath": ".",
      "recursive": false
    }
  })";

  char *results = nullptr;
  err = vxcore_search_by_tags(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT(json_results["matchCount"].get<int>() == 2);
  ASSERT(json_results["matches"].size() == 2);

  bool found_file1 = false;
  bool found_file2 = false;
  for (const auto &item : json_results["matches"]) {
    std::string path = item["path"].get<std::string>();
    if (path.find("file1.md") != std::string::npos) {
      found_file1 = true;
    }
    if (path.find("file2.md") != std::string::npos) {
      found_file2 = true;
    }
  }
  ASSERT(found_file1);
  ASSERT(found_file2);

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_search_tags_or"));
  std::cout << "  âœ“ test_search_by_tags_or_operator passed" << std::endl;
  return 0;
}

int test_search_by_tags_recursive() {
  std::cout << "  Running test_search_by_tags_recursive..." << std::endl;
  cleanup_test_dir(get_test_path("test_search_tags_recursive"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_search_tags_recursive").c_str(),
                               "{\"name\":\"Test Search Tags Recursive\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file1_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "file1.md", &file1_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file1_id);

  char *subfolder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "subfolder", &subfolder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(subfolder_id);

  char *file2_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, "subfolder", "file2.md", &file2_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file2_id);

  err = vxcore_tag_create(ctx, notebook_id, "project");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "file1.md", "project");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "subfolder/file2.md", "project");
  ASSERT_EQ(err, VXCORE_OK);

  const char *query_json = R"({
    "tags": ["project"],
    "operator": "AND",
    "maxResults": 100,
    "scope": {
      "folderPath": ".",
      "recursive": true
    }
  })";

  char *results = nullptr;
  err = vxcore_search_by_tags(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT(json_results["matchCount"].get<int>() == 2);

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_search_tags_recursive"));
  std::cout << "  âœ“ test_search_by_tags_recursive passed" << std::endl;
  return 0;
}

int test_search_by_tags_with_exclude_patterns() {
  std::cout << "  Running test_search_by_tags_with_exclude_patterns..." << std::endl;
  cleanup_test_dir(get_test_path("test_search_tags_exclude"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_search_tags_exclude").c_str(),
                               "{\"name\":\"Test Search Tags Exclude\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file1_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "file1.md", &file1_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file1_id);

  char *archive_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "archive", &archive_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(archive_id);

  char *file2_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, "archive", "file2.md", &file2_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file2_id);

  err = vxcore_tag_create(ctx, notebook_id, "important");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "file1.md", "important");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "archive/file2.md", "important");
  ASSERT_EQ(err, VXCORE_OK);

  const char *query_json = R"({
    "tags": ["important"],
    "operator": "AND",
    "maxResults": 100,
    "scope": {
      "folderPath": ".",
      "recursive": true,
      "excludePatterns": ["archive"]
    }
  })";

  char *results = nullptr;
  err = vxcore_search_by_tags(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT(json_results["matchCount"].get<int>() == 1);

  auto &item = json_results["matches"][0];
  std::string path = item["path"].get<std::string>();
  ASSERT(path.find("file1.md") != std::string::npos);
  ASSERT(path.find("archive") == std::string::npos);

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_search_tags_exclude"));
  std::cout << "  âœ“ test_search_by_tags_with_exclude_patterns passed" << std::endl;
  return 0;
}

int test_search_by_tags_max_results() {
  std::cout << "  Running test_search_by_tags_max_results..." << std::endl;
  cleanup_test_dir(get_test_path("test_search_tags_max"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_search_tags_max").c_str(),
                               "{\"name\":\"Test Search Tags Max\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_tag_create(ctx, notebook_id, "test");
  ASSERT_EQ(err, VXCORE_OK);

  for (int i = 1; i <= 10; i++) {
    char *file_id = nullptr;
    std::string filename = "file" + std::to_string(i) + ".md";
    err = vxcore_file_create(ctx, notebook_id, ".", filename.c_str(), &file_id);
    ASSERT_EQ(err, VXCORE_OK);
    vxcore_string_free(file_id);

    err = vxcore_file_tag(ctx, notebook_id, filename.c_str(), "test");
    ASSERT_EQ(err, VXCORE_OK);
  }

  const char *query_json = R"({
    "tags": ["test"],
    "operator": "AND",
    "maxResults": 5,
    "scope": {
      "folderPath": ".",
      "recursive": false
    }
  })";

  char *results = nullptr;
  err = vxcore_search_by_tags(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT(json_results["matchCount"].get<int>() == 5);
  ASSERT(json_results["matches"].size() == 5);
  ASSERT(json_results["truncated"].get<bool>() == true);

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_search_tags_max"));
  std::cout << "  âœ“ test_search_by_tags_max_results passed" << std::endl;
  return 0;
}

int test_search_by_tags_empty_results() {
  std::cout << "  Running test_search_by_tags_empty_results..." << std::endl;
  cleanup_test_dir(get_test_path("test_search_tags_empty"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_search_tags_empty").c_str(),
                               "{\"name\":\"Test Search Tags Empty\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file1_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "file1.md", &file1_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file1_id);

  err = vxcore_tag_create(ctx, notebook_id, "existing");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "file1.md", "existing");
  ASSERT_EQ(err, VXCORE_OK);

  const char *query_json = R"({
    "tags": ["nonexistent"],
    "operator": "AND",
    "maxResults": 100,
    "scope": {
      "folderPath": ".",
      "recursive": false
    }
  })";

  char *results = nullptr;
  err = vxcore_search_by_tags(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT(json_results["matchCount"].get<int>() == 0);
  ASSERT(json_results["matches"].size() == 0);
  ASSERT(json_results["truncated"].get<bool>() == false);

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_search_tags_empty"));
  std::cout << "  âœ“ test_search_by_tags_empty_results passed" << std::endl;
  return 0;
}

int test_search_files_with_file_patterns() {
  std::cout << "  Running test_search_files_with_file_patterns..." << std::endl;
  cleanup_test_dir(get_test_path("test_search_file_patterns"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_search_file_patterns").c_str(),
                               "{\"name\":\"Test Search File Patterns\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file1_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "readme.md", &file1_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file1_id);

  char *file2_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "todo.txt", &file2_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file2_id);

  char *file3_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "notes.md", &file3_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file3_id);

  char *file4_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "script.py", &file4_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file4_id);

  const char *query_json = R"({
    "pattern": "*",
    "includeFiles": true,
    "includeFolders": false,
    "maxResults": 100,
    "scope": {
      "folderPath": ".",
      "recursive": false,
      "filePatterns": ["*.md"]
    }
  })";

  char *results = nullptr;
  err = vxcore_search_files(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT(json_results["matchCount"].get<int>() == 2);

  for (const auto &item : json_results["matches"]) {
    std::string path = item["path"].get<std::string>();
    ASSERT(path.find(".md") != std::string::npos);
  }

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_search_file_patterns"));
  std::cout << "  âœ“ test_search_files_with_file_patterns passed" << std::endl;
  return 0;
}

int test_search_files_file_patterns_recursive() {
  std::cout << "  Running test_search_files_file_patterns_recursive..." << std::endl;
  cleanup_test_dir(get_test_path("test_search_file_patterns_recursive"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_search_file_patterns_recursive").c_str(),
                               "{\"name\":\"Test Search File Patterns Recursive\"}",
                               VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file1_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "root.cpp", &file1_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file1_id);

  char *file2_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "root.h", &file2_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file2_id);

  char *file3_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "readme.md", &file3_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file3_id);

  char *subfolder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "src", &subfolder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(subfolder_id);

  char *file4_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, "src", "main.cpp", &file4_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file4_id);

  char *file5_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, "src", "utils.h", &file5_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file5_id);

  char *file6_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, "src", "notes.txt", &file6_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file6_id);

  const char *query_json = R"({
    "pattern": "*",
    "includeFiles": true,
    "includeFolders": false,
    "maxResults": 100,
    "scope": {
      "folderPath": ".",
      "recursive": true,
      "filePatterns": ["*.cpp", "*.h"]
    }
  })";

  char *results = nullptr;
  err = vxcore_search_files(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT(json_results["matchCount"].get<int>() == 4);

  for (const auto &item : json_results["matches"]) {
    std::string path = item["path"].get<std::string>();
    bool is_cpp = path.find(".cpp") != std::string::npos;
    bool is_h = path.find(".h") != std::string::npos;
    ASSERT(is_cpp || is_h);
  }

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_search_file_patterns_recursive"));
  std::cout << "  âœ“ test_search_files_file_patterns_recursive passed" << std::endl;
  return 0;
}

int test_search_by_tags_with_file_patterns() {
  std::cout << "  Running test_search_by_tags_with_file_patterns..." << std::endl;
  cleanup_test_dir(get_test_path("test_search_tags_file_patterns"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_search_tags_file_patterns").c_str(),
                               "{\"name\":\"Test Search Tags File Patterns\"}",
                               VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file1_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "doc1.md", &file1_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file1_id);

  char *file2_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "doc2.txt", &file2_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file2_id);

  char *file3_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "doc3.md", &file3_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file3_id);

  err = vxcore_tag_create(ctx, notebook_id, "important");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "doc1.md", "important");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "doc2.txt", "important");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "doc3.md", "important");
  ASSERT_EQ(err, VXCORE_OK);

  const char *query_json = R"({
    "tags": ["important"],
    "operator": "AND",
    "maxResults": 100,
    "scope": {
      "folderPath": ".",
      "recursive": false,
      "filePatterns": ["*.md"]
    }
  })";

  char *results = nullptr;
  err = vxcore_search_by_tags(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT(json_results["matchCount"].get<int>() == 2);

  for (const auto &item : json_results["matches"]) {
    std::string path = item["path"].get<std::string>();
    ASSERT(path.find(".md") != std::string::npos);
    auto tags = item["tags"];
    ASSERT(std::find(tags.begin(), tags.end(), "important") != tags.end());
  }

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_search_tags_file_patterns"));
  std::cout << "  âœ“ test_search_by_tags_with_file_patterns passed" << std::endl;
  return 0;
}

int test_search_file_and_exclude_patterns() {
  std::cout << "  Running test_search_file_and_exclude_patterns..." << std::endl;
  cleanup_test_dir(get_test_path("test_search_include_exclude"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_search_include_exclude").c_str(),
                               "{\"name\":\"Test Search Include Exclude\"}",
                               VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file1_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "test.cpp", &file1_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file1_id);

  char *file2_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "main.cpp", &file2_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file2_id);

  char *file3_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "utils.h", &file3_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file3_id);

  char *file4_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "readme.md", &file4_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file4_id);

  const char *query_json = R"({
    "pattern": "*",
    "includeFiles": true,
    "includeFolders": false,
    "maxResults": 100,
    "scope": {
      "folderPath": ".",
      "recursive": false,
      "filePatterns": ["*.cpp", "*.h"],
      "excludePatterns": ["test.cpp"]
    }
  })";

  char *results = nullptr;
  err = vxcore_search_files(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT(json_results["matchCount"].get<int>() == 2);

  bool found_main = false;
  bool found_utils = false;
  for (const auto &item : json_results["matches"]) {
    std::string path = item["path"].get<std::string>();
    ASSERT(path.find("test.cpp") == std::string::npos);
    ASSERT(path.find("readme.md") == std::string::npos);
    if (path.find("main.cpp") != std::string::npos) {
      found_main = true;
    }
    if (path.find("utils.h") != std::string::npos) {
      found_utils = true;
    }
  }
  ASSERT(found_main);
  ASSERT(found_utils);

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_search_include_exclude"));
  std::cout << "  âœ“ test_search_file_and_exclude_patterns passed" << std::endl;
  return 0;
}

int test_search_content_exclude_patterns() {
  std::cout << "  Running test_search_content_exclude_patterns..." << std::endl;
  cleanup_test_dir(get_test_path("test_content_exclude"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_content_exclude").c_str(),
                               "{\"name\":\"Test Content Exclude\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file1_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "compiler.md", &file1_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file1_id);

  char *file2_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "tutorial.md", &file2_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file2_id);

  const char *query_json = R"({
    "pattern": "compiler",
    "excludePatterns": ["gcc"],
    "caseSensitive": false,
    "regex": false,
    "maxResults": 100,
    "scope": {
      "folderPath": ".",
      "recursive": false
    }
  })";

  char *results = nullptr;
  err = vxcore_search_content(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_content_exclude"));
  std::cout << "  âœ“ test_search_content_exclude_patterns passed" << std::endl;
  return 0;
}

int test_content_search_basic() {
  std::cout << "  Running test_content_search_basic..." << std::endl;
  cleanup_test_dir(get_test_path("test_content_basic"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_content_basic").c_str(),
                               "{\"name\":\"Test Content Basic\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file1_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "alpha.md", &file1_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file1_id);

  char *file2_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "beta.md", &file2_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file2_id);

  write_file(get_test_path("test_content_basic") + "/alpha.md", "prefix\nhello world\nsuffix\n");
  write_file(get_test_path("test_content_basic") + "/beta.md", "line 1\nHello World\n");

  const char *query_json = R"({
    "pattern": "hello",
    "caseSensitive": false,
    "wholeWord": false,
    "regex": false,
    "maxResults": 100,
    "scope": {
      "folderPath": ".",
      "recursive": false
    }
  })";

  char *results = nullptr;
  err = vxcore_search_content(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT_EQ(json_results["matchCount"].get<int>(), 2);
  ASSERT_EQ(json_results["truncated"].get<bool>(), false);
  ASSERT(json_results["matches"].is_array());

  bool found_alpha = false;
  bool found_beta = false;
  for (const auto &matched_file : json_results["matches"]) {
    const std::string path = matched_file["path"].get<std::string>();
    if (path.find("alpha.md") != std::string::npos) {
      found_alpha = true;
      ASSERT_EQ(matched_file["matchCount"].get<int>(), 1);
      ASSERT_EQ(matched_file["matches"].size(), 1);
      const auto &m = matched_file["matches"][0];
      ASSERT_EQ(m["lineNumber"].get<int>(), 2);
      ASSERT_EQ(m["columnStart"].get<int>(), 0);
      ASSERT_EQ(m["columnEnd"].get<int>(), 5);
      ASSERT_EQ(m["lineText"].get<std::string>(), "hello world");
    }
    if (path.find("beta.md") != std::string::npos) {
      found_beta = true;
      ASSERT_EQ(matched_file["matchCount"].get<int>(), 1);
      ASSERT_EQ(matched_file["matches"].size(), 1);
      const auto &m = matched_file["matches"][0];
      ASSERT_EQ(m["lineNumber"].get<int>(), 2);
      ASSERT_EQ(m["columnStart"].get<int>(), 0);
      ASSERT_EQ(m["columnEnd"].get<int>(), 5);
      ASSERT_EQ(m["lineText"].get<std::string>(), "Hello World");
    }
  }
  ASSERT(found_alpha);
  ASSERT(found_beta);

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_content_basic"));
  std::cout << "  âœ“ test_content_search_basic passed" << std::endl;
  return 0;
}

int test_content_search_case_insensitive() {
  std::cout << "  Running test_content_search_case_insensitive..." << std::endl;
  cleanup_test_dir(get_test_path("test_content_case_insensitive"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_content_case_insensitive").c_str(),
                               "{\"name\":\"Test Content Case Insensitive\"}",
                               VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "case.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  write_file(get_test_path("test_content_case_insensitive") + "/case.md", "hello world\n");

  const char *query_json = R"({
    "pattern": "Hello",
    "caseSensitive": false,
    "wholeWord": false,
    "regex": false,
    "maxResults": 100,
    "scope": {
      "folderPath": ".",
      "recursive": false
    }
  })";

  char *results = nullptr;
  err = vxcore_search_content(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT_EQ(json_results["matchCount"].get<int>(), 1);
  ASSERT_EQ(json_results["matches"][0]["matchCount"].get<int>(), 1);
  ASSERT_EQ(json_results["matches"][0]["matches"][0]["lineText"].get<std::string>(), "hello world");

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_content_case_insensitive"));
  std::cout << "  âœ“ test_content_search_case_insensitive passed" << std::endl;
  return 0;
}

int test_content_search_case_sensitive() {
  std::cout << "  Running test_content_search_case_sensitive..." << std::endl;
  cleanup_test_dir(get_test_path("test_content_case_sensitive"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_content_case_sensitive").c_str(),
                               "{\"name\":\"Test Content Case Sensitive\"}",
                               VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "case.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  write_file(get_test_path("test_content_case_sensitive") + "/case.md", "hello world\n");

  const char *query_json = R"({
    "pattern": "Hello",
    "caseSensitive": true,
    "wholeWord": false,
    "regex": false,
    "maxResults": 100,
    "scope": {
      "folderPath": ".",
      "recursive": false
    }
  })";

  char *results = nullptr;
  err = vxcore_search_content(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT_EQ(json_results["matchCount"].get<int>(), 0);
  ASSERT_EQ(json_results["matches"].size(), 0);
  ASSERT_EQ(json_results["truncated"].get<bool>(), false);

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_content_case_sensitive"));
  std::cout << "  âœ“ test_content_search_case_sensitive passed" << std::endl;
  return 0;
}

int test_content_search_whole_word() {
  std::cout << "  Running test_content_search_whole_word..." << std::endl;
  cleanup_test_dir(get_test_path("test_content_whole_word"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_content_whole_word").c_str(),
                               "{\"name\":\"Test Content Whole Word\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "whole.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  write_file(get_test_path("test_content_whole_word") + "/whole.md", "testing test case tester\n");

  const char *query_json = R"({
    "pattern": "test",
    "caseSensitive": false,
    "wholeWord": true,
    "regex": false,
    "maxResults": 100,
    "scope": {
      "folderPath": ".",
      "recursive": false
    }
  })";

  char *results = nullptr;
  err = vxcore_search_content(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT_EQ(json_results["matchCount"].get<int>(), 1);
  auto file = json_results["matches"][0];
  ASSERT_EQ(file["matchCount"].get<int>(), 1);
  auto match = file["matches"][0];
  ASSERT_EQ(match["lineNumber"].get<int>(), 1);
  ASSERT_EQ(match["columnStart"].get<int>(), 8);
  ASSERT_EQ(match["columnEnd"].get<int>(), 12);
  ASSERT_EQ(match["lineText"].get<std::string>(), "testing test case tester");

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_content_whole_word"));
  std::cout << "  âœ“ test_content_search_whole_word passed" << std::endl;
  return 0;
}

int test_content_search_regex() {
  std::cout << "  Running test_content_search_regex..." << std::endl;
  cleanup_test_dir(get_test_path("test_content_regex"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_content_regex").c_str(),
                               "{\"name\":\"Test Content Regex\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "regex.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  write_file(get_test_path("test_content_regex") + "/regex.md", "id 123\nabc\nline 45 and 678\n");

  const char *query_json = R"({
    "pattern": "\\d+",
    "caseSensitive": false,
    "wholeWord": false,
    "regex": true,
    "maxResults": 100,
    "scope": {
      "folderPath": ".",
      "recursive": false
    }
  })";

  char *results = nullptr;
  err = vxcore_search_content(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT_EQ(json_results["matchCount"].get<int>(), 1);
  auto file = json_results["matches"][0];
  ASSERT_EQ(file["matchCount"].get<int>(), 3);

  ASSERT_EQ(file["matches"][0]["lineNumber"].get<int>(), 1);
  ASSERT_EQ(file["matches"][0]["columnStart"].get<int>(), 3);
  ASSERT_EQ(file["matches"][0]["columnEnd"].get<int>(), 6);
  ASSERT_EQ(file["matches"][0]["lineText"].get<std::string>(), "id 123");

  ASSERT_EQ(file["matches"][1]["lineNumber"].get<int>(), 3);
  ASSERT_EQ(file["matches"][1]["columnStart"].get<int>(), 5);
  ASSERT_EQ(file["matches"][1]["columnEnd"].get<int>(), 7);
  ASSERT_EQ(file["matches"][1]["lineText"].get<std::string>(), "line 45 and 678");

  ASSERT_EQ(file["matches"][2]["lineNumber"].get<int>(), 3);
  ASSERT_EQ(file["matches"][2]["columnStart"].get<int>(), 12);
  ASSERT_EQ(file["matches"][2]["columnEnd"].get<int>(), 15);
  ASSERT_EQ(file["matches"][2]["lineText"].get<std::string>(), "line 45 and 678");

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_content_regex"));
  std::cout << "  âœ“ test_content_search_regex passed" << std::endl;
  return 0;
}

int test_content_search_multi_file() {
  std::cout << "  Running test_content_search_multi_file..." << std::endl;
  cleanup_test_dir(get_test_path("test_content_multi_file"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_content_multi_file").c_str(),
                               "{\"name\":\"Test Content Multi File\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  for (int i = 1; i <= 5; ++i) {
    char *file_id = nullptr;
    std::string name = "file" + std::to_string(i) + ".md";
    err = vxcore_file_create(ctx, notebook_id, ".", name.c_str(), &file_id);
    ASSERT_EQ(err, VXCORE_OK);
    vxcore_string_free(file_id);
  }

  write_file(get_test_path("test_content_multi_file") + "/file1.md", "needle here\n");
  write_file(get_test_path("test_content_multi_file") + "/file2.md", "no match\n");
  write_file(get_test_path("test_content_multi_file") + "/file3.md", "another needle\n");
  write_file(get_test_path("test_content_multi_file") + "/file4.md", "nothing\n");
  write_file(get_test_path("test_content_multi_file") + "/file5.md", "needle again\n");

  const char *query_json = R"({
    "pattern": "needle",
    "caseSensitive": false,
    "wholeWord": false,
    "regex": false,
    "maxResults": 100,
    "scope": {
      "folderPath": ".",
      "recursive": false
    }
  })";

  char *results = nullptr;
  err = vxcore_search_content(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT_EQ(json_results["matchCount"].get<int>(), 3);
  ASSERT_EQ(json_results["matches"].size(), 3);

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_content_multi_file"));
  std::cout << "  âœ“ test_content_search_multi_file passed" << std::endl;
  return 0;
}

int test_content_search_multiple_matches_per_line() {
  std::cout << "  Running test_content_search_multiple_matches_per_line..." << std::endl;
  cleanup_test_dir(get_test_path("test_content_multi_match_line"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_content_multi_match_line").c_str(),
                               "{\"name\":\"Test Content Multi Match Line\"}",
                               VXCORE_NOTEBOOK_BUNDLED, &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "aaa.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  write_file(get_test_path("test_content_multi_match_line") + "/aaa.md", "aaa\n");

  const char *query_json = R"({
    "pattern": "a",
    "caseSensitive": true,
    "wholeWord": false,
    "regex": false,
    "maxResults": 100,
    "scope": {
      "folderPath": ".",
      "recursive": false
    }
  })";

  char *results = nullptr;
  err = vxcore_search_content(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT_EQ(json_results["matchCount"].get<int>(), 1);
  auto file = json_results["matches"][0];
  ASSERT_EQ(file["matchCount"].get<int>(), 3);
  ASSERT_EQ(file["matches"][0]["columnStart"].get<int>(), 0);
  ASSERT_EQ(file["matches"][0]["columnEnd"].get<int>(), 1);
  ASSERT_EQ(file["matches"][1]["columnStart"].get<int>(), 1);
  ASSERT_EQ(file["matches"][1]["columnEnd"].get<int>(), 2);
  ASSERT_EQ(file["matches"][2]["columnStart"].get<int>(), 2);
  ASSERT_EQ(file["matches"][2]["columnEnd"].get<int>(), 3);
  ASSERT_EQ(file["matches"][0]["lineNumber"].get<int>(), 1);
  ASSERT_EQ(file["matches"][1]["lineNumber"].get<int>(), 1);
  ASSERT_EQ(file["matches"][2]["lineNumber"].get<int>(), 1);
  ASSERT_EQ(file["matches"][0]["lineText"].get<std::string>(), "aaa");

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_content_multi_match_line"));
  std::cout << "  âœ“ test_content_search_multiple_matches_per_line passed" << std::endl;
  return 0;
}

int test_content_search_result_ordering() {
  std::cout << "  Running test_content_search_result_ordering..." << std::endl;
  cleanup_test_dir(get_test_path("test_content_ordering"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_content_ordering").c_str(),
                               "{\"name\":\"Test Content Ordering\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  for (const char *name : {"a.md", "b.md", "c.md", "d.md"}) {
    char *file_id = nullptr;
    err = vxcore_file_create(ctx, notebook_id, ".", name, &file_id);
    ASSERT_EQ(err, VXCORE_OK);
    vxcore_string_free(file_id);
  }

  write_file(get_test_path("test_content_ordering") + "/a.md", "hit in a\n");
  write_file(get_test_path("test_content_ordering") + "/b.md", "nope\n");
  write_file(get_test_path("test_content_ordering") + "/c.md", "hit in c\n");
  write_file(get_test_path("test_content_ordering") + "/d.md", "hit in d\n");

  const char *query_json = R"({
    "pattern": "hit",
    "caseSensitive": false,
    "wholeWord": false,
    "regex": false,
    "maxResults": 100,
    "scope": {
      "folderPath": ".",
      "recursive": false
    }
  })";

  nlohmann::json input_files_json;
  input_files_json["files"] = nlohmann::json::array({"b.md", "d.md", "a.md", "c.md"});

  char *results = nullptr;
  err = vxcore_search_content(ctx, notebook_id, query_json, input_files_json.dump().c_str(),
                              &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT_EQ(json_results["matchCount"].get<int>(), 3);
  ASSERT_EQ(json_results["matches"].size(), 3);

  ASSERT(json_results["matches"][0]["path"].get<std::string>().find("d.md") != std::string::npos);
  ASSERT(json_results["matches"][1]["path"].get<std::string>().find("a.md") != std::string::npos);
  ASSERT(json_results["matches"][2]["path"].get<std::string>().find("c.md") != std::string::npos);

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_content_ordering"));
  std::cout << "  âœ“ test_content_search_result_ordering passed" << std::endl;
  return 0;
}

int test_content_search_max_results() {
  std::cout << "  Running test_content_search_max_results..." << std::endl;
  cleanup_test_dir(get_test_path("test_content_max_results"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_content_max_results").c_str(),
                               "{\"name\":\"Test Content Max Results\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  for (int i = 1; i <= 10; ++i) {
    char *file_id = nullptr;
    std::string file_name = "file" + std::to_string(i) + ".md";
    err = vxcore_file_create(ctx, notebook_id, ".", file_name.c_str(), &file_id);
    ASSERT_EQ(err, VXCORE_OK);
    vxcore_string_free(file_id);

    write_file(get_test_path("test_content_max_results") + "/" + file_name,
               "hit\nhit\nhit\nhit\nhit\n");
  }

  const char *query_json = R"({
    "pattern": "hit",
    "caseSensitive": false,
    "wholeWord": false,
    "regex": false,
    "maxResults": 10,
    "scope": {
      "folderPath": ".",
      "recursive": false
    }
  })";

  char *results = nullptr;
  err = vxcore_search_content(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT_EQ(json_results["truncated"].get<bool>(), true);
  ASSERT(json_results["matchCount"].get<int>() >= 1);

  int total_match_count = 0;
  for (const auto &matched_file : json_results["matches"]) {
    total_match_count += matched_file["matchCount"].get<int>();
  }
  ASSERT(total_match_count <= 10);

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_content_max_results"));
  std::cout << "  âœ“ test_content_search_max_results passed" << std::endl;
  return 0;
}

int test_content_search_empty_pattern() {
  std::cout << "  Running test_content_search_empty_pattern..." << std::endl;
  cleanup_test_dir(get_test_path("test_content_empty_pattern"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_content_empty_pattern").c_str(),
                               "{\"name\":\"Test Content Empty Pattern\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "empty.md", &file_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file_id);

  write_file(get_test_path("test_content_empty_pattern") + "/empty.md", "some content\n");

  const char *query_json = R"({
    "pattern": "",
    "caseSensitive": false,
    "wholeWord": false,
    "regex": false,
    "maxResults": 100,
    "scope": {
      "folderPath": ".",
      "recursive": false
    }
  })";

  char *results = nullptr;
  err = vxcore_search_content(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT_EQ(json_results["matchCount"].get<int>(), 0);
  ASSERT_EQ(json_results["matches"].size(), 0);
  ASSERT_EQ(json_results["truncated"].get<bool>(), false);

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_content_empty_pattern"));
  std::cout << "  âœ“ test_content_search_empty_pattern passed" << std::endl;
  return 0;
}

int test_content_search_no_matches() {
  std::cout << "  Running test_content_search_no_matches..." << std::endl;
  cleanup_test_dir(get_test_path("test_content_no_matches"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_content_no_matches").c_str(),
                               "{\"name\":\"Test Content No Matches\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file1_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "a.md", &file1_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file1_id);

  char *file2_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "b.md", &file2_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file2_id);

  write_file(get_test_path("test_content_no_matches") + "/a.md", "alpha beta\n");
  write_file(get_test_path("test_content_no_matches") + "/b.md", "gamma delta\n");

  const char *query_json = R"({
    "pattern": "unmatched-pattern",
    "caseSensitive": false,
    "wholeWord": false,
    "regex": false,
    "maxResults": 100,
    "scope": {
      "folderPath": ".",
      "recursive": false
    }
  })";

  char *results = nullptr;
  err = vxcore_search_content(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT_EQ(json_results["matchCount"].get<int>(), 0);
  ASSERT_EQ(json_results["matches"].size(), 0);
  ASSERT_EQ(json_results["truncated"].get<bool>(), false);

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_content_no_matches"));
  std::cout << "  âœ“ test_content_search_no_matches passed" << std::endl;
  return 0;
}

int test_search_by_tags_with_exclude_tags() {
  std::cout << "  Running test_search_by_tags_with_exclude_tags..." << std::endl;
  cleanup_test_dir(get_test_path("test_tags_exclude"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_tags_exclude").c_str(),
                               "{\"name\":\"Test Tags Exclude\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *file1_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "file1.md", &file1_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file1_id);

  char *file2_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "file2.md", &file2_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file2_id);

  char *file3_id = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "file3.md", &file3_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(file3_id);

  err = vxcore_tag_create(ctx, notebook_id, "lang");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_tag_create(ctx, notebook_id, "php");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_tag_create(ctx, notebook_id, "python");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "file1.md", "lang");
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_file_tag(ctx, notebook_id, "file1.md", "php");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "file2.md", "lang");
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_file_tag(ctx, notebook_id, "file2.md", "python");
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_file_tag(ctx, notebook_id, "file3.md", "python");
  ASSERT_EQ(err, VXCORE_OK);

  const char *query_json = R"({
    "tags": ["lang"],
    "operator": "AND",
    "maxResults": 100,
    "scope": {
      "folderPath": ".",
      "recursive": false,
      "excludeTags": ["php"]
    }
  })";

  char *results = nullptr;
  err = vxcore_search_by_tags(ctx, notebook_id, query_json, nullptr, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json_results = nlohmann::json::parse(results);
  ASSERT(json_results["matchCount"].get<int>() == 1);
  ASSERT(json_results["matches"].size() == 1);

  std::string path = json_results["matches"][0]["path"].get<std::string>();
  ASSERT(path.find("file2.md") != std::string::npos);

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_tags_exclude"));
  std::cout << "  âœ“ test_search_by_tags_with_exclude_tags passed" << std::endl;
  return 0;
}

// ============================================================================
// vxcore_tag_find_files Tests (Direct DB-backed tag queries)
// ============================================================================

int test_tag_find_files_and() {
  std::cout << "  Running test_tag_find_files_and..." << std::endl;
  cleanup_test_dir(get_test_path("test_tag_find_and"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_tag_find_and").c_str(),
                               "{\"name\":\"Test Tag Find AND\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create files
  char *f1 = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "file1.md", &f1);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(f1);

  char *f2 = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "file2.md", &f2);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(f2);

  char *f3 = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "file3.md", &f3);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(f3);

  // Create tags and assign
  err = vxcore_tag_create(ctx, notebook_id, "important");
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_tag_create(ctx, notebook_id, "todo");
  ASSERT_EQ(err, VXCORE_OK);

  // file1: important + todo, file2: important, file3: todo
  err = vxcore_file_tag(ctx, notebook_id, "file1.md", "important");
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_file_tag(ctx, notebook_id, "file1.md", "todo");
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_file_tag(ctx, notebook_id, "file2.md", "important");
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_file_tag(ctx, notebook_id, "file3.md", "todo");
  ASSERT_EQ(err, VXCORE_OK);

  // AND query: files with BOTH "important" AND "todo" -> only file1
  char *results = nullptr;
  err = vxcore_tag_find_files(ctx, notebook_id, "[\"important\", \"todo\"]", "AND", &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json = nlohmann::json::parse(results);
  ASSERT_EQ(json["matchCount"].get<int>(), 1);
  ASSERT(json["matches"].size() == 1);
  ASSERT(json["matches"][0]["fileName"].get<std::string>() == "file1.md");
  ASSERT(json["matches"][0]["filePath"].is_string());
  ASSERT(json["matches"][0]["tags"].is_array());

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_tag_find_and"));
  std::cout << "  \xE2\x9C\x93 test_tag_find_files_and passed" << std::endl;
  return 0;
}

int test_tag_find_files_or() {
  std::cout << "  Running test_tag_find_files_or..." << std::endl;
  cleanup_test_dir(get_test_path("test_tag_find_or"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_tag_find_or").c_str(),
                               "{\"name\":\"Test Tag Find OR\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create files
  char *f1 = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "alpha.md", &f1);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(f1);

  char *f2 = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "beta.md", &f2);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(f2);

  char *f3 = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "gamma.md", &f3);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(f3);

  // Create tags
  err = vxcore_tag_create(ctx, notebook_id, "work");
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_tag_create(ctx, notebook_id, "personal");
  ASSERT_EQ(err, VXCORE_OK);

  // alpha: work, beta: personal, gamma: no tags
  err = vxcore_file_tag(ctx, notebook_id, "alpha.md", "work");
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_file_tag(ctx, notebook_id, "beta.md", "personal");
  ASSERT_EQ(err, VXCORE_OK);

  // OR query: files with "work" OR "personal" -> alpha + beta
  char *results = nullptr;
  err = vxcore_tag_find_files(ctx, notebook_id, "[\"work\", \"personal\"]", "OR", &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json = nlohmann::json::parse(results);
  ASSERT_EQ(json["matchCount"].get<int>(), 2);
  ASSERT(json["matches"].size() == 2);

  // Verify both files are present
  bool found_alpha = false, found_beta = false;
  for (const auto &match : json["matches"]) {
    std::string name = match["fileName"].get<std::string>();
    if (name == "alpha.md") found_alpha = true;
    if (name == "beta.md") found_beta = true;
  }
  ASSERT(found_alpha);
  ASSERT(found_beta);

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_tag_find_or"));
  std::cout << "  \xE2\x9C\x93 test_tag_find_files_or passed" << std::endl;
  return 0;
}

int test_tag_find_files_empty_results() {
  std::cout << "  Running test_tag_find_files_empty_results..." << std::endl;
  cleanup_test_dir(get_test_path("test_tag_find_empty"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_tag_find_empty").c_str(),
                               "{\"name\":\"Test Tag Find Empty\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create a file with no tags
  char *f1 = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "lonely.md", &f1);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(f1);

  // Search for non-existent tag
  char *results = nullptr;
  err = vxcore_tag_find_files(ctx, notebook_id, "[\"nonexistent\"]", "AND", &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json = nlohmann::json::parse(results);
  ASSERT_EQ(json["matchCount"].get<int>(), 0);
  ASSERT(json["matches"].size() == 0);

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_tag_find_empty"));
  std::cout << "  \xE2\x9C\x93 test_tag_find_files_empty_results passed" << std::endl;
  return 0;
}

int test_tag_find_files_in_subfolder() {
  std::cout << "  Running test_tag_find_files_in_subfolder..." << std::endl;
  cleanup_test_dir(get_test_path("test_tag_find_subfolder"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_tag_find_subfolder").c_str(),
                               "{\"name\":\"Test Tag Find Subfolder\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create subfolder and files
  char *folder_id = nullptr;
  err = vxcore_folder_create(ctx, notebook_id, ".", "docs", &folder_id);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(folder_id);

  char *f1 = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "root_file.md", &f1);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(f1);

  char *f2 = nullptr;
  err = vxcore_file_create(ctx, notebook_id, "docs", "nested_file.md", &f2);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(f2);

  // Create tag and assign to both
  err = vxcore_tag_create(ctx, notebook_id, "review");
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_file_tag(ctx, notebook_id, "root_file.md", "review");
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_file_tag(ctx, notebook_id, "docs/nested_file.md", "review");
  ASSERT_EQ(err, VXCORE_OK);

  // Find files with "review" tag - should include both root and nested
  char *results = nullptr;
  err = vxcore_tag_find_files(ctx, notebook_id, "[\"review\"]", "OR", &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json = nlohmann::json::parse(results);
  ASSERT_EQ(json["matchCount"].get<int>(), 2);

  // Verify file paths are present and the nested file has proper path
  bool found_root = false, found_nested = false;
  for (const auto &match : json["matches"]) {
    std::string path = match["filePath"].get<std::string>();
    if (path == "root_file.md") found_root = true;
    if (path == "docs/nested_file.md") found_nested = true;
  }
  ASSERT(found_root);
  ASSERT(found_nested);

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_tag_find_subfolder"));
  std::cout << "  \xE2\x9C\x93 test_tag_find_files_in_subfolder passed" << std::endl;
  return 0;
}

int test_tag_find_files_invalid_params() {
  std::cout << "  Running test_tag_find_files_invalid_params..." << std::endl;
  cleanup_test_dir(get_test_path("test_tag_find_invalid"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_tag_find_invalid").c_str(),
                               "{\"name\":\"Test Tag Find Invalid\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  char *results = nullptr;

  // Null pointer checks
  err = vxcore_tag_find_files(nullptr, notebook_id, "[\"a\"]", "AND", &results);
  ASSERT_EQ(err, VXCORE_ERR_NULL_POINTER);

  err = vxcore_tag_find_files(ctx, nullptr, "[\"a\"]", "AND", &results);
  ASSERT_EQ(err, VXCORE_ERR_NULL_POINTER);

  err = vxcore_tag_find_files(ctx, notebook_id, nullptr, "AND", &results);
  ASSERT_EQ(err, VXCORE_ERR_NULL_POINTER);

  err = vxcore_tag_find_files(ctx, notebook_id, "[\"a\"]", nullptr, &results);
  ASSERT_EQ(err, VXCORE_ERR_NULL_POINTER);

  err = vxcore_tag_find_files(ctx, notebook_id, "[\"a\"]", "AND", nullptr);
  ASSERT_EQ(err, VXCORE_ERR_NULL_POINTER);

  // Invalid JSON
  err = vxcore_tag_find_files(ctx, notebook_id, "not json", "AND", &results);
  ASSERT_EQ(err, VXCORE_ERR_JSON_PARSE);

  // Not an array
  err = vxcore_tag_find_files(ctx, notebook_id, "\"single_string\"", "AND", &results);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  // Invalid operator
  err = vxcore_tag_find_files(ctx, notebook_id, "[\"a\"]", "XOR", &results);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  // Invalid notebook
  err = vxcore_tag_find_files(ctx, "nonexistent_id", "[\"a\"]", "AND", &results);
  ASSERT_EQ(err, VXCORE_ERR_NOT_FOUND);

  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_tag_find_invalid"));
  std::cout << "  \xE2\x9C\x93 test_tag_find_files_invalid_params passed" << std::endl;
  return 0;
}

int test_tag_count_files_by_tag() {
  std::cout << "  Running test_tag_count_files_by_tag..." << std::endl;
  cleanup_test_dir(get_test_path("test_tag_count"));

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *notebook_id = nullptr;
  err = vxcore_notebook_create(ctx, get_test_path("test_tag_count").c_str(),
                               "{\"name\":\"Test Tag Count\"}", VXCORE_NOTEBOOK_BUNDLED,
                               &notebook_id);
  ASSERT_EQ(err, VXCORE_OK);

  // Create files
  char *f1 = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "a.md", &f1);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(f1);

  char *f2 = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "b.md", &f2);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(f2);

  char *f3 = nullptr;
  err = vxcore_file_create(ctx, notebook_id, ".", "c.md", &f3);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(f3);

  // Create tags
  err = vxcore_tag_create(ctx, notebook_id, "urgent");
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_tag_create(ctx, notebook_id, "draft");
  ASSERT_EQ(err, VXCORE_OK);

  // a.md: urgent + draft, b.md: urgent, c.md: draft
  err = vxcore_file_tag(ctx, notebook_id, "a.md", "urgent");
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_file_tag(ctx, notebook_id, "a.md", "draft");
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_file_tag(ctx, notebook_id, "b.md", "urgent");
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_file_tag(ctx, notebook_id, "c.md", "draft");
  ASSERT_EQ(err, VXCORE_OK);

  // Count files by tag: urgent=2, draft=2
  char *results = nullptr;
  err = vxcore_tag_count_files_by_tag(ctx, notebook_id, &results);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(results);

  auto json = nlohmann::json::parse(results);
  ASSERT(json.is_array());
  ASSERT(json.size() >= 2);

  // Build a map for easier checking
  std::map<std::string, int> counts;
  for (const auto &entry : json) {
    counts[entry["tag"].get<std::string>()] = entry["count"].get<int>();
  }
  ASSERT_EQ(counts["urgent"], 2);
  ASSERT_EQ(counts["draft"], 2);

  vxcore_string_free(results);
  vxcore_string_free(notebook_id);
  vxcore_context_destroy(ctx);
  cleanup_test_dir(get_test_path("test_tag_count"));
  std::cout << "  \xE2\x9C\x93 test_tag_count_files_by_tag passed" << std::endl;
  return 0;
}

int main() {
  vxcore_set_test_mode(1);
  vxcore_clear_test_directory();

  std::cout << "\nRunning search tests..." << std::endl;

  RUN_TEST(test_search_files_basic);
  RUN_TEST(test_search_files_include_folders);
  RUN_TEST(test_search_files_pattern_matching);
  RUN_TEST(test_search_files_name_vs_path_ranking);
  RUN_TEST(test_search_files_exclude_patterns);
  RUN_TEST(test_search_files_max_results);
  RUN_TEST(test_search_files_with_file_patterns);
  RUN_TEST(test_search_files_file_patterns_recursive);

  RUN_TEST(test_search_by_tags_single_tag);
  RUN_TEST(test_search_by_tags_and_operator);
  RUN_TEST(test_search_by_tags_or_operator);
  RUN_TEST(test_search_by_tags_recursive);
  RUN_TEST(test_search_by_tags_with_exclude_patterns);
  RUN_TEST(test_search_by_tags_max_results);
  RUN_TEST(test_search_by_tags_empty_results);
  RUN_TEST(test_search_by_tags_with_file_patterns);
  RUN_TEST(test_search_file_and_exclude_patterns);

  RUN_TEST(test_search_content_exclude_patterns);
  RUN_TEST(test_search_by_tags_with_exclude_tags);

  RUN_TEST(test_tag_find_files_and);
  RUN_TEST(test_tag_find_files_or);
  RUN_TEST(test_tag_find_files_empty_results);
  RUN_TEST(test_tag_find_files_in_subfolder);
  RUN_TEST(test_tag_find_files_invalid_params);
  RUN_TEST(test_tag_count_files_by_tag);

  RUN_TEST(test_content_search_basic);
  RUN_TEST(test_content_search_case_insensitive);
  RUN_TEST(test_content_search_case_sensitive);
  RUN_TEST(test_content_search_whole_word);
  RUN_TEST(test_content_search_regex);
  RUN_TEST(test_content_search_multi_file);
  RUN_TEST(test_content_search_multiple_matches_per_line);
  RUN_TEST(test_content_search_result_ordering);
  RUN_TEST(test_content_search_max_results);
  RUN_TEST(test_content_search_empty_pattern);
  RUN_TEST(test_content_search_no_matches);

  std::cout << "All search tests passed!" << std::endl;
  return 0;
}
