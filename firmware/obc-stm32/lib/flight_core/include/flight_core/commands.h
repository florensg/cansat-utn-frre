#pragma once

#include <cstdint>

namespace cansat {

enum class CommandType : uint8_t {
  None = 0,
  Status = 1,
  SetTime = 2,
  CalibrateAltitude = 3,
  ActivateMechanism = 4,
};

CommandType parse_command(const char* line);

const char* command_name(CommandType type);

}  // namespace cansat
