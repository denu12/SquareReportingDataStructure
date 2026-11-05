#pragma once
#include <chrono>
#include <cstdint>
#include <memory>

#include "algorithm_impl.h"
#include "app/algorithms/algorithm_impl.h"
#include "utils/logging.h"
namespace diskreport::app::algorithms {
template <class T1, class T2>
struct Pair {
  std::pair<T1, T2> pair;
  static constexpr absl::string_view ds_name = "pair";
  Pair(T1 a, T2 b) : pair(a, b) {}
};
struct Empty {
  static constexpr absl::string_view ds_name = "pair";
  ::diskreport::app::app_io::Instance instance;
  Empty(const ::diskreport::app::app_io::Instance& inst) : instance(inst) {}
};
template <class InsertType, class QueryType, class IntermediateType, class OutType = InsertType>
struct TwoPhaseAlgorithmImpl : public AlgorithmImpl<Empty, OutType> {
  virtual absl::StatusOr<std::unique_ptr<IntermediateType>> Construction(InsertType&) {
    return absl::UnimplementedError("Construction Phase is not implemented");
  }
  virtual absl::StatusOr<std::unique_ptr<OutType>> Execute(std::unique_ptr<IntermediateType>,
                                                           QueryType&) {
    return absl::UnimplementedError("Execute Phase is not implemented");
  }
  // Overriden method for two phase execution
  absl::Status Run(const RunConfig& run_config, int index, std::unique_ptr<Empty> instance,
                   std::vector<AlgorithmRunInformation>& results) override {
    auto input_s = Generate(run_config, instance->instance);
    if (!input_s.ok()) {
      return input_s.status();
    }
    auto input = std::move(input_s.value());
    auto& [a, b] = input->pair;
    TIMED_FUNC(timer2);
    const auto& config = run_config.algorithm_configs().at(index);
    AlgorithmRunInformation result;
    int64_t total_duration = 0;
    auto t1_sys = std::chrono::system_clock::now();
    bool is_exact = false;
    int64_t k = 0;
    size_t counts = 0;
    size_t cadd = 0;
    int64_t true_time = 0;
    auto t_con1 = std::chrono::steady_clock::now();
    auto constr = Construction(a);
    if (!constr.ok()) {
      return constr.status();
    }
    auto t_con2 = std::chrono::steady_clock::now();
    std::cout << "Construction took " << (t_con2 - t_con1).count() << std::endl;
    auto t_exec1 = std::chrono::steady_clock::now();
    auto output = Execute(std::move(constr.value()), b);
    if (!output.ok()) {
      return output.status();
    }
    auto t_exec2 = std::chrono::steady_clock::now();
    auto t2_sys = std::chrono::system_clock::now();
    auto solution = std::move(output.value());
    auto con_dur = t_con2 - t_con1;
    int64_t construction_time =
        std::chrono::duration_cast<std::chrono::nanoseconds, int64_t>(con_dur).count();
    auto exec_dur = t_exec2 - t_exec1;
    int64_t query_time =
        std::chrono::duration_cast<std::chrono::nanoseconds, int64_t>(exec_dur).count();
    *result.mutable_construction_duration() =
        google::protobuf::util::TimeUtil::NanosecondsToDuration(construction_time);
    *result.mutable_query_duration() =
        google::protobuf::util::TimeUtil::NanosecondsToDuration(query_time);

    total_duration = query_time + construction_time;
    result.set_is_exact(is_exact);
    result.set_size(solution->size());
    if (!solution->valid()) {
      return absl::InternalError("Not a valid");
    }
    // TODO: Update to exact timing
    *result.mutable_start_time() = google::protobuf::util::TimeUtil::TimeTToTimestamp(
        std::chrono::system_clock::to_time_t(t1_sys));
    *result.mutable_end_time() = google::protobuf::util::TimeUtil::TimeTToTimestamp(
        std::chrono::system_clock::to_time_t(t2_sys));
    *result.mutable_algo_duration() =
        google::protobuf::util::TimeUtil::NanosecondsToDuration(total_duration);

    *result.mutable_algorithm_config() = config;
    results.push_back(result);
    if (index + 1 < run_config.algorithm_configs_size()) {
      return AlgorithmImplFactory<OutType>::getInstance().Run(run_config, index + 1,
                                                              std::move(solution), results);
    }
    return absl::OkStatus();
  }
  virtual absl::StatusOr<std::unique_ptr<Pair<InsertType, QueryType>>> Generate(
      const ::diskreport::app::app_io::RunConfig& run_config,
      const ::diskreport::app::app_io::Instance& instance) = 0;
  absl::StatusOr<std::unique_ptr<Empty>> Load(
      const ::diskreport::app::app_io::RunConfig& run_config,
      const ::diskreport::app::app_io::Instance& instance) override {
    return std::make_unique<Empty>(instance);
  };
};
}  // namespace diskreport::app::algorithms