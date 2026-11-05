// Standalone d-dimensional MortonPlus extension: C++20, AVX2, no external libraries.
// MortonPlus ideas: ESA authors Thomas Blasius, Jean-Pierre von der Heydt,
// Tobias Kempf, Dennis Kobert and Nikolai Maas; Rust implementation Dennis Kobert.
// Local extension: bounded binary-prefix refinement, capped lookup and bulk output.
#pragma once
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
#include <cstring>
#include <memory>
#include <span>
#include <limits>

namespace diskreport::ddimplus {
using Wide = unsigned __int128;
template<std::size_t D> class Results {
 public:
  using Point = std::array<std::uint32_t,D>;
 private:
  std::allocator<Point> alloc_;
  Point* data_ = nullptr;
  std::size_t size_ = 0, capacity_ = 0;
 public:
  Results() = default;
  Results(const Results&) = delete;
  Results& operator=(const Results&) = delete;
  Results(Results&& other) noexcept { swap(other); }
  Results& operator=(Results&& other) noexcept { swap(other); return *this; }
  ~Results() { if (data_) alloc_.deallocate(data_, capacity_); }
  void swap(Results& other) noexcept {
    std::swap(data_, other.data_); std::swap(size_, other.size_);
    std::swap(capacity_, other.capacity_);
  }
  void clear() noexcept { size_ = 0; }
  std::size_t size() const noexcept { return size_; }
  std::size_t capacity() const noexcept { return capacity_; }
  bool empty() const noexcept { return size_ == 0; }
  Point* data() noexcept { return data_; }
  const Point* data() const noexcept { return data_; }
  Point* begin() noexcept { return data_; }
  Point* end() noexcept { return size_ ? data_ + size_ : data_; }
  const Point* begin() const noexcept { return data_; }
  const Point* end() const noexcept { return size_ ? data_ + size_ : data_; }
  const Point& operator[](std::size_t i) const noexcept { return data_[i]; }
  std::span<const Point> view() const noexcept { return {data_, size_}; }
  void reserve(std::size_t requested) {
    if (requested <= capacity_) return;
    const auto limit = std::numeric_limits<std::size_t>::max() / sizeof(Point);
    if (requested > limit) throw std::length_error("MortonPlus output too large");
    const auto grown = capacity_ <= limit / 2 ? capacity_ * 2 : limit;
    const auto cap = std::max(requested, grown);
    auto* p = alloc_.allocate(cap);
    if (size_) std::memcpy(p, data_, size_ * sizeof(Point));
    if (data_) alloc_.deallocate(data_, capacity_);
    data_ = p; capacity_ = cap;
  }
  void push_back(Point p) {
    if (size_ == capacity_) reserve(size_ + 1);
    data_[size_++] = p;
  }
  void append(const Point* p, std::size_t count) {
    if (!count) return;
    if (count > std::numeric_limits<std::size_t>::max()-size_) throw std::length_error("Output too large");
    reserve(size_+count);
    std::memcpy(data_+size_,p,count*sizeof(Point));size_+=count;
  }
};


// Original interleaving: coordinate bit b on axis a occupies rank bit D*b+a.
// The most significant rank word comes first; padding words remain zero.
template<std::size_t D> struct Morton {
  static_assert(D >= 1 && D <= 16, "MortonPlusDDim supports 1 through 16 dimensions: cell enumeration is exponential in D");
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
template<std::size_t D> class MortonPlusDDim {
 public:
  using Codec = Morton<D>;
  using Point = typename Codec::Point;
  using Key = typename Codec::Key;
  explicit MortonPlusDDim(const std::vector<Point>& input) : size_(input.size()) {
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
    ordered_.resize(size_);
    for (auto& word : ranks_) word.resize((size_+7)/8*8+8, -1);
    for (std::size_t i=0;i<size_;++i) {
      ordered_[i]=input[sorted[i].id];
      for(std::size_t axis=0;axis<D;++axis) points_[i/8].axis[axis][i%8]=ordered_[i][axis];
      for(std::size_t w=0;w<Codec::words;++w) ranks_[w][i]=std::bit_cast<long long>(sorted[i].key[w]);
    }
    // Bound lookup memory independently of dimension (at most 4097 offsets).
    prefix_bits_=std::min<unsigned>(12,std::bit_width(size_/8));
    const std::size_t buckets=std::size_t{1}<<prefix_bits_;
    prefix_.resize(buckets+1);std::size_t at=0;
    for(std::size_t b=0;b<=buckets;++b) {
      while(at<size_ && prefix_of(sorted[at].key)<b)++at;
      prefix_[b]=at;
    }
  }
  // Both APIs append; Results<D> permits memcpy of fully contained cells.
  void query(const Point& center, std::uint64_t radius, auto&& output) const {
    query_squared(center, Wide(radius)*radius, output);
  }
  void query_squared(const Point& center, Wide radius_squared, auto&& output) const {
    if (!size_) return;
    if(radius_squared>=Wide(D)*UINT32_MAX*UINT32_MAX) { emit(0,size_,output);return; }
    Point low{},high;high.fill(UINT32_MAX);
    // Binary Morton-prefix cells avoid enumerating 2^D children at each level.
    // At most 4096 splits per query, then exact scanning of remaining ranges.
    std::size_t remaining=4096;
    auto visit=[&](auto&& self, Point& lo,Point& hi,const Key& key,
                   std::size_t first,std::size_t last,unsigned depth)->void {
      if(first==last)return;
      Wide near=0,far=0;
      for(std::size_t a=0;a<D;++a) {
        const auto c=std::uint64_t(center[a]);
        const auto nearest=c<lo[a]?lo[a]-c:c>hi[a]?c-hi[a]:0;
        const auto dl=c>lo[a]?c-lo[a]:lo[a]-c;
        const auto dh=c>hi[a]?c-hi[a]:hi[a]-c;
        near+=Wide(nearest)*nearest;far+=Wide(std::max(dl,dh))*std::max(dl,dh);
      }
      if(near>radius_squared)return;
      if(far<=radius_squared){emit(first,last,output);return;}
      if(last-first<=128 || depth==Codec::bits || remaining==0) {
        report(first,last,center,radius_squared,output);return;
      }
      --remaining;
      const auto bit=Codec::bits-1-depth;
      const auto axis=bit%D;const auto coordinate_bit=bit/D;
      Key right=key;right[Codec::words-1-bit/64]|=std::uint64_t{1}<<(bit%64);
      const auto middle=bound(right,false);
      const auto old_hi=hi[axis],old_lo=lo[axis];
      hi[axis]=lo[axis]+((std::uint32_t{1}<<coordinate_bit)-1);
      self(self,lo,hi,key,first,middle,depth+1);
      hi[axis]=old_hi;lo[axis]=old_lo+(std::uint32_t{1}<<coordinate_bit);
      self(self,lo,hi,right,middle,last,depth+1);
      lo[axis]=old_lo;
    };
    visit(visit,low,high,Key{},0,size_,0);
  }
 private:
  struct alignas(32) Block { std::uint32_t axis[D][8]{}; };
  std::vector<Block> points_;
  std::size_t size_;
  std::vector<Point> ordered_;
  unsigned prefix_bits_=0;
  std::vector<std::size_t> prefix_;
  std::size_t prefix_of(const Key& key) const {
    constexpr auto top_bits=Codec::bits-(Codec::words-1)*64;
    return prefix_bits_ ? key[0]>>(top_bits-prefix_bits_) : 0;
  }
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
    const auto bucket=prefix_of(key);
    std::size_t lo=prefix_[bucket],hi=prefix_[bucket+1];
    while(hi-lo>8) {
      const auto mid=lo+(hi-lo)/2;
      Key actual{};
      for(std::size_t w=0;w<Codec::words;++w)actual[w]=std::bit_cast<std::uint64_t>(ranks_[w][mid]);
      if(Codec::less(actual,key) || (upper && actual==key))lo=mid+1;
      else hi=mid;
    }
    return std::min(hi,lo+count(lo));
  }

  Point point(std::size_t index) const {
    Point result;
    for (std::size_t a = 0; a < D; ++a) result[a] = points_[index / 8].axis[a][index % 8];
    return result;
  }
  void emit(std::size_t first, std::size_t last, auto& output) const {
    if constexpr (requires { output.append(ordered_.data(),last-first); })
      output.append(ordered_.data()+first,last-first);
    else for (auto i = first; i < last; ++i) *output++ = ordered_[i];
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
        if constexpr (requires { output.push_back(ordered_[0]); }) output.push_back(ordered_[block*8+lane]);
        else *output++ = ordered_[block*8+lane];
        mask &= mask - 1;
      }
    }
  }
};
}  // namespace diskreport::ddimplus
