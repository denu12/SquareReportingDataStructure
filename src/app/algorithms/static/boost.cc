#include <boost/geometry.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/index/rtree.hpp>

#include "app/algorithms/static/adapter.hh"
namespace diskreport::sfc::app::algorithms::sfc {
template <template <typename> typename IDX, typename Input_t, typename Key_t = Input_t>
struct Boost {
  static constexpr bool reports_exact_circle = true;
  using IDXT = Input_t;
  using PointType = boost::geometry::model::d2::point_xy<Input_t>;
  using QueryType = boost::geometry::model::box<PointType>;
  IDX<PointType> index;
  Boost(const std::vector<PointType>& points) : index(points.begin(), points.end()) {}
  void query(const QueryType& q, auto&& out) {
    namespace bgi = boost::geometry::index;
    if (const auto* circle = ::esa_campaign::active_circle) {
      // Keep native rectangle pruning; test the exact disk before output.
      index.query(bgi::covered_by(q) && bgi::satisfies([circle](const PointType& p) {
        return circle->contains(p.x(), p.y());
      }), out);
    } else {
      index.query(bgi::covered_by(q), out);
    }
  }
};
template <typename PT>
using RTreeLinearPack = boost::geometry::index::rtree<PT, boost::geometry::index::linear<16>>;
template <typename PT>
using RTreeQuadraticPack = boost::geometry::index::rtree<PT, boost::geometry::index::quadratic<16>>;
template <typename PT>
using RTreeStarPack = boost::geometry::index::rtree<PT, boost::geometry::index::rstar<16>>;
using BoostAlgoLin = Algo<Boost<RTreeLinearPack, uint32_t>>;
using BoostAlgoQuad = Algo<Boost<RTreeQuadraticPack, uint32_t>>;
using BoostAlgoStar = Algo<Boost<RTreeStarPack, uint32_t>>;
using BoostAlgoLin16 = Algo<Boost<RTreeLinearPack, uint16_t>>;
using BoostAlgoQuad16 = Algo<Boost<RTreeQuadraticPack, uint16_t>>;
using BoostAlgoStar16 = Algo<Boost<RTreeStarPack, uint16_t>>;
REGISTER_IMPL_NAMED(BoostAlgoLin, "boost_lin");
REGISTER_IMPL_NAMED(BoostAlgoQuad, "boost_quad");
REGISTER_IMPL_NAMED(BoostAlgoStar, "boost_star");
REGISTER_IMPL_NAMED(BoostAlgoLin16, "boost_lin16");
REGISTER_IMPL_NAMED(BoostAlgoQuad16, "boost_quad16");
REGISTER_IMPL_NAMED(BoostAlgoStar16, "boost_star16");

}  // namespace diskreport::sfc::app::algorithms::sfc