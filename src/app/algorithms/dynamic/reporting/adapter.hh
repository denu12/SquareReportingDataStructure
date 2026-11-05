#pragma once
#include <functional>
#include <random>
#include <utility>
#include <vector>

#include "app/algorithms/dyn_algorithm_impl.h"
#include "benchmark/esa_campaign.hh"

namespace diskreport::app::algorithms::dynamic::sfc {

template <typename PT>
struct InMemoryQueryInstance {
  using PointType = PT;
  using StreamMember = std::pair<PointType, PointType>;
  enum class Mode { Insertion, Query, Delete };
  using Task = std::pair<StreamMember, Mode>;

  std::vector<Task> tasks;
  size_t offset = 0;
  explicit InMemoryQueryInstance(const std::vector<Task>& q) : tasks(q) {}
  bool good() const { return offset < tasks.size(); }
  auto next() { return tasks[offset++]; }
  static constexpr absl::string_view ds_name = "in_memory_queries";
};
template <class Impl, class QueryInstance, bool stddev = true>
struct DynSFC
    : public diskreport::app::algorithms::DynamicAlgorithmImpl<QueryInstance, typename Impl::Type,
                                                              stddev> {
  std::unique_ptr<typename Impl::Type> Init(const diskreport::app::app_io::AlgorithmConfig& config,
                                            const QueryInstance& stream) override {
    auto double_params = config.double_params();
    auto int64_params = config.int64_params();

    return std::make_unique<typename Impl::Type>(double_params, int64_params);
  }
  bool Stream(const typename QueryInstance::StreamMember& member,
              const typename QueryInstance::Mode mode, typename Impl::Type& solution) {
    // TIMED_FUNC(test);
    if (mode == QueryInstance::Mode::Insertion) {
      solution.handle_insertion(member.first);
    } else if (mode == QueryInstance::Mode::Delete) {
      solution.handle_deletion(member.first);
    } else {
      return solution.handle_query(member);
    }
    return false;
  }
};
template <template <typename> class Impl, typename InputType>
struct DynSFCInMemory
    : public DynSFC<Impl<InputType>, InMemoryQueryInstance<typename Impl<InputType>::PointType>> {
  template<class Concrete> static void RegisterEsa(const std::string& name) {
    using T=typename Impl<InputType>::Type;using P=typename Impl<InputType>::PointType;
    if constexpr(requires(T& t,P p,std::vector<P>& out){ t.esa_query(p,p,out); }) {
      if(!::esa_campaign::selected(name))return;
      ::esa_campaign::registry()[name]=[name](const ::esa_campaign::Dataset& d,bool verify){
        Concrete adapter;
        diskreport::app::app_io::AlgorithmConfig config;
        auto start=std::chrono::steady_clock::now();T index(config.double_params(),config.int64_params());
        for(auto p:d.points)index.handle_insertion(adapter.point_constructor(p.x,p.y));
        ::esa_campaign::Result r;r.build_seconds=::esa_campaign::elapsed(start);
        return ::esa_campaign::dynamic_queries<P>(d,verify,::esa_campaign::ours(name),index,
          [&](auto p){return adapter.point_constructor(p.x,p.y);},r);
      };
    }
  }
  using PointType = typename Impl<InputType>::PointType;
  enum Dist { UNIF, NORM, SKEW };  //, CLUSTER, SAMPLE };

  std::vector<std::pair<InputType, InputType>> gen_data(const Dist& type, const size_t n,
                                                        size_t width = 0, size_t height = 0,
                                                        const InputType seed = 0) {
    std::random_device seeder;
    std::mt19937_64 engine(seed ? seed : seeder());  // Note that this returns uint_64_t
                                                     // and needs specification otherwise
    // Downcasting from 64bits should produce random numbers of smaller sizes also
    // (for Mersenne Twisters)

    if (width == 0) width = std::numeric_limits<InputType>::max();
    if (height == 0) height = std::numeric_limits<InputType>::max();

    std::uniform_int_distribution<size_t> unif(0, width);
    std::normal_distribution<double> wnorm(
        width / 2.,
        width / 8.);  // Parameters DO NOT match Jianzhong Qi et.al., //TODO discuss this
                      // closer to (F4) of Kriegel et al.
    std::normal_distribution<double> hnorm(height / 2., height / 8.);
    std::geometric_distribution<size_t> geom(5. / height);
    std::normal_distribution<double> cnorm(
        0, width / 100.);  // Parameters match (F5) from Kriegel et. al.

    std::vector<std::pair<InputType, InputType>> data;

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
        //      InputType x, y;
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
  std::function<PointType(InputType, InputType)> point_constructor;
  DynSFCInMemory(std::function<PointType(InputType, InputType)> point_constructor =
                     [](InputType x, InputType y) { return PointType(x, y); })
      : point_constructor(point_constructor) {}

  absl::StatusOr<std::unique_ptr<InMemoryQueryInstance<PointType>>> Generate(
      const RunConfig& run_config, const Instance& instance) override {
    std::mt19937 engine(instance.seed());
    std::vector<typename InMemoryQueryInstance<PointType>::Task> tasks;
    std::vector<PointType> a, b;
    std::vector<std::pair<PointType, PointType>> c;
    Dist construct_dist = UNIF;
    auto string_params = instance.string_params();
    if (instance.construct_dist() == "norm") {
      construct_dist = NORM;
    } else if (instance.construct_dist() == "unif") {
      construct_dist = UNIF;
    } else if (instance.construct_dist() == "skew") {
      construct_dist = SKEW;
    } else {
      return absl::NotFoundError(instance.construct_dist());
    }
    Dist query_dist = UNIF;
    if (instance.query_dist() == "norm") {
      query_dist = NORM;
    } else if (instance.query_dist() == "unif") {
      query_dist = UNIF;
    } else if (instance.query_dist() == "skew") {
      query_dist = SKEW;
    } else {
      return absl::NotFoundError(instance.query_dist());
    }
    auto inserts1 = gen_data(construct_dist, instance.insertion_count(), instance.width(),
                             instance.height(), instance.seed());
    std::transform(inserts1.begin(), inserts1.end(), std::back_inserter(a),
                   [&](auto d) { return point_constructor(d.first, d.second); });
    auto queries = gen_data(query_dist, instance.query_count(), instance.width(), instance.height(),
                            instance.seed() + 10);
    auto double_params = instance.double_params();
    double fixed_w = double_params["fixed_w"];
    InputType x = instance.width() == 0 ? std::numeric_limits<InputType>::max() : instance.width();
    InputType w = std::ceil(fixed_w * x);
    static constexpr auto max = std::numeric_limits<InputType>::max();
    for (const auto& p : queries) {
      auto x = p.first;
      auto y = p.second;
      if (max - w - 1 < x) x = max - w - 1;
      if (max - w - 1 < y) y = max - w - 1;
      c.emplace_back(point_constructor(x, y), point_constructor(x + w + Impl<InputType>::offset,
                                                                y + w + Impl<InputType>::offset));
    }
    for (const PointType& p : a) {
      tasks.push_back(
          std::make_pair(std::make_pair(p, p), InMemoryQueryInstance<PointType>::Mode::Insertion));
    }

    if (instance.mix()) {
      std::shuffle(tasks.begin(), tasks.end(), engine);
    }
    std::set<size_t> deleted;
    if (instance.deletion_count() > instance.insertion_count()) {
      return absl::UnimplementedError("You can not have more deletions then insertions.");
    }
    std::vector<typename InMemoryQueryInstance<PointType>::Task> inserts;
    std::copy_if(tasks.begin(), tasks.end(), std::back_inserter(inserts), [](auto a) {
      return a.second == InMemoryQueryInstance<PointType>::Mode::Insertion;
    });
    if (instance.mix_deletions()) {
      std::shuffle(inserts.begin(), inserts.end(), engine);
    }
    std::transform(inserts.begin(), inserts.begin() + instance.deletion_count(),
                   std::back_inserter(tasks),
                   [](auto a) -> typename InMemoryQueryInstance<PointType>::Task {
                     return {a.first, InMemoryQueryInstance<PointType>::Mode::Delete};
                   });
    for (const auto& [p, q] : c) {
      tasks.push_back(
          std::make_pair(std::make_pair(p, q), InMemoryQueryInstance<PointType>::Mode::Query));
    }
    auto options = instance.int64_params();
    if (options["dynamic_interleave"] == 1) {
      // Spread insertions over the stream. Delete a previously inserted item
      // every other insertion and distribute queries evenly over all steps.
      // FIFO deletion keeps all deletions valid, including duplicate points.
      tasks.clear();
      size_t deleted_count = 0, queried_count = 0;
      for (size_t i = 0; i < inserts.size(); ++i) {
        tasks.push_back(inserts[i]);
        const size_t deletes_due = (i + 1) * instance.deletion_count() / inserts.size();
        while (deleted_count < deletes_due) {
          tasks.push_back({inserts[deleted_count].first, InMemoryQueryInstance<PointType>::Mode::Delete});
          ++deleted_count;
        }
        const size_t queries_due = (i + 1) * c.size() / inserts.size();
        while (queried_count < queries_due) {
          tasks.push_back({c[queried_count++], InMemoryQueryInstance<PointType>::Mode::Query});
        }
      }
      if (inserts.empty()) for (const auto& q : c)
        tasks.push_back({q, InMemoryQueryInstance<PointType>::Mode::Query});
    }
    auto instance2 = std::make_unique<InMemoryQueryInstance<PointType>>(tasks);
    return instance2;
  };
};
}  // namespace diskreport::app::algorithms::dynamic::sfc