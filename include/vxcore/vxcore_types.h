#ifndef VXCORE_TYPES_H
#define VXCORE_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) || defined(_WIN64)
#ifdef VXCORE_BUILD_DLL
#define VXCORE_API __declspec(dllexport)
#elif defined(VXCORE_SHARED)
#define VXCORE_API __declspec(dllimport)
#else
#define VXCORE_API
#endif
#else
#if defined(__GNUC__) && __GNUC__ >= 4
#define VXCORE_API __attribute__((visibility("default")))
#else
#define VXCORE_API
#endif
#endif

typedef enum {
  VXCORE_OK = 0,
  VXCORE_ERR_INVALID_PARAM = 1,
  VXCORE_ERR_NULL_POINTER = 2,
  VXCORE_ERR_OUT_OF_MEMORY = 3,
  VXCORE_ERR_NOT_FOUND = 4,
  VXCORE_ERR_ALREADY_EXISTS = 5,
  VXCORE_ERR_IO = 6,
  VXCORE_ERR_DATABASE = 7,
  VXCORE_ERR_JSON_PARSE = 8,
  VXCORE_ERR_JSON_SERIALIZE = 9,
  VXCORE_ERR_INVALID_STATE = 10,
  VXCORE_ERR_NOT_INITIALIZED = 11,
  VXCORE_ERR_ALREADY_INITIALIZED = 12,
  VXCORE_ERR_PERMISSION_DENIED = 13,
  VXCORE_ERR_UNSUPPORTED = 14,
  VXCORE_ERR_NOT_IMPLEMENTED = 15,
  VXCORE_ERR_BUFFER_NOT_FOUND = 16,
  VXCORE_ERR_WORKSPACE_NOT_FOUND = 17,
  VXCORE_ERR_FILE_CHANGED_OUTSIDE = 18,
  VXCORE_ERR_BUFFER_ALREADY_OPEN = 19,
  VXCORE_ERR_CANCELLED = 20,
  VXCORE_ERR_UNKNOWN = 999
} VxCoreError;

typedef enum { VXCORE_NOTEBOOK_BUNDLED = 0, VXCORE_NOTEBOOK_RAW = 1 } VxCoreNotebookType;

typedef enum {
  VXCORE_BUFFER_NORMAL = 0,
  VXCORE_BUFFER_FILE_MISSING = 1,
  VXCORE_BUFFER_FILE_CHANGED = 2,
  VXCORE_BUFFER_SAVE_FAILED = 3
} VxCoreBufferState;

typedef enum { VXCORE_DATA_APP = 0, VXCORE_DATA_LOCAL = 1 } VxCoreDataLocation;

typedef struct VxCoreContext *VxCoreContextHandle;

typedef struct {
  uint32_t major;
  uint32_t minor;
  uint32_t patch;
} VxCoreVersion;

typedef struct {
  VxCoreError code;
  const char *message;
} VxCoreResult;

#ifdef __cplusplus
}
#endif

// C++ only types
namespace vxcore {
enum class NodeType { File, Folder };
}  // namespace vxcore

#endif
