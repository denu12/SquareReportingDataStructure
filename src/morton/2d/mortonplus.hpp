// Standalone C++20 port of MortonPlus, no third-party dependencies.
// Source: TrueDoctor/DiskReportingDataStructure, commit 7e6f57558ca6ae8b93a3ca0f64a2d1b0ec579097.
// MortonPlus design and Rust implementation: the ESA authors, led by Dennis Kobert.
// Thomas Blasius, Jean-Pierre von der Heydt, Tobias Kempf, Dennis Kobert, Nikolai Maas.
// C++ translation prepared for DiskRangeReport, October 2026; see docs/THIRD_PARTY.md.
// Compile with GCC/Clang: -std=c++20 -O3 -mavx512f -mbmi2.
// Requires unsigned __int128; native MSVC is not supported.
//
// License notice: see the accompanying LICENSE file.

#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>
#include <immintrin.h>
#include "../box_refinement.hpp"

namespace mortonplus {
using Wide = unsigned __int128;
using Point = std::array<std::uint32_t, 2>;
static_assert(sizeof(Point) == 8);
// Reusable output storage. Unlike std::vector::resize, allocating writable slots
// does not zero-initialize them. Only initialized, accepted points become visible.
// clear() retains capacity. Use this buffer for the fastest bulk-output path.
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
  friend class MortonPlus;
};

namespace detail {
inline constexpr std::uint64_t even = 0x5555555555555555ULL;
inline std::uint64_t spread(std::uint32_t v) {
  return _pdep_u64(v, even);
}
inline std::uint32_t extract(std::uint64_t x) {
  return std::uint32_t(_pext_u64(x, even));
}
inline std::uint64_t span(unsigned depth) {
  return depth == 0 ? UINT64_MAX : (std::uint64_t(1) << (2 * (32 - depth))) - 1;
}
inline std::uint64_t delta(std::uint64_t a, std::uint64_t b) { return a > b ? a-b : b-a; }
inline bool contains(Point p, Point c, Wide r2) {
  const Wide x = delta(p[0], c[0]), y = delta(p[1], c[1]);
  return x*x + y*y <= r2;
}
using Vector = __m512i;
struct Prepared {
  __m512i cx, cy, low, radius;
  bool high;
  Prepared(Point c, Wide r) : cx(_mm512_set1_epi64(c[0])), cy(_mm512_set1_epi64(c[1])),
    low(_mm512_set1_epi64(UINT32_MAX)), radius(_mm512_set1_epi64(std::bit_cast<std::int64_t>(std::uint64_t(r)))),
    high((r >> 64) != 0) {}
};
inline Vector load(const Point* p) { return _mm512_loadu_si512(p); }
inline unsigned mask(Vector v, const Prepared& p) {
  const auto x = _mm512_and_si512(v, p.low), y = _mm512_srli_epi64(v, 32);
  const auto dx = _mm512_sub_epi64(_mm512_max_epu64(x,p.cx), _mm512_min_epu64(x,p.cx));
  const auto dy = _mm512_sub_epi64(_mm512_max_epu64(y,p.cy), _mm512_min_epu64(y,p.cy));
  const auto xx = _mm512_mul_epu32(dx,dx);
  const auto sum = _mm512_add_epi64(xx, _mm512_mul_epu32(dy,dy));
  const unsigned carry = _mm512_cmplt_epu64_mask(sum,xx);
  const unsigned le = _mm512_cmple_epu64_mask(sum,p.radius);
  return (p.high ? le | ~carry : le & ~carry) & 255;
}
inline unsigned emit(Vector v, unsigned m, Point* dst) {
  // Register compression plus a normal store avoids slow memory compression on Zen 4.
  _mm512_storeu_si512(dst, _mm512_maskz_compress_epi64(m,v));
  return std::popcount(m);
}
inline std::size_t count_below8(const std::uint64_t* p, std::uint64_t key) {
#if defined(__AVX512F__)
  return std::popcount(unsigned(_mm512_cmplt_epu64_mask(
    _mm512_loadu_si512(p), _mm512_set1_epi64(std::bit_cast<std::int64_t>(key)))));
#else
  std::size_t n=0;for(unsigned i=0;i<8;++i)n+=p[i]<key;return n;
#endif
}
struct RankTree {
  std::size_t n=0;
  std::vector<std::size_t> offsets;
  std::vector<std::uint64_t> keys;
  static std::size_t blocks(std::size_t n) { return (n+7)/8; }
  static std::size_t previous(std::size_t n) { return (blocks(n)+8)/9*8; }
  explicit RankTree(std::vector<std::uint64_t> codes):n(codes.size()),keys(std::move(codes)) {
    offsets.push_back(0);
    auto m=n; std::size_t k=0;
    while (m>8) { k+=blocks(m)*8; m=previous(m); offsets.push_back(k); }
    offsets.push_back(k+8); keys.resize(k+8,UINT64_MAX);
    for (std::size_t level=1;level+1<offsets.size();++level)
      for (std::size_t i=0;i<offsets[level+1]-offsets[level];++i) {
        auto child=(i/8)*9+i%8+1;
        for (std::size_t j=1;j<level;++j) child*=9;
        keys[offsets[level]+i]=child*8<n ? keys[child*8] : UINT64_MAX;
      }
  }
  std::size_t lower(std::uint64_t key) const {
    std::size_t k=0;
    for (std::size_t level=offsets.size()-2;level>0;--level)
      k=k*9+count_below8(keys.data()+offsets[level]+k,key)*8;
    return std::min(n,k+count_below8(keys.data()+k,key));
  }
};
struct Interval { std::uint64_t first,last; bool inside; };
inline thread_local std::vector<Interval> scratch;
} // namespace detail

inline constexpr unsigned CHUNK = 8;
inline constexpr const char* KERNEL = "avx512";
inline std::uint64_t morton(Point p) { return detail::spread(p[0]) | detail::spread(p[1])<<1; }

// Fixed experimental configuration: 32-point batches, six refinement levels,
// 512-point stopping threshold, prefix lookup, inside-cell copies and bulk output.
class MortonPlus {
  static constexpr std::size_t W = 32;
  detail::RankTree ranks_;
  std::vector<Point> points_;
  std::vector<std::uint32_t> lut_;
  unsigned lut_level_=1;
 public:
  explicit MortonPlus(std::span<const Point> input):ranks_(std::vector<std::uint64_t>{}) {
    if (input.size()>=UINT32_MAX) throw std::length_error("MortonPlus supports fewer than UINT32_MAX points");
    std::vector<std::pair<std::uint64_t,Point>> sorted; sorted.reserve(input.size());
    for (auto p:input) sorted.emplace_back(morton(p),p);
    std::sort(sorted.begin(),sorted.end(),[](const auto& a,const auto& b){return a.first<b.first;});
    std::vector<std::uint64_t> codes; codes.reserve(input.size()); points_.reserve(input.size());
    for (const auto& [code,p]:sorted) { codes.push_back(code); points_.push_back(p); }
    while (lut_level_<13 && (std::size_t(1)<<(2*lut_level_))*8<codes.size()) ++lut_level_;
    const auto buckets=std::size_t(1)<<(2*lut_level_);
    lut_.assign(buckets+1,std::uint32_t(codes.size()));
    for (std::size_t i=codes.size();i>0;--i) lut_[codes[i-1]>>(64-2*lut_level_)]=std::uint32_t(i-1);
    for (std::size_t b=buckets;b>0;--b) lut_[b-1]=std::min(lut_[b-1],lut_[b]);
    ranks_=detail::RankTree(std::move(codes));
  }
  std::size_t size() const noexcept { return points_.size(); }
  bool empty() const noexcept { return points_.empty(); }
  // Appends exact matches, preserving multiplicity; r2 is the squared radius.
  void query(Point c, Wide r2, Results& out) const { query_impl(c,r2,out); }
  void query_box(Point lo,Point hi,Results& out) const {
    morton_box::refine<2,std::uint64_t>(lo,hi,ranks_.keys,size(),lut_,lut_level_,morton,
        [](auto key,unsigned a){return detail::extract(key>>a);},[&](auto key){return lower(key);},
        [&](std::size_t first,std::size_t last,bool inside){
          if(inside){auto n=out.size();auto* dst=writable(out,last-first);std::memcpy(dst+n,points_.data()+first,(last-first)*sizeof(Point));out.size_=n+last-first;return;}
          out.reserve(out.size()+last-first);
          for(auto i=first;i<last;++i){const auto& p=points_[i];
            if(p[0]>=lo[0]&&p[0]<=hi[0]&&p[1]>=lo[1]&&p[1]<=hi[1])out.push_back(p);
          }
        });
  }
 private:
  template<bool Farthest> static Wide cell_distance(std::uint64_t code,unsigned depth,Point c) {
    Wide distance=0; const auto side=std::uint64_t(1)<<(32-depth);
    for (unsigned a=0;a<2;++a) {
      const std::uint64_t lo=detail::extract(code>>a),hi=lo+side-1;
      const Wide d=Farthest ? std::max(detail::delta(c[a],lo),detail::delta(c[a],hi))
                           : c[a]<lo ? lo-c[a] : c[a]>hi ? c[a]-hi : 0;
      distance+=d*d;
    }
    return distance;
  }
  void descend(std::uint64_t code,unsigned depth,unsigned target,Point c,Wide r2,
               std::vector<detail::Interval>& intervals) const {
    if (cell_distance<false>(code,depth,c)>r2) return;
    auto count=std::numeric_limits<std::size_t>::max();
    if (depth<=lut_level_) {
      const unsigned shift=2*(lut_level_-depth);
      const auto bucket=depth==0 ? 0 : (code>>(64-2*depth))<<shift;
      count=lut_[bucket+(std::size_t(1)<<shift)]-lut_[bucket];
      if (!count) return;
    }
    const auto side=std::uint64_t(1)<<(32-depth);
    const bool inside=Wide(side-1)*(side-1)/2<=r2 && cell_distance<true>(code,depth,c)<=r2;
    const bool small=count<=512 || depth>=lut_level_;
    if (!inside && depth<target && !small) {
      for (std::uint64_t child=0;child<4;++child)
        descend(code | child<<(2*(31-depth)),depth+1,target,c,r2,intervals);
      return;
    }
    const auto last=code | detail::span(depth);
    if (!intervals.empty() && intervals.back().inside==inside && intervals.back().last+1==code)
      intervals.back().last=last;
    else intervals.push_back({code,last,inside});
  }
  std::size_t lower(std::uint64_t key) const {
    const auto bucket=key>>(64-2*lut_level_);
    auto lo=std::size_t(lut_[bucket]); const auto hi=std::size_t(lut_[bucket+1]);
    if (!(key & ((std::uint64_t(1)<<(64-2*lut_level_))-1))) return lo;
    if (hi-lo>16) return ranks_.lower(key);
    while (lo<hi && ranks_.keys[lo]<key) ++lo;
    return lo;
  }
  static Point* writable(Results& out,std::size_t extra) {
    if (extra>std::numeric_limits<std::size_t>::max()-out.size_) throw std::length_error("output overflow");
    out.reserve(out.size_+extra); return out.data_;
  }
  void scan(std::size_t start,std::size_t end,bool inside,Point c,Wide r2,Results& out) const {
    if (start==end) return;
    if (inside) {
      const auto n=out.size(); auto* dst=writable(out,end-start);
      std::memcpy(dst+n,points_.data()+start,(end-start)*sizeof(Point)); out.size_=n+end-start; return;
    }
    const detail::Prepared pr(c,r2);
    auto i=start;
    auto n=out.size(); auto* dst=writable(out,end-start+W);
    for (;end-i>=W;i+=W) {
      detail::Vector v[W/8]; unsigned masks[W/8];
      for (std::size_t j=0;j<W/8;++j) { v[j]=detail::load(points_.data()+i+8*j); masks[j]=detail::mask(v[j],pr); }
      for (std::size_t j=0;j<W/8;++j) n+=detail::emit(v[j],masks[j],dst+n);
    }
    for (;end-i>=8;i+=8) { const auto v=detail::load(points_.data()+i); n+=detail::emit(v,detail::mask(v,pr),dst+n); }
    if (i<end) {
      const unsigned valid=(1u<<unsigned(end-i))-1;
      const auto v=_mm512_maskz_loadu_epi64(valid,points_.data()+i);
      n+=detail::emit(v,detail::mask(v,pr)&valid,dst+n);
    }
    out.size_=n;
  }
  // Keep this boundary: GCC inlining regressed small-output query timings.
  [[gnu::noinline]] void scan_intervals(std::span<const detail::Interval> intervals,Point c,Wide r2,Results& out) const {
    std::size_t cursor=0; const auto n=points_.size();
    for (auto iv:intervals) {
      const auto s=cursor<n && ranks_.keys[cursor]<iv.first ? lower(iv.first) : cursor;
      if (s==n || ranks_.keys[s]>iv.last) { cursor=s; continue; }
      const auto e=iv.last==UINT64_MAX ? n : lower(iv.last+1);
      scan(s,e,iv.inside,c,r2,out); cursor=e;
    }
  }
  void query_impl(Point c,Wide r2,Results& out) const {
    if (points_.empty()) return;
    if (r2>=Wide(2)*UINT32_MAX*UINT32_MAX) { scan(0,size(),true,c,r2,out); return; }
    const double rf=double(std::uint64_t(r2>>64))*18446744073709551616.0+double(std::uint64_t(r2));
    auto r=std::uint64_t(std::ceil(std::sqrt(rf)));
    while (Wide(r)*r<r2) ++r;
    while (r && Wide(r-1)*(r-1)>=r2) --r;
    r=std::min(r,std::uint64_t(UINT32_MAX));
    Point lo{},hi{};
    for (unsigned a=0;a<2;++a) {
      lo[a]=c[a]>r ? std::uint32_t(c[a]-r) : 0;
      hi[a]=std::uint32_t(std::min(std::uint64_t(c[a])+r,std::uint64_t(UINT32_MAX)));
    }
    const unsigned depth=std::countl_zero(std::max(hi[0]-lo[0],hi[1]-lo[1]));
    const auto suffix=detail::span(depth);
    std::array<std::uint64_t,4> cells{morton(lo)&~suffix,morton({hi[0],lo[1]})&~suffix,
      morton({lo[0],hi[1]})&~suffix,morton(hi)&~suffix};
    std::sort(cells.begin(),cells.end());
    const auto target=std::min(depth+6,32u);
    const bool split=depth<target && depth<lut_level_;
    if (!split) {
      std::array<detail::Interval,4> intervals{}; std::size_t n=0;
      for (unsigned i=0;i<4;++i) if ((!i || cells[i]!=cells[i-1]) && cell_distance<false>(cells[i],depth,c)<=r2) {
        const auto last=cells[i]|suffix;
        if (n && intervals[n-1].last+1==cells[i]) intervals[n-1].last=last;
        else intervals[n++]={cells[i],last,false};
      }
      scan_intervals(std::span(intervals.data(),n),c,r2,out); return;
    }
    auto& intervals=detail::scratch; intervals.clear();
    for (unsigned i=0;i<4;++i) if (!i || cells[i]!=cells[i-1]) descend(cells[i],depth,target,c,r2,intervals);
    scan_intervals(intervals,c,r2,out);
  }
};
} // namespace mortonplus
