#include "search_manager.h"

#include <algorithm>

#include "core/folder_manager.h"
#include "core/notebook.h"
#include "rg_search_backend.h"
#include "simple_search_backend.h"
#include "utils/logger.h"
#include "utils/string_utils.h"

namespace vxcore {

SearchManager::SearchManager(Notebook *notebook, const std::string &search_backend)
    : notebook_(notebook), search_backend_(nullptr) {
  if (search_backend == "rg") {
    if (RgSearchBackend::IsAvailable()) {
      VXCORE_LOG_INFO("Using ripgrep (rg) as the search backend");
      search_backend_.reset(new RgSearchBackend());
    } else {
      VXCORE_LOG_WARN(
          "ripgrep (rg) requested but not available, falling back to SimpleSearchBackend");
      search_backend_.reset(new SimpleSearchBackend());
    }
  } else if (search_backend == "simple") {
    VXCORE_LOG_INFO("Using SimpleSearchBackend");
    search_backend_.reset(new SimpleSearchBackend());
  } else {
    VXCORE_LOG_WARN("Unknown search backend '%s', using SimpleSearchBackend",
                    search_backend.c_str());
    search_backend_.reset(new SimpleSearchBackend());
  }
}

SearchManager::~SearchManager() = default;

void SearchManager::SetThreadPool(BS::thread_pool<> *pool) { thread_pool_ = pool; }

void SearchManager::SetCancelFlag(const volatile int *flag) { cancel_flag_ = flag; }

VxCoreError SearchManager::SearchFiles(const std::string &query_json,
                                       const std::string &input_files_json,
                                       std::string &out_results_json) {
  try {
    VXCORE_LOG_DEBUG("SearchManager::SearchFiles: query_json=%s", query_json.c_str());
    VXCORE_LOG_DEBUG("SearchManager::SearchFiles: input_files_json=%s",
                     input_files_json.empty() ? "(empty)" : input_files_json.c_str());

    auto query = SearchFilesQuery::FromJson(notebook_, nlohmann::json::parse(query_json));
    VXCORE_LOG_DEBUG(
        "SearchManager::SearchFiles: parsed pattern='%s' includeFiles=%d "
        "includeFolders=%d maxResults=%d",
        query.pattern.c_str(), query.include_files, query.include_folders, query.max_results);

    auto filtered_files = FetchFilesToSearch(query.scope, input_files_json, true);
    VXCORE_LOG_DEBUG("SearchManager::SearchFiles: filtered_files count=%zu", filtered_files.size());
    for (size_t i = 0; i < filtered_files.size() && i < 20; ++i) {
      VXCORE_LOG_DEBUG("SearchManager::SearchFiles: file[%zu] name='%s' path='%s' is_folder=%d", i,
                       filtered_files[i].name.c_str(), filtered_files[i].path.c_str(),
                       filtered_files[i].is_folder);
    }

    auto matched_files =
        GetMatchedFilesByPattern(std::move(filtered_files), query.pattern, query.include_files,
                                 query.include_folders, query.max_results);
    VXCORE_LOG_DEBUG("SearchManager::SearchFiles: matched_files count=%zu", matched_files.size());

    out_results_json = SerializeFileResults(matched_files, query.max_results);
    return VXCORE_OK;
  } catch (const nlohmann::json::exception &e) {
    VXCORE_LOG_ERROR("SearchFiles JSON error: %s", e.what());
    return VXCORE_ERR_JSON_PARSE;
  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("SearchFiles error: %s", e.what());
    return VXCORE_ERR_UNKNOWN;
  }
}

void SearchManager::CalculateAbsolutePaths(std::vector<SearchFileInfo> &files) const {
  for (auto &file : files) {
    file.absolute_path = notebook_->GetAbsolutePath(file.path);
  }
}

VxCoreError SearchManager::SearchContent(const std::string &query_json,
                                         const std::string &input_files_json,
                                         std::string &out_results_json) {
  try {
    auto query = SearchContentQuery::FromJson(notebook_, nlohmann::json::parse(query_json));
    auto filtered_files = FetchFilesToSearch(query.scope, input_files_json, false);
    CalculateAbsolutePaths(filtered_files);

    nlohmann::json result;
    result["matchCount"] = 0;
    result["truncated"] = false;
    result["matches"] = nlohmann::json::array();
    auto &total_matches = result["matches"];
    if (search_backend_) {
      if (auto *simple = dynamic_cast<SimpleSearchBackend *>(search_backend_.get())) {
        simple->SetThreadPool(thread_pool_);
        simple->SetCancelFlag(cancel_flag_);
      }

      ContentSearchResult search_result;

      VxCoreError search_err =
          search_backend_->Search(filtered_files, query.pattern, query.options,
                                  query.exclude_patterns, query.max_results, search_result);
      if (search_err == VXCORE_OK) {
        for (const auto &matched_file : search_result.matched_files) {
          nlohmann::json item;
          item["path"] = matched_file.path;
          item["id"] = matched_file.id;
          item["matchCount"] = matched_file.matches.size();
          item["matches"] = nlohmann::json::array();
          auto &matches = item["matches"];
          for (const auto &match : matched_file.matches) {
            nlohmann::json m;
            m["lineNumber"] = match.line_number;
            m["columnStart"] = match.column_start;
            m["columnEnd"] = match.column_end;
            m["lineText"] = match.line_text;
            matches.push_back(std::move(m));
          }

          total_matches.push_back(std::move(item));
        }

        result["matchCount"] = result["matches"].size();
        result["truncated"] = search_result.truncated;
      } else if (search_err == VXCORE_ERR_CANCELLED) {
        out_results_json = result.dump();
        return VXCORE_ERR_CANCELLED;
      } else {
        VXCORE_LOG_WARN("Search backend failed with error: %d", search_err);
      }
    }

    out_results_json = result.dump();
    return VXCORE_OK;
  } catch (const nlohmann::json::exception &e) {
    VXCORE_LOG_ERROR("SearchContent JSON error: %s", e.what());
    return VXCORE_ERR_JSON_PARSE;
  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("SearchContent error: %s", e.what());
    return VXCORE_ERR_UNKNOWN;
  }
}

VxCoreError SearchManager::SearchByTags(const std::string &query_json,
                                        const std::string &input_files_json,
                                        std::string &out_results_json) {
  try {
    auto query = SearchByTagsQuery::FromJson(notebook_, nlohmann::json::parse(query_json));
    auto filtered_files = FetchFilesToSearch(query.scope, input_files_json, false);

    auto matched_files = GetMatchedFilesByTags(std::move(filtered_files), query.tags,
                                               query.tag_operator, query.max_results);

    out_results_json = SerializeFileResults(matched_files, query.max_results);
    return VXCORE_OK;
  } catch (const nlohmann::json::exception &e) {
    VXCORE_LOG_ERROR("SearchByTags JSON error: %s", e.what());
    return VXCORE_ERR_JSON_PARSE;
  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("SearchByTags error: %s", e.what());
    return VXCORE_ERR_UNKNOWN;
  }
}

std::vector<SearchFileInfo> SearchManager::GetMatchedFilesByPattern(
    std::vector<SearchFileInfo> filtered_files, const std::string &pattern, bool include_files,
    bool include_folders, int max_results) {
  if (pattern.empty()) {
    std::vector<SearchFileInfo> matched_files;
    for (auto &file : filtered_files) {
      if ((file.is_folder && !include_folders) || (!file.is_folder && !include_files)) {
        continue;
      }
      matched_files.push_back(std::move(file));
      if (static_cast<int>(matched_files.size()) >= max_results) {
        break;
      }
    }
    return matched_files;
  }

  std::vector<SearchFileInfo> name_matches;
  std::vector<SearchFileInfo> path_matches;

  auto lower_pattern = ToLowerString(pattern);

  for (auto &file : filtered_files) {
    if ((file.is_folder && !include_folders) || (!file.is_folder && !include_files)) {
      continue;
    }

    bool name_match = MatchesPattern(ToLowerString(file.name), lower_pattern);
    bool path_match = !name_match && MatchesPattern(ToLowerString(file.path), lower_pattern);

    if (name_match) {
      name_matches.push_back(std::move(file));
    } else if (path_match) {
      path_matches.push_back(std::move(file));
    }

    if (static_cast<int>(name_matches.size() + path_matches.size()) >= max_results) {
      break;
    }
  }

  std::vector<SearchFileInfo> matched_files;
  matched_files.reserve(
      std::min(static_cast<size_t>(max_results), name_matches.size() + path_matches.size()));
  for (auto &file : name_matches) {
    if (static_cast<int>(matched_files.size()) >= max_results) {
      break;
    }
    matched_files.push_back(std::move(file));
  }
  for (auto &file : path_matches) {
    if (static_cast<int>(matched_files.size()) >= max_results) {
      break;
    }
    matched_files.push_back(std::move(file));
  }

  return matched_files;
}

std::vector<SearchFileInfo> SearchManager::GetMatchedFilesByTags(
    std::vector<SearchFileInfo> filtered_files, const std::vector<std::string> &tags,
    const std::string &tag_operator, int max_results) {
  std::vector<SearchFileInfo> matched_files;
  for (auto &file : filtered_files) {
    if (file.is_folder) {
      continue;
    }

    if (MatchesTags(file.tags, tags, tag_operator)) {
      matched_files.push_back(std::move(file));
      if (static_cast<int>(matched_files.size()) >= max_results) {
        break;
      }
    }
  }
  return matched_files;
}

std::string SearchManager::SerializeFileResults(const std::vector<SearchFileInfo> &matched_files,
                                                int max_results) {
  nlohmann::json result;
  result["matchCount"] = matched_files.size();
  result["truncated"] = static_cast<int>(matched_files.size()) >= max_results;
  result["matches"] = nlohmann::json::array();
  auto &matches = result["matches"];
  for (const auto &file : matched_files) {
    matches.push_back(file.ToJson());
  }

  return result.dump();
}

std::vector<SearchFileInfo> SearchManager::GetAllFiles(const SearchScope &scope,
                                                       const SearchInputFiles *input_files,
                                                       bool include_folders) {
  std::vector<SearchFileInfo> result;
  std::vector<std::string> lower_path_patterns;
  lower_path_patterns.reserve(scope.path_patterns.size());
  for (const auto &pattern : scope.path_patterns) {
    lower_path_patterns.push_back(ToLowerString(pattern));
  }

  std::vector<std::string> lower_exclude_path_patterns;
  lower_exclude_path_patterns.reserve(scope.exclude_path_patterns.size());
  for (const auto &pattern : scope.exclude_path_patterns) {
    lower_exclude_path_patterns.push_back(ToLowerString(pattern));
  }

  if (input_files && (!input_files->files.empty() || !input_files->folders.empty())) {
    VXCORE_LOG_DEBUG("SearchManager::GetAllFiles: input_files provided - files=%zu folders=%zu",
                     input_files->files.size(), input_files->folders.size());
    for (const auto &file_path : input_files->files) {
      const FileRecord *record = nullptr;
      VxCoreError err = notebook_->GetFolderManager()->GetFileInfo(file_path, &record);
      if (err == VXCORE_OK) {
        assert(record);
        VXCORE_LOG_DEBUG("SearchManager::GetAllFiles: GetFileInfo OK for '%s' name='%s'",
                         file_path.c_str(), record->name.c_str());
        result.push_back(SearchFileInfo::FromFileRecord(file_path, *record));
      } else {
        VXCORE_LOG_WARN("SearchManager::GetAllFiles: GetFileInfo FAILED for '%s' error=%d",
                        file_path.c_str(), err);
      }
    }

    for (const auto &folder_path : input_files->folders) {
      VXCORE_LOG_DEBUG("SearchManager::GetAllFiles: collecting folder '%s'", folder_path.c_str());
      CollectFilesInFolder(folder_path, scope.recursive, lower_path_patterns,
                           lower_exclude_path_patterns, include_folders, result);
    }
  } else {
    const std::string start_path = scope.folder_path.empty() ? "." : scope.folder_path;
    VXCORE_LOG_DEBUG("SearchManager::GetAllFiles: no input_files, scanning from '%s'",
                     start_path.c_str());
    CollectFilesInFolder(start_path, scope.recursive, lower_path_patterns,
                         lower_exclude_path_patterns, include_folders, result);
  }

  VXCORE_LOG_DEBUG("SearchManager::GetAllFiles: total files collected=%zu", result.size());
  return result;
}

std::vector<SearchFileInfo> SearchManager::FilterFilesByTagsAndDate(
    std::vector<SearchFileInfo> files, const SearchScope &scope) {
  std::vector<SearchFileInfo> result;
  const bool filter_by_created = scope.date_filter_field == "created";
  const bool filter_by_modified = scope.date_filter_field == "modified";
  for (auto &file : files) {
    bool matches = true;

    if (!scope.tags.empty() && !file.is_folder) {
      if (!MatchesTags(file.tags, scope.tags, scope.tag_operator)) {
        matches = false;
      }
    }

    if (matches && !scope.exclude_tags.empty() && !file.is_folder) {
      for (const auto &exclude_tag : scope.exclude_tags) {
        if (std::find(file.tags.begin(), file.tags.end(), exclude_tag) != file.tags.end()) {
          matches = false;
          break;
        }
      }
    }

    if (matches && !scope.date_filter_field.empty()) {
      int64_t timestamp = 0;
      if (filter_by_created) {
        timestamp = file.created_utc;
      } else if (filter_by_modified) {
        timestamp = file.modified_utc;
      }
      if (!MatchesDateFilter(timestamp, scope)) {
        matches = false;
      }
    }

    if (matches) {
      result.push_back(std::move(file));
    }
  }
  return result;
}

void SearchManager::CollectFilesInFolder(
    const std::string &folder_path, bool recursive,
    const std::vector<std::string> &lower_path_patterns,
    const std::vector<std::string> &lower_exclude_path_patterns, bool include_folders,
    std::vector<SearchFileInfo> &out_files) {
  const auto lower_folder_path = ToLowerString(folder_path);
  if (MatchesPatterns(lower_folder_path, lower_exclude_path_patterns)) {
    return;
  }

  FolderManager::FolderContents contents;
  if (notebook_->GetFolderManager()->ListFolderContents(folder_path, include_folders, contents) !=
      VXCORE_OK) {
    return;
  }

  for (const auto &file : contents.files) {
    const auto file_path = ConcatenatePaths(folder_path, file.name);
    const auto lower_file_path = ToLowerString(file_path);
    if (MatchesPatterns(lower_file_path, lower_exclude_path_patterns)) {
      continue;
    }
    if (!lower_path_patterns.empty() && !MatchesPatterns(lower_file_path, lower_path_patterns)) {
      continue;
    }
    out_files.push_back(SearchFileInfo::FromFileRecord(file_path, file));
  }

  for (const auto &folder : contents.folders) {
    std::string subfolder_path = ConcatenatePaths(folder_path, folder.name);
    const auto lower_subfolder_path = ToLowerString(subfolder_path);

    if (MatchesPatterns(lower_subfolder_path, lower_exclude_path_patterns)) {
      continue;
    }

    if (include_folders) {
      out_files.push_back(SearchFileInfo::FromFolderRecord(subfolder_path, folder));
    }

    if (recursive) {
      CollectFilesInFolder(subfolder_path, recursive, lower_path_patterns,
                           lower_exclude_path_patterns, include_folders, out_files);
    }
  }
}

bool SearchManager::MatchesTags(const std::vector<std::string> &file_tags,
                                const std::vector<std::string> &search_tags,
                                const std::string &tag_operator) const {
  if (search_tags.empty()) {
    return true;
  }

  if (tag_operator == "AND") {
    for (const auto &search_tag : search_tags) {
      if (std::find(file_tags.begin(), file_tags.end(), search_tag) == file_tags.end()) {
        return false;
      }
    }
    return true;
  } else {
    for (const auto &search_tag : search_tags) {
      if (std::find(file_tags.begin(), file_tags.end(), search_tag) != file_tags.end()) {
        return true;
      }
    }
    return false;
  }
}

bool SearchManager::MatchesDateFilter(int64_t timestamp, const SearchScope &scope) const {
  if (scope.date_filter_from > 0 && timestamp < scope.date_filter_from) {
    return false;
  }
  if (scope.date_filter_to > 0 && timestamp > scope.date_filter_to) {
    return false;
  }
  return true;
}

std::vector<SearchFileInfo> SearchManager::FetchFilesToSearch(const SearchScope &scope,
                                                              const std::string &input_files_json,
                                                              bool include_folders) {
  SearchInputFiles input_files;
  if (!input_files_json.empty()) {
    auto input_json = nlohmann::json::parse(input_files_json);
    input_files = SearchInputFiles::FromJson(notebook_, input_json);
    VXCORE_LOG_DEBUG(
        "SearchManager::FetchFilesToSearch: parsed input_files - files=%zu folders=%zu",
        input_files.files.size(), input_files.folders.size());
    for (size_t i = 0; i < input_files.files.size() && i < 20; ++i) {
      VXCORE_LOG_DEBUG("SearchManager::FetchFilesToSearch: input file[%zu]='%s'", i,
                       input_files.files[i].c_str());
    }
  } else {
    VXCORE_LOG_DEBUG("SearchManager::FetchFilesToSearch: no input_files_json provided");
  }
  auto all_files = GetAllFiles(scope, &input_files, include_folders);
  VXCORE_LOG_DEBUG("SearchManager::FetchFilesToSearch: all_files=%zu before tag/date filter",
                   all_files.size());
  auto result = FilterFilesByTagsAndDate(std::move(all_files), scope);
  VXCORE_LOG_DEBUG("SearchManager::FetchFilesToSearch: after filter=%zu", result.size());
  return result;
}

}  // namespace vxcore
