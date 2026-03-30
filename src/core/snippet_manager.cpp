#include "core/snippet_manager.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <nlohmann/json.hpp>

#include "core/config_manager.h"
#include "utils/file_utils.h"

namespace vxcore {

// Zero-pad an integer value to the given width.
static std::string ZeroPad(int value, int width) {
  std::string s = std::to_string(value);
  while (static_cast<int>(s.size()) < width) {
    s = "0" + s;
  }
  return s;
}

// Get the current local time as a struct tm.
static struct tm GetLocalTime() {
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  struct tm local_tm;
#ifdef _WIN32
  localtime_s(&local_tm, &t);
#else
  localtime_r(&t, &local_tm);
#endif
  return local_tm;
}

// Compute ISO 8601 week number (1-53). Week 1 contains January 4th.
// Monday = 1, Sunday = 7.
static int IsoWeekNumber(const struct tm &tm_val) {
  // tm_yday is 0-based (0 = Jan 1). Convert to 1-based.
  int yday = tm_val.tm_yday + 1;  // 1..366
  // tm_wday: 0=Sunday, 1=Monday, ..., 6=Saturday
  // Convert to ISO: Monday=1, ..., Sunday=7
  int wday_iso = (tm_val.tm_wday == 0) ? 7 : tm_val.tm_wday;
  // ISO week number formula: w = (yday - wday_iso + 10) / 7
  int w = (yday - wday_iso + 10) / 7;

  if (w == 0) {
    // Belongs to the last week of the previous year.
    // Compute Dec 31 of previous year.
    int year = tm_val.tm_year + 1900 - 1;
    // Jan 1 of current year: yday_jan1 = 1, wday is known from the tm.
    // Dec 31 of prev year: day_of_year for that year
    bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    int days_prev_year = leap ? 366 : 365;
    // Weekday of Dec 31 of prev year = weekday of Jan 1 of current year - 1
    // (mod 7). Jan 1 weekday: wday when yday=1.
    // We know the wday for our tm's date. Backtrack.
    // Actually simpler: compute for Dec 31 of prev year directly.
    // wday of Jan 1 current year: compute from our date.
    // wday_jan1_iso = ((wday_iso - yday) % 7 + 7) % 7; if 0, set to 7.
    int wday_jan1_iso = ((wday_iso - (yday - 1)) % 7 + 7) % 7;
    if (wday_jan1_iso == 0) wday_jan1_iso = 7;
    // Dec 31 prev year is one day before Jan 1 current year.
    int wday_dec31_iso = wday_jan1_iso - 1;
    if (wday_dec31_iso == 0) wday_dec31_iso = 7;
    w = (days_prev_year - wday_dec31_iso + 10) / 7;
  } else if (w == 53) {
    // Check if it's actually week 1 of next year.
    // Dec 31 is in week 1 of next year if its weekday is Mon, Tue, or Wed.
    // Weekday of Dec 31 of current year:
    int year = tm_val.tm_year + 1900;
    bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    int days_this_year = leap ? 366 : 365;
    int wday_dec31_iso = ((wday_iso + (days_this_year - yday)) % 7);
    if (wday_dec31_iso == 0) wday_dec31_iso = 7;
    // If Dec 31 falls on Mon(1), Tue(2), or Wed(3), week 53 doesn't exist.
    if (wday_dec31_iso <= 3) {
      w = 1;
    }
  }

  return w;
}

// Format a strftime result (small buffer, for day/month names).
static std::string FormatStrftime(const struct tm &tm_val, const char *fmt) {
  char buf[64];
  std::strftime(buf, sizeof(buf), fmt, &tm_val);
  return std::string(buf);
}

SnippetManager::SnippetManager(ConfigManager *config_manager)
    : config_manager_(config_manager),
      snippet_folder_path_(config_manager->GetAppDataPath() + "/snippets") {
  LoadBuiltInSnippets();
}

void SnippetManager::LoadBuiltInSnippets() {
  builtin_snippets_.clear();
  dynamic_callbacks_.clear();

  auto add = [this](const std::string &name, const std::string &description,
                    DynamicCallback callback) {
    SnippetData data;
    data.name = name;
    data.description = description;
    data.type = SnippetType::kDynamic;
    data.is_builtin = true;
    builtin_snippets_.push_back(std::move(data));
    dynamic_callbacks_[name] = std::move(callback);
  };

  // Day snippets.
  add("d", "the day as number without a leading zero (1 to 31)",
      []() { return std::to_string(GetLocalTime().tm_mday); });
  add("dd", "the day as number with a leading zero (01 to 31)",
      []() { return ZeroPad(GetLocalTime().tm_mday, 2); });
  add("ddd", "the abbreviated localized day name (Mon to Sun)",
      []() { return FormatStrftime(GetLocalTime(), "%a"); });
  add("dddd", "the full localized day name (Monday to Sunday)",
      []() { return FormatStrftime(GetLocalTime(), "%A"); });

  // Month snippets.
  add("M", "the month as number without a leading zero (1 to 12)",
      []() { return std::to_string(GetLocalTime().tm_mon + 1); });
  add("MM", "the month as number with a leading zero (01 to 12)",
      []() { return ZeroPad(GetLocalTime().tm_mon + 1, 2); });
  add("MMM", "the abbreviated localized month name (Jan to Dec)",
      []() { return FormatStrftime(GetLocalTime(), "%b"); });
  add("MMMM", "the full localized month name (January to December)",
      []() { return FormatStrftime(GetLocalTime(), "%B"); });

  // Year snippets.
  add("yy", "the year as two digit number (00 to 99)",
      []() { return FormatStrftime(GetLocalTime(), "%y"); });
  add("yyyy", "the year as four digit number",
      []() { return std::to_string(GetLocalTime().tm_year + 1900); });

  // Week snippets (ISO 8601).
  add("w", "the ISO week number without a leading zero (1 to 53)",
      []() { return std::to_string(IsoWeekNumber(GetLocalTime())); });
  add("ww", "the ISO week number with a leading zero (01 to 53)",
      []() { return ZeroPad(IsoWeekNumber(GetLocalTime()), 2); });

  // Hour snippets.
  add("H", "the hour without a leading zero (0 to 23)",
      []() { return std::to_string(GetLocalTime().tm_hour); });
  add("HH", "the hour with a leading zero (00 to 23)",
      []() { return ZeroPad(GetLocalTime().tm_hour, 2); });

  // Minute snippets.
  add("m", "the minute without a leading zero (0 to 59)",
      []() { return std::to_string(GetLocalTime().tm_min); });
  add("mm", "the minute with a leading zero (00 to 59)",
      []() { return ZeroPad(GetLocalTime().tm_min, 2); });

  // Second snippets.
  add("s", "the second without a leading zero (0 to 59)",
      []() { return std::to_string(GetLocalTime().tm_sec); });
  add("ss", "the second with a leading zero (00 to 59)",
      []() { return ZeroPad(GetLocalTime().tm_sec, 2); });

  // Composite date/time snippets.
  add("date", "the date in format YYYY-MM-DD", []() {
    auto tm = GetLocalTime();
    return std::to_string(tm.tm_year + 1900) + "-" + ZeroPad(tm.tm_mon + 1, 2) + "-" +
           ZeroPad(tm.tm_mday, 2);
  });
  add("da", "the date in format YYYYMMDD", []() {
    auto tm = GetLocalTime();
    return std::to_string(tm.tm_year + 1900) + ZeroPad(tm.tm_mon + 1, 2) + ZeroPad(tm.tm_mday, 2);
  });
  add("time", "the time in format HH:mm:ss", []() {
    auto tm = GetLocalTime();
    return ZeroPad(tm.tm_hour, 2) + ":" + ZeroPad(tm.tm_min, 2) + ":" + ZeroPad(tm.tm_sec, 2);
  });
  add("datetime", "the datetime in format YYYY-MM-DD_HH:mm:ss", []() {
    auto tm = GetLocalTime();
    return std::to_string(tm.tm_year + 1900) + "-" + ZeroPad(tm.tm_mon + 1, 2) + "-" +
           ZeroPad(tm.tm_mday, 2) + "_" + ZeroPad(tm.tm_hour, 2) + ":" + ZeroPad(tm.tm_min, 2) +
           ":" + ZeroPad(tm.tm_sec, 2);
  });

  // Context-dependent snippets (fallback to "[Value Not Available]").
  add("note", "the note name", []() { return std::string("[Value Not Available]"); });
  add("no", "the note name without suffix", []() { return std::string("[Value Not Available]"); });
}

VxCoreError SnippetManager::EnsureSnippetFolderExists() const {
  std::error_code ec;
  std::filesystem::create_directories(snippet_folder_path_, ec);
  if (ec) {
    return VXCORE_ERR_IO;
  }
  return VXCORE_OK;
}

bool SnippetManager::IsValidSnippetName(const std::string &name) const {
  if (name.empty()) return false;
  if (name == "." || name == "..") return false;
  if (!IsSingleName(name)) return false;
  if (name.find('%') != std::string::npos) return false;
  return true;
}

VxCoreError SnippetManager::GetSnippetFolderPath(std::string &out_path) const {
  VxCoreError err = EnsureSnippetFolderExists();
  if (err != VXCORE_OK) return err;
  out_path = snippet_folder_path_;
  return VXCORE_OK;
}

std::string SnippetManager::GetSnippetFilePath(const std::string &name) const {
  return snippet_folder_path_ + "/" + name + ".json";
}

const SnippetData *SnippetManager::FindSnippet(const std::string &name) const {
  for (const auto &snippet : builtin_snippets_) {
    if (snippet.name == name) {
      return &snippet;
    }
  }
  return nullptr;
}

VxCoreError SnippetManager::ListSnippets(std::vector<SnippetData> &out_snippets) const {
  VxCoreError err = EnsureSnippetFolderExists();
  if (err != VXCORE_OK) return err;

  out_snippets.clear();

  // Add all built-in snippets.
  for (const auto &snippet : builtin_snippets_) {
    out_snippets.push_back(snippet);
  }

  // Iterate snippet folder for user *.json files.
  std::error_code ec;
  for (const auto &entry : std::filesystem::directory_iterator(snippet_folder_path_, ec)) {
    if (!entry.is_regular_file(ec)) continue;
    std::string filename = entry.path().filename().string();
    if (filename.empty() || filename[0] == '.') continue;

    std::string ext = entry.path().extension().string();
    if (ext != ".json") continue;

    std::string stem = entry.path().stem().string();

    // Skip if name collides with a built-in (shouldn't happen, but be safe).
    if (FindSnippet(stem) != nullptr) continue;

    std::string content;
    VxCoreError read_err = ReadFile(entry.path(), content);
    if (read_err != VXCORE_OK) continue;

    try {
      auto j = nlohmann::json::parse(content);
      SnippetData data;
      data.name = stem;
      data.is_builtin = false;

      std::string type_str = j.value("type", "text");
      if (type_str == "dynamic") {
        data.type = SnippetType::kDynamic;
      } else {
        data.type = SnippetType::kText;
      }

      data.description = j.value("description", "");
      data.content = j.value("content", "");
      data.cursor_mark = j.value("cursorMark", "@@");
      data.selection_mark = j.value("selectionMark", "$$");
      data.indent_as_first_line = j.value("indentAsFirstLine", false);

      out_snippets.push_back(std::move(data));
    } catch (...) {
      // Skip malformed JSON files.
      continue;
    }
  }

  return VXCORE_OK;
}

VxCoreError SnippetManager::GetSnippet(const std::string &name, SnippetData &out_snippet) const {
  if (!IsValidSnippetName(name)) return VXCORE_ERR_INVALID_PARAM;

  // Check built-ins first.
  const SnippetData *builtin = FindSnippet(name);
  if (builtin) {
    out_snippet = *builtin;
    return VXCORE_OK;
  }

  // Check user snippet file.
  std::string file_path = GetSnippetFilePath(name);

  std::error_code ec;
  bool exists = std::filesystem::exists(file_path, ec);
  if (!exists) return VXCORE_ERR_NOT_FOUND;

  std::string content;
  VxCoreError err = ReadFile(file_path, content);
  if (err != VXCORE_OK) return err;

  try {
    auto j = nlohmann::json::parse(content);
    out_snippet.name = name;
    out_snippet.is_builtin = false;

    std::string type_str = j.value("type", "text");
    if (type_str == "dynamic") {
      out_snippet.type = SnippetType::kDynamic;
    } else {
      out_snippet.type = SnippetType::kText;
    }

    out_snippet.description = j.value("description", "");
    out_snippet.content = j.value("content", "");
    out_snippet.cursor_mark = j.value("cursorMark", "@@");
    out_snippet.selection_mark = j.value("selectionMark", "$$");
    out_snippet.indent_as_first_line = j.value("indentAsFirstLine", false);
  } catch (...) {
    return VXCORE_ERR_IO;
  }

  return VXCORE_OK;
}

VxCoreError SnippetManager::CreateSnippet(const std::string &name,
                                          const std::string &json_content) {
  if (!IsValidSnippetName(name)) return VXCORE_ERR_INVALID_PARAM;

  // Cannot create with a built-in name.
  if (FindSnippet(name) != nullptr) return VXCORE_ERR_ALREADY_EXISTS;

  VxCoreError err = EnsureSnippetFolderExists();
  if (err != VXCORE_OK) return err;

  std::string file_path = GetSnippetFilePath(name);

  std::error_code ec;
  bool exists = std::filesystem::exists(file_path, ec);
  if (exists) return VXCORE_ERR_ALREADY_EXISTS;

  // Validate that json_content is parseable.
  try {
    auto j = nlohmann::json::parse(json_content);
    (void)j;
  } catch (...) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  return WriteFile(file_path, json_content);
}

VxCoreError SnippetManager::DeleteSnippet(const std::string &name) {
  if (!IsValidSnippetName(name)) return VXCORE_ERR_INVALID_PARAM;

  // Cannot delete built-in snippets.
  if (FindSnippet(name) != nullptr) return VXCORE_ERR_INVALID_PARAM;

  std::string file_path = GetSnippetFilePath(name);

  std::error_code ec;
  bool exists = std::filesystem::exists(file_path, ec);
  if (!exists) return VXCORE_ERR_NOT_FOUND;

  std::filesystem::remove(file_path, ec);
  if (ec) return VXCORE_ERR_IO;

  return VXCORE_OK;
}

VxCoreError SnippetManager::RenameSnippet(const std::string &old_name,
                                          const std::string &new_name) {
  if (!IsValidSnippetName(old_name) || !IsValidSnippetName(new_name))
    return VXCORE_ERR_INVALID_PARAM;

  // Cannot rename built-in snippets.
  if (FindSnippet(old_name) != nullptr) return VXCORE_ERR_INVALID_PARAM;
  if (FindSnippet(new_name) != nullptr) return VXCORE_ERR_ALREADY_EXISTS;

  VxCoreError err = EnsureSnippetFolderExists();
  if (err != VXCORE_OK) return err;

  std::string old_path = GetSnippetFilePath(old_name);
  std::string new_path = GetSnippetFilePath(new_name);

  std::error_code ec;
  bool old_exists = std::filesystem::exists(old_path, ec);
  if (!old_exists) return VXCORE_ERR_NOT_FOUND;

  if (old_name == new_name) return VXCORE_OK;

  bool new_exists = std::filesystem::exists(new_path, ec);
  if (new_exists) return VXCORE_ERR_ALREADY_EXISTS;

  std::filesystem::rename(old_path, new_path, ec);
  if (ec) return VXCORE_ERR_IO;

  return VXCORE_OK;
}

VxCoreError SnippetManager::UpdateSnippet(const std::string &name,
                                          const std::string &json_content) {
  if (!IsValidSnippetName(name)) return VXCORE_ERR_INVALID_PARAM;

  // Cannot update built-in snippets.
  if (FindSnippet(name) != nullptr) return VXCORE_ERR_INVALID_PARAM;

  std::string file_path = GetSnippetFilePath(name);

  std::error_code ec;
  bool exists = std::filesystem::exists(file_path, ec);
  if (!exists) return VXCORE_ERR_NOT_FOUND;

  // Validate that json_content is parseable.
  try {
    auto j = nlohmann::json::parse(json_content);
    (void)j;
  } catch (...) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  return WriteFile(file_path, json_content);
}

// Replace all occurrences of |mark| in |text| with |replacement|.
static void ReplaceAll(std::string &text, const std::string &mark, const std::string &replacement) {
  if (mark.empty()) return;
  size_t pos = 0;
  while ((pos = text.find(mark, pos)) != std::string::npos) {
    text.replace(pos, mark.size(), replacement);
    pos += replacement.size();
  }
}

// Raw apply logic: indentation, cursor mark split, selection mark replacement.
// Does NOT chain ExpandSymbols. Used by both ApplySnippet (public) and ExpandSymbols (for nested).
static ApplyResult ApplySnippetRaw(const std::string &content, const std::string &selected_text,
                                   const std::string &indentation, const std::string &cursor_mark,
                                   const std::string &selection_mark, bool indent_first) {
  if (content.empty()) {
    return ApplyResult{};
  }

  std::string applied_text;

  // Step 1: Indentation — indent each line after the first.
  if (indent_first && !indentation.empty()) {
    std::string result;
    size_t start = 0;
    bool first_line = true;
    while (start < content.size()) {
      size_t nl = content.find('\n', start);
      if (nl == std::string::npos) {
        if (!first_line) {
          result += indentation;
        }
        result += content.substr(start);
        break;
      }
      if (!first_line) {
        result += indentation;
      }
      result += content.substr(start, nl - start + 1);
      start = nl + 1;
      first_line = false;
    }
    applied_text = result;
  } else {
    applied_text = content;
  }

  // Step 2: Cursor mark split — find first cursor mark, split into two parts.
  std::string second_part;
  if (!cursor_mark.empty()) {
    size_t cm_pos = applied_text.find(cursor_mark);
    if (cm_pos != std::string::npos) {
      second_part = applied_text.substr(cm_pos + cursor_mark.size());
      applied_text = applied_text.substr(0, cm_pos);
    }
  }

  // Step 3: Selection mark replacement in both parts.
  if (!selection_mark.empty()) {
    ReplaceAll(applied_text, selection_mark, selected_text);
    ReplaceAll(second_part, selection_mark, selected_text);
  }

  int cursor_offset = static_cast<int>(applied_text.size());
  return ApplyResult{applied_text + second_part, cursor_offset};
}

// Resolve snippet content and settings for a given name. Returns true if found.
// Does NOT apply marks or expand symbols — just resolves the raw content and marks.
static bool ResolveSnippetContent(SnippetManager *mgr, const std::string &name,
                                  const OverrideMap &overrides,
                                  const std::unordered_map<std::string, DynamicCallback> &callbacks,
                                  std::string &out_content, std::string &out_cursor_mark,
                                  std::string &out_selection_mark, bool &out_indent_first,
                                  bool &out_is_override, bool &out_is_dynamic_cb) {
  out_cursor_mark = "@@";
  out_selection_mark = "$$";
  out_indent_first = false;
  out_is_override = false;
  out_is_dynamic_cb = false;

  // Check overrides first.
  auto ov_it = overrides.find(name);
  if (ov_it != overrides.end()) {
    out_content = ov_it->second;
    out_is_override = true;
    return true;
  }

  // Check dynamic callback.
  auto cb_it = callbacks.find(name);
  if (cb_it != callbacks.end()) {
    out_content = cb_it->second();
    out_is_dynamic_cb = true;
    return true;
  }

  // Look up snippet data.
  SnippetData data;
  VxCoreError err = mgr->GetSnippet(name, data);
  if (err != VXCORE_OK) {
    return false;
  }

  // Built-in without callback → not available.
  if (data.is_builtin && data.type == SnippetType::kDynamic) {
    return false;
  }

  out_content = data.content;
  out_cursor_mark = data.cursor_mark;
  out_selection_mark = data.selection_mark;
  out_indent_first = data.indent_as_first_line;
  return true;
}

ApplyResult SnippetManager::ApplySnippet(const std::string &name, const std::string &selected_text,
                                         const std::string &indentation,
                                         const OverrideMap &overrides) {
  std::string content, cursor_mark, selection_mark;
  bool indent_first = false, is_override = false, is_dynamic_cb = false;

  bool found =
      ResolveSnippetContent(this, name, overrides, dynamic_callbacks_, content, cursor_mark,
                            selection_mark, indent_first, is_override, is_dynamic_cb);
  if (!found) {
    return ApplyResult{};
  }

  // Overrides and dynamic callbacks return content directly (no marks processing).
  if (is_override) {
    return ApplyResult{content, static_cast<int>(content.size())};
  }
  if (is_dynamic_cb) {
    return ApplyResult{content, static_cast<int>(content.size())};
  }

  // Apply raw logic (indent, cursor mark, selection mark).
  auto raw_result = ApplySnippetRaw(content, selected_text, indentation, cursor_mark,
                                    selection_mark, indent_first);

  // Chain ExpandSymbols on the result.
  int cursor_offset = raw_result.cursor_offset;
  std::string expanded = ExpandSymbols(raw_result.text, selected_text, cursor_offset, overrides);
  return ApplyResult{expanded, cursor_offset};
}

// Extract indentation (leading spaces/tabs) of the line containing position |pos| in |text|.
static std::string ExtractIndentation(const std::string &text, size_t pos) {
  // Find the start of the current line.
  size_t line_start = 0;
  if (pos > 0) {
    size_t nl = text.rfind('\n', pos - 1);
    if (nl != std::string::npos) {
      line_start = nl + 1;
    }
  }
  // Count leading whitespace from line_start.
  size_t i = line_start;
  while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) {
    ++i;
  }
  return text.substr(line_start, i - line_start);
}

std::string SnippetManager::ExpandSymbols(const std::string &content,
                                          const std::string &selected_text, int &cursor_offset,
                                          const OverrideMap &overrides) {
  std::string result = content;
  int max_times_at_same_pos = 100;
  int total_iterations = 0;
  const int kMaxTotalIterations = 1000;

  size_t pos = 0;
  while (pos < result.size()) {
    if (++total_iterations > kMaxTotalIterations) {
      break;
    }

    // Find the opening '%'.
    size_t open = result.find('%', pos);
    if (open == std::string::npos) {
      break;
    }

    // Find the closing '%'.
    size_t close = result.find('%', open + 1);
    if (close == std::string::npos) {
      break;
    }

    // Extract snippet name between the two '%' characters.
    std::string name = result.substr(open + 1, close - open - 1);

    // Empty name (i.e., "%%") — skip past the second '%'.
    if (name.empty()) {
      pos = close + 1;
      continue;
    }

    // Look up the snippet: check overrides first, then FindSnippet for known snippets.
    bool found = false;
    std::string after_text;

    auto ov_it = overrides.find(name);
    if (ov_it != overrides.end()) {
      after_text = ov_it->second;
      found = true;
    } else {
      // Resolve snippet content using the helper (same logic as ApplySnippet but NO expansion).
      std::string sn_content, sn_cursor_mark, sn_selection_mark;
      bool sn_indent_first = false, sn_is_override = false, sn_is_dynamic_cb = false;

      bool resolved = ResolveSnippetContent(this, name, overrides, dynamic_callbacks_, sn_content,
                                            sn_cursor_mark, sn_selection_mark, sn_indent_first,
                                            sn_is_override, sn_is_dynamic_cb);

      if (resolved) {
        if (sn_is_override || sn_is_dynamic_cb) {
          // Override/dynamic: use content directly.
          after_text = sn_content;
        } else {
          // User/text snippet: apply raw (indent, cursor mark, selection mark) without expansion.
          std::string indentation = ExtractIndentation(result, open);
          auto raw = ApplySnippetRaw(sn_content, selected_text, indentation, sn_cursor_mark,
                                     sn_selection_mark, sn_indent_first);
          after_text = raw.text;
        }
        found = true;
      }
    }

    if (!found) {
      // Unknown snippet — skip past the entire %name% token.
      pos = close + 1;
      continue;
    }

    // Replace %name% with the expanded text.
    size_t match_len = close - open + 1;  // includes both '%' characters
    result.replace(open, match_len, after_text);

    // Maintain cursor offset.
    if (cursor_offset > static_cast<int>(open)) {
      if (cursor_offset < static_cast<int>(open + match_len)) {
        cursor_offset = static_cast<int>(open);
      } else {
        cursor_offset += static_cast<int>(after_text.size()) - static_cast<int>(match_len);
      }
    }

    // Same-position guard: replacement may still contain %name% symbols.
    if (pos == open) {
      if (--max_times_at_same_pos == 0) {
        break;
      }
    } else {
      max_times_at_same_pos = 100;
    }
    pos = open;
  }

  return result;
}

}  // namespace vxcore
