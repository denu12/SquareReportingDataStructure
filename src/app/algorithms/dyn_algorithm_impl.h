#pragma once
#include <memory>

#include "app/algorithms/algorithm_impl.h"
#include "app/app_io.pb.h"
#include "utils/logging.h"

namespace diskreport::app::algorithms {
/*
 * StreamableInputType has good():bool method to return whether there is a next stream element
 * StreamableInputType has next():StreamMember returning the next element in Stream
 * OutputType stores the intermediate Results.
 * Output Type should conform to the general Result interface
 * Following methods need to be implemented:
 *   - std::unique_ptr<OutputType> Init(const app_io::AlgorithmConfig& config,const
 * StreamableInputType& stream) Creates the solution object (might use const properties of stream)
 *   - void Stream(const StreamableInputType& element, OutputType& solution)
 *     Streams the element, might apply changes to OutputType
 *   - absl::StatusOr<std::unique_ptr<OutputType>> Transform(OutputType& solution)
 *     Transforms the solution into the result (OutputType)
 */
diskreport::app::app_io::TimePerTickStat processTimeStat(const std::vector<int64_t>& times);
struct DynamicEmpty {
  static constexpr absl::string_view ds_name = "dynamic";
  ::diskreport::app::app_io::Instance instance;
  bool typedif;
  DynamicEmpty(const ::diskreport::app::app_io::Instance& inst) : instance(inst) {}
};
template <class StreamableInputType, class OutputType, bool stddev>
class DynamicAlgorithmImpl : public AlgorithmImpl<DynamicEmpty, OutputType> {
  virtual std::unique_ptr<OutputType> Init(const app_io::AlgorithmConfig& config,
                                           const StreamableInputType& stream) = 0;
  virtual std::unique_ptr<OutputType> Init(const app_io::AlgorithmConfig& config,
                                           const StreamableInputType& stream,
                                           app_io::DebugInformation& debug) {
    return Init(config, stream);
  }

  virtual absl::StatusOr<std::unique_ptr<StreamableInputType>> Generate(
      const ::diskreport::app::app_io::RunConfig& run_config,
      const ::diskreport::app::app_io::Instance& instance) = 0;
  // Stream into solution
  virtual bool Stream(const typename StreamableInputType::StreamMember& member,
                      const typename StreamableInputType::Mode mode, OutputType& solution) = 0;
  // Overriden method to stream
  absl::Status Run(const RunConfig& run_config, int index, std::unique_ptr<DynamicEmpty> input2,
                   std::vector<AlgorithmRunInformation>& results) override {
    TIMED_FUNC(timer2);
    auto input3 = Generate(run_config, input2->instance);
    if (!input3.ok()) {
      return input3.status();
    }
    auto input = std::move(input3.value());
    const auto& config = run_config.algorithm_configs().at(index);
    AlgorithmRunInformation result;
    int64_t total_duration = 0;
    auto t1_sys = std::chrono::system_clock::now();
    std::unique_ptr<OutputType> solution =
        Init(config, *input, *result.mutable_debug_information());
    bool is_exact = false;
    int64_t k = 0;
    size_t counts = 0;
    size_t cadd = 0;
    int64_t true_time = 0;
    std::map<typename StreamableInputType::Mode, int64_t> duration_pertype;
    std::map<typename StreamableInputType::Mode, std::vector<int64_t>> duration_pertype_vec;
    std::vector<int64_t> true_times;
    while (input->good()) {
      const auto [next, mode] = input->next();
      std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
      bool is_hit = false;
      //  if (mode == StreamableInputType::Mode::Insertion) {
      //    cadd++;
      //  }
      //  if (k == 69829) {
      //    std::cout << "trying" << cadd << std::endl;
      //    std::cout << k << next << " {" << std::endl;
      //    for (auto s : solution->hull()) {
      //      std::cout << ", " << s;
      //    }
      //    std::cout << "}" << std::endl;
      //  }
      is_hit = Stream(next, mode, *solution);
      counts += is_hit;
      // if (is_hit) {
      //   std::cout << k << next << " {" << std::endl;
      // }
      // if (false) {
      //   std::cout << k << next << " {" << std::endl;
      //   for (auto s : solution->hull()) {
      //     std::cout << ", " << s;
      //   }
      //   std::cout << "}" << std::endl;

      //   solution->printInfo();
      //   //  exit(0);
      // }
      std::chrono::steady_clock::time_point t2 = std::chrono::steady_clock::now();
      auto dur = t2 - t1;
      int64_t rtime = std::chrono::duration_cast<std::chrono::nanoseconds, int64_t>(dur).count();
      total_duration += rtime;
      duration_pertype[mode] += rtime;
      if constexpr (stddev) {
        duration_pertype_vec[mode].push_back(rtime);
      }
      if (is_hit) {
        if constexpr (stddev) {
          true_times.push_back(rtime);
        }
        true_time += rtime;
      }
      // if (k % sample_rate == 0) {
      //   TIMED_SCOPE(timer, std::to_string(k) + "sampling");
      //   auto* sample = result.add_dynamic_samples();
      //   sample->set_timestamp(k);
      // }
      k++;
      // TODO capture result every i-th iteration
    }
    if constexpr (stddev) {
      *(result.mutable_insertions_stat()) =
          processTimeStat(duration_pertype_vec[StreamableInputType::Mode::Insertion]);
      *(result.mutable_queries_stat()) =
          processTimeStat(duration_pertype_vec[StreamableInputType::Mode::Query]);
      *(result.mutable_deletion_stat()) =
          processTimeStat(duration_pertype_vec[StreamableInputType::Mode::Delete]);

      *(result.mutable_true_queries_stat()) = processTimeStat(true_times);
    }
    result.mutable_debug_information()->mutable_int64_info()->insert({"true_queries", counts});
    auto t2_sys = std::chrono::system_clock::now();
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
    *result.mutable_insertion_duration() = google::protobuf::util::TimeUtil::NanosecondsToDuration(
        duration_pertype[StreamableInputType::Mode::Insertion]);
    *result.mutable_query_duration() = google::protobuf::util::TimeUtil::NanosecondsToDuration(
        duration_pertype[StreamableInputType::Mode::Query]);
    *result.mutable_true_duration() =
        google::protobuf::util::TimeUtil::NanosecondsToDuration(true_time);
    *result.mutable_deletion_duration() = google::protobuf::util::TimeUtil::NanosecondsToDuration(
        duration_pertype[StreamableInputType::Mode::Delete]);
    *result.mutable_algorithm_config() = config;
    results.push_back(result);
    if (index + 1 < run_config.algorithm_configs_size()) {
      return AlgorithmImplFactory<OutputType>::getInstance().Run(run_config, index + 1,
                                                                 std::move(solution), results);
    }
    return absl::OkStatus();
  }
  absl::StatusOr<std::unique_ptr<DynamicEmpty>> Load(
      const ::diskreport::app::app_io::RunConfig& run_config,
      const ::diskreport::app::app_io::Instance& instance) override {
    return std::make_unique<DynamicEmpty>(instance);
  };
};
}  // namespace diskreport::app::algorithms