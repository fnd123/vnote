#include "string_utils.h"

#include <algorithm>
#include <cctype>

namespace vxcore {

std::string ToLowerString(const std::string &str) {
  std::string result = str;
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return result;
}

VxCoreError PreprocessExcludePatterns(const std::vector<std::string> &raw_patterns,
                                      bool case_sensitive, bool regex,
                                      std::vector<std::string> &out_patterns,
                                      std::vector<std::regex> &out_regexes) {
  if (raw_patterns.empty()) {
    return VXCORE_OK;
  }

  if (regex) {
    out_regexes.reserve(raw_patterns.size());
    for (const auto &exclude_pattern : raw_patterns) {
      try {
        out_regexes.emplace_back(
            exclude_pattern,
            case_sensitive ? std::regex::ECMAScript : (std::regex::ECMAScript | std::regex::icase));
      } catch (const std::regex_error &) {
        return VXCORE_ERR_INVALID_PARAM;
      }
    }
  } else if (!case_sensitive) {
    out_patterns.reserve(raw_patterns.size());
    for (const auto &exclude_pattern : raw_patterns) {
      out_patterns.push_back(ToLowerString(exclude_pattern));
    }
  }

  return VXCORE_OK;
}

bool IsLineExcluded(const std::string &line, const std::vector<std::string> &exclude_patterns,
                    const std::vector<std::string> &lowercased_exclude_patterns,
                    const std::vector<std::regex> &exclude_regexes) {
  if (exclude_patterns.empty()) {
    return false;
  }

  if (!exclude_regexes.empty()) {
    for (const auto &exclude_regex : exclude_regexes) {
      if (std::regex_search(line, exclude_regex)) {
        return true;
      }
    }
  } else if (!lowercased_exclude_patterns.empty()) {
    auto line_to_check = ToLowerString(line);
    for (const auto &exclude_pattern : lowercased_exclude_patterns) {
      if (line_to_check.find(exclude_pattern) != std::string::npos) {
        return true;
      }
    }
  } else {
    for (const auto &exclude_pattern : exclude_patterns) {
      if (line.find(exclude_pattern) != std::string::npos) {
        return true;
      }
    }
  }

  return false;
}

bool MatchesPattern(const std::string &text, const std::string &pattern) {
  if (pattern.find('*') == std::string::npos && pattern.find('?') == std::string::npos) {
    return text.find(pattern) != std::string::npos;
  }

  size_t text_idx = 0;
  size_t pattern_idx = 0;
  size_t star_idx = std::string::npos;
  size_t match_idx = 0;

  while (text_idx < text.size()) {
    if (pattern_idx < pattern.size() &&
        (pattern[pattern_idx] == '?' || pattern[pattern_idx] == text[text_idx])) {
      text_idx++;
      pattern_idx++;
    } else if (pattern_idx < pattern.size() && pattern[pattern_idx] == '*') {
      star_idx = pattern_idx;
      match_idx = text_idx;
      pattern_idx++;
    } else if (star_idx != std::string::npos) {
      pattern_idx = star_idx + 1;
      match_idx++;
      text_idx = match_idx;
    } else {
      return false;
    }
  }

  while (pattern_idx < pattern.size() && pattern[pattern_idx] == '*') {
    pattern_idx++;
  }

  return pattern_idx == pattern.size();
}

bool MatchesPatternCaseInsensitive(const std::string &text, const std::string &pattern) {
  return MatchesPattern(ToLowerString(text), ToLowerString(pattern));
}

bool MatchesPatterns(const std::string &text, const std::vector<std::string> &patterns) {
  for (const auto &pattern : patterns) {
    if (MatchesPattern(text, pattern)) {
      return true;
    }
  }
  return false;
}

}  // namespace vxcore
