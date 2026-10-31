#pragma once

#include <cstddef>
#include <cstdint>

#include "flight_core/fsm.h"

namespace cansat {

constexpr uint32_t kPersistentMagic = 0x43414E53;  // "CANS"
constexpr uint8_t kPersistentVersion = 1;

struct PersistentState {
  uint32_t magic = kPersistentMagic;
  uint8_t version = kPersistentVersion;
  uint32_t packet_count = 0;
  uint32_t mission_time_ms = 0;
  float altitude_offset_m = 0.0f;
  State state = State::LaunchPad;
  bool armed = false;
  bool second_parachute_deployed = false;
  uint32_t checksum = 0;
};

uint32_t compute_checksum(const PersistentState& state);

class IStorage {
 public:
  virtual ~IStorage() = default;
  virtual bool load(PersistentState& out) = 0;
  virtual bool save(const PersistentState& state) = 0;
  virtual void clear() = 0;
};

class MemoryStorage final : public IStorage {
 public:
  bool load(PersistentState& out) override;
  bool save(const PersistentState& state) override;
  void clear() override;

 private:
  PersistentState state_;
  bool has_state_ = false;
};

}  // namespace cansat
