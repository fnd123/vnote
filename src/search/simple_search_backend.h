#ifndef VXCORE_SIMPLE_SEARCH_BACKEND_H
#define VXCORE_SIMPLE_SEARCH_BACKEND_H

#include <BS_thread_pool/BS_thread_pool.hpp>
#include <functional>
#include <regex>
#include <string>
#include <vector>

#include "search_backend.h"

class SimpleSearchBackendTest;

namespace vxcore {

class SimpleSearchBackend : public ISearchBackend {
 public:
  SimpleSearchBackend() = default;
  ~SimpleSearchBackend() override = default;

  VxCoreError Search(const std::vector<SearchFileInfo> &files, const std::string &pattern,
                     SearchOption options, const std::vector<std::string> &content_exclude_patterns,
                     int max_results, ContentSearchResult &out_result) override;

  void SetThreadPool(BS::thread_pool<> *pool);
  void SetCancelFlag(const volatile int *flag);

 private:
  friend class ::SimpleSearchBackendTest;

  VxCoreError SearchSequential(
      const std::vector<SearchFileInfo> &files,
      const std::function<bool(const std::string &, int, std::vector<SearchMatch> &)> &do_match,
      const std::vector<std::string> &content_exclude_patterns,
      const std::vector<std::string> &lowercased_exclude_patterns,
      const std::vector<std::regex> &exclude_regexes, int max_results,
      ContentSearchResult &out_result);
  VxCoreError SearchParallel(
      const std::vector<SearchFileInfo> &files,
      const std::function<bool(const std::string &, int, std::vector<SearchMatch> &)> &do_match,
      const std::vector<std::string> &content_exclude_patterns,
      const std::vector<std::string> &lowercased_exclude_patterns,
      const std::vector<std::regex> &exclude_regexes, int max_results,
      ContentSearchResult &out_result);

  bool MatchesPattern(const std::string &line, const std::string &pattern, SearchOption options,
                      std::vector<SearchMatch> &out_matches);

  BS::thread_pool<> *thread_pool_ = nullptr;
  const volatile int *cancel_flag_ = nullptr;
};

}  // namespace vxcore

#endif
