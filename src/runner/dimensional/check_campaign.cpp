#include "manual_adapters.hpp"
#include <fstream>
#include <iostream>
#include <string>
#include <chrono>
bool benchmark_mode=false;

template<class T> T read(std::istream& in) {
  T value{};
  if (!in.read(reinterpret_cast<char*>(&value), sizeof(value)))
    throw std::runtime_error("Truncated campaign input");
  return value;
}
template<std::size_t D, template<std::size_t> class Algorithm> int run(std::istream& in) {
  using I = Algorithm<D>;
  using P = typename I::Point;
  using W = unsigned __int128;
  auto n = read<std::uint64_t>(in), q = read<std::uint64_t>(in);
  std::vector<P> points(n);
  for (auto& p : points) for (auto& x : p) x = read<std::uint32_t>(in);
  std::vector<std::pair<P,W>> queries(q);
  for(auto& [center,radius2]:queries){
    for(auto& x:center)x=read<std::uint32_t>(in);
    radius2=read<std::uint64_t>(in);radius2|=W(read<std::uint64_t>(in))<<64;
  }
  const auto build_start=std::chrono::steady_clock::now();
  I index(points);
  const double build_seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-build_start).count();
  double query_seconds=0;
  std::size_t answers = 0;
  std::vector<P> actual;
  const auto batch_start=std::chrono::steady_clock::now();
  for (std::size_t j = 0; j < q; ++j) {
    const auto& [center,radius2]=queries[j];
    const auto query_start=benchmark_mode?batch_start:std::chrono::steady_clock::now();
    actual.clear();
    std::vector<P> expected;
    index.ball_squared(center, radius2, [&](auto id) { actual.push_back(points.at(id)); });
    if(!benchmark_mode)query_seconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-query_start).count();
    answers += actual.size();
    if (benchmark_mode) continue;
    // Independent full scan, accumulating rather than subtracting distances.
    for (std::size_t i = 0; i < n; ++i) {
      W distance = 0;
      for (std::size_t a = 0; a < D; ++a) {
        auto delta = std::int64_t(points[i][a]) - center[a];
        auto mag = std::uint64_t(delta < 0 ? -delta : delta);
        distance += W(mag) * mag;
      }
      if (distance <= radius2) expected.push_back(points[i]);
    }
    std::sort(actual.begin(), actual.end());
    std::sort(expected.begin(), expected.end());
    if (actual != expected) {
      std::cout << "{\"status\":\"incorrect\",\"query\":" << j << "}\n";
      return 2;
    }
  }
  if(benchmark_mode)query_seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-batch_start).count();
  std::cout << "{\"status\":\"success\",\"points\":" << n
            << ",\"queries\":" << q << ",\"answers\":" << answers
            << ",\"build_seconds\":" << build_seconds << ",\"query_seconds\":" << query_seconds << "}\n";
  return 0;
}
int main(int argc, char** argv) {
  try {
    if (argc < 2 || argc > 4) throw std::runtime_error("Usage: ddim_check CASE.bin [ALGORITHM] [--bench]");
    benchmark_mode=argc==4 && std::string(argv[3])=="--bench";
    const std::string algorithm = argc >= 3 ? argv[2] : "chan_sss_ddim_MANUALLY_ADAPTED";
    std::ifstream in(argv[1], std::ios::binary);
    auto d = read<std::uint32_t>(in);
    switch (d) {
#define DIM(N) case N: \
  if (algorithm == "chan_sss_ddim_MANUALLY_ADAPTED") return run<N,diskreport::ddim::ChanManuallyAdapted>(in); \
  if (algorithm == "stann_fr_ddim_MANUALLY_ADAPTED") return run<N,diskreport::ddim::StannManuallyAdapted>(in); \
  throw std::runtime_error("Unknown algorithm");
      DIM(1) DIM(2) DIM(3) DIM(4) DIM(5) DIM(6) DIM(7) DIM(8)
      DIM(9) DIM(10) DIM(11) DIM(12) DIM(13) DIM(14) DIM(15) DIM(16) DIM(32)
#undef DIM
      default: throw std::runtime_error("Unsupported campaign dimension");
    }
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
