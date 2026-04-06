#ifndef VXCORE_SEARCH_MANAGER_H
#define VXCORE_SEARCH_MANAGER_H

#include <BS_thread_pool/BS_thread_pool.hpp>
#include <memory>
#include <string>
#include <vector>

#include "search_backend.h"
#include "search_file_info.h"
#include "search_query.h"
#include "vxcore/vxcore_types.h"

namespace vxcore {

class Notebook;

struct FileRecord;
struct FolderRecord;

class SearchManager {
 public:
  explicit SearchManager(Notebook *notebook, const std::string &search_backend);
  ~SearchManager();

  VxCoreError SearchFiles(const std::string &query_json, const std::string &input_files_json,
                          std::string &out_results_json);

  VxCoreError SearchContent(const std::string &query_json, const std::string &input_files_json,
                            std::string &out_results_json);

  VxCoreError SearchByTags(const std::string &query_json, const std::string &input_files_json,
                           std::string &out_results_json);

  void SetThreadPool(BS::thread_pool<> *pool);
  void SetCancelFlag(const volatile int *flag);

 private:
  std::vector<SearchFileInfo> GetAllFiles(const SearchScope &scope,
                                          const SearchInputFiles *input_files,
                                          bool include_folders);

  std::vector<SearchFileInfo> FilterFilesByTagsAndDate(std::vector<SearchFileInfo> files,
                                                       const SearchScope &scope);

  std::vector<SearchFileInfo> FetchFilesToSearch(const SearchScope &scope,
                                                 const std::string &input_files_json,
                                                 bool include_folders);

  std::vector<SearchFileInfo> GetMatchedFilesByPattern(std::vector<SearchFileInfo> filtered_files,
                                                       const std::string &pattern,
                                                       bool include_files, bool include_folders,
                                                       int max_results);

  std::vector<SearchFileInfo> GetMatchedFilesByTags(std::vector<SearchFileInfo> filtered_files,
                                                    const std::vector<std::string> &tags,
                                                    const std::string &tag_operator,
                                                    int max_results);

  std::string SerializeFileResults(const std::vector<SearchFileInfo> &matched_files,
                                   int max_results);

  void CollectFilesInFolder(const std::string &folder_path, bool recursive,
                            const std::vector<std::string> &lower_path_patterns,
                            const std::vector<std::string> &lower_exclude_path_patterns,
                            bool include_folders, std::vector<SearchFileInfo> &out_files);

  bool MatchesTags(const std::vector<std::string> &file_tags,
                   const std::vector<std::string> &search_tags,
                   const std::string &tag_operator) const;

  bool MatchesDateFilter(int64_t timestamp, const SearchScope &scope) const;

  void CalculateAbsolutePaths(std::vector<SearchFileInfo> &files) const;

  Notebook *notebook_;
  std::unique_ptr<ISearchBackend> search_backend_;
  BS::thread_pool<> *thread_pool_ = nullptr;
  const volatile int *cancel_flag_ = nullptr;
};

}  // namespace vxcore

#endif
