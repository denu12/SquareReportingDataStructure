#pragma once
// Readable 3D Morton index. C++20, -O3 -march=native (BMI2 for encoding).
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <immintrin.h>
#include <vector>

namespace diskreport::morton3d {
using Wide = unsigned __int128;
using Point = std::array<uint32_t, 3>;
// All 96 Morton bits are retained, including the top bits of each coordinate.
inline Wide code(Point p) {
  Wide key = 0;
  for (unsigned a = 0; a < 3; ++a)
    key |= (Wide(_pdep_u64(p[a], 0x1249249249249249ULL)) |
            Wide(_pdep_u64(p[a] >> 21, 0x1249249249249249ULL)) << 63) << a;
  return key;
}
inline bool contains(Point p, Point center, Wide radius2) {
  uint64_t sum = 0, high = 0;
  for (unsigned a = 0; a < 3; ++a) {
    const uint32_t d = std::max(p[a], center[a]) - std::min(p[a], center[a]);
    const uint64_t next = sum + uint64_t(d) * d;
    high += next < sum; sum = next;
  }
  const uint64_t radius_high = uint64_t(radius2 >> 64);
  return (high < radius_high) | ((high == radius_high) & (sum <= uint64_t(radius2)));
}
class Morton3D {
 public:
  using Point = morton3d::Point;
  explicit Morton3D(const std::vector<Point>& input) {
    std::vector<std::pair<Wide, Point>> sorted;
    sorted.reserve(input.size());
    for (auto p : input) sorted.emplace_back(code(p), p);
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    points_.reserve(input.size()); ranks_.reserve(input.size());
    for (const auto& [key, p] : sorted) { ranks_.push_back(key); points_.push_back(p); }
  }
  void query(Point center, uint64_t radius, auto output) const {
    query_squared(center, Wide(radius) * radius, output);
  }
  void query_squared(Point center, Wide radius2, auto output) const {
    visit_ranges(center, radius2,
        [&](Wide key, bool upper) { return upper
          ? std::upper_bound(ranks_.begin(), ranks_.end(), key) - ranks_.begin()
          : std::lower_bound(ranks_.begin(), ranks_.end(), key) - ranks_.begin(); },
        [&](size_t first, size_t last) {
          for (size_t i = first; i < last; i += 32) {
            const size_t count = std::min<size_t>(32, last - i);
            uint64_t accepted[32];
            for (size_t j = 0; j < count; ++j) accepted[j] = contains(points_[i+j], center, radius2);
            for (size_t j = 0; j < count; ++j) if (accepted[j]) *output++ = points_[i+j];
          }
        });
  }
  void query_box(Point lo, Point hi, auto output) const {
    visit_box_ranges(lo,hi,
        [&](Wide key, bool upper) { return upper
          ? std::upper_bound(ranks_.begin(),ranks_.end(),key)-ranks_.begin()
          : std::lower_bound(ranks_.begin(),ranks_.end(),key)-ranks_.begin(); },
        [&](size_t first,size_t last) {
          for(size_t i=first;i<last;++i){auto& p=points_[i];
            if(p[0]>=lo[0]&&p[0]<=hi[0]&&p[1]>=lo[1]&&p[1]<=hi[1]&&p[2]>=lo[2]&&p[2]<=hi[2])
              *output++=p;
          }
        });
  }
 protected:
  std::vector<Point> points_;
  std::vector<Wide> ranks_;
  // A query box meets at most eight cells at this octree depth.
  void visit_ranges(Point center, Wide radius2, auto bound, auto report) const {
    if (points_.empty()) return;
    uint64_t radius = UINT32_MAX;
    if (radius2 < Wide(UINT32_MAX) * UINT32_MAX) {
      radius = uint64_t(std::sqrt(static_cast<long double>(radius2)));
      while (Wide(radius) * radius < radius2) ++radius;
      while (radius && Wide(radius-1) * (radius-1) >= radius2) --radius;
    }
    Point lo{}, hi{};
    for (unsigned a = 0; a < 3; ++a) {
      lo[a] = center[a] > radius ? center[a] - radius : 0;
      hi[a] = std::min<uint64_t>(UINT32_MAX, uint64_t(center[a]) + radius);
    }
    visit_box_ranges(lo,hi,bound,report);
  }
  void visit_box_ranges(Point lo,Point hi,auto bound,auto report) const {
    uint32_t width=0;
    for(unsigned a=0;a<3;++a){if(lo[a]>hi[a])return;width=std::max(width,hi[a]-lo[a]);}
    const unsigned suffix_bits = 3 * std::bit_width(width);
    const Wide suffix = (Wide(1) << suffix_bits) - 1;
    std::array<Wide, 8> cells;
    for (unsigned i = 0; i < 8; ++i)
      cells[i] = code({i&1 ? hi[0] : lo[0], i&2 ? hi[1] : lo[1], i&4 ? hi[2] : lo[2]}) & ~suffix;
    std::sort(cells.begin(), cells.end());
    size_t cursor = 0;
    for (unsigned i = 0; i < 8; ++i) {
      if (i && cells[i] == cells[i-1]) continue;
      const auto first = cursor < points_.size() && ranks_[cursor] < cells[i] ? size_t(bound(cells[i],false)) : cursor;
      const auto last = first < points_.size() && ranks_[first] <= (cells[i]|suffix)
          ? size_t(bound(cells[i]|suffix,true)) : first;
      report(first,last); cursor=last;
    }
  }
};
} // namespace diskreport::morton3d
