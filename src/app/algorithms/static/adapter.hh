#pragma once
#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>
#include <random>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "app/algorithms/algorithm_impl.h"
#include "app/algorithms/two_phase_algorithm_impl.h"
#include "app/app_io.pb.h"
#include "benchmark/common.hh"
#include "benchmark/esa_campaign.hh"
namespace diskreport::sfc::app::algorithms::sfc {
template <class VecType>
struct Shell {
  bool valid() { return true; }
  size_t size() { return overall; }
  VecType result;
  size_t overall = 0;
  Shell(const size_t& vec) : overall(vec) {}
};
template <typename T, typename = std::void_t<>>
struct has_xfunc : std::false_type {};

template <typename T>
struct has_xfunc<T, std::void_t<decltype(std::declval<T>().x())>> : std::true_type {};

template <typename T, typename = std::void_t<>>
struct has_yvalue : std::false_type {};

template <typename T>
struct has_yvalue<T, std::void_t<decltype(std::declval<T>().y)>> : std::true_type {};

template <typename T>
inline constexpr bool has_func_v = has_xfunc<T>::value;
// A. Specialization for types that HAVE the function (Tag = std::true_type)
template <typename T>
void impl_perform_action(T& obj, std::true_type) {
  std::cout << obj.x() << " " << obj.y() << std::endl;
}

// B. Specialization for types that DO NOT have the function (Tag = std::false_type)
template <typename T>
void impl_perform_action(T& obj, std::false_type) {
  std::cout << obj.x << " " << obj.y << std::endl;

  // This is the fallback logic (e.g., do nothing, log a warning, etc.)
}

template <typename T>
void print(T& obj) {
  // 1. Get the tag type (std::true_type or std::false_type)
  using Tag = has_xfunc<T>;

  // 2. Call the private implementation, passing the tag
  impl_perform_action(obj, Tag{});
}

template <typename T1>
bool comp1(T1 a, T1 b) {
  if constexpr (has_xfunc<T1>::value) {
    // Only compiled if has_xfunc is true
    return a.x() < b.x() || ((a.x() == b.x()) && (a.y() < b.y()));
  } else if constexpr (has_yvalue<T1>::value) {
    // Only compiled if has_xfunc is false
    return a.x < b.x || ((a.x == b.x) && (a.y < b.y));
  } else {
    return a.x[0] < b.x[0] || ((a.x[0] == b.x[0]) && a.x[1] < b.x[1]);
  }
}
template <typename T1>
bool comp_equal(T1 a, T1 b) {
  if constexpr (has_xfunc<T1>::value) {
    // Only compiled if has_xfunc is true
    return a.x() == b.x() && (a.y() == b.y());
  } else if constexpr (has_yvalue<T1>::value) {
    return a.x == b.x && (a.y == b.y);
  } else {
    return a.x[0] == b.x[0] && a.x[1] == b.x[1];
  }
}
template <typename T1, typename T2>
bool contained(T1 px, T2 l, T2 h) {
  if constexpr (has_xfunc<T1>::value) {
    // Only compiled if has_xfunc is true
    return (px.x() >= l.x()) & (px.y() >= l.y()) & (px.x() <= h.x()) & (px.y() <= h.y());
  } else if constexpr (has_yvalue<T1>::value) {
    // Only compiled if has_xfunc is false
    return (px.x >= l.x) & (px.y >= l.y) & (px.x <= h.x) & (px.y <= h.y);
  } else {
    return (px.x[0] >= l.x[0]) & (px.x[1] >= l.x[0]) & (px.x[0] <= h.x[0]) & (px.x[1] <= h.x[1]);
  }
}
template <class DataStructure, int add = 0>
struct Algo : public ::diskreport::app::algorithms::TwoPhaseAlgorithmImpl<
                  std::vector<typename DataStructure::PointType>,
                  std::vector<typename DataStructure::QueryType>, DataStructure,
                  Shell<std::vector<typename DataStructure::PointType>>> {
  template<class Concrete> static void RegisterEsa(const std::string& name) {
    if constexpr(sizeof(typename DataStructure::IDXT)==4) {
      if(!::esa_campaign::selected(name))return;
      ::esa_campaign::registry()[name]=[name](const ::esa_campaign::Dataset& d,bool verify){
        if(!d.events.empty())throw std::runtime_error("Static entry cannot execute dynamic operations");
        Concrete adapter;
        if constexpr(requires { DataStructure::prepare_runtime(); })DataStructure::prepare_runtime();
        auto start=std::chrono::steady_clock::now();
        std::vector<typename DataStructure::PointType> points;points.reserve(d.points.size());
        for(auto p:d.points)points.push_back(adapter.point_constructor(p.x,p.y));
        auto index=[&] {
          if constexpr([] {
              if constexpr(requires { DataStructure::uses_radius_hint; })return DataStructure::uses_radius_hint;
              else return false;
            }()) {
            long double sum=0;for(const auto& c:d.queries)sum+=std::sqrt(static_cast<long double>(c.radius2()));
            return DataStructure(points,d.queries.empty()?0.0:double(sum/d.queries.size()));
          } else return DataStructure(points);
        }();
        ::esa_campaign::Result r;r.build_seconds=::esa_campaign::elapsed(start);
        constexpr bool exact_circle=[] {
          if constexpr(requires { DataStructure::reports_exact_circle; })return DataStructure::reports_exact_circle;
          else return false;
        }();
        const bool direct=::esa_campaign::ours(name)||exact_circle;
        const bool keep_original=name=="cgal_kd"||name=="cgal_rt"||name=="boost_lin"||name=="boost_quad"||name=="boost_star";
        return ::esa_campaign::queries<typename DataStructure::PointType>(d,verify,direct||!keep_original,
          [&](auto lo,auto hi,auto& out){
            auto l=adapter.point_constructor(lo.x,lo.y),h=adapter.point_constructor(hi.x,hi.y);
            // Native circle adapters report directly; CGAL requires a concrete back-insert iterator.
            if constexpr(exact_circle || requires { DataStructure::native_output_iterator; }) {
              index.query(typename DataStructure::QueryType(l,h),std::back_inserter(out));
            } else {
              if(direct)index.query(typename DataStructure::QueryType(l,h),std::back_inserter(out));
              else index.query(typename DataStructure::QueryType(l,h),::esa_campaign::CircleOutput<typename DataStructure::PointType>{&out,::esa_campaign::active_circle});
            }
          },r,nullptr,[] {
            if constexpr(requires { DataStructure::uses_native_radius; })return DataStructure::uses_native_radius;
            else return false;
          }());
      };
    }
  }
  using VecI = std::vector<typename DataStructure::PointType>;
  using VecQ = std::vector<typename DataStructure::QueryType>;
  std::function<typename DataStructure::PointType(typename DataStructure::IDXT,
                                                  typename DataStructure::IDXT)>
      point_constructor;
  Algo(std::function<typename DataStructure::PointType(typename DataStructure::IDXT,
                                                       typename DataStructure::IDXT)>
           point_constructor =
               [](typename DataStructure::IDXT x, typename DataStructure::IDXT y) {
                 return typename DataStructure::PointType(x, y);
               })
      : point_constructor(point_constructor) {}
  static constexpr absl::string_view algo_name = "sfc";
  enum Dist { UNIF, NORM, SKEW };  //, CLUSTER, SAMPLE };

  std::vector<std::pair<typename DataStructure::IDXT, typename DataStructure::IDXT>> gen_data(
      const Dist& type, const size_t n, size_t width = 0, size_t height = 0,
      const typename DataStructure::IDXT seed = 0) {
    std::random_device seeder;
    std::mt19937_64 engine(seed ? seed : seeder());  // Note that this returns uint_64_t
                                                     // and needs specification otherwise
    // Downcasting from 64bits should produce random numbers of smaller sizes also
    // (for Mersenne Twisters)

    if (width == 0) width = std::numeric_limits<typename DataStructure::IDXT>::max();
    if (height == 0) height = std::numeric_limits<typename DataStructure::IDXT>::max();

    std::uniform_int_distribution<size_t> unif(0, width);
    std::normal_distribution<double> wnorm(
        width / 2.,
        width / 8.);  // Parameters DO NOT match Jianzhong Qi et.al., //TODO discuss this
                      // closer to (F4) of Kriegel et al.
    std::normal_distribution<double> hnorm(height / 2., height / 8.);
    std::geometric_distribution<size_t> geom(5. / height);
    std::normal_distribution<double> cnorm(
        0, width / 100.);  // Parameters match (F5) from Kriegel et. al.

    std::vector<std::pair<typename DataStructure::IDXT, typename DataStructure::IDXT>> data;

    data.reserve(n);

    switch (type) {
        //   case SAMPLE:
        //     std::cout << "Can't sample without informed data " << std::endl;
        //     break;
      case UNIF:
        for (size_t i = 0; i < n; ++i) {
          data.emplace_back(unif(engine), unif(engine));
        }
        break;
      case NORM:
        for (size_t i = 0; i < n; ++i) {
          data.emplace_back(wnorm(engine), hnorm(engine));
        }
        break;
      case SKEW:  // Parameters match Jianzhong Qi et. al.
        for (size_t i = 0; i < n; ++i) {
          auto y = geom(engine) % ((size_t)height);

          data.emplace_back(unif(engine), y);
        }
        break;
        // TODO discuss and implement cluster
        //    case CLUSTER:
        //      auto root = sqrtl(n);
        //      typename DataStructure::IDXT x, y;
        //      for (size_t i = 0; i < root; ++i) {
        //        x = unif(engine);
        //        y = unif(engine);
        //        for (size_t j = i * root; j < (i + 1) * root && j < n; ++j) {
        //          std::cout << std::clamp(IDXT(x + cnorm(engine)), IDXT(0), IDXT(width)) << " "
        //                    << (std::clamp(IDXT(y + cnorm(engine)), IDXT(0), IDXT(height))) <<
        //                    std::endl;
        //        }
        //      }
        //      break;
    }
    return data;
  }
  template <class A, class B>
  using Pair = ::diskreport::app::algorithms::Pair<A, B>;
  virtual absl::StatusOr<std::unique_ptr<Pair<VecI, VecQ>>> Generate(
      const ::diskreport::app::app_io::RunConfig& run_config,
      const ::diskreport::app::app_io::Instance& instance) override {
    return Generate(run_config, instance, nullptr);
  }
  absl::StatusOr<std::unique_ptr<Pair<VecI, VecQ>>> Generate(
      const ::diskreport::app::app_io::RunConfig& run_config,
      const ::diskreport::app::app_io::Instance& instance,
      std::vector<std::pair<typename DataStructure::PointType, typename DataStructure::PointType>>*
          ptr) {
    VecI a, b;
    Dist query_dist = UNIF;
    if (instance.query_dist() == "norm") {
      query_dist = NORM;
    } else if (instance.query_dist() == "unif") {
      query_dist = UNIF;
    } else if (instance.query_dist() == "skew") {
      query_dist = SKEW;
    } else {
      return absl::NotFoundError("Query:" + instance.query_dist());
    }
    if (instance.file_path() != "" && instance.format() != "") {
      if (instance.format() == "out") {
        std::ifstream input(instance.file_path());
        typename DataStructure::IDXT x, y;
        while (input >> x && input >> y) {
          a.emplace_back(point_constructor(x, y));
        }
      } else {
        return absl::UnimplementedError("Format " + instance.format());
      }
    } else {
      Dist construct_dist = UNIF;
      auto string_params = instance.string_params();
      if (instance.construct_dist() == "norm") {
        construct_dist = NORM;
      } else if (instance.construct_dist() == "unif") {
        construct_dist = UNIF;
      } else if (instance.construct_dist() == "skew") {
        construct_dist = SKEW;
      } else {
        return absl::NotFoundError("Construct:" + instance.construct_dist());
      }

      auto inserts = gen_data(construct_dist, instance.insertion_count(), instance.width(),
                              instance.height(), instance.seed());
      std::transform(inserts.begin(), inserts.end(), std::back_inserter(a),
                     [&](auto d) { return point_constructor(d.first, d.second); });
    }
    auto queries = gen_data(query_dist, instance.query_count(), instance.width(), instance.height(),
                            instance.seed() + 10);
    VecQ c;
    auto double_params = instance.double_params();
    double fixed_w = double_params["fixed_w"];
    typename DataStructure::IDXT x = instance.width() == 0
                                         ? std::numeric_limits<typename DataStructure::IDXT>::max()
                                         : instance.width();
    typename DataStructure::IDXT w = std::ceil(fixed_w * x);
    static constexpr auto max = std::numeric_limits<typename DataStructure::IDXT>::max();
    for (const auto& p : queries) {
      auto x = p.first;
      auto y = p.second;
      if (max - w - 1 < x) x = max - w - 1;
      if (max - w - 1 < y) y = max - w - 1;
      if (ptr != nullptr) {
        ptr->push_back(
            std::make_pair(point_constructor(x, y), point_constructor(x + w + add, y + w + add)));
      }
      c.emplace_back(point_constructor(x, y), point_constructor(x + w + add, y + w + add));
    }
    return std::make_unique<Pair<VecI, VecQ>>(a, c);
  }
  absl::StatusOr<std::unique_ptr<DataStructure>> Construction(VecI& in) override {
    return std::make_unique<DataStructure>(in);
  }
  absl::StatusOr<std::unique_ptr<Shell<VecI>>> Execute(std::unique_ptr<DataStructure> instance,
                                                       VecQ& queries) override {
    VecI out;
    size_t overall = 0;
    for (const auto& q : queries) {
      instance->query(q, std::back_inserter(out));
      overall += out.size();
      out.clear();
    }
    // std::cout << "Result: " << std::endl;
    // for (auto s : out) {
    //   print(s);
    // }
    return std::make_unique<Shell<VecI>>(overall);
  }
  virtual absl::Status Verify(
      std::unique_ptr<DataStructure> instance, const VecQ& queries,
      std::function<bool(typename DataStructure::QueryType, const VecI&)> callback) {
    VecI out, ground_t;
    size_t overall = 0;
    for (const auto& q : queries) {
      instance->query(q, std::back_inserter(out));
      std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return comp1(a, b); });
      if (!callback(q, out)) {
        return absl::NotFoundError("Mismatch detected");
      };
      out.clear();
    }
    return absl::OkStatus();
  }
  virtual absl::Status Verify(const diskreport::app::app_io::Instance& instance) override {
    diskreport::app::app_io::RunConfig run_config;
    std::vector<std::pair<typename DataStructure::PointType, typename DataStructure::PointType>>
        queries1;
    instance.PrintDebugString();
    auto inp = Generate(run_config, instance, &queries1);
    if (!inp.ok()) {
      return inp.status();
    }
    auto input = std::move(inp.value());
    auto& [cons, queries] = input->pair;
    auto to_test = std::move(Construction(cons).value());
    size_t j = 0;
    return Verify(
        std::move(to_test), queries, [&](typename DataStructure::QueryType q, const VecI& out) {
          VecI truth;
          const auto [l, h] = queries1[j++];

          for (auto& px : cons) {
            if (contained(px, l, h)) {
              truth.push_back(px);
            }
          }
          std::sort(truth.begin(), truth.end(),
                    [](const auto& a, const auto& b) { return comp1(a, b); });
          if (truth.size() != out.size()) {
            std::cerr << "Size mismatch: " << truth.size() << "!=" << out.size() << std::endl;

            return false;
          }
          for (size_t i = 0; i < out.size(); i++) {
            if (!comp_equal(out[i], truth[i])) {
              std::cerr << "Point mismatch" << std::endl;
              return false;
            }
          }
          return true;
        });
  }
};
}  // namespace diskreport::sfc::app::algorithms::sfc
