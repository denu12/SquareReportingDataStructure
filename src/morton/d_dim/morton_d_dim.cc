// Standalone d-dimensional Morton reporting: C++20, x86-64 AVX2, no external libraries.
// Include this file in a client compiled with -O3 -mavx2. No early-termination oracle.
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <immintrin.h>
#include <stdexcept>
#include <utility>
#include <vector>

namespace diskreport::ddim {
using Wide = unsigned __int128;

// Original interleaving: coordinate bit b on axis a occupies rank bit D*b+a.
// The most significant rank word comes first; padding words remain zero.
template<std::size_t D> struct Morton {
  static_assert(D >= 1 && D <= 16, "MortonDDim supports 1 through 16 dimensions: cell enumeration is exponential in D");
  using Point = std::array<std::uint32_t, D>;
  static_assert(sizeof(Point) == D * sizeof(std::uint32_t));
  static constexpr std::size_t bits = 32 * D;
  static constexpr std::size_t words = (bits + 63) / 64;
  using Key = std::array<std::uint64_t, (words + 3) / 4 * 4>;

  // Dilate consecutive coordinate bits into every D-th rank bit. All masks and
  // shifts are compile-time constants; no BMI2 instructions are required.
  template<unsigned N, unsigned Stages> static __m256i spread(__m256i x) {
    if constexpr (Stages == 0 || D == 1) return x;
    else {
      constexpr unsigned stage = Stages - 1;
      constexpr unsigned shift = (D - 1) * (1U << stage);
      constexpr std::uint64_t mask = [] {
        std::uint64_t value = 0;
        for (unsigned j = 0; j < N; ++j)
          value |= std::uint64_t{1} << (j + (D - 1) * ((j >> stage) << stage));
        return value;
      }();
      x = _mm256_and_si256(_mm256_or_si256(x, _mm256_slli_epi64(x, shift)),
                          _mm256_set1_epi64x(mask));
      return spread<N, Stages - 1>(x);
    }
  }
  template<std::size_t Word, std::size_t Axis = 0>
  static __m256i pack_word(const __m256i* coordinates) {
    if constexpr (Axis == D) return _mm256_setzero_si256();
    else {
      constexpr auto first = (words - 1 - Word) * 64;
      constexpr auto end = std::min(bits, first + 64);
      constexpr auto start_bit = first > Axis ? (first - Axis + D - 1) / D : 0;
      constexpr auto end_bit = end > Axis ? (end - 1 - Axis) / D + 1 : 0;
      auto x = _mm256_setzero_si256();
      if constexpr (end_bit > start_bit) {
        constexpr unsigned n = end_bit - start_bit;
        constexpr unsigned offset = D * start_bit + Axis - first;
        x = _mm256_and_si256(_mm256_srli_epi64(coordinates[Axis], start_bit),
                            _mm256_set1_epi64x((std::uint64_t{1} << n) - 1));
        x = spread<n, std::bit_width(n - 1)>(x);
        x = _mm256_slli_epi64(x, offset);
      }
      return _mm256_or_si256(x, pack_word<Word, Axis + 1>(coordinates));
    }
  }
  template<std::size_t Word = 0>
  static void encode_words(const __m256i* coordinates, std::array<Key, 4>& result, std::size_t count) {
    if constexpr (Word < words) {
      alignas(32) std::uint64_t lanes[4];
      _mm256_store_si256(reinterpret_cast<__m256i*>(lanes), pack_word<Word>(coordinates));
      for (std::size_t lane = 0; lane < count; ++lane) result[lane][Word] = lanes[lane];
      encode_words<Word + 1>(coordinates, result, count);
    }
  }
  // Encode four points in parallel; unused lanes repeat the first point safely.
  static std::array<Key, 4> encode_four(const Point* points, std::size_t count,
                                      unsigned depth = 32) {
    if (depth > 32 || count == 0 || count > 4) throw std::invalid_argument("Invalid encoding input");
    const auto offsets = _mm_setr_epi32(0, count > 1 ? D : 0,
                                       count > 2 ? 2 * D : 0, count > 3 ? 3 * D : 0);
    const auto mask = _mm256_set1_epi64x(depth ? std::uint64_t(UINT32_MAX) << (32 - depth) : 0);
    alignas(32) __m256i coordinates[D];
    for (std::size_t a = 0; a < D; ++a) {
      const auto values = _mm_i32gather_epi32(reinterpret_cast<const int*>(points->data() + a), offsets, 4);
      coordinates[a] = _mm256_and_si256(_mm256_cvtepu32_epi64(values), mask);
    }
    std::array<Key, 4> result{};
    encode_words(coordinates, result, count);
    return result;
  }
  static Key encode(const Point& point, unsigned depth = 32) {
    return encode_four(&point, 1, depth)[0];
  }
  static std::pair<Key, Key> cell(const Point& point, unsigned depth) {
    Key lo = encode(point, depth), hi = lo;
    const std::size_t suffix_bits = D * (32 - depth);
    for (std::size_t w = 0; w < words; ++w) {
      const auto first_bit = (words - 1 - w) * 64;
      const auto count = suffix_bits > first_bit ? std::min<std::size_t>(64, suffix_bits - first_bit) : 0;
      hi[w] |= count == 64 ? UINT64_MAX : (std::uint64_t{1} << count) - 1;
    }
    return {lo, hi};
  }
  static __m256i unsigned_greater(__m256i a, __m256i b) {
    const auto sign = _mm256_set1_epi64x(INT64_MIN);
    return _mm256_cmpgt_epi64(_mm256_xor_si256(a, sign), _mm256_xor_si256(b, sign));
  }
  static bool less(const Key& a, const Key& b) {
    // SIMD lexicographic comparison, four rank words at a time.
    for (std::size_t w = 0; w < a.size(); w += 4) {
      const auto x = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a.data() + w));
      const auto y = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b.data() + w));
      const unsigned same = _mm256_movemask_pd(_mm256_castsi256_pd(_mm256_cmpeq_epi64(x, y)));
      if (same != 15) {
        const unsigned lane = std::countr_zero((~same) & 15U);
        return (_mm256_movemask_pd(_mm256_castsi256_pd(unsigned_greater(y, x))) >> lane) & 1;
      }
    }
    return false;
  }
};

// Static index. Output iterators receive actual original integer coordinates.
template<std::size_t D> class MortonDDim {
 public:
  using Codec = Morton<D>;
  using Point = typename Codec::Point;
  using Key = typename Codec::Key;
  explicit MortonDDim(const std::vector<Point>& input) : size_(input.size()) {
    struct Entry { Key key; std::size_t id; };
    std::vector<Entry> sorted;
    sorted.reserve(size_);
    for (std::size_t i = 0; i < size_; i += 4) {
      const auto count = std::min<std::size_t>(4, size_ - i);
      const auto keys = Codec::encode_four(input.data() + i, count);
      for (std::size_t lane = 0; lane < count; ++lane) sorted.push_back({keys[lane], i + lane});
    }
    std::sort(sorted.begin(), sorted.end(), [](const Entry& a, const Entry& b) { return Codec::less(a.key, b.key); });
    points_.resize((size_ + 7) / 8);
    // Leaves and each upper level contain groups of eight ordered separators.
    // Nine children per internal node keep the search depth logarithmic.
    std::size_t level_size = size_, offset = 0;
    offsets_.push_back(0);
    while (level_size > 8) {
      offset += (level_size + 7) / 8 * 8;
      offsets_.push_back(offset);
      level_size = ((level_size + 7) / 8 + 8) / 9 * 8;
    }
    for (auto& word : ranks_) word.resize(offset + 8, -1);
    for (std::size_t i = 0; i < size_; ++i) {
      for (std::size_t a = 0; a < D; ++a) points_[i / 8].axis[a][i % 8] = input[sorted[i].id][a];
      for (std::size_t w = 0; w < Codec::words; ++w) ranks_[w][i] = std::bit_cast<long long>(sorted[i].key[w]);
    }
    for (std::size_t h = 1; h < offsets_.size(); ++h) {
      const auto end = h + 1 < offsets_.size() ? offsets_[h + 1] : ranks_[0].size();
      for (std::size_t i = 0; i < end - offsets_[h]; ++i) {
        auto leaf = (i / 8) * 9 + i % 8 + 1;
        for (std::size_t l = 0; l < h - 1; ++l) leaf *= 9;
        if (leaf * 8 < size_)
          for (std::size_t w = 0; w < Codec::words; ++w) ranks_[w][offsets_[h] + i] = ranks_[w][leaf * 8];
      }
    }
  }
  void query(const Point& center, std::uint64_t radius, auto output) const {
    query_squared(center, Wide(radius) * radius, output);
  }
  void query_squared(const Point& center, Wide radius_squared, auto output) const {
    if (!size_) return;
    if (radius_squared >= Wide(D) * UINT32_MAX * UINT32_MAX) {
      emit(0, size_, output);
      return;
    }
    std::uint64_t radius = UINT32_MAX;
    if (radius_squared < Wide(UINT32_MAX) * UINT32_MAX) {
      radius = std::uint64_t(std::sqrt(static_cast<long double>(radius_squared)));
      // Floating point only estimates the bounding radius; corrections are exact.
      while (Wide(radius) * radius < radius_squared) ++radius;
      while (radius && Wide(radius - 1) * (radius - 1) >= radius_squared) --radius;
    }
    Point lo{}, hi{};
    std::uint32_t diameter = 0;
    for (std::size_t a = 0; a < D; ++a) {
      lo[a] = center[a] > radius ? center[a] - radius : 0;
      hi[a] = std::min<std::uint64_t>(UINT32_MAX, std::uint64_t(center[a]) + radius);
      diameter = std::max(diameter, hi[a] - lo[a]);
    }
    const unsigned suffix = std::bit_width(diameter), depth = 32 - suffix;
    // Sort crossing axes by their most significant changed Morton bit. Binary
    // corner masks in this order enumerate the cells in ascending rank order.
    // Only D axis deltas are encoded, rather than up to 2^D complete corners.
    std::array<std::pair<unsigned, std::size_t>, D> crossing{};
    std::size_t axes = 0;
    for (std::size_t a = 0; a < D; ++a)
      if ((std::uint64_t(lo[a]) >> suffix) != (std::uint64_t(hi[a]) >> suffix))
        crossing[axes++] = {unsigned(D * (31 - std::countl_zero(lo[a] ^ hi[a])) + a), a};
    std::sort(crossing.begin(), crossing.begin() + axes);
    const Key base = Codec::encode(lo, depth);
    std::array<Key, D> changes{};
    for (std::size_t i = 0; i < axes; i += 4) {
      const auto count = std::min<std::size_t>(4, axes - i);
      std::array<Point, 4> corners{};
      for (std::size_t lane = 0; lane < count; ++lane) {
        corners[lane] = lo;
        const auto a = crossing[i + lane].second;
        corners[lane][a] = hi[a];
      }
      const auto keys = Codec::encode_four(corners.data(), count, depth);
      for (std::size_t lane = 0; lane < count; ++lane)
        for (std::size_t w = 0; w < Codec::words; ++w) changes[i + lane][w] = keys[lane][w] ^ base[w];
    }
    Key suffix_mask{};
    const std::size_t suffix_bits = D * suffix;
    for (std::size_t w = 0; w < Codec::words; ++w) {
      const auto first_bit = (Codec::words - 1 - w) * 64;
      const auto count = suffix_bits > first_bit ? std::min<std::size_t>(64, suffix_bits - first_bit) : 0;
      suffix_mask[w] = count == 64 ? UINT64_MAX : (std::uint64_t{1} << count) - 1;
    }
    auto cell_high = [&](const Key& key) {
      Key result = key;
      for (std::size_t w = 0; w < Codec::words; ++w) result[w] |= suffix_mask[w];
      return result;
    };
    auto adjacent = [&](const Key& left, const Key& right) {
      Key next = left;
      for (std::size_t w = Codec::words; w-- > 0;) if (++next[w]) return next == right;
      return false;
    };
    Key key = base, first = base, last = cell_high(base);
    for (unsigned mask = 1; mask < (1U << axes); ++mask) {
      unsigned changed = mask ^ (mask - 1);
      while (changed) {
        const auto a = std::countr_zero(changed);
        for (std::size_t w = 0; w < Codec::words; ++w) key[w] ^= changes[a][w];
        changed &= changed - 1;
      }
      if (!adjacent(last, key)) {
        report(bound(first, false), bound(last, true), center, radius_squared, output);
        first = key;
      }
      last = cell_high(key);
    }
    report(bound(first, false), bound(last, true), center, radius_squared, output);
  }
 private:
  struct alignas(32) Block { std::uint32_t axis[D][8]{}; };
  std::vector<Block> points_;
  std::size_t size_;
  std::vector<std::size_t> offsets_;
  std::array<std::vector<long long>, Codec::words> ranks_;

  std::size_t bound(const Key& key, bool upper) const {
    auto count = [&](std::size_t offset) {
      unsigned mask = 0;
      for (unsigned half = 0; half < 2; ++half) {
        auto equal = _mm256_set1_epi64x(-1), less = _mm256_setzero_si256();
        for (std::size_t w = 0; w < Codec::words; ++w) {
          const auto values = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ranks_[w].data() + offset + half * 4));
          const auto wanted = _mm256_set1_epi64x(key[w]);
          less = _mm256_or_si256(less, _mm256_and_si256(equal, Codec::unsigned_greater(wanted, values)));
          equal = _mm256_and_si256(equal, _mm256_cmpeq_epi64(values, wanted));
        }
        if (upper) less = _mm256_or_si256(less, equal);
        mask |= unsigned(_mm256_movemask_pd(_mm256_castsi256_pd(less))) << (4 * half);
      }
      return std::size_t(std::popcount(mask));
    };
    std::size_t k = 0;
    for (std::size_t h = offsets_.size() - 1; h > 0; --h) {
      const auto end = h + 1 < offsets_.size() ? offsets_[h + 1] : ranks_[0].size();
      // A fully maximal key can equal padded separators for D divisible by two.
      // Guard each level rather than treating padding as a distinct valid key.
      if (k >= end - offsets_[h]) return size_;
      k = k * 9 + count(offsets_[h] + k) * 8;
    }
    if (k >= size_) return size_;
    return std::min(size_, k + count(k));
  }
  Point point(std::size_t index) const {
    Point result;
    for (std::size_t a = 0; a < D; ++a) result[a] = points_[index / 8].axis[a][index % 8];
    return result;
  }
  void emit(std::size_t first, std::size_t last, auto& output) const {
    for (auto i = first; i < last; ++i) *output++ = point(i);
  }
  void report(std::size_t first, std::size_t last, const Point& center,
              Wide radius_squared, auto& output) const {
    if (first == last) return;
    const auto radius_lo = _mm256_set1_epi64x(std::uint64_t(radius_squared));
    const auto radius_hi = _mm256_set1_epi64x(std::uint64_t(radius_squared >> 64));
    for (std::size_t block = first / 8; block < (last + 7) / 8; ++block) {
      const auto begin = std::max(first, block * 8) - block * 8;
      const auto end = std::min(last, block * 8 + 8) - block * 8;
      const unsigned valid = ((1U << end) - 1) ^ ((1U << begin) - 1);
      __m256i sums[2]{_mm256_setzero_si256(), _mm256_setzero_si256()};
      __m256i carries[2]{_mm256_setzero_si256(), _mm256_setzero_si256()};
      for (std::size_t a = 0; a < D; ++a) {
        const auto values = _mm256_load_si256(reinterpret_cast<const __m256i*>(points_[block].axis[a]));
        const auto c = _mm256_set1_epi32(center[a]);
        const auto delta = _mm256_sub_epi32(_mm256_max_epu32(values, c), _mm256_min_epu32(values, c));
        const __m256i halves[2]{_mm256_cvtepu32_epi64(_mm256_castsi256_si128(delta)),
                               _mm256_cvtepu32_epi64(_mm256_extracti128_si256(delta, 1))};
        for (unsigned half = 0; half < 2; ++half) {
          const auto previous = sums[half];
          sums[half] = _mm256_add_epi64(sums[half], _mm256_mul_epu32(halves[half], halves[half]));
          carries[half] = _mm256_sub_epi64(carries[half], Codec::unsigned_greater(previous, sums[half]));
        }
      }
      unsigned mask = 0;
      for (unsigned half = 0; half < 2; ++half) {
        const auto inside = _mm256_or_si256(Codec::unsigned_greater(radius_hi, carries[half]),
            _mm256_and_si256(_mm256_cmpeq_epi64(carries[half], radius_hi),
                _mm256_xor_si256(Codec::unsigned_greater(sums[half], radius_lo), _mm256_set1_epi64x(-1))));
        mask |= unsigned(_mm256_movemask_pd(_mm256_castsi256_pd(inside))) << (4 * half);
      }
      mask &= valid;
      while (mask) {
        const auto lane = std::countr_zero(mask);
        *output++ = point(block * 8 + lane);
        mask &= mask - 1;
      }
    }
  }
};
}  // namespace diskreport::ddim
