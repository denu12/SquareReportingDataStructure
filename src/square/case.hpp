#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace square {
using U32 = std::uint32_t;
using Id = std::size_t;
template<unsigned D> using Point = std::array<U32, D>;
template<unsigned D> struct Box {
    Point<D> lo, hi;
    bool contains(const Point<D>& p) const {
        for (unsigned a=0; a<D; ++a) if (p[a]<lo[a] || p[a]>hi[a]) return false;
        return true;
    }
};

template<class T> T read(std::istream& in) {
    T v{};
    if (!in.read(reinterpret_cast<char*>(&v),sizeof v)) throw std::runtime_error("Truncated square case");
    return v;
}

template<unsigned D> struct Case {
    std::vector<Point<D>> points;
    std::vector<Box<D>> boxes;
};
template<unsigned D> Case<D> load(std::istream& in) {
    static_assert(std::endian::native==std::endian::little);
    const auto n=read<std::uint64_t>(in), q=read<std::uint64_t>(in);
    Case<D> data;
    if (n>data.points.max_size() || q>data.boxes.max_size()) throw std::runtime_error("Case too large");
    data.points.resize(n); data.boxes.resize(q);
    auto point=[&](auto& p){for (auto& x:p) x=read<U32>(in);};
    for (auto& p:data.points) point(p);
    for (auto& b:data.boxes) {
        point(b.lo); point(b.hi);
        for (unsigned a=0;a<D;++a)
            if (b.lo[a]>b.hi[a] || b.hi[a]-b.lo[a]!=b.hi[0]-b.lo[0])
                throw std::runtime_error("Query must be a closed square or cube");
    }
    if (in.peek()!=EOF) throw std::runtime_error("Trailing square case bytes");
    return data;
}

using Clock = std::chrono::steady_clock;
inline double seconds(Clock::time_point t) {return std::chrono::duration<double>(Clock::now()-t).count();}

// Read the returned sequence only after the timed query. Verify IDs as IDs,
// and coordinate results as a multiset, so coincident input records count.
template<bool IDs,unsigned D,class Search,class Convert>
int measure_native(const Case<D>& data, std::string name, bool verify, double build,
                   Search search, Convert convert, const char* path="native_box") {
    double time=0;
    std::uint64_t m=0;
    auto batch=Clock::now();
    for (Id j=0;j<data.boxes.size();++j) {
        auto t=verify?Clock::now():batch;
        const auto hits=search(data.boxes[j]);
        asm volatile("" :: "r"(hits.data()), "r"(hits.size()) : "memory");
        if (verify) time+=seconds(t);
        m+=hits.size();
        if (verify) {
            if constexpr(IDs) {
                std::vector<Id> got, expected;
                for (auto& x:hits) got.push_back(convert(x));
                for (Id i=0;i<data.points.size();++i) if (data.boxes[j].contains(data.points[i])) expected.push_back(i);
                std::sort(got.begin(),got.end());
                if (got!=expected) {std::cerr<<name<<": wrong IDs at query "<<j<<'\n';return 2;}
            } else {
                std::vector<Point<D>> got, expected;
                for (auto& x:hits) got.push_back(convert(x));
                for (const auto& p:data.points) if (data.boxes[j].contains(p)) expected.push_back(p);
                std::sort(got.begin(),got.end()); std::sort(expected.begin(),expected.end());
                if (got!=expected) {std::cerr<<name<<": wrong point multiset at query "<<j<<'\n';return 2;}
            }
        }
    }
    if (!verify) time=seconds(batch);
    std::cout<<"{\"status\":\"success\",\"algorithm\":\""<<name<<"\",\"dimension\":"<<D
        <<",\"n\":"<<data.points.size()<<",\"queries\":"<<data.boxes.size()<<",\"m\":"<<m
        <<",\"build_seconds\":"<<build<<",\"query_seconds\":"<<time
        <<",\"output\":\""<<(IDs?"ids":"points")<<"\",\"query_path\":\""<<path
        <<"\",\"verified\":"<<(verify?"true":"false")<<"}\n";
    return 0;
}

template<bool IDs,unsigned D,class Native,class Search,class Convert>
int measure(const Case<D>& data, std::string name, bool verify, double build,
            Search search, Convert convert, const char* path="native_box") {
    std::vector<Native> hits;
    return measure_native<IDs,D>(data,name,verify,build,[&](const Box<D>& b){
        hits.clear();search(b,hits);return std::span<const Native>(hits);
    },convert,path);
}
} // namespace square
