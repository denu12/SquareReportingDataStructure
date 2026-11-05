#include <sys/types.h>

#include <limits>
#include <vector>

#include "THST/QuadTree.h"
#include "THST/RTree.h"
#include "app/algorithms/algorithm_impl.h"
#include "app/algorithms/static/adapter.hh"
namespace diskreport::sfc::app::algorithms::sfc {
template <typename Input_t, typename Key_t = Input_t>
struct THSTQuad {
  template <typename T>
  struct Point {
    using ElementType = T;
    union {
      T min[2];
      T max[2];
      struct {
        T x, y;
      };
    };
    Point() {}
    Point(T _x, T _y) : x(_x), y(_y) {}
    bool operator==(const Point& b) const { return min[0] == b.min[0] && min[1] == b.min[1]; }
    bool operator<(const Point& b) { return x < b.x || (x == b.x && y < b.y); }

    explicit operator spatial::BoundingBox<T, 2>() const {
      return spatial::BoundingBox<T, 2>(min, min);
    }
  };
  using IDXT = Input_t;
  using PointType = Point<Input_t>;
  using QueryType = Query<PointType>;
  using Tree = spatial::QuadTree<IDXT, PointType, 2>;
  Tree tree;
  static constexpr IDXT min[2] = {std::numeric_limits<IDXT>::min(),
                                  std::numeric_limits<IDXT>::min()};
  static constexpr IDXT max[2] = {std::numeric_limits<IDXT>::max(),
                                  std::numeric_limits<IDXT>::max()};
  THSTQuad(const std::vector<PointType>& points) : tree(min, max, points.begin(), points.end()) {}
  void query(const QueryType& q, auto&& out) {
    tree.query(spatial::within<2>(q.p.min, q.q.min), out);
  }
};
template <typename Input_t, typename Key_t = Input_t>
struct THSTRTree {
  template <typename T>
  struct Point {
    using ElementType = T;
    union {
      T min[2];
      T max[2];
      struct {
        T x, y;
      };
    };
    Point() {}
    Point(T _x, T _y) : x(_x), y(_y) {}
    bool operator==(const Point& b) const { return min[0] == b.min[0] && min[1] == b.min[1]; }
    bool operator<(const Point& b) { return x < b.x || (x == b.x && y < b.y); }

    explicit operator spatial::BoundingBox<T, 2>() const {
      return spatial::BoundingBox<T, 2>(min, min);
    }
  };
  using IDXT = Input_t;
  using PointType = Point<Input_t>;
  using QueryType = Query<PointType>;
  using Tree = spatial::RTree<IDXT, PointType, 2>;
  Tree tree;
  THSTRTree(const std::vector<PointType>& points) : tree(points.begin(), points.end()) {}
  void query(const QueryType& q, auto&& out) {
    tree.query(spatial::within<2>(q.p.min, q.q.min), out);
  }
};
using THSTQuadImpl = Algo<THSTQuad<u_int32_t>>;
using THSTRTreeImpl = Algo<THSTRTree<u_int32_t>>;

REGISTER_IMPL_NAMED(THSTQuadImpl, "thst_quad");
REGISTER_IMPL_NAMED(THSTRTreeImpl, "thst_rtree");

}  // namespace diskreport::sfc::app::algorithms::sfc