#include <boost/geometry.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/index/rtree.hpp>
#include <iterator>
#include <vector>

#include "app/algorithms/dynamic/reporting/adapter.hh"
namespace diskreport::app::algorithms::dynamic::sfc {
template <typename Input_t, template <typename> typename IDX>
struct BoostDyn {
  using PointType = boost::geometry::model::d2::point_xy<Input_t>;
  using PT = PointType;
  using QueryType = boost::geometry::model::box<PointType>;

  using IDXT = Input_t;
  struct Type {
    size_t query_results = 0;
    void handle_deletion(PT p) { index.remove(p); }
    void handle_insertion(PT p) { index.insert(p); }
    IDX<PointType> index;
    void esa_query(PT lo,PT hi,std::vector<PT>& out) { index.query(boost::geometry::index::covered_by(QueryType(lo,hi)),std::back_inserter(out)); }
    bool handle_query(std::pair<PT, PT> p) {
      std::vector<PT> out;
      QueryType q(p.first, p.second);

      index.query(boost::geometry::index::covered_by(q), std::back_inserter(out));

      query_results += out.size();
      return true;
    }
    bool valid() { return true; }
    template <template <typename, typename> class MapType, typename StringType>
    Type(MapType<StringType, double> double_params, MapType<StringType, int64_t> int64_params) {
      // index.insert(PointType(10, 10));
      // index.insert(PointType(10, 10));
      // index.insert(PointType(10, 10));
      // index.remove(PointType(10, 10));

      // std::vector<PT> out;
      // QueryType q(PointType(0, 0), PointType(20, 20));

      // index.query(boost::geometry::index::covered_by(q), std::back_inserter(out));
      // for (const auto& a : out) {
      //   std::cout << a.x() << " " << a.y() << std::endl;
      // }
    }
    size_t size() { return query_results; }
  };
  static constexpr int offset = 0;
};

template <typename PT>
using RTreeLinearPack = boost::geometry::index::rtree<PT, boost::geometry::index::linear<16>>;
template <typename PT>
using RTreeQuadraticPack = boost::geometry::index::rtree<PT, boost::geometry::index::quadratic<16>>;
template <typename PT>
using RTreeStarPack = boost::geometry::index::rtree<PT, boost::geometry::index::rstar<16>>;
template <typename T>
using BoostDynLin = BoostDyn<T, RTreeLinearPack>;
template <typename T>
using BoostDynQuad = BoostDyn<T, RTreeQuadraticPack>;

template <typename T>
using BoostDynRstar = BoostDyn<T, RTreeStarPack>;
using BoostDynI = DynSFCInMemory<BoostDynLin, uint32_t>;
using BoostDynStarI = DynSFCInMemory<BoostDynRstar, uint32_t>;
using BoostDynQuadI = DynSFCInMemory<BoostDynQuad, uint32_t>;

REGISTER_IMPL_NAMED(BoostDynI, "boost_lin_dyn");
REGISTER_IMPL_NAMED(BoostDynQuadI, "boost_quad_dyn");
REGISTER_IMPL_NAMED(BoostDynStarI, "boost_star_dyn");

}  // namespace diskreport::app::algorithms::dynamic::sfc