#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include "benchmark/circle.hh"
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>
#include "ANN/ANN.h"
#include "benchmark/common.hh"
#ifdef ANN_USE_LINF
#error "AnnFR requires Euclidean ANN."
#endif

namespace diskreport::sfc::app::algorithms::sfc {
// ANN's unmodified exact fixed-radius API, configured for Euclidean circle reporting.
// See third_party/ann/NOTICE.txt for metric, two-pass costs, and provenance.
class AnnFR {
 public:
  using IDXT = uint32_t;
  using PointType = Point<IDXT>;
  using QueryType = Query<PointType>;

  explicit AnnFR(const std::vector<PointType>& points) {
    if (points.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
      throw std::length_error("ANN uses signed int point counts");
    coords_.reserve(points.size());
    for (auto p : points) coords_.push_back({double(p.x), double(p.y)});
    rows_.reserve(points.size());
    for (auto& p : coords_) rows_.push_back(p.data());
    if (!points.empty())
      tree_ = std::make_unique<ANNkd_tree>(rows_.data(), int(points.size()),
                                         2, 1, ANN_KD_SL_MIDPT);
  }

  template <class Output> void query(const QueryType& q, Output out) const {
    if (!tree_ || q.p.x > q.q.x || q.p.y > q.q.y) return;
    ANNcoord center[2] = {(double(q.p.x) + double(q.q.x)) * 0.5,
                          (double(q.p.y) + double(q.q.y)) * 0.5};
    double dx=(double(q.q.x)-q.p.x)*0.5,dy=(double(q.q.y)-q.p.y)*0.5;
    ANNdist radius=dx*dx+dy*dy;
    if(auto c=::esa_campaign::active_circle){center[0]=c->x;center[1]=c->y;radius=double(c->radius2());}
    radius=std::nextafter(radius,std::numeric_limits<double>::infinity());
    const int count = tree_->annkFRSearch(center, radius, 0, nullptr, nullptr, 0.0);
    if (count == 0) return;
    indices_.resize(count);
    tree_->annkFRSearch(center, radius, count, indices_.data(), nullptr, 0.0);
    for (int i : indices_) {
      const auto& p = coords_[i];
      if (::esa_campaign::active_circle || (p[0] >= q.p.x && p[0] <= q.q.x && p[1] >= q.p.y && p[1] <= q.q.y))
        *out++ = PointType{static_cast<IDXT>(p[0]), static_cast<IDXT>(p[1])};
    }
  }

 private:
  std::vector<std::array<ANNcoord, 2>> coords_;
  std::vector<ANNpoint> rows_;
  // ANNkd_tree retains pointers into coords_ and rows_; destroy it first.
  mutable std::vector<ANNidx> indices_;
  std::unique_ptr<ANNkd_tree> tree_;
};
}  // namespace diskreport::sfc::app::algorithms::sfc
