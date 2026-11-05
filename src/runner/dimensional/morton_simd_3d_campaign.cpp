#include "../../morton/3d/mortonSIMD3D.hpp"
#include <iterator>
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
template<std::size_t D> int run(std::istream& in) {
  using I = diskreport::morton3d::MortonSIMD3D;
  using P = diskreport::morton3d::Point;
  using W = unsigned __int128;
  auto n = read<std::uint64_t>(in), q = read<std::uint64_t>(in);
  std::vector<P> points(n);
  for (auto& p : points) for (auto& x : p) x = read<std::uint32_t>(in);
  std::vector<std::pair<P,W>> queries(q);
  for(auto& [center,radius2]:queries){
    for(auto& x:center)x=read<std::uint32_t>(in);
    radius2=read<std::uint64_t>(in);radius2|=W(read<std::uint64_t>(in))<<64;
  }
  if (in.peek()!=EOF) throw std::runtime_error("Trailing input data");
  const auto build_start=std::chrono::steady_clock::now();
  I index(points);
  const double build_seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-build_start).count();
  double query_seconds=0;
  std::size_t answers = 0;
  diskreport::morton3d::Results actual;
  const auto batch_start=std::chrono::steady_clock::now();
  for (std::size_t j = 0; j < q; ++j) {
    const auto& [center,radius2]=queries[j];
    const auto query_start=benchmark_mode?batch_start:std::chrono::steady_clock::now();
    actual.clear();
    std::vector<P> expected;
    index.query_squared(center, radius2, actual);
    asm volatile("" : : "r"(actual.data()), "r"(actual.size()) : "memory");
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
    std::vector<P> sorted(actual.begin(),actual.end());
    std::sort(sorted.begin(), sorted.end());
    std::sort(expected.begin(), expected.end());
    if (sorted != expected) {
      std::cout << "{\"status\":\"incorrect\",\"query\":" << j << "}\n";
      return 2;
    }
  }
  if(benchmark_mode)query_seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-batch_start).count();
  std::cout << "{\"status\":\"success\",\"points\":" << n
            << ",\"queries\":" << q << ",\"answers\":" << answers
            << ",\"dimension\":3,\"output_contract\":\"integer_points\",\"membership_contract\":\"closed_integer_ball\""
            << ",\"build_seconds\":" << build_seconds << ",\"query_seconds\":" << query_seconds << "}\n";
  return 0;
}
int main(int argc, char** argv) {
  try {
    if (argc < 2 || argc > 4) throw std::runtime_error("Usage: morton_simd_3d_check CASE.bin [ALGORITHM] [--bench]");
    if(argc==4 && std::string(argv[3])!="--bench") throw std::runtime_error("Unknown option");
    benchmark_mode=argc==4 && std::string(argv[3])=="--bench";
    const std::string algorithm = argc >= 3 ? argv[2] : "MortonSIMD3D";
    if (algorithm != "MortonSIMD3D") throw std::runtime_error("Unknown algorithm");
    std::ifstream in(argv[1], std::ios::binary);
    auto d = read<std::uint32_t>(in);
    switch (d) {
#define DIM(N) case N: return run<N>(in);
      DIM(3)
#undef DIM
      default: throw std::runtime_error("Unsupported campaign dimension");
    }
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
