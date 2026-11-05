#pragma once
#include <memory>
#include <limits>
#include <span>
#include <cstring>
// Explicit SIMD 3D Morton reporting: C++20, x86-64 AVX-512F, no external libraries.
// Includes batched native output; no early-termination oracle.
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

namespace diskreport::morton3d {
using Point = std::array<uint32_t,3>;
class Results {
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
 private:
  friend class MortonSIMD3D;
};

using Wide = unsigned __int128;

// Three uint32 coordinates produce a 96-bit Morton key, stored high word first.
struct MortonCodec3D {
  using Point = std::array<std::uint32_t, 3>;
  using Key = std::array<std::uint64_t, 2>;
  static constexpr std::size_t words = 2;
  static_assert(sizeof(Point) == 12);

  // Spread 21 bits into every third bit of each AVX2 lane.
  static __m256i spread(__m256i x) {
    x = _mm256_and_si256(x, _mm256_set1_epi64x(0x1fffff));
    x = _mm256_and_si256(_mm256_or_si256(x, _mm256_slli_epi64(x,32)), _mm256_set1_epi64x(0x1f00000000ffff));
    x = _mm256_and_si256(_mm256_or_si256(x, _mm256_slli_epi64(x,16)), _mm256_set1_epi64x(0x1f0000ff0000ff));
    x = _mm256_and_si256(_mm256_or_si256(x, _mm256_slli_epi64(x,8)), _mm256_set1_epi64x(0x100f00f00f00f00f));
    x = _mm256_and_si256(_mm256_or_si256(x, _mm256_slli_epi64(x,4)), _mm256_set1_epi64x(0x10c30c30c30c30c3));
    return _mm256_and_si256(_mm256_or_si256(x, _mm256_slli_epi64(x,2)), _mm256_set1_epi64x(0x1249249249249249));
  }
  static __m256i interleave(__m256i x, __m256i y, __m256i z) {
    return _mm256_or_si256(spread(x), _mm256_or_si256(
        _mm256_slli_epi64(spread(y),1), _mm256_slli_epi64(spread(z),2)));
  }
  static std::array<Key,4> encode_four(const Point* p, std::size_t count) {
    const auto offsets = _mm_setr_epi32(0,count>1?3:0,count>2?6:0,count>3?9:0);
    auto axis = [&](unsigned a) { return _mm256_cvtepu32_epi64(
        _mm_i32gather_epi32(reinterpret_cast<const int*>(p->data()+a),offsets,4)); };
    const auto x=axis(0), y=axis(1), z=axis(2);
    const auto upper=interleave(_mm256_srli_epi64(x,21),_mm256_srli_epi64(y,21),_mm256_srli_epi64(z,21));
    const auto low=_mm256_or_si256(interleave(x,y,z),_mm256_slli_epi64(upper,63));
    const auto high=_mm256_srli_epi64(upper,1);
    alignas(32) std::uint64_t lo[4],hi[4];
    _mm256_store_si256(reinterpret_cast<__m256i*>(lo),low);
    _mm256_store_si256(reinterpret_cast<__m256i*>(hi),high);
    std::array<Key,4> result{};
    for(std::size_t lane=0;lane<count;++lane) result[lane]={hi[lane],lo[lane]};
    return result;
  }
  static Key encode(const Point& p) { return encode_four(&p,1)[0]; }
  static std::pair<Key,Key> cell(Key key,unsigned depth) {
    Key lo=key,hi=lo;
    const unsigned suffix=3*(32-depth);
    const auto low_mask=suffix>=64?UINT64_MAX:(std::uint64_t{1}<<suffix)-1;
    const auto high_mask=suffix>64?(std::uint64_t{1}<<(suffix-64))-1:0;
    lo[0]&=~high_mask; lo[1]&=~low_mask;
    hi[0]|=high_mask; hi[1]|=low_mask;
    return {lo,hi};
  }
  static __m256i unsigned_greater(__m256i a,__m256i b) {
    const auto sign=_mm256_set1_epi64x(INT64_MIN);
    return _mm256_cmpgt_epi64(_mm256_xor_si256(a,sign),_mm256_xor_si256(b,sign));
  }
  static bool less(const Key& a,const Key& b) {
    const auto x=_mm_loadu_si128(reinterpret_cast<const __m128i*>(a.data()));
    const auto y=_mm_loadu_si128(reinterpret_cast<const __m128i*>(b.data()));
    const unsigned same=_mm_movemask_pd(_mm_castsi128_pd(_mm_cmpeq_epi64(x,y)));
    if(same==3) return false;
    const unsigned lane=std::countr_zero((~same)&3U);
    const auto sign=_mm_set1_epi64x(INT64_MIN);
    const auto greater=_mm_cmpgt_epi64(_mm_xor_si128(y,sign),_mm_xor_si128(x,sign));
    return (_mm_movemask_pd(_mm_castsi128_pd(greater))>>lane)&1;
  }
};

namespace simd_detail {
// Compact each coordinate axis, then interleave the accepted xyz triples.
inline unsigned append_axes(Point* out,const __m256i* axes,unsigned mask) {
  if(!mask)return 0;
  const auto x=_mm512_maskz_compress_epi32(mask,_mm512_castsi256_si512(axes[0]));
  const auto y=_mm512_maskz_compress_epi32(mask,_mm512_castsi256_si512(axes[1]));
  const auto z=_mm512_maskz_compress_epi32(mask,_mm512_castsi256_si512(axes[2]));
  auto first=_mm512_permutex2var_epi32(x,_mm512_setr_epi32(0,16,0,1,17,0,2,18,0,3,19,0,4,20,0,5),y);
  first=_mm512_mask_mov_epi32(first,0x4924,_mm512_permutexvar_epi32(_mm512_setr_epi32(0,0,0,0,0,1,0,0,2,0,0,3,0,0,4,0),z));
  auto second=_mm512_permutex2var_epi32(x,_mm512_setr_epi32(21,0,6,22,0,7,23,0,0,0,0,0,0,0,0,0),y);
  second=_mm512_mask_mov_epi32(second,0x92,_mm512_permutexvar_epi32(_mm512_setr_epi32(0,5,0,0,6,0,0,7,0,0,0,0,0,0,0,0),z));
  _mm512_storeu_si512(out,first);_mm512_storeu_si512(reinterpret_cast<char*>(out)+64,second);
  return std::popcount(mask);
}
}

// Static index. Results receive original integer triples in contiguous batches.
class MortonSIMD3D {
  static constexpr std::size_t D = 3;
 public:
  using Codec = MortonCodec3D;
  using Point = typename Codec::Point;
  using Key = typename Codec::Key;
  explicit MortonSIMD3D(const std::vector<Point>& input) {
    struct Entry { Key key; std::size_t id; };
    std::vector<Entry> sorted;
    sorted.reserve(input.size());
    for (std::size_t i = 0; i < input.size(); i += 4) {
      const auto count = std::min<std::size_t>(4, input.size() - i);
      const auto keys = Codec::encode_four(input.data() + i, count);
      for (std::size_t lane = 0; lane < count; ++lane) sorted.push_back({keys[lane], i + lane});
    }
    std::sort(sorted.begin(), sorted.end(), [](const Entry& a, const Entry& b) { return Codec::less(a.key, b.key); });
    points_.resize((input.size()+7)/8);
    size_=input.size();
    std::size_t level_size=size_,offset=0;
    offsets_.push_back(0);
    while(level_size>8) {
      const auto blocks=(level_size+7)/8;
      offset+=blocks*8;offsets_.push_back(offset);
      level_size=(blocks+8)/9*8;
    }
    for(auto& axis:ranks_)axis.resize(offset+8,-1);
    for (std::size_t i = 0; i < sorted.size(); ++i) {
      for(unsigned a=0;a<3;++a)points_[i/8].axis[a][i%8]=input[sorted[i].id][a];
      for (std::size_t w = 0; w < Codec::words; ++w) ranks_[w][i] = std::bit_cast<long long>(sorted[i].key[w]);
    }
    for(std::size_t h=1;h<offsets_.size();++h) {
      const auto end=h+1<offsets_.size()?offsets_[h+1]:ranks_[0].size();
      for(std::size_t i=0;i<end-offsets_[h];++i) {
        auto k=(i/8)*9+i%8+1;
        for(std::size_t l=0;l<h-1;++l)k*=9;
        if(k*8<size_)for(unsigned w=0;w<2;++w)ranks_[w][offsets_[h]+i]=ranks_[w][k*8];
      }
    }
  }
  void query(const Point& center, std::uint64_t radius, Results& output) const {
    query_squared(center, Wide(radius) * radius, output);
  }
  void query_squared(const Point& center, Wide radius_squared, Results& output) const {
    std::uint64_t radius = UINT32_MAX;
    // Integer ceiling square root; larger radii already enclose every axis.
    if (radius_squared < Wide(UINT32_MAX) * UINT32_MAX) {
      radius = std::uint64_t(std::sqrt(static_cast<long double>(radius_squared)));
      // The estimate is only a starting point; these corrections are exact.
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
    // Only eight possible corners in 3D; noncrossing axes need one choice.
    unsigned crossing=0;
    for(unsigned axis=0;axis<3;++axis)
      if((std::uint64_t(lo[axis])>>suffix)!=(std::uint64_t(hi[axis])>>suffix)) crossing|=1U<<axis;
    std::array<Point,8> corners;
    std::size_t count = 0;
    for(unsigned mask=0;mask<8;++mask) {
      if(mask&~crossing) continue;
      corners[count++] = {mask&1?hi[0]:lo[0],mask&2?hi[1]:lo[1],mask&4?hi[2]:lo[2]};
    }
    std::array<std::pair<Key,Key>,8> ranges;
    for(std::size_t i=0;i<count;i+=4) {
      const auto lanes=std::min<std::size_t>(4,count-i);
      const auto keys=Codec::encode_four(corners.data()+i,lanes);
      for(std::size_t j=0;j<lanes;++j) ranges[i+j]=Codec::cell(keys[j],depth);
    }
    std::sort(ranges.begin(), ranges.begin()+count, [](const auto& a, const auto& b) { return Codec::less(a.first, b.first); });
    for (std::size_t i=0;i<count;++i)
      report(bound(ranges[i].first, false), bound(ranges[i].second, true), center, radius_squared, output);
  }

 private:
  // One coordinate copy, grouped into SIMD-sized blocks.
  struct alignas(32) Block { std::uint32_t axis[3][8]{}; };
  std::vector<Block> points_;
  std::size_t size_;
  std::vector<std::size_t> offsets_;
  std::array<std::vector<long long>, Codec::words> ranks_;

  // Each search node is eight consecutive 96-bit keys, loaded without gathers.
  std::size_t bound(const Key& key, bool upper) const {
    auto count=[&](std::size_t offset) {
      const auto hi=_mm512_loadu_si512(ranks_[0].data()+offset),lo=_mm512_loadu_si512(ranks_[1].data()+offset);
      const auto h=_mm512_set1_epi64(key[0]),l=_mm512_set1_epi64(key[1]);
      const auto same=_mm512_cmpeq_epi64_mask(hi,h);
      unsigned mask=_mm512_cmplt_epu64_mask(hi,h)|(same&_mm512_cmplt_epu64_mask(lo,l));
      if(upper)mask|=same&_mm512_cmpeq_epi64_mask(lo,l);
      return std::size_t(std::popcount(mask));
    };
    std::size_t k=0;
    for(std::size_t h=offsets_.size()-1;h>0;--h)k=k*9+count(offsets_[h]+k)*8;
    return k+count(k);
  }
  void report(std::size_t first,std::size_t last,const Point& c,Wide r2,Results& out) const {
    if(first==last)return;
    out.reserve(out.size_+last-first+11);
    const auto radius_low=_mm512_set1_epi64(uint64_t(r2)),radius_high=_mm512_set1_epi64(uint64_t(r2>>64));
    const auto one=_mm512_set1_epi64(1);
    for(size_t block=first/8;block<(last+7)/8;++block) {
      const unsigned begin=std::max(first,block*8)-block*8,end=std::min(last,block*8+8)-block*8;
      const unsigned valid=((1u<<end)-1)^((1u<<begin)-1);
      __m256i axes[3];auto sum=_mm512_setzero_si512(),high=sum;
      for(unsigned a=0;a<3;++a) {
        axes[a]=_mm256_load_si256(reinterpret_cast<const __m256i*>(points_[block].axis[a]));
        const auto values=_mm512_cvtepu32_epi64(axes[a]),center=_mm512_set1_epi64(c[a]);
        const auto d=_mm512_sub_epi64(_mm512_max_epu64(values,center),_mm512_min_epu64(values,center));
        const auto next=_mm512_add_epi64(sum,_mm512_mul_epu32(d,d));
        high=_mm512_mask_add_epi64(high,_mm512_cmplt_epu64_mask(next,sum),high,one);sum=next;
      }
      const unsigned mask=valid&(_mm512_cmplt_epu64_mask(high,radius_high)|
          (_mm512_cmpeq_epi64_mask(high,radius_high)&_mm512_cmple_epu64_mask(sum,radius_low)));
      out.size_+=simd_detail::append_axes(out.data_+out.size_,axes,mask);
    }
  }
};
} // namespace diskreport::morton3d
