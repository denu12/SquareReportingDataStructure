#pragma once
// Local reporting adaptations, not upstream radius-query implementations.
// Provenance and changes: MANUAL-ADAPTATIONS.md.
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include <numeric>
#include <random>
#include <sfcnn.hpp>

namespace diskreport::ddim {
template<std::size_t D> struct BallBounds {
  using Point = std::array<std::uint32_t, D>;
  using Wide = unsigned __int128;
  Point center, lo{}, hi{};
  Wide radius2;
  BallBounds(Point c, Wide r2) : center(c), radius2(r2) {
    std::uint64_t low = 0, high = UINT64_MAX;
    if (r2 > Wide(high)*high) throw std::invalid_argument("Squared radius too large");
    while (low < high) {
      auto mid = low + (high-low)/2;
      if (Wide(mid)*mid < r2) low = mid+1;
      else high = mid;
    }
    for (std::size_t a=0; a<D; ++a) {
      lo[a] = low > c[a] ? 0 : c[a]-low;
      hi[a] = low > UINT32_MAX-c[a] ? UINT32_MAX : c[a]+low;
    }
  }
  template<class P> bool contains(const P& p) const {
    Wide remaining = radius2;
    for (std::size_t a=0; a<D; ++a) {
      auto delta = std::int64_t(p[a])-center[a];
      auto magnitude = std::uint64_t(delta<0 ? -delta : delta);
      Wide term = Wide(magnitude)*magnitude;
      if (term > remaining) return false;
      remaining -= term;
    }
    return true;
  }
};

// Generalization of src/app/algorithms/sfc/chan_sss.cc, derived from
// Timothy M. Chan's SSS method. The original solves approximate NN.
template<std::size_t D> class ChanManuallyAdapted {
 public:
  using Point = std::array<std::uint32_t, D>;
  using WidePoint = std::array<std::uint64_t,D>;
  explicit ChanManuallyAdapted(const std::vector<Point>& points) {
    shift_ = static_cast<std::uint32_t>(std::mt19937_64(12121)());
    for (std::size_t i=0; i<points.size(); ++i) entries_.push_back({points[i],i});
    std::sort(entries_.begin(), entries_.end(), [&](const auto& a,const auto& b) {
      return less(shifted(a.point),shifted(b.point));
    });
  }
  template<class Emit> void ball_squared(const Point& c, unsigned __int128 r2, Emit&& emit) const {
    BallBounds<D> ball(c,r2);
    search(0,entries_.size(),shifted(ball.lo),shifted(ball.hi),ball,emit);
  }
 private:
  struct Entry { Point point; std::size_t id; };
  std::vector<Entry> entries_;
  std::uint64_t shift_;
  WidePoint shifted(const Point& p) const {
    WidePoint result{};
    for (std::size_t a=0; a<D; ++a) result[a]=std::uint64_t(p[a])+shift_;
    return result;
  }
  static bool less(const WidePoint& p,const WidePoint& q) {
    std::size_t axis=0; std::uint64_t most=0;
    for (std::size_t a=0; a<D; ++a) {
      auto diff=p[a]^q[a];
      if (most<diff && most<(most^diff)) {most=diff;axis=a;}
    }
    return p[axis]<q[axis];
  }
  template<class Emit> void search(std::size_t first,std::size_t last,
      const WidePoint& lo,const WidePoint& hi,const BallBounds<D>& ball,Emit& emit) const {
    if (first==last) return;
    auto a=shifted(entries_[first].point), b=shifted(entries_[last-1].point);
    std::uint64_t diff=0;
    for (std::size_t i=0;i<D;++i) diff |= a[i]^b[i];
    auto mask=(std::uint64_t{1}<<std::bit_width(diff))-1;
    for (std::size_t i=0;i<D;++i) {
      auto low=a[i]&~mask;
      if (hi[i]<low || lo[i]>(low|mask)) return;
    }
    auto mid=first+(last-first)/2;
    const auto& entry=entries_[mid];
    auto p=shifted(entry.point);
    if (ball.contains(entry.point)) emit(entry.id);
    if (!less(p,lo)) search(first,mid,lo,hi,ball,emit);
    if (!less(hi,p)) search(mid+1,last,lo,hi,ball,emit);
  }
};

// Reuses the existing manually modified STANN reporting traversal unchanged.
template<std::size_t D> class StannManuallyAdapted {
 public:
  using Point = std::array<std::uint32_t, D>;
  struct NativePoint : reviver::dpoint<unsigned int,D> { std::size_t id=0; };
  explicit StannManuallyAdapted(const std::vector<Point>& points) {
    std::vector<NativePoint> input(points.size());
    for (std::size_t i=0;i<points.size();++i) {
      for (std::size_t a=0;a<D;++a) input[i][a]=points[i][a];
      input[i].id=i;
    }
    index_.init_reporting(std::move(input));
  }
  template<class Emit> void ball_squared(const Point& c,unsigned __int128 r2,Emit&& emit) {
    BallBounds<D> ball(c,r2);
    NativePoint lo,hi;
    for (std::size_t a=0;a<D;++a) {lo[a]=ball.lo[a];hi[a]=ball.hi[a];}
    index_.box_report(lo,hi,[&](const NativePoint& p) {
      if (ball.contains(p)) emit(p.id);
    });
  }
 private:
  sfcnn_work<NativePoint> index_;
};
} // namespace diskreport::ddim
