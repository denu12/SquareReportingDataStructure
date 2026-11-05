#pragma once
// Standalone 2D MortonSIMD: C++20, x86-64 AVX-512F and BMI2; no library dependencies.
// Include this file in a client compiled with -O3 -mavx512f -mbmi2.
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <immintrin.h>
#include <memory>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
namespace diskreport::morton_MortonSIMD {
using Wide = unsigned __int128;
struct Point {
  using ElementType = uint32_t;
  uint32_t x, y;
};
// Reusable coordinate output. Allocated Point objects need no zero initialization;
// only the initialized prefix [begin(), end()) is exposed to callers.
class Results {
  std::unique_ptr<Point[]> storage;
  size_t used = 0, capacity = 0;
 public:
  Results() = default;
  Results(const Results&) = delete;
  Results& operator=(const Results&) = delete;
  void clear() noexcept { used = 0; }
  size_t size() const noexcept { return used; }
  Point* data() noexcept { return storage.get(); }
  const Point* data() const noexcept { return storage.get(); }
  const Point* begin() const noexcept { return data(); }
  const Point* end() const noexcept { return used ? data() + used : data(); }
  void reserve_extra(size_t extra) {
    const size_t limit = std::numeric_limits<size_t>::max() / sizeof(Point);
    if (extra > limit - used) throw std::length_error("Morton SIMD output too large");
    const size_t required = used + extra;
    if (required <= capacity) return;
    const size_t grown = capacity <= limit / 2 ? capacity * 2 : limit;
    const size_t next_capacity = std::max(required, grown);
    std::unique_ptr<Point[]> next(new Point[next_capacity]);
    if (used) std::memcpy(next.get(), data(), used * sizeof(Point));
    storage = std::move(next); capacity = next_capacity;
  }
  void push_back(Point p) { reserve_extra(1); storage[used++] = p; }
 private:
  friend class MortonSIMD;
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
    const uint64_t dx = p.x > center.x ? uint64_t(p.x) - center.x : uint64_t(center.x) - p.x;
    const uint64_t dy = p.y > center.y ? uint64_t(p.y) - center.y : uint64_t(center.y) - p.y;
    return Wide(dx) * dx + Wide(dy) * dy <= radius_squared;
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

struct RankIndex {
  // Original layered rank index, with one AVX-512 vector per node.
  static constexpr size_t B = 8;
  size_t N, H, overall_size;
  std::vector<size_t> offsets;
  std::vector<uint64_t> btree;
  static size_t blocks(size_t n) { return (n + B - 1) / B; }
  static size_t prev_keys(size_t n) { return (blocks(n) + B) / (B + 1) * B; }
  static size_t height(size_t n) { return n <= B ? 1 : height(prev_keys(n)) + 1; }
  explicit RankIndex(size_t n) : N(n), H(height(n)), offsets(H + 1) {
    size_t k = 0;
    for (size_t h = 1; h < H; ++h) { k += blocks(n) * B; n = prev_keys(n); offsets[h] = k; }
    overall_size = k + B; offsets[H] = overall_size; btree.resize(overall_size);
  }
  uint64_t operator[](size_t i) const { return btree[i]; }
  void build() {
    std::fill(btree.begin() + N, btree.end(), UINT64_MAX);
    for (size_t h = 1; h < H; ++h)
      for (size_t i = 0; i < offsets[h + 1] - offsets[h]; ++i) {
        size_t k = (i / B) * (B + 1) + i % B + 1;
        for (size_t l = 0; l < h - 1; ++l) k *= B + 1;
        btree[offsets[h] + i] = k * B < N ? btree[k * B] : UINT64_MAX;
      }
  }
  size_t lower_bound(uint64_t key) const {
    const auto x = _mm512_set1_epi64(std::bit_cast<int64_t>(key));
    auto count = [&](size_t offset) {
      return size_t(std::popcount(unsigned(_mm512_cmplt_epu64_mask(
          _mm512_loadu_si512(btree.data() + offset), x))));
    };
    size_t k = 0;
    for (size_t h = H - 1; h > 0; --h) k = k * (B + 1) + count(offsets[h] + k) * B;
    return k + count(k);
  }
  size_t upper_bound(uint64_t key) const {
    return N == 0 || key >= btree[N - 1] ? N : lower_bound(key + 1);
  }
};

class MortonSIMD {
 public:
  using IDXT = uint32_t;
  using PointType = Point;
  using QueryType = Box;
  using Rank = uint64_t;
  static constexpr bool reports_exact_circle = true;
  explicit MortonSIMD(const std::vector<PointType>& input) : ranks(input.size()) {
    // Original associative sort: zip ranks with points, sort, then unzip.
    std::vector<std::pair<Rank, PointType>> sorted;
    sorted.reserve(input.size());
    for (const auto p : input) sorted.emplace_back(morton(p.x, p.y), p);
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    points.reserve(sorted.size());
    for (size_t i = 0; i < sorted.size(); ++i) {
      ranks.btree[i] = sorted[i].first;
      points.push_back(sorted[i].second);
    }
    ranks.build();

  }

  void query(Point center, uint64_t radius, auto&& output) const {
    query(Circle{center, Wide(radius) * radius}, std::forward<decltype(output)>(output));
  }
  void query(const Circle& circle, auto output) const { scan(circle.bounds(), &circle, output); }
  void query(const Box& box, auto output) const { scan(box, nullptr, output); }
  void query(const Circle& circle, Results& output) const {
    const Box box = circle.bounds();
    visit_ranges(box, [&](size_t start, size_t end) { report(start, end, box, &circle, output); });
  }
  void query(const Box& box, Results& output) const {
    visit_ranges(box, [&](size_t start, size_t end) { report(start, end, box, nullptr, output); });
  }
 protected:
  // Separate harness adapters can supply an already computed bounding square.
  void scan(const Box& box, const Circle* circle, auto output) const {
    visit_ranges(box, [&](size_t start, size_t end) { report(start, end, box, circle, output); });
  }
  void visit_ranges(const Box& box, auto visit) const {
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
      if (start < points.size() && ranks[start] < first) start = lower_bound(first);
      const size_t end = start < points.size() && ranks[start] <= last
                             ? upper_bound(last) : start;
      visit(start, end);
      // Sorted intervals may repeat; never report a point twice.
      start = end;
    }
  }
 protected:
  std::vector<PointType> points;
  RankIndex ranks;

  // Original Morton encoding. PDEP deposits scalar bits; it is not SIMD.
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
  size_t lower_bound(Rank key) const { return ranks.lower_bound(key); }
  size_t upper_bound(Rank key) const { return ranks.upper_bound(key); }


  void report(size_t start, size_t end, const Box& box, const Circle* circle, auto& output) const {
    constexpr bool batched = std::is_same_v<std::remove_cvref_t<decltype(output)>, Results>;
    if (start == end) return;
    // Eight-lane stores may leave seven uncommitted scratch points after the output.
    if constexpr (batched) output.reserve_extra(end - start + 7);
    if (circle) {
      static_assert(sizeof(Point) == 8 && offsetof(Point, y) == 4);
      const auto cx = _mm512_set1_epi64(circle->center.x);
      const auto cy = _mm512_set1_epi64(circle->center.y);
      const auto low = _mm512_set1_epi64(UINT32_MAX);
      const auto radius = _mm512_set1_epi64(std::bit_cast<int64_t>(uint64_t(circle->radius_squared)));
      const uint64_t high = uint64_t(circle->radius_squared >> 64);
      auto report_batch = [&](const __m512i points, unsigned valid) {
        const auto x = _mm512_and_si512(points, low), y = _mm512_srli_epi64(points, 32);
        const auto dx = _mm512_sub_epi64(_mm512_max_epu64(x,cx), _mm512_min_epu64(x,cx));
        const auto dy = _mm512_sub_epi64(_mm512_max_epu64(y,cy), _mm512_min_epu64(y,cy));
        const auto xx = _mm512_mul_epu32(dx,dx);
        const auto sum = _mm512_add_epi64(xx, _mm512_mul_epu32(dy,dy));
        const unsigned carry = _mm512_cmplt_epu64_mask(sum,xx);
        const unsigned le = _mm512_cmple_epu64_mask(sum,radius);
        const unsigned accepted = (high > 1 ? 255 : high ? le | ~carry : le & ~carry) & valid;
        const auto packed = _mm512_maskz_compress_epi64(accepted,points);
        const unsigned count = std::popcount(accepted);
        if constexpr (batched) {
          _mm512_storeu_si512(output.data() + output.used, packed);
          output.used += count;
        } else {
          alignas(64) Point temporary[8];
          _mm512_store_si512(temporary, packed);
          for (unsigned i = 0; i < count; ++i) *output++ = temporary[i];
        }
      };
      for (; end - start >= 8; start += 8)
        report_batch(_mm512_loadu_si512(points.data()+start),255);
      if (start < end) {
        const unsigned valid = (1u << unsigned(end-start))-1;
        report_batch(_mm512_maskz_loadu_epi64(valid,points.data()+start),valid);
      }
      return;
    }
    for (; start < end; ++start) {
      const auto p = points[start];
      if (circle ? circle->contains(p) : p.x >= box.p.x && p.x <= box.q.x && p.y >= box.p.y && p.y <= box.q.y)
        if constexpr (batched) output.storage[output.used++] = p;
        else *output++ = p;
    }
  }
};
}  // namespace diskreport::morton_MortonSIMD
