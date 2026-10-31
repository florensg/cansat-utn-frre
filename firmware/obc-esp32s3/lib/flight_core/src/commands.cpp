#include "flight_core/commands.h"

#include <cstring>

namespace cansat {
namespace {

bool matches(const char* line, const char* token) {
  const std::size_t length = std::strlen(token);
  if (std::strncmp(line, token, length) != 0) {
    return false;
  }
  const char next = line[length];
  return next == '\0' || next == ' ' || next == '\r' || next == '\n';
}

}  // namespace

CommandType parse_command(const char* line) {
  if (line == nullptr) {
    return CommandType::None;
  }
  if (matches(line, "CX")) {
    return CommandType::Status;
  }
  if (matches(line, "ST")) {
    return CommandType::SetTime;
  }
  if (matches(line, "CAL")) {
    return CommandType::CalibrateAltitude;
  }
  if (matches(line, "MEC")) {
    return CommandType::ActivateMechanism;
  }
  return CommandType::None;
}

const char* command_name(CommandType type) {
  switch (type) {
    case CommandType::None:
      return "NONE";
    case CommandType::Status:
      return "CX";
    case CommandType::SetTime:
      return "ST";
    case CommandType::CalibrateAltitude:
      return "CAL";
    case CommandType::ActivateMechanism:
      return "MEC";
  }
  return "NONE";
}

}  // namespace cansat
