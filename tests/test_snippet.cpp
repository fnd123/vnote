#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>

#include "core/context.h"
#include "core/snippet_manager.h"
#include "test_utils.h"
#include "vxcore/vxcore.h"

// Helper: remove the snippets folder for test isolation.
static void clean_snippets_folder(VxCoreContextHandle ctx) {
  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  std::string path;
  VxCoreError err = mgr->GetSnippetFolderPath(path);
  if (err == VXCORE_OK) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
}

static const char *kTestSnippetJson =
    R"({"type":"text","description":"test desc","content":"hello @@world","cursorMark":"@@","selectionMark":"$$","indentAsFirstLine":false})";

int test_snippet_folder_path() {
  std::cout << "  Running test_snippet_folder_path..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(ctx);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();
  ASSERT_NOT_NULL(mgr);

  std::string path;
  err = mgr->GetSnippetFolderPath(path);
  ASSERT_EQ(err, VXCORE_OK);

  // Path should contain "snippets" substring.
  ASSERT_TRUE(path.find("snippets") != std::string::npos);

  // Folder should be auto-created.
  ASSERT_TRUE(path_exists(path));

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_folder_path passed" << std::endl;
  return 0;
}

int test_snippet_create_and_get() {
  std::cout << "  Running test_snippet_create_and_get..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);
  clean_snippets_folder(ctx);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  // Create a snippet.
  err = mgr->CreateSnippet("greeting", kTestSnippetJson);
  ASSERT_EQ(err, VXCORE_OK);

  // Get it back.
  vxcore::SnippetData data;
  err = mgr->GetSnippet("greeting", data);
  ASSERT_EQ(err, VXCORE_OK);

  ASSERT_EQ(data.name, std::string("greeting"));
  ASSERT_EQ(data.description, std::string("test desc"));
  ASSERT_TRUE(data.type == vxcore::SnippetType::kText);
  ASSERT_EQ(data.content, std::string("hello @@world"));
  ASSERT_EQ(data.cursor_mark, std::string("@@"));
  ASSERT_EQ(data.selection_mark, std::string("$$"));
  ASSERT_FALSE(data.indent_as_first_line);
  ASSERT_FALSE(data.is_builtin);

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_create_and_get passed" << std::endl;
  return 0;
}

int test_snippet_list() {
  std::cout << "  Running test_snippet_list..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);
  clean_snippets_folder(ctx);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  // Create 2 user snippets.
  err = mgr->CreateSnippet("user_a", kTestSnippetJson);
  ASSERT_EQ(err, VXCORE_OK);
  err = mgr->CreateSnippet("user_b", kTestSnippetJson);
  ASSERT_EQ(err, VXCORE_OK);

  // List all snippets.
  std::vector<vxcore::SnippetData> snippets;
  err = mgr->ListSnippets(snippets);
  ASSERT_EQ(err, VXCORE_OK);

  // Should have 24 built-ins + 2 user = at least 26.
  ASSERT_TRUE(snippets.size() >= 26u);

  // Verify user snippets are present.
  bool found_a = false, found_b = false;
  int builtin_count = 0;
  for (const auto &s : snippets) {
    if (s.name == "user_a") found_a = true;
    if (s.name == "user_b") found_b = true;
    if (s.is_builtin) builtin_count++;
  }
  ASSERT_TRUE(found_a);
  ASSERT_TRUE(found_b);
  ASSERT_EQ(builtin_count, 24);

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_list passed" << std::endl;
  return 0;
}

int test_snippet_delete() {
  std::cout << "  Running test_snippet_delete..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);
  clean_snippets_folder(ctx);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  // Create then delete.
  err = mgr->CreateSnippet("to_delete", kTestSnippetJson);
  ASSERT_EQ(err, VXCORE_OK);

  err = mgr->DeleteSnippet("to_delete");
  ASSERT_EQ(err, VXCORE_OK);

  // Get should return NOT_FOUND.
  vxcore::SnippetData data;
  err = mgr->GetSnippet("to_delete", data);
  ASSERT_EQ(err, VXCORE_ERR_NOT_FOUND);

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_delete passed" << std::endl;
  return 0;
}

int test_snippet_rename() {
  std::cout << "  Running test_snippet_rename..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);
  clean_snippets_folder(ctx);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  // Create "old_name".
  err = mgr->CreateSnippet("old_name", kTestSnippetJson);
  ASSERT_EQ(err, VXCORE_OK);

  // Rename to "new_name".
  err = mgr->RenameSnippet("old_name", "new_name");
  ASSERT_EQ(err, VXCORE_OK);

  // Old name should not exist.
  vxcore::SnippetData data;
  err = mgr->GetSnippet("old_name", data);
  ASSERT_EQ(err, VXCORE_ERR_NOT_FOUND);

  // New name should have the content.
  err = mgr->GetSnippet("new_name", data);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_EQ(data.content, std::string("hello @@world"));

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_rename passed" << std::endl;
  return 0;
}

int test_snippet_update() {
  std::cout << "  Running test_snippet_update..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);
  clean_snippets_folder(ctx);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  // Create a snippet.
  err = mgr->CreateSnippet("updatable", kTestSnippetJson);
  ASSERT_EQ(err, VXCORE_OK);

  // Update with new content.
  const char *new_json =
      R"({"type":"text","description":"updated desc","content":"new content","cursorMark":"@@","selectionMark":"$$","indentAsFirstLine":true})";
  err = mgr->UpdateSnippet("updatable", new_json);
  ASSERT_EQ(err, VXCORE_OK);

  // Get returns updated content.
  vxcore::SnippetData data;
  err = mgr->GetSnippet("updatable", data);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_EQ(data.description, std::string("updated desc"));
  ASSERT_EQ(data.content, std::string("new content"));
  ASSERT_TRUE(data.indent_as_first_line);

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_update passed" << std::endl;
  return 0;
}

int test_snippet_error_cases() {
  std::cout << "  Running test_snippet_error_cases..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);
  clean_snippets_folder(ctx);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  // Invalid names: ".", "..", path traversal, percent sign.
  err = mgr->CreateSnippet(".", kTestSnippetJson);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = mgr->CreateSnippet("..", kTestSnippetJson);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = mgr->CreateSnippet("a/b", kTestSnippetJson);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = mgr->CreateSnippet("my%name", kTestSnippetJson);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  // Duplicate create.
  err = mgr->CreateSnippet("exists", kTestSnippetJson);
  ASSERT_EQ(err, VXCORE_OK);
  err = mgr->CreateSnippet("exists", kTestSnippetJson);
  ASSERT_EQ(err, VXCORE_ERR_ALREADY_EXISTS);

  // Delete non-existent.
  err = mgr->DeleteSnippet("nonexistent");
  ASSERT_EQ(err, VXCORE_ERR_NOT_FOUND);

  // Create with built-in name should fail.
  err = mgr->CreateSnippet("date", kTestSnippetJson);
  ASSERT_EQ(err, VXCORE_ERR_ALREADY_EXISTS);

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_error_cases passed" << std::endl;
  return 0;
}

int test_snippet_builtin_names_reserved() {
  std::cout << "  Running test_snippet_builtin_names_reserved..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);
  clean_snippets_folder(ctx);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  // Cannot create user snippet with built-in name.
  err = mgr->CreateSnippet("date", kTestSnippetJson);
  ASSERT_EQ(err, VXCORE_ERR_ALREADY_EXISTS);

  err = mgr->CreateSnippet("note", kTestSnippetJson);
  ASSERT_EQ(err, VXCORE_ERR_ALREADY_EXISTS);

  err = mgr->CreateSnippet("d", kTestSnippetJson);
  ASSERT_EQ(err, VXCORE_ERR_ALREADY_EXISTS);

  // Cannot delete built-in snippet.
  err = mgr->DeleteSnippet("date");
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  err = mgr->DeleteSnippet("note");
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  // Cannot rename a built-in snippet (as source).
  err = mgr->RenameSnippet("date", "mydate");
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  // Cannot rename a user snippet TO a built-in name.
  err = mgr->CreateSnippet("user_snippet", kTestSnippetJson);
  ASSERT_EQ(err, VXCORE_OK);
  err = mgr->RenameSnippet("user_snippet", "date");
  ASSERT_EQ(err, VXCORE_ERR_ALREADY_EXISTS);

  // Cannot update a built-in snippet.
  err = mgr->UpdateSnippet("date", kTestSnippetJson);
  ASSERT_EQ(err, VXCORE_ERR_INVALID_PARAM);

  // Can get built-in snippets.
  vxcore::SnippetData data;
  err = mgr->GetSnippet("date", data);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_TRUE(data.is_builtin);
  ASSERT_EQ(data.name, std::string("date"));

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_builtin_names_reserved passed" << std::endl;
  return 0;
}

int test_snippet_apply_basic() {
  std::cout << "  Running test_snippet_apply_basic..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);
  clean_snippets_folder(ctx);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  // Create a snippet with cursor mark.
  const char *json =
      R"({"type":"text","description":"","content":"Hello @@World","cursorMark":"@@","selectionMark":"$$","indentAsFirstLine":false})";
  err = mgr->CreateSnippet("test_cm", json);
  ASSERT_EQ(err, VXCORE_OK);

  vxcore::OverrideMap overrides;
  auto result = mgr->ApplySnippet("test_cm", "", "", overrides);

  // Cursor mark removed, cursor_offset at position where @@ was.
  ASSERT_EQ(result.text, std::string("Hello World"));
  ASSERT_EQ(result.cursor_offset, 6);

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_apply_basic passed" << std::endl;
  return 0;
}

int test_snippet_apply_selection_mark() {
  std::cout << "  Running test_snippet_apply_selection_mark..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);
  clean_snippets_folder(ctx);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  // Snippet with selection mark and cursor mark.
  const char *json =
      R"({"type":"text","description":"","content":"begin $$@@end $$","cursorMark":"@@","selectionMark":"$$","indentAsFirstLine":false})";
  err = mgr->CreateSnippet("test_sel", json);
  ASSERT_EQ(err, VXCORE_OK);

  vxcore::OverrideMap overrides;
  auto result = mgr->ApplySnippet("test_sel", "SELECTED", "", overrides);

  // Content: "begin $$@@end $$"
  // After indent (no-op): "begin $$@@end $$"
  // Cursor split: first="begin $$", second="end $$"
  // Selection replace: first="begin SELECTED", second="end SELECTED"
  // cursor_offset = len("begin SELECTED") = 14
  ASSERT_EQ(result.text, std::string("begin SELECTEDend SELECTED"));
  ASSERT_EQ(result.cursor_offset, 14);

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_apply_selection_mark passed" << std::endl;
  return 0;
}

int test_snippet_apply_indentation() {
  std::cout << "  Running test_snippet_apply_indentation..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);
  clean_snippets_folder(ctx);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  // Multi-line snippet with indentAsFirstLine=true.
  const char *json =
      R"({"type":"text","description":"","content":"line1\nline2\nline3","cursorMark":"@@","selectionMark":"$$","indentAsFirstLine":true})";
  err = mgr->CreateSnippet("test_indent", json);
  ASSERT_EQ(err, VXCORE_OK);

  vxcore::OverrideMap overrides;
  auto result = mgr->ApplySnippet("test_indent", "", "  ", overrides);

  // Lines 2+ get "  " prefix.
  ASSERT_EQ(result.text, std::string("line1\n  line2\n  line3"));
  // No cursor mark → cursor at end.
  ASSERT_EQ(result.cursor_offset, static_cast<int>(result.text.size()));

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_apply_indentation passed" << std::endl;
  return 0;
}

int test_snippet_apply_indent_trailing_newline() {
  std::cout << "  Running test_snippet_apply_indent_trailing_newline..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);
  clean_snippets_folder(ctx);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  // Content ends with \n — should NOT produce trailing whitespace after indentation.
  const char *json =
      R"({"type":"text","description":"","content":"line1\nline2\n","cursorMark":"@@","selectionMark":"$$","indentAsFirstLine":true})";
  err = mgr->CreateSnippet("test_trailing_nl", json);
  ASSERT_EQ(err, VXCORE_OK);

  vxcore::OverrideMap overrides;
  auto result = mgr->ApplySnippet("test_trailing_nl", "", "  ", overrides);

  // Line 2 gets "  " prefix, but no trailing whitespace after final \n.
  ASSERT_EQ(result.text, std::string("line1\n  line2\n"));
  // No cursor mark → cursor at end.
  ASSERT_EQ(result.cursor_offset, static_cast<int>(result.text.size()));

  vxcore_context_destroy(ctx);
  std::cout << "  ✓ test_snippet_apply_indent_trailing_newline passed" << std::endl;
  return 0;
}

int test_snippet_apply_no_cursor_mark() {
  std::cout << "  Running test_snippet_apply_no_cursor_mark..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);
  clean_snippets_folder(ctx);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  // Snippet without cursor mark in content.
  const char *json =
      R"({"type":"text","description":"","content":"no mark here","cursorMark":"@@","selectionMark":"$$","indentAsFirstLine":false})";
  err = mgr->CreateSnippet("test_nocm", json);
  ASSERT_EQ(err, VXCORE_OK);

  vxcore::OverrideMap overrides;
  auto result = mgr->ApplySnippet("test_nocm", "", "", overrides);

  ASSERT_EQ(result.text, std::string("no mark here"));
  // cursor_offset == text length when no cursor mark found.
  ASSERT_EQ(result.cursor_offset, static_cast<int>(result.text.size()));

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_apply_no_cursor_mark passed" << std::endl;
  return 0;
}

int test_snippet_apply_override() {
  std::cout << "  Running test_snippet_apply_override..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);
  clean_snippets_folder(ctx);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  // "date" is a built-in — override it.
  vxcore::OverrideMap overrides;
  overrides["date"] = "2025-01-01";

  auto result = mgr->ApplySnippet("date", "", "", overrides);

  ASSERT_EQ(result.text, std::string("2025-01-01"));
  ASSERT_EQ(result.cursor_offset, 10);

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_apply_override passed" << std::endl;
  return 0;
}

int test_snippet_apply_custom_marks() {
  std::cout << "  Running test_snippet_apply_custom_marks..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);
  clean_snippets_folder(ctx);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  // Custom cursor mark "<<>>" and selection mark "[[]]".
  const char *json =
      R"({"type":"text","description":"","content":"A [[]]<<>>B [[]]","cursorMark":"<<>>","selectionMark":"[[]]","indentAsFirstLine":false})";
  err = mgr->CreateSnippet("test_custom", json);
  ASSERT_EQ(err, VXCORE_OK);

  vxcore::OverrideMap overrides;
  auto result = mgr->ApplySnippet("test_custom", "X", "", overrides);

  // Content: "A [[]]<<>>B [[]]"
  // Cursor split on "<<>>": first="A [[]]", second="B [[]]"
  // Selection replace: first="A X", second="B X"
  // cursor_offset = len("A X") = 3
  ASSERT_EQ(result.text, std::string("A XB X"));
  ASSERT_EQ(result.cursor_offset, 3);

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_apply_custom_marks passed" << std::endl;
  return 0;
}

int test_snippet_builtin_list() {
  std::cout << "  Running test_snippet_builtin_list..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);
  clean_snippets_folder(ctx);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  std::vector<vxcore::SnippetData> snippets;
  err = mgr->ListSnippets(snippets);
  ASSERT_EQ(err, VXCORE_OK);

  // All 24 built-in names must be present.
  const char *expected[] = {"d",  "dd",   "ddd",  "dddd", "M",    "MM",       "MMM",  "MMMM",
                            "yy", "yyyy", "w",    "ww",   "H",    "HH",       "m",    "mm",
                            "s",  "ss",   "date", "da",   "time", "datetime", "note", "no"};
  for (const auto &name : expected) {
    bool found = false;
    for (const auto &s : snippets) {
      if (s.name == name) {
        found = true;
        ASSERT_TRUE(s.is_builtin);
        break;
      }
    }
    if (!found) {
      std::cerr << "  FAIL: built-in snippet '" << name << "' not found" << std::endl;
      return 1;
    }
  }

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_builtin_list passed" << std::endl;
  return 0;
}

int test_snippet_builtin_date_format() {
  std::cout << "  Running test_snippet_builtin_date_format..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  vxcore::OverrideMap overrides;
  auto result = mgr->ApplySnippet("date", "", "", overrides);

  // Format: YYYY-MM-DD (length 10, dashes at 4 and 7).
  ASSERT_EQ(static_cast<int>(result.text.size()), 10);
  ASSERT_EQ(result.text[4], '-');
  ASSERT_EQ(result.text[7], '-');
  // All other positions should be digits.
  for (int i = 0; i < 10; ++i) {
    if (i == 4 || i == 7) continue;
    ASSERT_TRUE(result.text[i] >= '0' && result.text[i] <= '9');
  }

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_builtin_date_format passed" << std::endl;
  return 0;
}

int test_snippet_builtin_da_format() {
  std::cout << "  Running test_snippet_builtin_da_format..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  vxcore::OverrideMap overrides;
  auto result = mgr->ApplySnippet("da", "", "", overrides);

  // Format: YYYYMMDD (length 8, all digits).
  ASSERT_EQ(static_cast<int>(result.text.size()), 8);
  for (int i = 0; i < 8; ++i) {
    ASSERT_TRUE(result.text[i] >= '0' && result.text[i] <= '9');
  }

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_builtin_da_format passed" << std::endl;
  return 0;
}

int test_snippet_builtin_time_format() {
  std::cout << "  Running test_snippet_builtin_time_format..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  vxcore::OverrideMap overrides;
  auto result = mgr->ApplySnippet("time", "", "", overrides);

  // Format: HH:MM:SS (length 8, colons at 2 and 5).
  ASSERT_EQ(static_cast<int>(result.text.size()), 8);
  ASSERT_EQ(result.text[2], ':');
  ASSERT_EQ(result.text[5], ':');

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_builtin_time_format passed" << std::endl;
  return 0;
}

int test_snippet_builtin_datetime_format() {
  std::cout << "  Running test_snippet_builtin_datetime_format..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  vxcore::OverrideMap overrides;
  auto result = mgr->ApplySnippet("datetime", "", "", overrides);

  // Format: YYYY-MM-DD_HH:mm:ss (length 19, underscore at 10).
  ASSERT_EQ(static_cast<int>(result.text.size()), 19);
  ASSERT_EQ(result.text[4], '-');
  ASSERT_EQ(result.text[7], '-');
  ASSERT_EQ(result.text[10], '_');
  ASSERT_EQ(result.text[13], ':');
  ASSERT_EQ(result.text[16], ':');

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_builtin_datetime_format passed" << std::endl;
  return 0;
}

int test_snippet_builtin_day_range() {
  std::cout << "  Running test_snippet_builtin_day_range..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  vxcore::OverrideMap overrides;
  auto result = mgr->ApplySnippet("d", "", "", overrides);
  int d = std::stoi(result.text);
  ASSERT_TRUE(d >= 1 && d <= 31);

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_builtin_day_range passed" << std::endl;
  return 0;
}

int test_snippet_builtin_month_range() {
  std::cout << "  Running test_snippet_builtin_month_range..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  vxcore::OverrideMap overrides;
  auto result = mgr->ApplySnippet("M", "", "", overrides);
  int m = std::stoi(result.text);
  ASSERT_TRUE(m >= 1 && m <= 12);

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_builtin_month_range passed" << std::endl;
  return 0;
}

int test_snippet_builtin_week_range() {
  std::cout << "  Running test_snippet_builtin_week_range..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  vxcore::OverrideMap overrides;
  auto result = mgr->ApplySnippet("w", "", "", overrides);
  int w = std::stoi(result.text);
  ASSERT_TRUE(w >= 1 && w <= 53);

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_builtin_week_range passed" << std::endl;
  return 0;
}

int test_snippet_builtin_note_fallback() {
  std::cout << "  Running test_snippet_builtin_note_fallback..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  vxcore::OverrideMap overrides;
  auto result = mgr->ApplySnippet("note", "", "", overrides);
  ASSERT_EQ(result.text, std::string("[Value Not Available]"));

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_builtin_note_fallback passed" << std::endl;
  return 0;
}

int test_snippet_builtin_note_override() {
  std::cout << "  Running test_snippet_builtin_note_override..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  vxcore::OverrideMap overrides;
  overrides["note"] = "MyNote.md";
  auto result = mgr->ApplySnippet("note", "", "", overrides);
  ASSERT_EQ(result.text, std::string("MyNote.md"));
  ASSERT_EQ(result.cursor_offset, 9);

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_builtin_note_override passed" << std::endl;
  return 0;
}

int test_snippet_builtin_no_override() {
  std::cout << "  Running test_snippet_builtin_no_override..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  vxcore::OverrideMap overrides;
  overrides["no"] = "MyNote";
  auto result = mgr->ApplySnippet("no", "", "", overrides);
  ASSERT_EQ(result.text, std::string("MyNote"));
  ASSERT_EQ(result.cursor_offset, 6);

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_builtin_no_override passed" << std::endl;
  return 0;
}

int test_snippet_expand_basic() {
  std::cout << "  Running test_snippet_expand_basic..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);
  clean_snippets_folder(ctx);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  // Create a snippet whose content references %date%.
  const char *json =
      R"({"type":"text","description":"","content":"Today is %date%","cursorMark":"@@","selectionMark":"$$","indentAsFirstLine":false})";
  err = mgr->CreateSnippet("today_msg", json);
  ASSERT_EQ(err, VXCORE_OK);

  vxcore::OverrideMap overrides;
  auto result = mgr->ApplySnippet("today_msg", "", "", overrides);

  // %date% should be expanded to YYYY-MM-DD (length 10).
  // Result: "Today is " (9 chars) + date (10 chars) = 19 chars.
  ASSERT_EQ(static_cast<int>(result.text.size()), 19);
  ASSERT_TRUE(result.text.find("%date%") == std::string::npos);
  ASSERT_TRUE(result.text.substr(0, 9) == "Today is ");

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_expand_basic passed" << std::endl;
  return 0;
}

int test_snippet_expand_nested() {
  std::cout << "  Running test_snippet_expand_nested..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);
  clean_snippets_folder(ctx);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  // User snippet "greeting" has content referencing %date%.
  const char *greeting_json =
      R"({"type":"text","description":"","content":"Hello on %date%","cursorMark":"@@","selectionMark":"$$","indentAsFirstLine":false})";
  err = mgr->CreateSnippet("greeting", greeting_json);
  ASSERT_EQ(err, VXCORE_OK);

  // User snippet "msg" references %greeting%.
  const char *msg_json =
      R"({"type":"text","description":"","content":"%greeting%!","cursorMark":"@@","selectionMark":"$$","indentAsFirstLine":false})";
  err = mgr->CreateSnippet("msg", msg_json);
  ASSERT_EQ(err, VXCORE_OK);

  vxcore::OverrideMap overrides;
  auto result = mgr->ApplySnippet("msg", "", "", overrides);

  // "msg" -> "%greeting%!" -> "Hello on %date%!" -> "Hello on YYYY-MM-DD!"
  ASSERT_TRUE(result.text.find("%greeting%") == std::string::npos);
  ASSERT_TRUE(result.text.find("%date%") == std::string::npos);
  ASSERT_TRUE(result.text.substr(0, 9) == "Hello on ");
  ASSERT_TRUE(result.text.back() == '!');
  // Total: "Hello on " (9) + date (10) + "!" (1) = 20
  ASSERT_EQ(static_cast<int>(result.text.size()), 20);

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_expand_nested passed" << std::endl;
  return 0;
}

int test_snippet_expand_unknown() {
  std::cout << "  Running test_snippet_expand_unknown..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);
  clean_snippets_folder(ctx);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  // Content with unknown snippet name — should be left as-is.
  const char *json =
      R"({"type":"text","description":"","content":"value is %unknown_xyz%","cursorMark":"@@","selectionMark":"$$","indentAsFirstLine":false})";
  err = mgr->CreateSnippet("test_unk", json);
  ASSERT_EQ(err, VXCORE_OK);

  vxcore::OverrideMap overrides;
  auto result = mgr->ApplySnippet("test_unk", "", "", overrides);

  ASSERT_EQ(result.text, std::string("value is %unknown_xyz%"));

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_expand_unknown passed" << std::endl;
  return 0;
}

int test_snippet_expand_override() {
  std::cout << "  Running test_snippet_expand_override..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);
  clean_snippets_folder(ctx);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  // Content references %note% which is a built-in with override.
  const char *json =
      R"({"type":"text","description":"","content":"File: %note%","cursorMark":"@@","selectionMark":"$$","indentAsFirstLine":false})";
  err = mgr->CreateSnippet("test_ov", json);
  ASSERT_EQ(err, VXCORE_OK);

  vxcore::OverrideMap overrides;
  overrides["note"] = "test.md";
  auto result = mgr->ApplySnippet("test_ov", "", "", overrides);

  ASSERT_EQ(result.text, std::string("File: test.md"));

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_expand_override passed" << std::endl;
  return 0;
}

int test_snippet_expand_infinite_loop_guard() {
  std::cout << "  Running test_snippet_expand_infinite_loop_guard..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);
  clean_snippets_folder(ctx);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  // Self-referencing snippet: "loop" expands to "%loop%".
  const char *json =
      R"({"type":"text","description":"","content":"%loop%","cursorMark":"@@","selectionMark":"$$","indentAsFirstLine":false})";
  err = mgr->CreateSnippet("loop", json);
  ASSERT_EQ(err, VXCORE_OK);

  vxcore::OverrideMap overrides;
  auto result = mgr->ApplySnippet("loop", "", "", overrides);

  // Must terminate (not hang). The result will be some partially expanded string.
  // The key assertion is that we get here at all (no infinite loop).
  ASSERT_TRUE(result.text.size() > 0u ||
              result.text.size() == 0u);  // Always true — just verifies termination.

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_expand_infinite_loop_guard passed" << std::endl;
  return 0;
}

int test_snippet_expand_cursor_offset_adjusted() {
  std::cout << "  Running test_snippet_expand_cursor_offset_adjusted..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);
  clean_snippets_folder(ctx);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  // Snippet with %date% BEFORE cursor mark @@.
  // Content: "%date% at @@end"
  // After apply: cursor split on "@@" → first="%date% at ", second="end"
  // cursor_offset = len("%date% at ") = 10
  // After expand: "%date%" (6 chars) replaced with "YYYY-MM-DD" (10 chars)
  // first becomes "YYYY-MM-DD at " (14 chars), cursor_offset adjusted: 10 + (10-6) = 14
  const char *json =
      R"({"type":"text","description":"","content":"%date% at @@end","cursorMark":"@@","selectionMark":"$$","indentAsFirstLine":false})";
  err = mgr->CreateSnippet("test_co", json);
  ASSERT_EQ(err, VXCORE_OK);

  vxcore::OverrideMap overrides;
  auto result = mgr->ApplySnippet("test_co", "", "", overrides);

  // After expansion: "YYYY-MM-DD at end"
  // cursor_offset should be 14 (position after "YYYY-MM-DD at ").
  ASSERT_TRUE(result.text.find("@@") == std::string::npos);
  ASSERT_TRUE(result.text.find("%date%") == std::string::npos);
  ASSERT_EQ(result.cursor_offset, 14);

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_expand_cursor_offset_adjusted passed" << std::endl;
  return 0;
}

int test_snippet_expand_empty_percent() {
  std::cout << "  Running test_snippet_expand_empty_percent..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);
  clean_snippets_folder(ctx);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  // Content with %% — empty snippet name, should be skipped/left as-is.
  const char *json =
      R"({"type":"text","description":"","content":"100%% done","cursorMark":"@@","selectionMark":"$$","indentAsFirstLine":false})";
  err = mgr->CreateSnippet("test_pct", json);
  ASSERT_EQ(err, VXCORE_OK);

  vxcore::OverrideMap overrides;
  auto result = mgr->ApplySnippet("test_pct", "", "", overrides);

  // %% is skipped — left as-is in output.
  ASSERT_EQ(result.text, std::string("100%% done"));

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_expand_empty_percent passed" << std::endl;
  return 0;
}

int test_snippet_expand_multiple() {
  std::cout << "  Running test_snippet_expand_multiple..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);
  clean_snippets_folder(ctx);

  auto *vctx = reinterpret_cast<vxcore::VxCoreContext *>(ctx);
  auto *mgr = vctx->snippet_manager.get();

  // Content with multiple snippet references: %yyyy%-%MM%-%dd%.
  const char *json =
      R"({"type":"text","description":"","content":"%yyyy%-%MM%-%dd%","cursorMark":"@@","selectionMark":"$$","indentAsFirstLine":false})";
  err = mgr->CreateSnippet("test_multi", json);
  ASSERT_EQ(err, VXCORE_OK);

  vxcore::OverrideMap overrides;
  auto result = mgr->ApplySnippet("test_multi", "", "", overrides);

  // Should produce YYYY-MM-DD format (same as "date" built-in).
  ASSERT_EQ(static_cast<int>(result.text.size()), 10);
  ASSERT_EQ(result.text[4], '-');
  ASSERT_EQ(result.text[7], '-');
  // All other positions should be digits.
  for (int i = 0; i < 10; ++i) {
    if (i == 4 || i == 7) continue;
    ASSERT_TRUE(result.text[i] >= '0' && result.text[i] <= '9');
  }

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_expand_multiple passed" << std::endl;
  return 0;
}

// ========== C API Tests ==========
// These tests use ONLY vxcore_snippet_* C functions (no direct SnippetManager access).

int test_snippet_api_null_checks() {
  std::cout << "  Running test_snippet_api_null_checks..." << std::endl;

  VxCoreError err;
  char *out = nullptr;

  // vxcore_snippet_get_folder_path
  err = vxcore_snippet_get_folder_path(nullptr, &out);
  ASSERT_EQ(err, VXCORE_ERR_NULL_POINTER);

  // vxcore_snippet_list
  err = vxcore_snippet_list(nullptr, &out);
  ASSERT_EQ(err, VXCORE_ERR_NULL_POINTER);

  // vxcore_snippet_get
  err = vxcore_snippet_get(nullptr, "x", &out);
  ASSERT_EQ(err, VXCORE_ERR_NULL_POINTER);

  // vxcore_snippet_create
  err = vxcore_snippet_create(nullptr, "x", "{}");
  ASSERT_EQ(err, VXCORE_ERR_NULL_POINTER);

  // vxcore_snippet_delete
  err = vxcore_snippet_delete(nullptr, "x");
  ASSERT_EQ(err, VXCORE_ERR_NULL_POINTER);

  // vxcore_snippet_rename
  err = vxcore_snippet_rename(nullptr, "x", "y");
  ASSERT_EQ(err, VXCORE_ERR_NULL_POINTER);

  // vxcore_snippet_update
  err = vxcore_snippet_update(nullptr, "x", "{}");
  ASSERT_EQ(err, VXCORE_ERR_NULL_POINTER);

  // vxcore_snippet_apply
  err = vxcore_snippet_apply(nullptr, "x", "", "", "{}", &out);
  ASSERT_EQ(err, VXCORE_ERR_NULL_POINTER);

  std::cout << "  \xe2\x9c\x93 test_snippet_api_null_checks passed" << std::endl;
  return 0;
}

int test_snippet_api_crud_cycle() {
  std::cout << "  Running test_snippet_api_crud_cycle..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);
  clean_snippets_folder(ctx);

  // 1. Get folder path.
  char *folder_path = nullptr;
  err = vxcore_snippet_get_folder_path(ctx, &folder_path);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(folder_path);
  vxcore_string_free(folder_path);

  // 2. Create a snippet.
  const char *create_json =
      R"({"type":"text","description":"api test","content":"hello @@world","cursorMark":"@@","selectionMark":"$$","indentAsFirstLine":false})";
  err = vxcore_snippet_create(ctx, "apitest", create_json);
  ASSERT_EQ(err, VXCORE_OK);

  // 3. List — verify "apitest" is in the array.
  char *list_json = nullptr;
  err = vxcore_snippet_list(ctx, &list_json);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(list_json);
  {
    auto j = nlohmann::json::parse(list_json);
    bool found = false;
    for (const auto &item : j) {
      if (item["name"] == "apitest") {
        found = true;
        ASSERT_EQ(item["type"].get<std::string>(), std::string("text"));
        ASSERT_EQ(item["description"].get<std::string>(), std::string("api test"));
        ASSERT_FALSE(item["isBuiltin"].get<bool>());
        break;
      }
    }
    ASSERT_TRUE(found);
  }
  vxcore_string_free(list_json);

  // 4. Get — verify all fields.
  char *get_json = nullptr;
  err = vxcore_snippet_get(ctx, "apitest", &get_json);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(get_json);
  {
    auto j = nlohmann::json::parse(get_json);
    ASSERT_EQ(j["name"].get<std::string>(), std::string("apitest"));
    ASSERT_EQ(j["content"].get<std::string>(), std::string("hello @@world"));
    ASSERT_EQ(j["cursorMark"].get<std::string>(), std::string("@@"));
    ASSERT_EQ(j["selectionMark"].get<std::string>(), std::string("$$"));
    ASSERT_FALSE(j["indentAsFirstLine"].get<bool>());
    ASSERT_FALSE(j["isBuiltin"].get<bool>());
  }
  vxcore_string_free(get_json);

  // 5. Update — change content.
  const char *update_json =
      R"({"type":"text","description":"updated","content":"updated content","cursorMark":"@@","selectionMark":"$$","indentAsFirstLine":true})";
  err = vxcore_snippet_update(ctx, "apitest", update_json);
  ASSERT_EQ(err, VXCORE_OK);

  // Verify update via get.
  char *get_json2 = nullptr;
  err = vxcore_snippet_get(ctx, "apitest", &get_json2);
  ASSERT_EQ(err, VXCORE_OK);
  {
    auto j = nlohmann::json::parse(get_json2);
    ASSERT_EQ(j["description"].get<std::string>(), std::string("updated"));
    ASSERT_EQ(j["content"].get<std::string>(), std::string("updated content"));
    ASSERT_TRUE(j["indentAsFirstLine"].get<bool>());
  }
  vxcore_string_free(get_json2);

  // 6. Rename.
  err = vxcore_snippet_rename(ctx, "apitest", "apitest2");
  ASSERT_EQ(err, VXCORE_OK);

  // Old name should not exist.
  char *get_old = nullptr;
  err = vxcore_snippet_get(ctx, "apitest", &get_old);
  ASSERT_EQ(err, VXCORE_ERR_NOT_FOUND);

  // New name should exist.
  char *get_new = nullptr;
  err = vxcore_snippet_get(ctx, "apitest2", &get_new);
  ASSERT_EQ(err, VXCORE_OK);
  vxcore_string_free(get_new);

  // 7. Delete.
  err = vxcore_snippet_delete(ctx, "apitest2");
  ASSERT_EQ(err, VXCORE_OK);

  // 8. Get after delete → NOT_FOUND.
  char *get_deleted = nullptr;
  err = vxcore_snippet_get(ctx, "apitest2", &get_deleted);
  ASSERT_EQ(err, VXCORE_ERR_NOT_FOUND);

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_api_crud_cycle passed" << std::endl;
  return 0;
}

int test_snippet_api_apply() {
  std::cout << "  Running test_snippet_api_apply..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  // Apply "date" via C API.
  char *out_json = nullptr;
  err = vxcore_snippet_apply(ctx, "date", "", "", "{}", &out_json);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(out_json);
  {
    auto j = nlohmann::json::parse(out_json);
    ASSERT_TRUE(j.contains("text"));
    ASSERT_TRUE(j.contains("cursorOffset"));
    std::string text = j["text"].get<std::string>();
    int offset = j["cursorOffset"].get<int>();
    // "date" produces YYYY-MM-DD (length 10).
    ASSERT_EQ(static_cast<int>(text.size()), 10);
    ASSERT_EQ(text[4], '-');
    ASSERT_EQ(text[7], '-');
    ASSERT_EQ(offset, 10);
  }
  vxcore_string_free(out_json);

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_api_apply passed" << std::endl;
  return 0;
}

int test_snippet_api_apply_with_overrides() {
  std::cout << "  Running test_snippet_api_apply_with_overrides..." << std::endl;
  VxCoreContextHandle ctx = nullptr;
  VxCoreError err = vxcore_context_create(nullptr, &ctx);
  ASSERT_EQ(err, VXCORE_OK);

  // Apply "note" with override via C API.
  char *out_json = nullptr;
  err = vxcore_snippet_apply(ctx, "note", "", "", R"({"note":"test.md"})", &out_json);
  ASSERT_EQ(err, VXCORE_OK);
  ASSERT_NOT_NULL(out_json);
  {
    auto j = nlohmann::json::parse(out_json);
    std::string text = j["text"].get<std::string>();
    int offset = j["cursorOffset"].get<int>();
    ASSERT_EQ(text, std::string("test.md"));
    ASSERT_EQ(offset, 7);
  }
  vxcore_string_free(out_json);

  vxcore_context_destroy(ctx);
  std::cout << "  \xe2\x9c\x93 test_snippet_api_apply_with_overrides passed" << std::endl;
  return 0;
}

int main() {
  vxcore_set_test_mode(1);
  vxcore_clear_test_directory();

  std::cout << "Running snippet tests..." << std::endl;

  RUN_TEST(test_snippet_folder_path);
  RUN_TEST(test_snippet_create_and_get);
  RUN_TEST(test_snippet_list);
  RUN_TEST(test_snippet_delete);
  RUN_TEST(test_snippet_rename);
  RUN_TEST(test_snippet_update);
  RUN_TEST(test_snippet_error_cases);
  RUN_TEST(test_snippet_builtin_names_reserved);
  RUN_TEST(test_snippet_apply_basic);
  RUN_TEST(test_snippet_apply_selection_mark);
  RUN_TEST(test_snippet_apply_indentation);
  RUN_TEST(test_snippet_apply_indent_trailing_newline);
  RUN_TEST(test_snippet_apply_no_cursor_mark);
  RUN_TEST(test_snippet_apply_override);
  RUN_TEST(test_snippet_apply_custom_marks);
  RUN_TEST(test_snippet_builtin_list);
  RUN_TEST(test_snippet_builtin_date_format);
  RUN_TEST(test_snippet_builtin_da_format);
  RUN_TEST(test_snippet_builtin_time_format);
  RUN_TEST(test_snippet_builtin_datetime_format);
  RUN_TEST(test_snippet_builtin_day_range);
  RUN_TEST(test_snippet_builtin_month_range);
  RUN_TEST(test_snippet_builtin_week_range);
  RUN_TEST(test_snippet_builtin_note_fallback);
  RUN_TEST(test_snippet_builtin_note_override);
  RUN_TEST(test_snippet_builtin_no_override);
  RUN_TEST(test_snippet_expand_basic);
  RUN_TEST(test_snippet_expand_nested);
  RUN_TEST(test_snippet_expand_unknown);
  RUN_TEST(test_snippet_expand_override);
  RUN_TEST(test_snippet_expand_infinite_loop_guard);
  RUN_TEST(test_snippet_expand_cursor_offset_adjusted);
  RUN_TEST(test_snippet_expand_empty_percent);
  RUN_TEST(test_snippet_expand_multiple);
  RUN_TEST(test_snippet_api_null_checks);
  RUN_TEST(test_snippet_api_crud_cycle);
  RUN_TEST(test_snippet_api_apply);
  RUN_TEST(test_snippet_api_apply_with_overrides);

  std::cout << "\nAll snippet tests passed!" << std::endl;
  vxcore_clear_test_directory();
  return 0;
}
