#ifndef VXCORE_TEST_UTILS_H_
#define VXCORE_TEST_UTILS_H_

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#define ASSERT(condition)                                                                        \
  do {                                                                                           \
    if (!(condition)) {                                                                          \
      std::cerr << "✗ Assertion failed: " << #condition << " at " << __FILE__ << ":" << __LINE__ \
                << std::endl;                                                                    \
      return 1;                                                                                  \
    }                                                                                            \
  } while (0)

#define ASSERT_EQ(a, b)                                                                      \
  do {                                                                                       \
    if ((a) != (b)) {                                                                        \
      std::cerr << "✗ Assertion failed: " << #a << " == " << #b << " at " << __FILE__ << ":" \
                << __LINE__ << " (" << (a) << " != " << (b) << ")" << std::endl;             \
      return 1;                                                                              \
    }                                                                                        \
  } while (0)

#define ASSERT_NE(a, b)                                                                      \
  do {                                                                                       \
    if ((a) == (b)) {                                                                        \
      std::cerr << "✗ Assertion failed: " << #a << " != " << #b << " at " << __FILE__ << ":" \
                << __LINE__ << " (" << (a) << " == " << (b) << ")" << std::endl;             \
      return 1;                                                                              \
    }                                                                                        \
  } while (0)

#define ASSERT_TRUE(condition) ASSERT(condition)
#define ASSERT_FALSE(condition) ASSERT(!(condition))
#define ASSERT_NULL(ptr) ASSERT((ptr) == nullptr)
#define ASSERT_NOT_NULL(ptr) ASSERT((ptr) != nullptr)

#define RUN_TEST(test_func)                                                                \
  do {                                                                                     \
    int result = test_func();                                                              \
    if (result != 0) {                                                                     \
      std::cerr << "✗ Test " << #test_func << " failed at " << __FILE__ << ":" << __LINE__ \
                << std::endl;                                                              \
      return result;                                                                       \
    }                                                                                      \
  } while (0)

inline bool path_exists(const std::string &path) { return std::filesystem::exists(path); }

inline void remove_directory_recursive(const std::string &path) {
  std::error_code ec;
  std::filesystem::remove_all(path, ec);
}

inline void cleanup_test_dir(const std::string &path) {
  if (path_exists(path)) {
    remove_directory_recursive(path);
  }
}

inline void write_file(const std::string &path, const std::string &content) {
  std::ofstream file(path, std::ios::binary);
  if (file.is_open()) {
    file.write(content.c_str(), content.length());
  }
}

inline void create_directory(const std::string &path) {
  std::error_code ec;
  std::filesystem::create_directories(path, ec);
}

// Returns a test path under the system temp directory.
// This ensures test data is created in temp folder, not in the repo root.
inline std::string get_test_path(const std::string &name) {
  return (std::filesystem::temp_directory_path() / "vxcore_test_data" / name).string();
}
inline std::string normalize_path(const std::string &path) {
  std::string result = path;
  std::replace(result.begin(), result.end(), '\\', '/');
  return result;
}

#endif
