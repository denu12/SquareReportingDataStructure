#pragma once
// Standalone compiler-vectorized 2D Morton index: include in a C++20 client compiled with -mbmi2.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <immintrin.h>
#include <utility>
#include <vector>

namespace diskreport::morton_Morton {
using Wide = unsigned __int128;
struct Point {
  using ElementType = uint32_t;
  uint32_t x, y;
};
struct Box {
  Point p, q;
  Box(Point lo, Point hi) : p(lo), q(hi) {}
  Box(uint32_t x, uint32_t y, uint32_t u, uint32_t v) : p{x, y}, q{u, v} {}
};
struct Circle {
  Point center;
  Wide radius_squared;
  bool contains(Point p) const {
    const uint64_t dx = uint64_t(std::max(p.x, center.x)) - std::min(p.x, center.x);
    const uint64_t dy = uint64_t(std::max(p.y, center.y)) - std::min(p.y, center.y);
    const uint64_t xx = dx * dx, sum = xx + dy * dy;
    // Each square fits in 64 bits; their sum needs one additional carry bit.
    const uint64_t carry = sum < xx, high = uint64_t(radius_squared >> 64);
    return (carry < high) | ((carry == high) & (sum <= uint64_t(radius_squared)));
  }
  Box bounds() const {
    if (radius_squared >= Wide(2) * UINT32_MAX * UINT32_MAX)
      return {{0, 0}, {UINT32_MAX, UINT32_MAX}};
    uint64_t radius = uint64_t(std::ceil(std::sqrt(static_cast<long double>(radius_squared))));
    // Correct any floating-point rounding before forming the integer enclosure.
    while (Wide(radius) * radius < radius_squared) ++radius;
    while (radius && Wide(radius - 1) * (radius - 1) >= radius_squared) --radius;
    radius = std::min<uint64_t>(radius, UINT32_MAX);
    const Point lo{uint32_t(center.x > radius ? center.x - radius : 0),
                   uint32_t(center.y > radius ? center.y - radius : 0)};
    const Point hi{uint32_t(std::min<uint64_t>(UINT32_MAX, uint64_t(center.x) + radius)),
                   uint32_t(std::min<uint64_t>(UINT32_MAX, uint64_t(center.y) + radius))};
    return {lo, hi};
  }
};

class Morton {
 public:
  using IDXT = uint32_t;
  using PointType = Point;
  using QueryType = Box;
  using Rank = uint64_t;
  static constexpr bool reports_exact_circle = true;
  explicit Morton(const std::vector<PointType>& input) : ranks(input.size()) {
    // Original associative sort: zip ranks with points, sort, then unzip.
    std::vector<std::pair<Rank, PointType>> sorted;
    sorted.reserve(input.size());
    for (const auto p : input) sorted.emplace_back(morton(p.x, p.y), p);
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    points.reserve(sorted.size());
    for (size_t i = 0; i < sorted.size(); ++i) {
      ranks[i] = sorted[i].first;
      points.push_back(sorted[i].second);
    }
  }
  // Closed disk query with an integer radius, returning actual point coordinates.
  void query(Point center, uint64_t radius, auto output) const {
    query(Circle{center, Wide(radius) * radius}, output);
  }
  // Squared radius also permits the full 65-bit squared-distance range.
  void query(const Circle& circle, auto output) const {
    visit_ranges(circle.bounds(),
        [&](Rank key) { return std::lower_bound(ranks.begin(), ranks.end(), key) - ranks.begin(); },
        [&](Rank key) { return std::upper_bound(ranks.begin(), ranks.end(), key) - ranks.begin(); },
        [&](size_t begin, size_t end) {
          for (size_t start = begin; start < end; start += 32) {
            const size_t count = std::min<size_t>(32, end - start);
            uint64_t accepted[32];
            // Independent tests let the compiler choose its own vector instructions.
            for (size_t j = 0; j < count; ++j)
              accepted[j] = circle.contains(points[start + j]);
            for (size_t j = 0; j < count; ++j)
              if (accepted[j]) *output++ = points[start + j];
          }
        });
  }

  // Optional closed rectangle query, using the same index.
  void query(const Box& box, auto output) const {
    scan(box, [&](Point p) {
      return p.x >= box.p.x && p.x <= box.q.x &&
             p.y >= box.p.y && p.y <= box.q.y;
    }, output);
  }
 protected:
  // The benchmark supplies its already-computed bounding box through this path.
  void scan(const Box& box, auto accepts, auto output) const {
    scan(box, accepts, output,
         [&](Rank key) { return std::lower_bound(ranks.begin(), ranks.end(), key) - ranks.begin(); },
         [&](Rank key) { return std::upper_bound(ranks.begin(), ranks.end(), key) - ranks.begin(); });
  }
  // Share the original scan with variants that only change rank lookup.
  void scan(const Box& box, auto accepts, auto output, auto lower, auto upper) const {
    visit_ranges(box, lower, upper, [&](size_t start, size_t end) {
      for (size_t i = start; i < end; ++i)
        if (accepts(points[i])) *output++ = points[i];
    });
  }
  // Share the unchanged four-cell search with alternative reporting routines.
  void visit_ranges(const Box& box, auto lower, auto upper, auto report) const {
    const auto lo = box.p, hi = box.q;
    if (lo.x > hi.x || lo.y > hi.y) return;
    // The bounding square meets at most four cells at this quadtree depth.
    const uint32_t diameter = std::max(hi.x - lo.x, hi.y - lo.y);
    const unsigned depth = diameter == 0 ? 32 : __builtin_clz(diameter);
    std::array<std::pair<Rank, Rank>, 4> ranges = {
        interval(lo.x, lo.y, depth), interval(hi.x, lo.y, depth),
        interval(lo.x, hi.y, depth), interval(hi.x, hi.y, depth)};
    sort4(ranges);
    size_t start = 0;
    for (const auto [first, last] : ranges) {
      if (start < points.size() && ranks[start] < first)
        start = lower(first);
      const size_t end = start < points.size() && ranks[start] <= last
          ? upper(last) : start;
      report(start, end);
      // Sorted intervals may repeat; never report a point twice.
      start = end;
    }
  }
 protected:
  std::vector<PointType> points;
  std::vector<Rank> ranks;
  // Original Morton encoding. PDEP deposits scalar bits
  static Rank morton(uint32_t x, uint32_t y) {
    return _pdep_u64(x, UINT64_C(0x5555555555555555)) |
           _pdep_u64(y, UINT64_C(0xaaaaaaaaaaaaaaaa));
  }
  static std::pair<Rank, Rank> interval(uint32_t x, uint32_t y, unsigned depth) {
    const unsigned suffix_bits = 2 * (32 - depth);
    if (suffix_bits == 64) return {0, UINT64_MAX};
    const Rank suffix_mask = (Rank(1) << suffix_bits) - 1;
    const Rank first = morton(x, y) & ~suffix_mask;
    return {first, first | suffix_mask};
  }
  static void sort4(std::array<std::pair<Rank, Rank>, 4>& a) {
    // Original five-comparison sorting network.
    if (a[0].first > a[1].first) std::swap(a[0], a[1]);
    if (a[2].first > a[3].first) std::swap(a[2], a[3]);
    if (a[0].first > a[2].first) std::swap(a[0], a[2]);
    if (a[1].first > a[3].first) std::swap(a[1], a[3]);
    if (a[1].first > a[2].first) std::swap(a[1], a[2]);
  }
};
}  // namespace diskreport::morton_Morton
