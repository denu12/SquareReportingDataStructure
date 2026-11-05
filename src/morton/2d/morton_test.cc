#include "morton.cc"
#include <iostream>
#include <iterator>
#include <random>
using namespace diskreport::morton_Morton;

int main() {
  std::mt19937 rng(713);
  size_t checks = 0;
  auto keys = [](const auto& points) {
    std::vector<uint64_t> result;
    for (auto p : points) result.push_back((uint64_t(p.x) << 32) | p.y);
    std::sort(result.begin(), result.end());
    return result;
  };
  for (unsigned n : {0, 1, 7, 15, 31, 32, 33, 63, 64, 65, 128, 129, 512, 513, 4096}) {
    for (bool clustered : {false, true}) {
      std::vector<Point> points;
      for (unsigned i = 0; i < n; ++i)
        points.push_back({uint32_t(rng()) & (clustered ? 255u : UINT32_MAX),
                          uint32_t(rng()) & (clustered ? 255u : UINT32_MAX)});
      if (n > 32) {
        points[0] = points[1] = {0, 0};
        points[2] = points[3] = {UINT32_MAX, UINT32_MAX};
        points[4] = {3, 4};
        points[5] = {uint32_t(1) << 31, uint32_t(1) << 31};
      }
      Morton table(points);
      Morton scalar(points);
      for (unsigned q = 0; q < 60; ++q) {
        Point center = q % 3 == 0 ? Point{0, 0} : q % 3 == 1
            ? Point{UINT32_MAX, UINT32_MAX} : Point{uint32_t(rng()), uint32_t(rng())};
        for (Wide radius : {Wide(0), Wide(24), Wide(25), Wide(26), Wide(rng())*rng(),
                            (Wide(1)<<64)-1, Wide(1)<<64, (Wide(1)<<64)+1, ~Wide(0), Wide(2)*UINT32_MAX*UINT32_MAX}) {
          Circle circle{center, radius};
          std::vector<Point> expected{{77, 88}}, actual = expected, original = expected;
          // Independent exact integer oracle, preserving duplicate multiplicities.
          for (auto p : points) {
            const __int128 dx = __int128(p.x) - center.x, dy = __int128(p.y) - center.y;
            if (Wide(dx*dx + dy*dy) <= radius) expected.push_back(p);
          }
          table.query(circle, std::back_inserter(actual));
          scalar.query(circle, std::back_inserter(original));
          if (keys(actual) != keys(expected) || keys(original) != keys(expected))
            throw std::runtime_error("Incorrect circle result");
          ++checks;
        }
        // Rectangle queries must still use the original closed-box semantics.
        Box box{uint32_t(rng()), uint32_t(rng()), uint32_t(rng()), uint32_t(rng())};
        std::vector<Point> expected, actual;
        for (auto p : points)
          if (p.x >= box.p.x && p.x <= box.q.x && p.y >= box.p.y && p.y <= box.q.y)
            expected.push_back(p);
        table.query(box, std::back_inserter(actual));
        if (keys(actual) != keys(expected)) throw std::runtime_error("Incorrect rectangle result");
        ++checks;
      }
      std::vector<Point> actual;
      table.query(Point{0, 0}, UINT64_MAX, std::back_inserter(actual));
      if (keys(actual) != keys(points)) throw std::runtime_error("Integer radius overload");
    }
  }
  std::cout << checks << " Morton queries passed\n";
}
