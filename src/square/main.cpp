#include "case.hpp"
#include <iterator>
#include <memory>
#include <utility>

#if defined(SQ_MORTON)
#include "morton/2d/morton.cc"
#include "morton/3d/morton3D.cc"
#include "morton/2d/mortonplus.hpp"
#include "morton/3d/mortonplus3d.hpp"
#elif defined(SQ_MORTON_SIMD)
#include "morton/2d/mortonSIMD.cc"
#elif defined(SQ_BOOST)
#include <boost/geometry.hpp>
#include <boost/geometry/index/rtree.hpp>
#elif defined(SQ_PH_TREE)
#include "phtree/phtree_multimap.h"
#elif defined(SQ_INDEXICON)
#include "indexicon_backends.hpp"
#elif defined(SQ_OTHER)
#include "other_backends.hpp"
#endif

namespace square {
template<unsigned D>
int run(const Case<D>& data, const std::string& name, bool verify) {
    const auto start=Clock::now();
#if defined(SQ_MORTON) || defined(SQ_MORTON_SIMD)
    if constexpr(D==2) {
        auto morton=[&]<class T>(){
            using P=typename T::PointType;
            std::vector<P> points; points.reserve(data.points.size());
            for(const auto& p:data.points) points.push_back(P{p[0],p[1]});
            T tree(points);
            return measure<false,D,P>(data,name,verify,seconds(start),[&](auto& b,auto& out){
                tree.query(typename T::QueryType(b.lo[0],b.lo[1],b.hi[0],b.hi[1]),std::back_inserter(out));
            },[](auto& p){return Point<D>{p.x,p.y};});
        };
#if defined(SQ_MORTON)
        if(name=="morton") return morton.template operator()<diskreport::morton_Morton::Morton>();
#else
        if(name=="morton_simd") return morton.template operator()<diskreport::morton_MortonSIMD::MortonSIMD>();
#endif
    }
#if defined(SQ_MORTON)
    auto plus=[&]<class Tree,class Results>(){
        Tree tree(std::span<const Point<D>>(data.points)); Results out;
        return measure_native<false,D>(data,name,verify,seconds(start),[&](auto& b){
            out.clear();tree.query_box(b.lo,b.hi,out);return out.view();
        },[](auto& p){return p;});
    };
    if constexpr(D==2)if(name=="morton_plus")return plus.template operator()<mortonplus::MortonPlus,mortonplus::Results>();
    if constexpr(D==3)if(name=="morton_plus_3d")return plus.template operator()<mortonplus3d::MortonPlus,mortonplus3d::Results>();
    if constexpr(D==3) if(name=="morton_3d") {
        diskreport::morton3d::Morton3D tree(data.points);
        return measure<false,D,Point<D>>(data,name,verify,seconds(start),[&](auto& b,auto& out){
            tree.query_box(b.lo,b.hi,std::back_inserter(out));
        },[](auto& p){return p;});
    }
#endif
#elif defined(SQ_BOOST)
    namespace bg=boost::geometry; namespace bgi=bg::index;
    using P=bg::model::point<U32,D,bg::cs::cartesian>;
    auto point=[]<std::size_t... A>(const Point<D>& p,std::index_sequence<A...>){P v; (bg::set<A>(v,p[A]),...); return v;};
    auto native=[&](const Point<D>& p){return point(p,std::make_index_sequence<D>{});};
    auto boost=[&]<class Params>() {
        std::vector<P> points;points.reserve(data.points.size());
        for(auto& p:data.points) points.push_back(native(p));
        bgi::rtree<P,Params> tree(points.begin(),points.end());
        return measure<false,D,P>(data,name,verify,seconds(start),[&](auto& b,auto& out){
            tree.query(bgi::covered_by(bg::model::box<P>{native(b.lo),native(b.hi)}),std::back_inserter(out));
        },[](const P& p){
            return [&]<std::size_t... B>(std::index_sequence<B...>){return Point<D>{bg::get<B>(p)...};}(std::make_index_sequence<D>{});
        });
    };
    if(name=="boost_linear") return boost.template operator()<bgi::linear<16>>();
    if(name=="boost_quadratic") return boost.template operator()<bgi::quadratic<16>>();
    if(name=="boost_rstar") return boost.template operator()<bgi::rstar<16>>();
#elif defined(SQ_PH_TREE)
    if(name=="phtree") {
        using P=improbable::phtree::PhPoint<D>;
        auto point=[](auto& p){P v;std::copy(p.begin(),p.end(),v.begin());return v;};
        improbable::phtree::PhTreeMultiMap<D,Id> tree;
        for(Id i=0;i<data.points.size();++i) tree.emplace(point(data.points[i]),i);
        return measure<true,D,Id>(data,name,verify,seconds(start),[&](auto& b,auto& out){
            for(auto it=tree.begin_query({point(b.lo),point(b.hi)});it!=tree.end();++it) out.push_back(*it);
        },[](Id i){return i;});
    }
#elif defined(SQ_INDEXICON)
    return indexicon<D>(data,name,verify,start);
#elif defined(SQ_OTHER)
    return other<D>(data,name,verify,start);
#else
    if(name=="scan") return measure<true,D,Id>(data,name,verify,0,[&](auto& b,auto& out){
        for(Id i=0;i<data.points.size();++i) if(b.contains(data.points[i]))out.push_back(i);
    },[](Id i){return i;},"scan");
#endif
    throw std::runtime_error("Algorithm or dimension is unsupported by this executable");
}
} // namespace square

int main(int argc,char** argv) {
    try {
        if(argc!=3 && argc!=4)throw std::runtime_error("Usage: EXECUTABLE CASE ALGORITHM [--bench]");
        const bool verify=argc==3;
        if(!verify && std::string(argv[3])!="--bench") throw std::runtime_error("Unknown mode");
        std::ifstream in(argv[1],std::ios::binary);
        if(square::read<std::array<char,8>>(in)!=std::array<char,8>{'S','Q','U','A','R','E','1','\0'})
            throw std::runtime_error("Not a SQUARE1 input");
        const auto d=square::read<square::U32>(in);
        if(d==2)return square::run(square::load<2>(in),argv[2],verify);
        if(d==3)return square::run(square::load<3>(in),argv[2],verify);
        throw std::runtime_error("Supported dimensions are 2 and 3");
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
