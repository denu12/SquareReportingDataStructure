// Standalone C++20 3D extension of the ESA authors' MortonPlus.
// MortonPlus design / Rust source: Dennis Kobert and the ESA authors.
// Local 3D extension: 96-bit Morton keys, octree refinement and exact 66-bit distances.
// Requires GCC/Clang unsigned __int128 and AVX-512F and BMI2 (-mavx512f -mbmi2).
// License notice: see the accompanying LICENSE file.

#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
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
namespace mortonplus3d {
using Wide = unsigned __int128;
using Point = std::array<std::uint32_t,3>;
static_assert(sizeof(Point)==12);
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
inline std::uint64_t spread(std::uint64_t x) {
  x &= 0x1fffff;
  x=(x|x<<32)&0x1f00000000ffffULL;
  x=(x|x<<16)&0x1f0000ff0000ffULL;
  x=(x|x<<8)&0x100f00f00f00f00fULL;
  x=(x|x<<4)&0x10c30c30c30c30c3ULL;
  return (x|x<<2)&0x1249249249249249ULL;
}
inline std::uint32_t compact(std::uint64_t x) {
  x &= 0x1249249249249249ULL;
  x=(x^(x>>2))&0x10c30c30c30c30c3ULL;
  x=(x^(x>>4))&0x100f00f00f00f00fULL;
  x=(x^(x>>8))&0x1f0000ff0000ffULL;
  x=(x^(x>>16))&0x1f00000000ffffULL;
  return std::uint32_t((x^(x>>32))&0x1fffff);
}
inline Wide code(Point p) {
  Wide result=0;
  for(unsigned a=0;a<3;++a)
    result|=(Wide(spread(p[a])) | Wide(spread(p[a]>>21))<<63)<<a;
  return result;
}
inline std::uint32_t coordinate(Wide key,unsigned a) {
  return compact(std::uint64_t(key>>a)) | compact(std::uint64_t(key>>(63+a)))<<21;
}
inline Wide suffix(unsigned depth) { return (Wide(1)<<(3*(32-depth)))-1; }
inline std::uint64_t delta(std::uint64_t a,std::uint64_t b) { return a>b?a-b:b-a; }
inline Wide distance(Point p,Point c) {
  Wide d=0;for(unsigned a=0;a<3;++a){const Wide x=delta(p[a],c[a]);d+=x*x;}return d;
}
// Eight xyz triples occupy 24 uint32 words. Masked loads also handle tails.
struct Batch8 {
  __m512i first, second;
  unsigned count;
  Batch8(const Point* p, unsigned n) : count(n) {
    const auto words = n * 3;
    first = _mm512_maskz_loadu_epi32((1u << std::min(words,16u))-1, p);
    second = words > 16 ? _mm512_maskz_loadu_epi32((1u << (words-16))-1,
        reinterpret_cast<const char*>(p)+64) : _mm512_setzero_si512();
  }
  unsigned mask(Point c, Wide r2) const {
    auto sum = _mm512_setzero_si512(), high = sum;
    const auto one = _mm512_set1_epi64(1);
    for (unsigned a=0; a<3; ++a) {
      const auto indices = _mm512_setr_epi32(a,a+3,a+6,a+9,a+12,a+15,a+18,a+21,0,0,0,0,0,0,0,0);
      const auto axis = _mm512_permutex2var_epi32(first,indices,second);
      const auto p = _mm512_cvtepu32_epi64(_mm512_castsi512_si256(axis));
      const auto center = _mm512_set1_epi64(c[a]);
      const auto d = _mm512_sub_epi64(_mm512_max_epu64(p,center),_mm512_min_epu64(p,center));
      const auto next = _mm512_add_epi64(sum,_mm512_mul_epu32(d,d));
      high = _mm512_mask_add_epi64(high,_mm512_cmplt_epu64_mask(next,sum),high,one);sum=next;
    }
    const auto hi = _mm512_set1_epi64(uint64_t(r2>>64)), lo = _mm512_set1_epi64(uint64_t(r2));
    return ((1u<<count)-1) & (_mm512_cmplt_epu64_mask(high,hi) |
        (_mm512_cmpeq_epi64_mask(high,hi) & _mm512_cmple_epu64_mask(sum,lo)));
  }
  // Expand every accepted point bit to its three coordinate bits; preserve xyz order.
  // Caller reserves eleven Points: the two full stores may write scratch space.
  unsigned append(Point* out, unsigned accepted) const {
    if (!accepted) return 0;
    const unsigned words = _pdep_u32(accepted,0x249249u) * 7;
    const unsigned first_words = std::popcount(words & 0xffffu);
    _mm512_storeu_si512(out,_mm512_maskz_compress_epi32(words & 0xffffu,first));
    _mm512_storeu_si512(reinterpret_cast<char*>(out)+4*first_words,
                       _mm512_maskz_compress_epi32(words>>16,second));
    return std::popcount(accepted);
  }
};
struct RankTree {
  std::size_t n;
  std::vector<std::size_t> offsets{0};
  std::vector<Wide> keys;
  explicit RankTree(std::vector<Wide> codes):n(codes.size()),keys(std::move(codes)) {
    auto m=n;std::size_t k=0;
    while(m>8){k+=(m+7)/8*8;m=((m+7)/8+8)/9*8;offsets.push_back(k);}
    offsets.push_back(k+8);keys.resize(k+8,~Wide(0));
    for(std::size_t h=1;h+1<offsets.size();++h)
      for(std::size_t i=0;i<offsets[h+1]-offsets[h];++i){
        auto child=i/8*9+i%8+1;for(std::size_t j=1;j<h;++j)child*=9;
        keys[offsets[h]+i]=child*8<n?keys[child*8]:~Wide(0);
      }
  }
  std::size_t count(std::size_t offset,Wide key) const {
#if defined(__AVX512F__)
    const auto a=_mm512_loadu_si512(keys.data()+offset),b=_mm512_loadu_si512(keys.data()+offset+4);
    const auto lo=_mm512_permutex2var_epi64(a,_mm512_setr_epi64(0,2,4,6,8,10,12,14),b);
    const auto hi=_mm512_permutex2var_epi64(a,_mm512_setr_epi64(1,3,5,7,9,11,13,15),b);
    const auto wh=_mm512_set1_epi64(uint64_t(key>>64)),wl=_mm512_set1_epi64(uint64_t(key));
    return std::popcount(unsigned(_mm512_cmplt_epu64_mask(hi,wh) |
        (_mm512_cmpeq_epi64_mask(hi,wh)&_mm512_cmplt_epu64_mask(lo,wl))));
#else
    std::size_t n=0;for(unsigned i=0;i<8;++i)n+=keys[offset+i]<key;return n;
#endif
  }
  std::size_t lower(Wide key) const {
    std::size_t k=0;for(std::size_t h=offsets.size()-2;h>0;--h)k=k*9+count(offsets[h]+k,key)*8;
    return std::min(n,k+count(k,key));
  }
};
struct Interval { Wide first,last;bool inside; };
inline thread_local std::vector<Interval> scratch;
} // namespace detail

// Fixed MortonPlus defaults, extended from quadtree cells to octree cells.
class MortonPlus {
  detail::RankTree ranks_;
  std::vector<Point> points_;
  std::vector<std::uint32_t> lut_;
  unsigned lut_level_=1;
  template<bool Far> static Wide cell_distance(Wide code,unsigned depth,Point c) {
    Wide sum=0;const auto side=std::uint64_t(1)<<(32-depth);
    for(unsigned a=0;a<3;++a){
      const std::uint64_t lo=detail::coordinate(code,a),hi=lo+side-1;
      const Wide d=Far?std::max(detail::delta(c[a],lo),detail::delta(c[a],hi))
                      :c[a]<lo?lo-c[a]:c[a]>hi?c[a]-hi:0;
      sum+=d*d;
    }return sum;
  }
  void descend(Wide code,unsigned depth,unsigned target,Point c,Wide r2,std::vector<detail::Interval>& ranges) const {
    if(cell_distance<false>(code,depth,c)>r2)return;
    std::size_t count=SIZE_MAX;
    if(depth<=lut_level_){
      const unsigned shift=3*(lut_level_-depth);
      const auto bucket=depth==0?0:std::size_t(code>>(96-3*depth))<<shift;
      count=lut_[bucket+(std::size_t(1)<<shift)]-lut_[bucket];if(!count)return;
    }
    const auto side=std::uint64_t(1)<<(32-depth);
    const bool inside=Wide(3)*(side-1)*(side-1)/4<=r2 && cell_distance<true>(code,depth,c)<=r2;
    if(!inside && depth<target && count>512 && depth<lut_level_){
      for(unsigned child=0;child<8;++child)descend(code|Wide(child)<<(3*(31-depth)),depth+1,target,c,r2,ranges);
      return;
    }
    const auto last=code|detail::suffix(depth);
    if(!ranges.empty() && ranges.back().inside==inside && ranges.back().last+1==code)ranges.back().last=last;
    else ranges.push_back({code,last,inside});
  }
  std::size_t lower(Wide key) const {
    const auto bucket=std::size_t(key>>(96-3*lut_level_));
    auto lo=std::size_t(lut_[bucket]);const auto hi=std::size_t(lut_[bucket+1]);
    if(!(key&detail::suffix(lut_level_)))return lo;
    if(hi-lo>16)return ranks_.lower(key);
    while(lo<hi && ranks_.keys[lo]<key)++lo;return lo;
  }
  void scan(std::size_t first,std::size_t last,bool inside,Point c,Wide r2,Results& out) const {
    if(first==last)return;
    const auto extra=last-first+11;
    if(extra>SIZE_MAX-out.size_)throw std::length_error("output overflow");
    out.reserve(out.size_+extra);auto n=out.size_;
    if(inside){std::memcpy(out.data_+n,points_.data()+first,(last-first)*sizeof(Point));out.size_+=last-first;return;}
    for(auto i=first;i<last;i+=8) {
      detail::Batch8 batch(points_.data()+i,unsigned(std::min<std::size_t>(8,last-i)));
      n+=batch.append(out.data_+n,batch.mask(c,r2));
    }
    out.size_=n;
  }

  [[gnu::noinline]] void scan_intervals(std::span<const detail::Interval> ranges,Point c,Wide r2,Results& out) const {
    std::size_t cursor=0;
    for(auto iv:ranges){
      const auto s=cursor<size() && ranks_.keys[cursor]<iv.first?lower(iv.first):cursor;
      if(s==size() || ranks_.keys[s]>iv.last){cursor=s;continue;}
      const auto e=iv.last==detail::suffix(0)?size():lower(iv.last+1);
      scan(s,e,iv.inside,c,r2,out);cursor=e;
    }
  }
 public:
  explicit MortonPlus(std::span<const Point> input):ranks_(std::vector<Wide>{}) {
    if(input.size()>=UINT32_MAX)throw std::length_error("MortonPlus3D requires fewer than UINT32_MAX points");
    std::vector<std::pair<Wide,Point>> sorted;sorted.reserve(input.size());
    for(auto p:input)sorted.emplace_back(detail::code(p),p);
    std::sort(sorted.begin(),sorted.end(),[](const auto& a,const auto& b){return a.first<b.first;});
    std::vector<Wide> codes;codes.reserve(input.size());points_.reserve(input.size());
    for(auto [key,p]:sorted){codes.push_back(key);points_.push_back(p);}
    while(lut_level_<8 && (std::size_t(1)<<(3*lut_level_))*8<codes.size())++lut_level_;
    const auto buckets=std::size_t(1)<<(3*lut_level_);lut_.assign(buckets+1,std::uint32_t(codes.size()));
    for(auto i=codes.size();i>0;--i)lut_[std::size_t(codes[i-1]>>(96-3*lut_level_))]=std::uint32_t(i-1);
    for(auto i=buckets;i>0;--i)lut_[i-1]=std::min(lut_[i-1],lut_[i]);
    ranks_=detail::RankTree(std::move(codes));
  }
  std::size_t size() const noexcept{return points_.size();}
  bool empty() const noexcept{return points_.empty();}
  void query_box(Point lo,Point hi,Results& out) const {
    morton_box::refine<3,Wide>(lo,hi,ranks_.keys,size(),lut_,lut_level_,detail::code,
        [](auto key,unsigned a){return detail::coordinate(key,a);},[&](auto key){return lower(key);},
        [&](std::size_t first,std::size_t last,bool inside){
          out.reserve(out.size_+last-first);
          if(inside){std::memcpy(out.data_+out.size_,points_.data()+first,(last-first)*sizeof(Point));out.size_+=last-first;return;}
          for(auto i=first;i<last;++i){const auto& p=points_[i];
            if(p[0]>=lo[0]&&p[0]<=hi[0]&&p[1]>=lo[1]&&p[1]<=hi[1]&&p[2]>=lo[2]&&p[2]<=hi[2])
              out.data_[out.size_++]=p;
          }
        });
  }
  // Append original integer triples; duplicates retained. Radius is squared.
  void query(Point c,Wide r2,Results& out) const {
    if(empty())return;
    if(r2>=Wide(3)*UINT32_MAX*UINT32_MAX){scan(0,size(),true,c,r2,out);return;}
    auto r=std::uint64_t(std::ceil(std::sqrt(static_cast<double>(r2))));
    while(Wide(r)*r<r2)++r;while(r && Wide(r-1)*(r-1)>=r2)--r;
    r=std::min(r,std::uint64_t(UINT32_MAX));Point lo{},hi{};std::uint32_t width=0;
    for(unsigned a=0;a<3;++a){lo[a]=c[a]>r?std::uint32_t(c[a]-r):0;hi[a]=std::uint32_t(std::min(std::uint64_t(c[a])+r,std::uint64_t(UINT32_MAX)));width=std::max(width,hi[a]-lo[a]);}
    const unsigned depth=std::countl_zero(width);const auto suffix=detail::suffix(depth);
    std::array<Wide,8> cells;
    for(unsigned i=0;i<8;++i)cells[i]=detail::code({i&1?hi[0]:lo[0],i&2?hi[1]:lo[1],i&4?hi[2]:lo[2]})&~suffix;
    std::sort(cells.begin(),cells.end());const auto target=std::min(depth+6,32u);
    if(depth>=lut_level_ || depth==target){
      std::array<detail::Interval,8> ranges{};std::size_t n=0;
      for(unsigned i=0;i<8;++i)if((!i || cells[i]!=cells[i-1]) && cell_distance<false>(cells[i],depth,c)<=r2){
        const auto last=cells[i]|suffix;
        if(n && ranges[n-1].last+1==cells[i])ranges[n-1].last=last;else ranges[n++]={cells[i],last,false};
      }scan_intervals({ranges.data(),n},c,r2,out);return;
    }
    auto& ranges=detail::scratch;ranges.clear();
    for(unsigned i=0;i<8;++i)if(!i || cells[i]!=cells[i-1])descend(cells[i],depth,target,c,r2,ranges);
    scan_intervals(ranges,c,r2,out);
  }
};
} // namespace mortonplus3d
