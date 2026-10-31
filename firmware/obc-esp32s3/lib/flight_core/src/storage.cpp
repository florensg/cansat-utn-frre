#include "flight_core/storage.h"

#include <cstring>

namespace cansat {
namespace {

void mix_u32(uint32_t& hash, uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8) {
    hash ^= (value >> shift) & 0xFFu;
    hash *= 16777619u;
  }
}

void mix_float(uint32_t& hash, float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  mix_u32(hash, bits);
}

}  // namespace

uint32_t compute_checksum(const PersistentState& state) {
  uint32_t hash = 2166136261u;
  mix_u32(hash, state.magic);
  mix_u32(hash, state.version);
  mix_u32(hash, state.packet_count);
  mix_u32(hash, state.mission_time_ms);
  mix_float(hash, state.altitude_offset_m);
  mix_u32(hash, static_cast<uint32_t>(state.state));
  mix_u32(hash, state.armed ? 1u : 0u);
  mix_u32(hash, state.second_parachute_deployed ? 1u : 0u);
  return hash;
}

bool MemoryStorage::load(PersistentState& out) {
  if (!has_state_) {
    return false;
  }
  out = state_;
  return out.checksum == compute_checksum(out);
}

bool MemoryStorage::save(const PersistentState& state) {
  state_ = state;
  state_.checksum = compute_checksum(state_);
  has_state_ = true;
  return true;
}

void MemoryStorage::clear() {
  state_ = PersistentState{};
  has_state_ = false;
}

}  // namespace cansat
