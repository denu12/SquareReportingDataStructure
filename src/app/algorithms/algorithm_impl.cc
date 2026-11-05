#include "app/algorithms/algorithm_impl.h"

#include "app/app_io.pb.h"
#include "build-info.h"
#include "src/google/protobuf/util/time_util.h"

namespace diskreport::app::algorithms {
absl::StatusOr<Result> Run(const Instance& instance_conf, const RunConfig& config, bool validate) {
  {
    Result result;
    auto& s_factory = SolutionFactory::getInstance();
    if (!s_factory.has(config.algorithm_configs().at(0).data_structure())) {
      for (const auto& [name, _] : s_factory.map()) {
        std::cerr << "Registered: " << name << std::endl;
      }
      return absl::NotFoundError(config.algorithm_configs().at(0).data_structure() +
                                 " is not registered");
    }
    // Execute configs & measure time
    auto t1_sys = std::chrono::system_clock::now();
    bool is_exact = true;
    std::vector<AlgorithmRunInformation> results;
    auto st = s_factory.find(config.algorithm_configs().at(0).data_structure())
                  ->Run(config, instance_conf, results);
    if (!st.ok()) {
      return st;
    }
    // initial lookup
    *result.mutable_algorithm_run_informations() = {results.begin(), results.end()};
    std::chrono::steady_clock::time_point t2 = std::chrono::steady_clock::now();
    auto t2_sys = std::chrono::system_clock::now();
    for (const auto& res : results) {
      is_exact &= res.is_exact();
    }
    result.set_is_exact(is_exact);
    result.set_weight(results.back().weight());
    result.set_quality(results.back().quality());
    result.set_size(results.back().size());
    // TODO: Update to exact timing
    *result.mutable_run_information()->mutable_start_time() =
        google::protobuf::util::TimeUtil::TimeTToTimestamp(
            std::chrono::system_clock::to_time_t(t1_sys));
    *result.mutable_run_information()->mutable_end_time() =
        google::protobuf::util::TimeUtil::TimeTToTimestamp(
            std::chrono::system_clock::to_time_t(t2_sys));
    int64_t dur = 0L;
    int64_t dur_insert = 0L;
    int64_t dur_query = 0L;
    int64_t dur_true = 0L;
    int64_t dur_deletion = 0L;

    for (const auto& r : results) {
      dur += google::protobuf::util::TimeUtil::DurationToNanoseconds(r.algo_duration());
      dur_insert += google::protobuf::util::TimeUtil::DurationToNanoseconds(r.insertion_duration());
      dur_query += google::protobuf::util::TimeUtil::DurationToNanoseconds(r.query_duration());
      dur_true += google::protobuf::util::TimeUtil::DurationToNanoseconds(r.true_duration());
      dur_deletion +=
          google::protobuf::util::TimeUtil::DurationToNanoseconds(r.deletion_duration());
    }
    *result.mutable_run_information()->mutable_algo_duration() =
        google::protobuf::util::TimeUtil::NanosecondsToDuration((int64_t)dur);
    *result.mutable_run_information()->mutable_query_duration() =
        google::protobuf::util::TimeUtil::NanosecondsToDuration((int64_t)dur_query);
    *result.mutable_run_information()->mutable_insertion_duration() =
        google::protobuf::util::TimeUtil::NanosecondsToDuration((int64_t)dur_insert);
    *result.mutable_run_information()->mutable_true_duration() =
        google::protobuf::util::TimeUtil::NanosecondsToDuration((int64_t)dur_true);
    *result.mutable_run_information()->mutable_deletion_duration() =
        google::protobuf::util::TimeUtil::NanosecondsToDuration((int64_t)dur_deletion);
    *result.mutable_run_config() = config;
    *result.mutable_run_information()->mutable_git_sha() = STABLE_VERSION;
#ifdef HASHING_ACTIVATED
    result.mutable_run_information()->set_edge_hashing_enabled(true);
#endif
    *result.mutable_run_information()->mutable_malloc_implementation() = MALLOC_IMPLEMENTATION;
    *result.mutable_run_information()->mutable_git_describe() = STABLE_SCM_DESCRIBE;
    *result.mutable_run_information()->mutable_machine()->mutable_host_name() = HOSTNAME_STRING;
    *result.mutable_run_information()->mutable_machine()->mutable_build_user() = USERNAME;
    *result.mutable_run_information()->mutable_exec_host_name() =
        diskreport::utils::systeminfo::getHostname();
    *result.mutable_run_information()->mutable_exec_user_name() =
        diskreport::utils::systeminfo::getUsername();
    *result.mutable_instance() = instance_conf;
    return result;
  }
}
}  // namespace diskreport::app::algorithms