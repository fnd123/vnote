#include "simple_search_backend.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <future>

#include "search_file_info.h"
#include "utils/string_utils.h"
#include "utils/utils.h"

namespace vxcore {

namespace {

constexpr int kParallelSearchThreshold = 50;

}  // namespace

bool DoRegexMatch(const std::regex &pattern_regex, const std::string &line, int line_number,
                  std::vector<SearchMatch> &out_matches) {
  auto begin = std::sregex_iterator(line.begin(), line.end(), pattern_regex);
  auto end = std::sregex_iterator();

  bool has_match = false;
  for (std::sregex_iterator it = begin; it != end; ++it) {
    has_match = true;
    SearchMatch match;
    // TODO: optimize to avoid copying line multiple times
    match.line_text = line;
    match.line_number = line_number;
    match.column_start = static_cast<int>(it->position());
    match.column_end = match.column_start + static_cast<int>(it->length());
    out_matches.push_back(std::move(match));
  }
  return has_match;
}

bool DoPatternMatch(const std::string &transformed_pattern, bool case_sensitive, bool whole_word,
                    const std::string &line, int line_number,
                    std::vector<SearchMatch> &out_matches) {
  std::string lowercased_line = case_sensitive ? std::string() : ToLowerString(line);
  const std::string &search_line = case_sensitive ? line : lowercased_line;

  size_t pos = 0;
  bool has_match = false;
  while ((pos = search_line.find(transformed_pattern, pos)) != std::string::npos) {
    if (whole_word) {
      bool word_start = (pos == 0 || !std::isalnum(static_cast<unsigned char>(line[pos - 1])));
      size_t end_pos = pos + transformed_pattern.length();
      bool word_end =
          (end_pos >= line.length() || !std::isalnum(static_cast<unsigned char>(line[end_pos])));

      if (!word_start || !word_end) {
        pos++;
        continue;
      }
    }

    has_match = true;
    SearchMatch match;
    // TODO: optimize to avoid copying line multiple times
    match.line_text = line;
    match.line_number = line_number;
    match.column_start = static_cast<int>(pos);
    match.column_end = static_cast<int>(pos + transformed_pattern.length());
    out_matches.push_back(std::move(match));

    pos += transformed_pattern.length();
  }

  return has_match;
}

bool SimpleSearchBackend::MatchesPattern(const std::string &line, const std::string &pattern,
                                         SearchOption options,
                                         std::vector<SearchMatch> &out_matches) {
  bool case_sensitive = HasFlag(options, SearchOption::kCaseSensitive);
  bool whole_word = HasFlag(options, SearchOption::kWholeWord);
  bool regex = HasFlag(options, SearchOption::kRegex);

  std::regex pattern_regex;
  std::string lowercased_pattern;
  if (regex) {
    try {
      pattern_regex =
          std::regex(pattern, case_sensitive ? std::regex::ECMAScript
                                             : (std::regex::ECMAScript | std::regex::icase));
    } catch (const std::regex_error &) {
      return false;
    }
  } else if (!case_sensitive) {
    lowercased_pattern = ToLowerString(pattern);
  }

  if (regex) {
    return DoRegexMatch(pattern_regex, line, 0, out_matches);
  } else if (!case_sensitive) {
    return DoPatternMatch(lowercased_pattern, false, whole_word, line, 0, out_matches);
  } else {
    return DoPatternMatch(pattern, true, whole_word, line, 0, out_matches);
  }
}

void SimpleSearchBackend::SetThreadPool(BS::thread_pool<> *pool) { thread_pool_ = pool; }

void SimpleSearchBackend::SetCancelFlag(const volatile int *flag) { cancel_flag_ = flag; }

VxCoreError SimpleSearchBackend::SearchSequential(
    const std::vector<SearchFileInfo> &files,
    const std::function<bool(const std::string &, int, std::vector<SearchMatch> &)> &do_match,
    const std::vector<std::string> &content_exclude_patterns,
    const std::vector<std::string> &lowercased_exclude_patterns,
    const std::vector<std::regex> &exclude_regexes, int max_results,
    ContentSearchResult &out_result) {
  int total_matches = 0;
  for (const auto &file_info : files) {
    if (out_result.truncated) {
      break;
    }

    std::ifstream file(file_info.absolute_path);
    if (!file.is_open()) {
      continue;
    }

    std::vector<SearchMatch> file_matches;
    std::string line;
    int line_number = 0;

    while (!out_result.truncated && std::getline(file, line)) {
      line_number++;

      if (IsLineExcluded(line, content_exclude_patterns, lowercased_exclude_patterns,
                         exclude_regexes)) {
        continue;
      }

      std::vector<SearchMatch> line_matches;
      if (do_match(line, line_number, line_matches)) {
        for (auto &match : line_matches) {
          total_matches++;
          file_matches.push_back(std::move(match));
          if (max_results > 0 && total_matches >= max_results) {
            out_result.truncated = true;
            break;
          }
        }
      }
    }

    if (!file_matches.empty()) {
      ContentSearchMatchedFile matched_file;
      matched_file.path = file_info.path;
      matched_file.id = file_info.id;
      matched_file.matches = std::move(file_matches);
      out_result.matched_files.push_back(std::move(matched_file));
    }
  }

  return VXCORE_OK;
}

VxCoreError SimpleSearchBackend::SearchParallel(
    const std::vector<SearchFileInfo> &files,
    const std::function<bool(const std::string &, int, std::vector<SearchMatch> &)> &do_match,
    const std::vector<std::string> &content_exclude_patterns,
    const std::vector<std::string> &lowercased_exclude_patterns,
    const std::vector<std::regex> &exclude_regexes, int max_results,
    ContentSearchResult &out_result) {
  struct ChunkSearchResult {
    ContentSearchResult result;
    bool cancelled = false;
  };

  const size_t file_count = files.size();
  const size_t worker_count =
      std::max(static_cast<size_t>(1), static_cast<size_t>(thread_pool_->get_thread_count()));
  const size_t chunk_count = std::min(file_count, worker_count);
  const size_t base_chunk_size = file_count / chunk_count;
  const size_t remainder = file_count % chunk_count;

  std::vector<std::future<ChunkSearchResult>> futures;
  futures.reserve(chunk_count);

  size_t start = 0;
  for (size_t chunk_index = 0; chunk_index < chunk_count; ++chunk_index) {
    const size_t chunk_size = base_chunk_size + (chunk_index < remainder ? 1 : 0);
    const size_t end = start + chunk_size;

    futures.push_back(thread_pool_->submit_task([&, start, end]() -> ChunkSearchResult {
      ChunkSearchResult chunk_result;

      for (size_t i = start; i < end; ++i) {
        if (cancel_flag_ && *cancel_flag_ != 0) {
          chunk_result.cancelled = true;
          break;
        }

        const auto &file_info = files[i];
        std::ifstream file(file_info.absolute_path);
        if (!file.is_open()) {
          continue;
        }

        std::vector<SearchMatch> file_matches;
        std::string line;
        int line_number = 0;

        while (std::getline(file, line)) {
          line_number++;

          if (IsLineExcluded(line, content_exclude_patterns, lowercased_exclude_patterns,
                             exclude_regexes)) {
            continue;
          }

          std::vector<SearchMatch> line_matches;
          if (do_match(line, line_number, line_matches)) {
            for (auto &match : line_matches) {
              file_matches.push_back(std::move(match));
            }
          }
        }

        if (!file_matches.empty()) {
          ContentSearchMatchedFile matched_file;
          matched_file.path = file_info.path;
          matched_file.id = file_info.id;
          matched_file.matches = std::move(file_matches);
          chunk_result.result.matched_files.push_back(std::move(matched_file));
        }
      }

      return chunk_result;
    }));

    start = end;
  }

  bool cancelled = false;
  std::vector<ChunkSearchResult> chunk_results;
  chunk_results.reserve(chunk_count);
  for (auto &future : futures) {
    auto chunk_result = future.get();
    cancelled = cancelled || chunk_result.cancelled;
    chunk_results.push_back(std::move(chunk_result));
  }

  for (auto &chunk_result : chunk_results) {
    for (auto &matched_file : chunk_result.result.matched_files) {
      out_result.matched_files.push_back(std::move(matched_file));
    }
  }

  if (max_results > 0) {
    int total = 0;
    for (size_t i = 0; i < out_result.matched_files.size(); ++i) {
      auto &matched_file = out_result.matched_files[i];
      for (size_t j = 0; j < matched_file.matches.size(); ++j) {
        total++;
        if (total >= max_results) {
          matched_file.matches.resize(j + 1);
          out_result.matched_files.resize(i + 1);
          out_result.truncated = true;
          goto done_truncation;
        }
      }
    }
  }

done_truncation:
  if (cancelled || (cancel_flag_ && *cancel_flag_ != 0)) {
    return VXCORE_ERR_CANCELLED;
  }

  return VXCORE_OK;
}

VxCoreError SimpleSearchBackend::Search(const std::vector<SearchFileInfo> &files,
                                        const std::string &pattern, SearchOption options,
                                        const std::vector<std::string> &content_exclude_patterns,
                                        int max_results, ContentSearchResult &out_result) {
  out_result.matched_files.clear();
  out_result.truncated = false;

  if (pattern.empty()) {
    return VXCORE_OK;
  }

  bool case_sensitive = HasFlag(options, SearchOption::kCaseSensitive);
  bool whole_word = HasFlag(options, SearchOption::kWholeWord);
  bool regex = HasFlag(options, SearchOption::kRegex);

  std::regex pattern_regex;
  std::string lowercased_pattern;
  if (regex) {
    try {
      pattern_regex =
          std::regex(pattern, case_sensitive ? std::regex::ECMAScript
                                             : (std::regex::ECMAScript | std::regex::icase));
    } catch (const std::regex_error &) {
      return VXCORE_ERR_INVALID_PARAM;
    }
  } else if (!case_sensitive) {
    lowercased_pattern = ToLowerString(pattern);
  }

  std::function<bool(const std::string &, int, std::vector<SearchMatch> &)> do_match;
  if (regex) {
    do_match = [&pattern_regex](const std::string &line, int line_number,
                                std::vector<SearchMatch> &out_matches) -> bool {
      return DoRegexMatch(pattern_regex, line, line_number, out_matches);
    };
  } else if (!case_sensitive) {
    do_match = [&lowercased_pattern, whole_word](const std::string &line, int line_number,
                                                 std::vector<SearchMatch> &out_matches) -> bool {
      return DoPatternMatch(lowercased_pattern, false, whole_word, line, line_number, out_matches);
    };
  } else {
    do_match = [&pattern, whole_word](const std::string &line, int line_number,
                                      std::vector<SearchMatch> &out_matches) -> bool {
      return DoPatternMatch(pattern, true, whole_word, line, line_number, out_matches);
    };
  }

  std::vector<std::regex> exclude_regexes;
  std::vector<std::string> lowercased_exclude_patterns;
  auto err = PreprocessExcludePatterns(content_exclude_patterns, case_sensitive, regex,
                                       lowercased_exclude_patterns, exclude_regexes);
  if (err != VXCORE_OK) {
    return err;
  }

  if (!thread_pool_ || static_cast<int>(files.size()) < kParallelSearchThreshold) {
    return SearchSequential(files, do_match, content_exclude_patterns, lowercased_exclude_patterns,
                            exclude_regexes, max_results, out_result);
  }

  return SearchParallel(files, do_match, content_exclude_patterns, lowercased_exclude_patterns,
                        exclude_regexes, max_results, out_result);
}

}  // namespace vxcore
