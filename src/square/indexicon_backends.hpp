#pragma once
#include <cfloat>
#include <cmath>
#if defined(SQ_INDEXICON_RPOINT)
#include "rtree_point.hpp"
#elif defined(SQ_INDEXICON_RMBR)
#include "rtree_mbr.hpp"
#elif defined(SQ_INDEXICON_KD)
#include "kdtree.hpp"
#elif defined(SQ_INDEXICON_QUAD)
#include "quadtree.hpp"
#elif defined(SQ_INDEXICON_MXCIF)
#include "mxcif_quadtree.hpp"
#elif defined(SQ_INDEXICON_OCT)
#include "octtree.hpp"
#endif

namespace square {
// The point/box constructor is the only conversion. All candidates are answers.
inline double coord(U32 x){return std::ldexp(double(x),-32);}

template<unsigned D,class Tree,class Entry,class QueryBox>
int indexicon_query(const Case<D>& data,const std::string& name,bool verify,Clock::time_point start,Entry entry,QueryBox box) {
    using T=decltype(entry(Id{},std::declval<const Point<D>&>()));
    std::vector<T> records; records.reserve(data.points.size());
    for(Id i=0;i<data.points.size();++i)records.push_back(entry(i,data.points[i]));
    // The upstream packed constructors assume a nonempty iterator range.
    std::unique_ptr<Tree> tree;
    if(!records.empty())tree=std::make_unique<Tree>(records.begin(),records.end());
    return measure<true,D,Id>(data,name,verify,seconds(start),[&](auto& b,auto& out){
        if(tree)tree->query(box(b),std::back_inserter(out));
    },[](Id i){return i;});
}

template<unsigned D> int indexicon(const Case<D>& data,const std::string& name,bool verify,Clock::time_point start) {
#if defined(SQ_INDEXICON_RPOINT) || defined(SQ_INDEXICON_RMBR)
    using P=RPoint<double,D>;using B=RBox<double,D>;
    auto point=[](const Point<D>& p){P x;for(unsigned a=0;a<D;++a)x[a]=coord(p[a]);return x;};
    auto box=[&](const Box<D>& b){return B(point(b.lo),point(b.hi));};
#if defined(SQ_INDEXICON_RPOINT)
    auto entry=[&](Id id,const Point<D>& p){return std::pair{point(p),id};};
    if(name=="indexicon_rtree_point")return indexicon_query<D,RTree<Id,double,D>>(data,name,verify,start,entry,box);
#else
    auto entry=[&](Id id,const Point<D>& p){auto x=point(p);return std::pair{B(x,x),id};};
    if(name=="indexicon_rtree_mbr")return indexicon_query<D,RTree<Id,double,D>>(data,name,verify,start,entry,box);
#endif
#elif defined(SQ_INDEXICON_KD)
    auto point=[](const Point<D>& p){KDPoint<double,D> x;for(unsigned a=0;a<D;++a)x[a]=coord(p[a]);return x;};
    auto entry=[&](Id id,const Point<D>& p){return std::pair{point(p),id};};
    auto box=[&](const Box<D>& b){KDRegion<double,D> r;r.low=point(b.lo);r.high=point(b.hi);return r;};
    auto kd=[&]<KDSplit S>(){return indexicon_query<D,KDTree<double,Id,D,128,S>>(data,name,verify,start,entry,box);};
    if(name=="indexicon_kdtree_adaptive")return kd.template operator()<KDSplit::ADAPTIVE>();
    if(name=="indexicon_kdtree_round_robin")return kd.template operator()<KDSplit::ROUND_ROBIN>();
    if(name=="indexicon_kdtree_longest_axis")return kd.template operator()<KDSplit::LONGEST_AXIS>();
#elif defined(SQ_INDEXICON_QUAD)
    if constexpr(D==2){
        auto entry=[](Id id,const Point<2>& p){return QPoint<double,Id>(id,coord(p[0]),coord(p[1]));};
        auto box=[](const Box<2>& b){return QBox<double,Id>(coord(b.lo[0]),coord(b.lo[1]),coord(b.hi[0]),coord(b.hi[1]),0);};
        auto quad=[&]<SplitStrategy S>(){return indexicon_query<2,QuadTreeNode<double,Id,128,S>>(data,name,verify,start,entry,box);};
        if(name=="indexicon_quadtree_pr")return quad.template operator()<SplitStrategy::PR>();
        if(name=="indexicon_quadtree_pseudo_median")return quad.template operator()<SplitStrategy::PSEUDO_MEDIAN>();
        if(name=="indexicon_quadtree_longest_axis")return quad.template operator()<SplitStrategy::POINT_LONGEST_AXIS>();
    }
#elif defined(SQ_INDEXICON_MXCIF)
    if constexpr(D==2){
        auto entry=[](Id id,const Point<2>& p){auto x=coord(p[0]),y=coord(p[1]);return MXCIFBox<double,Id>(id,x,y,x,y);};
        auto box=[](const Box<2>& b){return MXCIFBox<double,Id>(0,coord(b.lo[0]),coord(b.lo[1]),coord(b.hi[0]),coord(b.hi[1]));};
        if(name=="indexicon_mxcif_quadtree")return indexicon_query<2,MXCIFQuadTreeNode<double,Id>>(data,name,verify,start,entry,box);
    }
#elif defined(SQ_INDEXICON_OCT)
    if constexpr(D==3){
        auto entry=[](Id id,const Point<3>& p){return OPoint<double,Id>(id,coord(p[0]),coord(p[1]),coord(p[2]));};
        auto box=[](const Box<3>& b){return OBox<double,Id>(coord(b.lo[0]),coord(b.lo[1]),coord(b.lo[2]),coord(b.hi[0]),coord(b.hi[1]),coord(b.hi[2]));};
        if(name=="indexicon_octree")return indexicon_query<3,OctreeNode<double,Id>>(data,name,verify,start,entry,box);
    }
#endif
    throw std::runtime_error("Unsupported Indexicon variant or dimension");
}
} // namespace square
