#pragma once

#if defined(SQ_PAM)
#include <pam/pam.h>
#include "range_utils.h"
#include "range_tree.h"
#elif defined(SQ_PARGEO)
#include "pargeo/kdTree.h"
#elif defined(SQ_PKD)
#include "cpdd/basic_point.h"
#include "cpdd/cpdd.h"
#endif

namespace square {
#if defined(SQ_PAM)
using PamIndex = ::RangeQuery<U32, U32>;

struct PamReporter {
    U32 low, high;
    std::vector<Point<2>>& hits;

    void add_entry(PamIndex::main_entry entry) {
        const auto [x, y] = entry.first;
        if (low <= y && y <= high) hits.insert(hits.end(), entry.second, {x, y});
    }

    void add_aug_val(PamIndex::inner_map map) {
        auto range = PamIndex::inner_map::range(map, {low, 0}, {high, UINT32_MAX});
        PamIndex::inner_map::foreach_index(range, [&](const auto& entry, std::size_t) {
            hits.insert(hits.end(), entry.second, {entry.first.second, entry.first.first});
        }, 0, std::numeric_limits<std::size_t>::max());
    }
};
#elif defined(SQ_PARGEO)
// ParGeo adds the endpoints of each split before widening the midpoint.
using ParGeoPoint = pargeo::_point<2, double, pargeo::_empty, pargeo::_empty>;
using ParGeoNode = pargeo::kdNode<2, ParGeoPoint>;

inline void report_pargeo(ParGeoNode& node, const ParGeoPoint& low,
                          const ParGeoPoint& high, std::vector<ParGeoPoint>& hits) {
    const int relation = node.boxCompare(low, high, node.getMin(), node.getMax());
    if (relation == node.boxExclude) return;
    if (relation == node.boxInclude || node.isLeaf()) {
        for (std::size_t i = 0; i < node.size(); ++i) {
            const auto& p = *node.getItem(i);
            if (relation == node.boxInclude ||
                (low.x[0] <= p.x[0] && p.x[0] <= high.x[0] &&
                 low.x[1] <= p.x[1] && p.x[1] <= high.x[1])) {
                hits.push_back(p);
            }
        }
    } else {
        report_pargeo(*node.L(), low, high, hits);
        report_pargeo(*node.R(), low, high, hits);
    }
}
#endif

inline int rectangle(const Case<2>& data, const std::string& name, bool verify,
                     Clock::time_point start) {
#if defined(SQ_PAM)
    if (name == "pam") {
        if (!data.points.empty()) PamIndex::reserve(data.points.size());
        auto entries = parlay::tabulate(data.points.size(), [&](std::size_t i) -> PamIndex::main_entry {
            return {{data.points[i][0], data.points[i][1]}, 1};
        });
        PamIndex::outer_map index(entries, [](U32 a, U32 b) { return a + b; });
        return measure<false, 2, Point<2>>(data, name, verify, seconds(start),
            [&](const auto& box, auto& hits) {
                PamReporter report{box.lo[1], box.hi[1], hits};
                index.range_sum({box.lo[0], 0}, {box.hi[0], UINT32_MAX}, report);
            }, [](const auto& p) { return p; });
    }
#elif defined(SQ_PARGEO)
    if (name == "pargeo") {
        auto native = [](const Point<2>& p) {
            double xy[]{double(p[0]), double(p[1])};
            return ParGeoPoint(xy);
        };
        auto points = parlay::tabulate(data.points.size(), [&](std::size_t i) {
            return native(data.points[i]);
        });
        parlay::sequence<ParGeoPoint*> items(points.size());
        std::unique_ptr<ParGeoNode, decltype(&std::free)> tree(
            points.empty() ? nullptr : pargeo::buildKdt<2>(points, false, false, &items), &std::free);
        return measure<false, 2, ParGeoPoint>(data, name, verify, seconds(start),
            [&](const auto& box, auto& hits) {
                if (tree) report_pargeo(*tree, native(box.lo), native(box.hi), hits);
            }, [](const auto& p) { return Point<2>{U32(p.x[0]), U32(p.x[1])}; },
            "local_box_traversal");
    }
#elif defined(SQ_PKD)
    if (name == "pkd") {
        using PkdPoint = cpdd::PointType<U32, 2>;
        auto points = parlay::tabulate(data.points.size(), [&](std::size_t i) {
            return PkdPoint(data.points[i]);
        });
        cpdd::ParallelKDtree<PkdPoint> index;
        if (!points.empty()) index.build(parlay::make_slice(points), 2);
        parlay::sequence<PkdPoint> hits(points.size());
        return measure_native<false, 2>(data, name, verify, seconds(start),
            [&](const auto& box) {
                const auto count = points.empty() ? 0 : index.range_query_serial(
                    {PkdPoint(box.lo), PkdPoint(box.hi)}, parlay::make_slice(hits));
                return std::span<const PkdPoint>(hits.data(), count);
            }, [](const auto& p) { return p.pnt; });
    }
#endif
    throw std::runtime_error("Unknown rectangle backend");
}
} // namespace square
