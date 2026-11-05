#pragma once
#include <immintrin.h>

#include <execution>
#include "benchmark/circle.hh"
namespace diskreport::sfc {
#ifdef PARALLEL
constexpr auto policy = std::execution::par;
#else
constexpr auto policy = std::execution::seq;
#endif

template <typename T>
struct Point {
  using ElementType = T;
  T x;
  T y;
  bool operator<(const Point& b) { return x < b.x || (x == b.x && y < b.y); }
};
template <typename PT>
struct Query {
  using PointType = PT;
  PT p, q;
  // double d;
  Query(PointType p, PointType q) : p(p), q(q) {}
  Query(typename PointType::ElementType x, typename PointType::ElementType y,
        typename PointType::ElementType u, typename PointType::ElementType v)
      : p(x, y), q(u, v){};
};
}  // namespace diskreport::sfc