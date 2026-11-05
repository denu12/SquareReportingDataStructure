#include "app/algorithms/static/esa2026.hh"
#include <iostream>
#include <random>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <tuple>

using namespace diskreport::sfc::app::algorithms::sfc;
using StannFR=Esa2026<0>;
using P = StannFR::PointType;
using Q = StannFR::QueryType;
size_t checks = 0;
template<class Backend> void check(const std::vector<P>& points, const Backend& tree, Q q) {
  std::vector<P> got, expected;
  tree.query(q, std::back_inserter(got));
  for (auto p : points)
    if (p.x >= q.p.x && p.x <= q.q.x && p.y >= q.p.y && p.y <= q.q.y)
      expected.push_back(p);
  auto less = [](P a, P b) { return std::tie(a.x,a.y) < std::tie(b.x,b.y); };
  std::sort(got.begin(), got.end(), less);
  std::sort(expected.begin(), expected.end(), less);
  if (got.size() != expected.size()) throw std::runtime_error("Result count mismatch");
  for (size_t i = 0; i < got.size(); ++i)
    if (got[i].x != expected[i].x || got[i].y != expected[i].y)
      { std::cerr << "query " << q.p.x << "," << q.p.y << "--" << q.q.x << "," << q.q.y << " got " << got[i].x << "," << got[i].y << " expected " << expected[i].x << "," << expected[i].y << " n=" << points.size() << " check=" << checks << std::endl; throw std::runtime_error("Reported point mismatch"); }
  ++checks;
}
template<class Backend>void run() {
  constexpr auto M = std::numeric_limits<uint32_t>::max();
  std::mt19937_64 rng(827361);
  for (uint64_t seed = 0; seed < 24; ++seed) {
    std::vector<P> points;
    for (size_t i=0; i < (seed * 11) % 250; ++i) {
      uint32_t x=rng(), y=rng();
      if (seed % 3 == 0) { x %= 16; y %= 16; }
      if (seed % 3 == 1) { x = (x % 32) + (1u << 31) - 16; y = M - y % 32; }
      points.push_back({x,y});
    }
    if (seed % 5 != 0) {
      for (auto p : std::vector<P>{{0,0},{M,M},{0,M},{M,0},{1u<<31,1u<<31}}) {
        points.push_back(p); points.push_back(p);
      }
    }
    Backend tree(points);
    check(points,tree,Q(0,0,M,M));
    check(points,tree,Q(1,1,0,0));
    for (auto p : points) {
      check(points,tree,Q(p,p));
      check(points,tree,Q(0,p.y,M,p.y));
      check(points,tree,Q(p.x,0,p.x,M));
    }
    // Inclusive square corners, half-integer centers, and empty near misses.
    for (auto p : points) {
      if (p.x < M && p.y < M) check(points,tree,Q(p.x,p.y,p.x+1,p.y+1));
      if (p.x && p.y) check(points,tree,Q(p.x-1,p.y-1,p.x,p.y));
    }
    for (int j=0;j<200;++j) {
      uint32_t x=rng(),y=rng(),u=rng(),v=rng();
      if (seed%3==0 && j%2) { x%=32;y%=32;u%=32;v%=32; }
      check(points,tree,Q(std::min(x,u),std::min(y,v),std::max(x,u),std::max(y,v)));
    }
  }
  // An array consisting entirely of duplicates exercises the zero-width cell.
  std::vector<P> identical(1000,P{M,M});
  { Backend same(identical);
  check(identical,same,Q(M,M,M,M));
  check(identical,same,Q(0,0,M-1,M-1));
  }
  std::cout << checks << " exact queries matched brute force, including duplicates and uint32 boundaries.\n";
}

int main(){
std::cerr<<"Backend sprk\n";run<Esa2026<0>>();
std::cerr<<"Backend kiddo\n";run<Esa2026<1>>();
std::cerr<<"Backend nabo\n";run<Esa2026<2>>();
std::cerr<<"Backend neighbourhood\n";run<Esa2026<3>>();
std::cerr<<"Backend vptree\n";run<Esa2026<4>>();
std::cerr<<"Backend orthtree\n";run<Esa2026<5>>();
std::cerr<<"Backend grid\n";run<Esa2026<6>>();
std::cerr<<"Backend sklearn_kd\n";run<Esa2026<7>>();
std::cerr<<"Backend sklearn_ball\n";run<Esa2026<8>>();
std::cerr<<"Backend snn\n";run<Esa2026<9>>();
std::cerr<<"Backend nanoflann\n";run<EsaNanoflann>();}

