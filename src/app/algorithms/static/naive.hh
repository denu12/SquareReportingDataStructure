#pragma once
#include "app/algorithms/static/adapter.hh"

namespace diskreport::sfc::app::algorithms::sfc {
template <typename Input_t, typename Key_t = Input_t>
struct Naive {
  using IDXT = Input_t;
  static constexpr bool reports_exact_circle=true;
  using PointType = Point<Input_t>;
  using QueryType = Query<PointType>;
  std::vector<PointType> index;
  Naive(const std::vector<PointType>& points) : index(points) {}
  void query(const QueryType& q, auto&& out) {
    for (auto& n : index) {
      if (::esa_campaign::point_membership(n,q.p,q.q)) {
        *out = n;
      }
    }
  }
};
}  // namespace diskreport::sfc::app::algorithms::sfc