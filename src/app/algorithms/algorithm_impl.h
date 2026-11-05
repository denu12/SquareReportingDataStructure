#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "app/app_io.pb.h"
#include "build-info.h"
#include "google/protobuf/util/time_util.h"
#include "utils/systeminfo.h"

#define VAL(str) #str
#define TOSTRING(str) VAL(str)
#define HOSTNAME_STRING TOSTRING(BUILD_HOST)
#define USERNAME TOSTRING(BUILD_USER)
// TODO: Add direct register as template function
namespace diskreport::app::algorithms {

using diskreport::app::app_io::AlgorithmConfig;
using diskreport::app::app_io::AlgorithmRunInformation;
using diskreport::app::app_io::Instance;
using diskreport::app::app_io::Result;
using diskreport::app::app_io::RunConfig;

/**
 * @brief Algorithms to be run need to provide a AlgorithmName constexpr and
 * override execute. registerImpl to be added to the factory.
 * with constexpr transformer = true there should exists a using statement TransformToType
 *
 *
 */
template <class InputT>
class RunInterface {
 public:
  RunInterface() {}
  RunInterface(const RunInterface&) = delete;
  RunInterface(RunInterface&&) = delete;
  RunInterface& operator=(const RunInterface&) = delete;
  RunInterface& operator=(RunInterface&&) = delete;
  virtual ~RunInterface() {}
  virtual absl::Status Run(const RunConfig& run_config, int index, std::unique_ptr<InputT> input,
                           std::vector<AlgorithmRunInformation>& results) = 0;
  // This is used to Load in a Problem from memory and to Construct it
  virtual absl::StatusOr<std::unique_ptr<InputT>> Load(const RunConfig& run_config,
                                                       const Instance& instance) = 0;
  virtual absl::Status Verify(const Instance& instance) {
    return absl::UnimplementedError("verify not impled");
  }
};
template <class T>
class AlgorithmImplFactory;
template <class InputT, class OutputT = InputT>
class AlgorithmImpl : public RunInterface<InputT> {
 protected:
  virtual absl::StatusOr<std::unique_ptr<OutputT>> Execute(const AlgorithmConfig& config,
                                                           std::unique_ptr<InputT> bm) {
    return absl::UnimplementedError("Something went wrong and you called an unimplemented method.");
  };
  virtual absl::StatusOr<std::unique_ptr<OutputT>> Execute(const AlgorithmConfig& config,
                                                           std::unique_ptr<InputT> bm,
                                                           bool& is_exact) {
    is_exact = false;
    return Execute(config, std::move(bm));
  }
  virtual absl::StatusOr<std::unique_ptr<OutputT>> Execute(
      const AlgorithmConfig& config, std::unique_ptr<InputT> bm, bool& is_exact,
      app_io::DebugInformation& mutable_debug) {
    return Execute(config, std::move(bm), is_exact);
  }

 public:
  AlgorithmImpl() {}
  AlgorithmImpl(const AlgorithmImpl&) = delete;
  AlgorithmImpl(AlgorithmImpl&&) = delete;
  AlgorithmImpl& operator=(const AlgorithmImpl&) = delete;
  AlgorithmImpl& operator=(AlgorithmImpl&&) = delete;
  using BaseClass = AlgorithmImpl<InputT, OutputT>;
  using InputType = InputT;
  using OutputType = OutputT;
  virtual ~AlgorithmImpl() {}
  absl::Status Run(const RunConfig& run_config, int index, std::unique_ptr<InputType> input,
                   std::vector<AlgorithmRunInformation>& results) override {
    const auto& config = run_config.algorithm_configs().at(index);
    AlgorithmRunInformation result;
    auto t1_sys = std::chrono::system_clock::now();
    std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
    bool is_exact = false;
    auto status = Execute(config, std::move(input), is_exact, *result.mutable_debug_information());
    if (!status.ok()) {
      return status.status();
    }
    std::chrono::steady_clock::time_point t2 = std::chrono::steady_clock::now();
    auto t2_sys = std::chrono::system_clock::now();
    result.set_is_exact(is_exact);
    std::unique_ptr<OutputT> solution = std::move(status.value());

    if (!solution->valid()) {
      return absl::InternalError("Not a valid");
    }
    // TODO: Update to exact timing
    *result.mutable_start_time() = google::protobuf::util::TimeUtil::TimeTToTimestamp(
        std::chrono::system_clock::to_time_t(t1_sys));
    *result.mutable_end_time() = google::protobuf::util::TimeUtil::TimeTToTimestamp(
        std::chrono::system_clock::to_time_t(t2_sys));
    auto dur = t2 - t1;
    *result.mutable_algo_duration() = google::protobuf::util::TimeUtil::NanosecondsToDuration(
        std::chrono::duration_cast<std::chrono::nanoseconds, int64_t>(dur).count());

    *result.mutable_algorithm_config() = config;
    results.push_back(result);
    if (index + 1 < run_config.algorithm_configs_size()) {
      return AlgorithmImplFactory<OutputT>::getInstance().Run(run_config, index + 1,
                                                              std::move(solution), results);
    }
    return absl::OkStatus();
  }
  virtual absl::Status ValidateConfig(const AlgorithmConfig& config) {
    return absl::NotFoundError("Config should be validateable.");
  };
};
class Factory {
 public:
  Factory() {}
  Factory(const Factory&) = default;
  Factory(Factory&&) = delete;
  Factory& operator=(const Factory&) = default;
  Factory& operator=(Factory&&) = delete;
  virtual ~Factory() {}
  virtual bool has(const std::string& name) = 0;
  virtual absl::Status Validate(const RunConfig& config) {
    return absl::UnimplementedError("Validate is not implemented");
  };
  virtual absl::Status Run(const RunConfig& run_config, const Instance& h,
                           std::vector<AlgorithmRunInformation>& results) {
    return absl::UnimplementedError("Run is not implemented");
  }
  virtual absl::Status Verify(const std::string& name, const Instance& h) {
    return absl::UnimplementedError("Verify is not implemented");
  }
};
class SolutionFactory {
 public:
  SolutionFactory(const SolutionFactory&) = delete;
  SolutionFactory(SolutionFactory&&) = delete;
  SolutionFactory& operator=(const SolutionFactory&) = delete;
  SolutionFactory& operator=(SolutionFactory&&) = delete;
  static SolutionFactory& getInstance() {
    static SolutionFactory instance;
    return instance;
  }
  bool has(const std::string& name) { return _map.find(name) != _map.end(); }
  Factory* find(const std::string& name) { return _map[name](); }
  auto map() const { return _map; }

 private:
  SolutionFactory() {}
  ~SolutionFactory() = default;
  std::map<std::string, std::function<Factory*()>> _map;
  bool addSolution(const std::string& name, std::function<Factory*()> func) {
    if (_map.find(name) != _map.end()) {
      return false;
    }
    _map[name] = std::move(func);
    return true;
  }
  template <class Solution>
  friend bool registerSolution();
};

template <class F>
bool registerSolution() {
  auto& instance = SolutionFactory::getInstance();
  std::function<Factory*()> fac = []() -> Factory* { return &F::getInstance(); };
  std::string name(F::InputType::ds_name);
  return instance.addSolution(name, fac);
}
template <class InputT>
class AlgorithmImplFactory : public Factory {
 public:
  using InputType = InputT;
  using Impl = RunInterface<InputType>;
  AlgorithmImplFactory(const AlgorithmImplFactory&) = delete;
  AlgorithmImplFactory(AlgorithmImplFactory&&) = delete;
  AlgorithmImplFactory& operator=(const AlgorithmImplFactory&) = delete;
  AlgorithmImplFactory& operator=(AlgorithmImplFactory&&) = delete;

  static AlgorithmImplFactory& getInstance() {
    static AlgorithmImplFactory instance;
    return instance;
  }
  std::function<std::unique_ptr<Impl>()> operator[](absl::string_view name) { return _map[name]; }
  absl::Status Run(const RunConfig& run_config, size_t index, std::unique_ptr<InputType> input,
                   std::vector<AlgorithmRunInformation>& results) {
    const auto& config = run_config.algorithm_configs().at(static_cast<int>(index));
    if (has(config.algorithm_name())) {
      // TODO add silent check to config, if types do match...
      return _map[config.algorithm_name()]()->Run(run_config, index, std::move(input), results);
    }
    for (auto [k, v] : _map) {
      std::cout << k << std::endl;
    }
    return absl::NotFoundError(config.algorithm_name() + " is not a valid algo name");
  }
  absl::Status Run(const RunConfig& run_config, const Instance& h,
                   std::vector<AlgorithmRunInformation>& results) override {
    const auto& config = run_config.algorithm_configs().at(0);
    if (has(config.algorithm_name())) {
      auto impl = _map[config.algorithm_name()];
      auto inp = impl()->Load(run_config, h);
      if (!inp.ok()) {
        return inp.status();
      }
      return impl()->Run(run_config, 0, std::move(inp.value()), results);
    }
    for (auto [k, v] : _map) {
      std::cout << k << std::endl;
    }
    return absl::NotFoundError(config.algorithm_name() + " is not a valid algo name");
  }
  bool has(const std::string& name) override { return _map.find(name) != _map.end(); }
  absl::Status Verify(const std::string& name, const Instance& h) {
    if (has(name)) {
      return _map[name]()->Verify(h);
    }
    for (auto [k, v] : _map) {
      std::cout << k << std::endl;
    }
    return absl::NotFoundError(name + " is not a valid algo name");
  }

 private:
  AlgorithmImplFactory() {}
  ~AlgorithmImplFactory() override = default;
  std::map<std::string, std::function<std::unique_ptr<Impl>()>> _map;
  bool addAlgorithmImpl(const std::string& name, std::function<std::unique_ptr<Impl>()> func) {
    if (_map.find(name) != _map.end()) {
      return false;
    }
    _map[name] = func;
    return true;
  }
  template <class Impl>
  friend bool registerImpl(const std::string&);
};

template <class Impl>
bool registerImpl(const std::string& name = Impl::AlgorithmName) {
  if constexpr (requires { Impl::template RegisterEsa<Impl>(name); }) Impl::template RegisterEsa<Impl>(name);
  using AFactory = AlgorithmImplFactory<typename Impl::InputType>;
  registerSolution<AFactory>();
  auto& instance = AFactory::getInstance();
  std::function<std::unique_ptr<RunInterface<typename Impl::InputType>>()> fac =
      []() -> std::unique_ptr<RunInterface<typename Impl::InputType>> {
    return std::make_unique<Impl>();
  };
  return instance.addAlgorithmImpl(name, fac);
}

absl::StatusOr<Result> Run(const Instance& instance_conf, const RunConfig& config, bool validate);
}  // namespace diskreport::app::algorithms
// NOLINTBEGIN
#define REGISTER_IMPL_NAMED(I, N) \
  const static bool impl_##I = diskreport::app::algorithms::registerImpl<I>(N)
#define REGISTER_IMPL(I) const static bool impl_##I = diskreport::app::algorithms::registerImpl<I>()
#define REGISTER_IMPL_BASE_CLASS(I, C) \
  const static bool impl_##I_##C = diskreport::app::algorithms::registerImpl<I<C>>()
// NOLINTEND

// end of file