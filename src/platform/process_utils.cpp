#include "process_utils.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <sstream>

namespace vxcore {

constexpr int kPipeBufferSize = 8192;

FILE *ProcessUtils::OpenPipe(const std::string &command, const char *mode) {
#ifdef _WIN32
  return _popen(command.c_str(), mode);
#else
  return popen(command.c_str(), mode);
#endif
}

int ProcessUtils::ClosePipe(FILE *pipe) {
#ifdef _WIN32
  return _pclose(pipe);
#else
  return pclose(pipe);
#endif
}

bool ProcessUtils::ExecuteCommand(const std::string &command, ProcessResult &result) {
  result.output.clear();
  result.exit_code = -1;
  result.success = false;

  FILE *pipe = OpenPipe(command, "r");
  if (!pipe) {
    return false;
  }

  char buffer[kPipeBufferSize];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    result.output += buffer;
  }

  result.exit_code = ClosePipe(pipe);
  result.success = (result.exit_code == 0);

  return true;
}

bool ProcessUtils::IsCommandAvailable(const std::string &command_name) {
#ifdef _WIN32
  std::string check_command = "where " + command_name;
#else
  std::string check_command = "which " + command_name;
#endif

  FILE *pipe = OpenPipe(check_command, "r");
  if (!pipe) {
    return false;
  }

  char buffer[128];
  bool found = fgets(buffer, sizeof(buffer), pipe) != nullptr;

  ClosePipe(pipe);

  return found;
}

std::string ProcessUtils::EscapeShellArg(const std::string &arg) {
#ifdef _WIN32
  std::ostringstream escaped;
  escaped << '"';
  for (char c : arg) {
    if (c == '"') {
      escaped << "\\\"";
    } else {
      escaped << c;
    }
  }
  escaped << '"';
  return escaped.str();
#else
  std::ostringstream escaped;
  escaped << '\'';
  for (char c : arg) {
    if (c == '\'') {
      escaped << "'\\''";
    } else {
      escaped << c;
    }
  }
  escaped << '\'';
  return escaped.str();
#endif
}

}  // namespace vxcore
