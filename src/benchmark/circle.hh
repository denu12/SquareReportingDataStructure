#pragma once
#include <cstdint>

namespace esa_campaign {
// Per-thread benchmark query context. A null context preserves rectangle APIs.
struct Circle {
  uint32_t x, y;
  uint64_t radius2_lo, radius2_hi;
  unsigned __int128 radius2() const {
    return (static_cast<unsigned __int128>(radius2_hi) << 64) | radius2_lo;
  }
  bool contains(uint32_t px, uint32_t py) const {
    uint64_t dx = px > x ? uint64_t(px)-x : uint64_t(x)-px;
    uint64_t dy = py > y ? uint64_t(py)-y : uint64_t(y)-py;
    return static_cast<unsigned __int128>(dx)*dx + static_cast<unsigned __int128>(dy)*dy <= radius2();
  }
};
inline thread_local const Circle* active_circle = nullptr;
template<class P> bool point_membership(P v, P lo, P hi) {
  if (active_circle) return active_circle->contains(v.x, v.y);
  return v.x >= lo.x && v.y >= lo.y && v.x <= hi.x && v.y <= hi.y;
}
}
