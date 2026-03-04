#include <iostream>
#include <nlohmann/json.hpp>

#include "test_utils.h"
#include "vxcore/vxcore.h"

// Test default file types are correctly initialized
int test_filetype_defaults() {
  std::cout << "  Running test_filetype_defaults..." << std::endl;
  vxcore_set_test_mode(1);

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(ctx);

  char *json_str = nullptr;
  err = vxcore_filetype_list(ctx, &json_str);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(json_str);

  nlohmann::json j = nlohmann::json::parse(json_str);
  ASSERT_EQ(j.size(), 5);
  ASSERT_EQ(j[0]["name"], "Markdown");
  ASSERT_EQ(j[1]["name"], "Text");
  ASSERT_EQ(j[2]["name"], "PDF");
  ASSERT_EQ(j[3]["name"], "MindMap");
  ASSERT_EQ(j[4]["name"], "Others");

  vxcore_string_free(json_str);
  vxcore_context_destroy(ctx);
  std::cout << "  ✓ test_filetype_defaults passed" << std::endl;
  return 0;
}

// Test GetBySuffix returns Markdown for "md"
int test_filetype_get_by_suffix_md() {
  std::cout << "  Running test_filetype_get_by_suffix_md..." << std::endl;
  vxcore_set_test_mode(1);

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *json_str = nullptr;
  err = vxcore_filetype_get_by_suffix(ctx, "md", &json_str);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(json_str);

  nlohmann::json j = nlohmann::json::parse(json_str);
  ASSERT_EQ(j["name"], "Markdown");

  vxcore_string_free(json_str);
  vxcore_context_destroy(ctx);
  std::cout << "  ✓ test_filetype_get_by_suffix_md passed" << std::endl;
  return 0;
}

// Test GetBySuffix returns Others for unknown suffix
int test_filetype_get_by_suffix_unknown() {
  std::cout << "  Running test_filetype_get_by_suffix_unknown..." << std::endl;
  vxcore_set_test_mode(1);

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *json_str = nullptr;
  err = vxcore_filetype_get_by_suffix(ctx, "xyz", &json_str);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(json_str);

  nlohmann::json j = nlohmann::json::parse(json_str);
  ASSERT_EQ(j["name"], "Others");

  vxcore_string_free(json_str);
  vxcore_context_destroy(ctx);
  std::cout << "  ✓ test_filetype_get_by_suffix_unknown passed" << std::endl;
  return 0;
}

// Test case-insensitive suffix lookup
int test_filetype_case_insensitive() {
  std::cout << "  Running test_filetype_case_insensitive..." << std::endl;
  vxcore_set_test_mode(1);

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *json1 = nullptr, *json2 = nullptr, *json3 = nullptr;
  err = vxcore_filetype_get_by_suffix(ctx, "md", &json1);
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_filetype_get_by_suffix(ctx, "MD", &json2);
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_filetype_get_by_suffix(ctx, "Md", &json3);
  ASSERT_EQ(err, VXCORE_OK);

  nlohmann::json j1 = nlohmann::json::parse(json1);
  nlohmann::json j2 = nlohmann::json::parse(json2);
  nlohmann::json j3 = nlohmann::json::parse(json3);

  ASSERT_EQ(j1["name"], j2["name"]);
  ASSERT_EQ(j2["name"], j3["name"]);
  ASSERT_EQ(j1["name"], "Markdown");

  vxcore_string_free(json1);
  vxcore_string_free(json2);
  vxcore_string_free(json3);
  vxcore_context_destroy(ctx);
  std::cout << "  ✓ test_filetype_case_insensitive passed" << std::endl;
  return 0;
}

// Test PDF has isNewable=false
int test_filetype_pdf_not_newable() {
  std::cout << "  Running test_filetype_pdf_not_newable..." << std::endl;
  vxcore_set_test_mode(1);

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  // Get PDF type
  char *pdf_json = nullptr;
  err = vxcore_filetype_get_by_suffix(ctx, "pdf", &pdf_json);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(pdf_json);

  nlohmann::json pdf = nlohmann::json::parse(pdf_json);
  ASSERT_EQ(pdf["name"], "PDF");
  ASSERT_FALSE(pdf["isNewable"]);
  vxcore_string_free(pdf_json);

  // Get all types to verify others are newable
  char *list_json = nullptr;
  err = vxcore_filetype_list(ctx, &list_json);
  ASSERT_EQ(err, VXCORE_OK);

  nlohmann::json types = nlohmann::json::parse(list_json);
  ASSERT_TRUE(types[0]["isNewable"]);  // Markdown
  ASSERT_TRUE(types[1]["isNewable"]);  // Text
  ASSERT_TRUE(types[3]["isNewable"]);  // MindMap
  ASSERT_TRUE(types[4]["isNewable"]);  // Others
  vxcore_string_free(list_json);

  vxcore_context_destroy(ctx);
  std::cout << "  ✓ test_filetype_pdf_not_newable passed" << std::endl;
  return 0;
}

// Test GetDisplayName returns default display name
int test_filetype_display_name_default() {
  std::cout << "  Running test_filetype_display_name_default..." << std::endl;
  vxcore_set_test_mode(1);

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *json_str = nullptr;
  err = vxcore_filetype_get_by_suffix(ctx, "md", &json_str);
  ASSERT_EQ(err, VXCORE_OK);

  nlohmann::json j = nlohmann::json::parse(json_str);
  ASSERT_EQ(j["displayName"], "Markdown");

  vxcore_string_free(json_str);
  vxcore_context_destroy(ctx);
  std::cout << "  ✓ test_filetype_display_name_default passed" << std::endl;
  return 0;
}

// Test metadata field is optional and can contain locale-specific names
int test_filetype_display_name_locale() {
  std::cout << "  Running test_filetype_display_name_locale..." << std::endl;
  vxcore_set_test_mode(1);

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *json_str = nullptr;
  err = vxcore_filetype_get_by_suffix(ctx, "md", &json_str);
  ASSERT_EQ(err, VXCORE_OK);

  nlohmann::json j = nlohmann::json::parse(json_str);

  // Metadata field is optional (only present if not empty)
  // If present, it must be a valid JSON object
  if (j.contains("metadata")) {
    ASSERT_TRUE(j["metadata"].is_object());
  }

  vxcore_string_free(json_str);
  vxcore_context_destroy(ctx);
  std::cout << "  ✓ test_filetype_display_name_locale passed" << std::endl;
  return 0;
}

// Test C API: vxcore_filetype_list returns 5 types
int test_filetype_c_api_list() {
  std::cout << "  Running test_filetype_c_api_list..." << std::endl;
  vxcore_set_test_mode(1);

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(ctx);

  char *json = nullptr;
  err = vxcore_filetype_list(ctx, &json);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(json);

  // Parse and verify
  nlohmann::json types_array = nlohmann::json::parse(json);
  ASSERT_TRUE(types_array.is_array());
  ASSERT_EQ(types_array.size(), 5);

  // Verify first entry structure
  ASSERT_TRUE(types_array[0].contains("name"));
  ASSERT_TRUE(types_array[0].contains("suffixes"));
  ASSERT_TRUE(types_array[0].contains("isNewable"));
  ASSERT_TRUE(types_array[0].contains("displayName"));

  vxcore_string_free(json);
  vxcore_context_destroy(ctx);

  std::cout << "  ✓ test_filetype_c_api_list passed" << std::endl;
  return 0;
}

// Test C API: vxcore_filetype_get_by_suffix works
int test_filetype_c_api_suffix() {
  std::cout << "  Running test_filetype_c_api_suffix..." << std::endl;
  vxcore_set_test_mode(1);

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  // Test known suffix
  char *json1 = nullptr;
  err = vxcore_filetype_get_by_suffix(ctx, "md", &json1);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(json1);

  nlohmann::json entry1 = nlohmann::json::parse(json1);
  ASSERT_EQ(entry1["name"], "Markdown");
  vxcore_string_free(json1);

  // Test PDF (not newable)
  char *json2 = nullptr;
  err = vxcore_filetype_get_by_suffix(ctx, "pdf", &json2);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(json2);

  nlohmann::json entry2 = nlohmann::json::parse(json2);
  ASSERT_EQ(entry2["name"], "PDF");
  ASSERT_FALSE(entry2["isNewable"]);
  vxcore_string_free(json2);

  // Test unknown suffix returns Others
  char *json3 = nullptr;
  err = vxcore_filetype_get_by_suffix(ctx, "xyz", &json3);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(json3);

  nlohmann::json entry3 = nlohmann::json::parse(json3);
  ASSERT_EQ(entry3["name"], "Others");
  vxcore_string_free(json3);

  vxcore_context_destroy(ctx);

  std::cout << "  ✓ test_filetype_c_api_suffix passed" << std::endl;
  return 0;
}

// Test C API: vxcore_filetype_get_by_name works
int test_filetype_c_api_name() {
  std::cout << "  Running test_filetype_c_api_name..." << std::endl;
  vxcore_set_test_mode(1);

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *json = nullptr;
  err = vxcore_filetype_get_by_name(ctx, "Markdown", &json);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(json);

  nlohmann::json entry = nlohmann::json::parse(json);
  ASSERT_EQ(entry["name"], "Markdown");

  vxcore_string_free(json);
  vxcore_context_destroy(ctx);

  std::cout << "  ✓ test_filetype_c_api_name passed" << std::endl;
  return 0;
}

// Test C API: Unknown name returns VXCORE_ERR_NOT_FOUND
int test_filetype_c_api_name_not_found() {
  std::cout << "  Running test_filetype_c_api_name_not_found..." << std::endl;
  vxcore_set_test_mode(1);

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *json = nullptr;
  err = vxcore_filetype_get_by_name(ctx, "UnknownType", &json);
  ASSERT_EQ(err, VXCORE_ERR_NOT_FOUND);
  ASSERT_NULL(json);

  vxcore_context_destroy(ctx);

  std::cout << "  ✓ test_filetype_c_api_name_not_found passed" << std::endl;
  return 0;
}

// Test GetByName is case-insensitive
int test_filetype_name_case_insensitive() {
  std::cout << "  Running test_filetype_name_case_insensitive..." << std::endl;
  vxcore_set_test_mode(1);

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  char *json1 = nullptr, *json2 = nullptr, *json3 = nullptr;
  err = vxcore_filetype_get_by_name(ctx, "Markdown", &json1);
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_filetype_get_by_name(ctx, "markdown", &json2);
  ASSERT_EQ(err, VXCORE_OK);
  err = vxcore_filetype_get_by_name(ctx, "MARKDOWN", &json3);
  ASSERT_EQ(err, VXCORE_OK);

  nlohmann::json j1 = nlohmann::json::parse(json1);
  nlohmann::json j2 = nlohmann::json::parse(json2);
  nlohmann::json j3 = nlohmann::json::parse(json3);

  ASSERT_EQ(j1["name"], "Markdown");
  ASSERT_EQ(j2["name"], "Markdown");
  ASSERT_EQ(j3["name"], "Markdown");

  vxcore_string_free(json1);
  vxcore_string_free(json2);
  vxcore_string_free(json3);
  vxcore_context_destroy(ctx);

  std::cout << "  ✓ test_filetype_name_case_insensitive passed" << std::endl;
  return 0;
}

// Test vxcore_filetype_set: add a new type
int test_filetype_set_add_type() {
  std::cout << "  Running test_filetype_set_add_type..." << std::endl;
  vxcore_set_test_mode(1);

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  // Get current types and add a new one
  char *list_json = nullptr;
  err = vxcore_filetype_list(ctx, &list_json);
  ASSERT_EQ(err, VXCORE_OK);

  nlohmann::json types = nlohmann::json::parse(list_json);
  vxcore_string_free(list_json);
  ASSERT_EQ(types.size(), 5);  // Default 5 types

  // Add new type
  nlohmann::json new_type;
  new_type["name"] = "CustomCode";
  new_type["suffixes"] = nlohmann::json::array({"cc", "cpp", "cxx"});
  new_type["isNewable"] = true;
  new_type["displayName"] = "Custom Code Files";
  types.push_back(new_type);

  // Set the new list
  std::string new_json = types.dump();
  err = vxcore_filetype_set(ctx, new_json.c_str());
  ASSERT_EQ(err, VXCORE_OK);

  // Verify it was added
  char *updated_json = nullptr;
  err = vxcore_filetype_list(ctx, &updated_json);
  ASSERT_EQ(err, VXCORE_OK);

  nlohmann::json updated = nlohmann::json::parse(updated_json);
  vxcore_string_free(updated_json);
  ASSERT_EQ(updated.size(), 6);
  ASSERT_EQ(updated[5]["name"], "CustomCode");
  ASSERT_EQ(updated[5]["suffixes"].size(), 3);

  // Verify lookup works
  char *lookup_json = nullptr;
  err = vxcore_filetype_get_by_suffix(ctx, "cpp", &lookup_json);
  ASSERT_EQ(err, VXCORE_OK);

  nlohmann::json lookup = nlohmann::json::parse(lookup_json);
  vxcore_string_free(lookup_json);
  ASSERT_EQ(lookup["name"], "CustomCode");

  vxcore_context_destroy(ctx);
  std::cout << "  ✓ test_filetype_set_add_type passed" << std::endl;
  return 0;
}

// Test vxcore_filetype_set: duplicate names should fail
int test_filetype_set_duplicate_name() {
  std::cout << "  Running test_filetype_set_duplicate_name..." << std::endl;
  vxcore_set_test_mode(1);

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  // Create array with duplicate names (case-insensitive)
  nlohmann::json types = nlohmann::json::array();
  types.push_back(
      {{"name", "Type1"}, {"suffixes", {"a"}}, {"isNewable", true}, {"displayName", "Type 1"}});
  types.push_back(
      {{"name", "type1"}, {"suffixes", {"b"}}, {"isNewable", true}, {"displayName", "Type 1 Dup"}});

  std::string json_str = types.dump();
  err = vxcore_filetype_set(ctx, json_str.c_str());
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  vxcore_context_destroy(ctx);
  std::cout << "  ✓ test_filetype_set_duplicate_name passed" << std::endl;
  return 0;
}

// Test vxcore_filetype_set: empty name should fail
int test_filetype_set_empty_name() {
  std::cout << "  Running test_filetype_set_empty_name..." << std::endl;
  vxcore_set_test_mode(1);

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  nlohmann::json types = nlohmann::json::array();
  types.push_back(
      {{"name", ""}, {"suffixes", {"x"}}, {"isNewable", true}, {"displayName", "Empty"}});

  std::string json_str = types.dump();
  err = vxcore_filetype_set(ctx, json_str.c_str());
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  vxcore_context_destroy(ctx);
  std::cout << "  ✓ test_filetype_set_empty_name passed" << std::endl;
  return 0;
}

// Test vxcore_filetype_set: invalid JSON should fail
int test_filetype_set_invalid_json() {
  std::cout << "  Running test_filetype_set_invalid_json..." << std::endl;
  vxcore_set_test_mode(1);

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  err = vxcore_filetype_set(ctx, "not valid json");
  ASSERT_EQ(err, VXCORE_ERR_JSON_PARSE);

  // Also test non-array JSON
  err = vxcore_filetype_set(ctx, "{\"name\": \"test\"}");
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  vxcore_context_destroy(ctx);
  std::cout << "  ✓ test_filetype_set_invalid_json passed" << std::endl;
  return 0;
}

// Test vxcore_filetype_set: remove types by omission
int test_filetype_set_remove_type() {
  std::cout << "  Running test_filetype_set_remove_type..." << std::endl;
  vxcore_set_test_mode(1);

  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  // Set only 2 types (removing the defaults)
  nlohmann::json types = nlohmann::json::array();
  types.push_back({{"name", "Markdown"},
                   {"suffixes", {"md"}},
                   {"isNewable", true},
                   {"displayName", "Markdown"}});
  types.push_back({{"name", "Others"},
                   {"suffixes", nlohmann::json::array()},
                   {"isNewable", true},
                   {"displayName", "Others"}});

  std::string json_str = types.dump();
  err = vxcore_filetype_set(ctx, json_str.c_str());
  ASSERT_EQ(err, VXCORE_OK);

  // Verify only 2 remain
  char *list_json = nullptr;
  err = vxcore_filetype_list(ctx, &list_json);
  ASSERT_EQ(err, VXCORE_OK);

  nlohmann::json result = nlohmann::json::parse(list_json);
  vxcore_string_free(list_json);
  ASSERT_EQ(result.size(), 2);
  ASSERT_EQ(result[0]["name"], "Markdown");
  ASSERT_EQ(result[1]["name"], "Others");

  vxcore_context_destroy(ctx);
  std::cout << "  ✓ test_filetype_set_remove_type passed" << std::endl;
  return 0;
}

int main() {
  std::cout << "Running filetype tests..." << std::endl;

  vxcore_set_test_mode(1);

  RUN_TEST(test_filetype_defaults);
  RUN_TEST(test_filetype_get_by_suffix_md);
  RUN_TEST(test_filetype_get_by_suffix_unknown);
  RUN_TEST(test_filetype_case_insensitive);
  RUN_TEST(test_filetype_pdf_not_newable);
  RUN_TEST(test_filetype_display_name_default);
  RUN_TEST(test_filetype_display_name_locale);
  RUN_TEST(test_filetype_c_api_list);
  RUN_TEST(test_filetype_c_api_suffix);
  RUN_TEST(test_filetype_c_api_name);
  RUN_TEST(test_filetype_c_api_name_not_found);
  RUN_TEST(test_filetype_name_case_insensitive);
  RUN_TEST(test_filetype_set_add_type);
  RUN_TEST(test_filetype_set_duplicate_name);
  RUN_TEST(test_filetype_set_empty_name);
  RUN_TEST(test_filetype_set_invalid_json);
  RUN_TEST(test_filetype_set_remove_type);

  std::cout << "✓ All filetype tests passed" << std::endl;
  return 0;
}
