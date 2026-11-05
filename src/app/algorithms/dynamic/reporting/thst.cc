#include <iterator>
#include <map>

#include "THST/QuadTree.h"
#include "THST/RTree.h"
#include "absl/container/btree_set.h"
#include "app/algorithms/dynamic/reporting/adapter.hh"
#include "benchmark/common.hh"
namespace diskreport::app::algorithms::dynamic::sfc {
template <typename Input_t>
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
    explicit operator spatial::BoundingBox<T, 2>() const {
      return spatial::BoundingBox<T, 2>(min, min);
    }
  };
  using PointType = Point<Input_t>;
  using PT = PointType;

  using IDXT = Input_t;
  struct Type {
    size_t query_results = 0;
    void handle_deletion(PT p) { index.remove(p); }
    void handle_insertion(PT p) { index.insert(p); }
    using Tree = spatial::QuadTree<IDXT, PointType, 2>;
    Tree index;
    bool handle_query(std::pair<PT, PT> p) {
      auto [l, h] = p;
      std::vector<PT> out;
      int i = 0;
      index.query(spatial::within<2>(l.min, h.max), std::back_inserter(out));

      query_results += out.size();
      // std::cout << p.first.x << " " << p.first.y << std::endl;
      // std::cout << p.second.x << " " << p.second.y << std::endl;

      // std::cout << out.size() << std::endl;
      return true;
    }
    bool valid() { return true; }
    static constexpr Input_t min[2] = {std::numeric_limits<IDXT>::min(),
                                       std::numeric_limits<IDXT>::min()};
    static constexpr Input_t max[2] = {std::numeric_limits<IDXT>::max(),
                                       std::numeric_limits<IDXT>::max()};
    template <template <typename, typename> class MapType, typename StringType>
    Type(MapType<StringType, double> double_params, MapType<StringType, int64_t> int64_params)
        : index(min, max) {}
    size_t size() { return query_results; }
  };
  static constexpr int offset = 0;
};
template <typename Input_t>
struct THSTR {
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
    explicit operator spatial::BoundingBox<T, 2>() const {
      return spatial::BoundingBox<T, 2>(min, min);
    }
  };
  using PointType = Point<Input_t>;
  using PT = PointType;

  using IDXT = Input_t;
  struct Type {
    size_t query_results = 0;
    void handle_deletion(PT p) { index.remove(p); }
    void handle_insertion(PT p) { index.insert(p); }
    using Tree = spatial::RTree<IDXT, PointType, 2>;
    Tree index;
    bool handle_query(std::pair<PT, PT> p) {
      auto [l, h] = p;
      std::vector<PT> out;
      int i = 0;
      index.query(spatial::within<2>(l.min, h.max), std::back_inserter(out));

      query_results += out.size();
      // std::cout << p.first.x << " " << p.first.y << std::endl;
      // std::cout << p.second.x << " " << p.second.y << std::endl;

      // std::cout << out.size() << std::endl;
      return true;
    }
    bool valid() { return true; }
    template <template <typename, typename> class MapType, typename StringType>
    Type(MapType<StringType, double> double_params, MapType<StringType, int64_t> int64_params) {}
    size_t size() { return query_results; }
  };
  static constexpr int offset = 0;
};
using THSTqq = DynSFCInMemory<THSTQuad, uint32_t>;
using THSTrr = DynSFCInMemory<THSTR, uint32_t>;
REGISTER_IMPL_NAMED(THSTqq, "thst_quad");
REGISTER_IMPL_NAMED(THSTrr, "thst_rtree");

}  // namespace diskreport::app::algorithms::dynamic::sfc