#ifndef VXCORE_CORE_STRING_UTILS_H_
#define VXCORE_CORE_STRING_UTILS_H_

#include <regex>
#include <string>

#include "vxcore/vxcore_types.h"

namespace vxcore {

std::string ToLowerString(const std::string &str);

VxCoreError PreprocessExcludePatterns(const std::vector<std::string> &raw_patterns,
                                      bool case_sensitive, bool regex,
                                      std::vector<std::string> &out_patterns,
                                      std::vector<std::regex> &out_regexes);

// Should be used paired with PreprocessExcludePatterns.
bool IsLineExcluded(const std::string &line, const std::vector<std::string> &exclude_patterns,
                    const std::vector<std::string> &lowercased_exclude_patterns,
                    const std::vector<std::regex> &exclude_regexes);

// Returns true if |text| matches |pattern|. Supports '*' and '?' wildcards.
bool MatchesPattern(const std::string &text, const std::string &pattern);

// Case-insensitive version of MatchesPattern.
bool MatchesPatternCaseInsensitive(const std::string &text, const std::string &pattern);

bool MatchesPatterns(const std::string &text, const std::vector<std::string> &patterns);

}  // namespace vxcore

#endif
