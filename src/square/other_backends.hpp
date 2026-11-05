#pragma once
#include <cmath>
#if defined(SQ_CGAL)
#include <CGAL/Simple_cartesian.h>
#include <CGAL/Kd_tree.h>
#include <CGAL/Fuzzy_iso_box.h>
#include <CGAL/Search_traits_2.h>
#include <CGAL/Search_traits_3.h>
#include <CGAL/Range_segment_tree_traits.h>
#include <CGAL/Range_tree_k.h>
#include <map>
#elif defined(SQ_SPATIALINDEX)
#include <spatialindex/SpatialIndex.h>
#elif defined(SQ_PSI)
#include "psi/kd_tree.h"
#include "psi/orth_tree.h"
#include "psi/p_tree.h"
#include "psi/dependence/splitter.h"
#elif defined(SQ_PGM)
#include "pgm/pgm_index_variants.hpp"
#elif defined(SQ_NANOFLANN)
#include "nanoflann.hpp"
#elif defined(SQ_THST)
#include "THST/QuadTree.h"
#include "THST/RTree.h"
#endif

namespace square {
#if defined(SQ_SPATIALINDEX)
template<unsigned D> auto region(const Point<D>& lo,const Point<D>& hi){
    std::array<double,D> a,b;
    std::copy(lo.begin(),lo.end(),a.begin());std::copy(hi.begin(),hi.end(),b.begin());
    return SpatialIndex::Region(a.data(),b.data(),D);
}
template<unsigned D> struct Stream:SpatialIndex::IDataStream {
    const Case<D>& data;Id next=0;
    explicit Stream(const Case<D>& d):data(d){}
    SpatialIndex::IData* getNext() override {
        if(!hasNext())return nullptr;
        auto& p=data.points[next];auto b=region<D>(p,p);
        return new SpatialIndex::RTree::Data(0,nullptr,b,next++);
    }
    bool hasNext()override{return next<data.points.size();}
    std::uint32_t size()override{return data.points.size();}
    void rewind()override{next=0;}
};
struct Visitor:SpatialIndex::IVisitor {
    std::vector<Id>& hits;
    explicit Visitor(std::vector<Id>& v):hits(v){}
    void visitNode(const SpatialIndex::INode&)override{}
    void visitData(const SpatialIndex::IData& p)override{hits.push_back(p.getIdentifier());}
    void visitData(std::vector<const SpatialIndex::IData*>& v)override{for(auto* p:v)visitData(*p);}
};
#elif defined(SQ_PSI)
struct PsiId {
    using id_type=Id;using curve_code_type=std::uint64_t;
    curve_code_type code=0;id_type id=0;
    void set_member(curve_code_type x){code=x;}
    bool operator<(const PsiId& b)const{return code==b.code?id<b.id:code<b.code;}
    bool operator==(const PsiId& b)const{return id==b.id;}
    friend std::ostream& operator<<(std::ostream& o,const PsiId& p){return o<<p.id;}
};
template<unsigned D> using PsiPoint=psi::aug_point<std::int64_t,D,PsiId>;
template<unsigned D,class Tree>
int psi_query(const Case<D>& data,const std::string& name,bool verify,Clock::time_point start) {
    using P=PsiPoint<D>;
    parlay::sequence<P> points(data.points.size()),out(data.points.size());
    for(Id i=0;i<points.size();++i){for(unsigned a=0;a<D;++a)points[i][a]=data.points[i][a];points[i].aug.id=i;}
    Tree tree;
    if(!points.empty())tree.build(parlay::make_slice(points));
    return measure_native<false,D>(data,name,verify,seconds(start),[&](auto& b){
        if(points.empty())return std::span<const P>{};
        typename Tree::box_type box;
        for(unsigned a=0;a<D;++a){box.first[a]=b.lo[a];box.second[a]=b.hi[a];}
        auto [count,logger]=tree.range_query(box,parlay::make_slice(out));
        return std::span<const P>(out.data(),count);
    },[](const P& p){Point<D> x;for(unsigned a=0;a<D;++a)x[a]=p[a];return x;});
}
#elif defined(SQ_NANOFLANN)
template<unsigned D> struct Cloud {
    const std::vector<Point<D>>& points;
    std::size_t kdtree_get_point_count()const{return points.size();}
    double kdtree_get_pt(Id i,unsigned a)const{return points[i][a];}
    template<class B>bool kdtree_get_bbox(B&)const{return false;}
};
#elif defined(SQ_THST)
struct ThstPoint {
    using ElementType=std::int64_t;
    std::int64_t min[2];
    Id id{};
    ThstPoint()=default;
    ThstPoint(std::int64_t x,std::int64_t y,Id i):min{x,y},id(i){}
    // THST deduplicates equal values. Distinct input occurrences need IDs.
    bool operator==(const ThstPoint& b)const{return id==b.id;}
    bool operator<(const ThstPoint& b)const{return id<b.id;}
    explicit operator spatial::BoundingBox<std::int64_t,2>()const{return {min,min};}
};
struct ThstIndexable {
    const std::int64_t* min(const ThstPoint& p)const{return p.min;}
    const std::int64_t* max(const ThstPoint& p)const{return p.min;}
};
#endif

template<unsigned D>
int other(const Case<D>& data,const std::string& name,bool verify,Clock::time_point start) {
#if defined(SQ_CGAL)
    using K=CGAL::Simple_cartesian<double>;
    using Traits=std::conditional_t<D==2,CGAL::Search_traits_2<K>,CGAL::Search_traits_3<K>>;
    using P=typename Traits::Point_d;
    auto point=[](const Point<D>& p,double offset=0){if constexpr(D==2)return P(p[0]+offset,p[1]+offset);else return P(p[0]+offset,p[1]+offset,p[2]+offset);};
    auto convert=[](const P& p){if constexpr(D==2)return Point<D>{U32(p.x()),U32(p.y())};else return Point<D>{U32(p.x()),U32(p.y()),U32(p.z())};};
    std::vector<P> points;points.reserve(data.points.size());for(auto& p:data.points)points.push_back(point(p));
    if(name=="cgal_kd") {
        CGAL::Kd_tree<Traits> tree(points.begin(),points.end());
        if(!points.empty())tree.build();
        return measure<false,D,P>(data,name,verify,seconds(start),[&](auto& b,auto& hits){
            if(!points.empty())tree.search(std::back_inserter(hits),CGAL::Fuzzy_iso_box<Traits>(point(b.lo,-.5),point(b.hi,.5)));
        },convert);
    }
    if constexpr(D==2) if(name=="cgal_range") {
        using RT=CGAL::Range_segment_tree_set_traits_2<K>;
        CGAL::Range_tree_2<RT> tree(points.begin(),points.end());
        return measure<false,D,P>(data,name,verify,seconds(start),[&](auto& b,auto& hits){
            if(!points.empty())tree.window_query(typename RT::Interval(point(b.lo),point(b.hi,1)),std::back_inserter(hits));
        },convert);
    }
#elif defined(SQ_SPATIALINDEX)
    using namespace SpatialIndex;
    const auto variant=name=="lsi_linear"?RTree::RV_LINEAR:name=="lsi_quadratic"?RTree::RV_QUADRATIC:RTree::RV_RSTAR;
    if(name!="lsi_linear" && name!="lsi_quadratic" && name!="lsi_rstar" && name!="lsi_str")throw std::runtime_error("Unknown libspatialindex variant");
    if(data.points.size()>UINT32_MAX)throw std::runtime_error("libspatialindex stream size overflow");
    std::unique_ptr<IStorageManager> storage(StorageManager::createNewMemoryStorageManager());
    id_type id;Stream<D> stream(data);
    std::unique_ptr<ISpatialIndex> tree(name=="lsi_str"&&!data.points.empty()
        ?RTree::createAndBulkLoadNewRTree(RTree::BLM_STR,stream,*storage,.7,16,16,D,RTree::RV_RSTAR,id)
        :RTree::createNewRTree(*storage,variant==RTree::RV_RSTAR?.7:.5,16,16,D,variant,id));
    if(name!="lsi_str")for(Id i=0;i<data.points.size();++i)tree->insertData(0,nullptr,region<D>(data.points[i],data.points[i]),i);
    return measure<true,D,Id>(data,name,verify,seconds(start),[&](auto& b,auto& hits){
        Visitor visitor(hits);tree->intersectsWithQuery(region<D>(b.lo,b.hi),visitor);
    },[](Id i){return i;});
#elif defined(SQ_PSI)
    using P=PsiPoint<D>;using B=psi::point_traits<P>;
    using K=psi::orthogonal_split_rule<psi::max_stretch_dim<P>,psi::object_median<P>>;
    using O=psi::orthogonal_split_rule<psi::rotate_dim<P>,psi::spatial_median<P>>;
    using Z=psi::spatial_filling_curve<psi::morton_curve<P>>;
    using H=psi::spatial_filling_curve<psi::hilbert_curve<P>>;
    if(name=="psi_kd")return psi_query<D,psi::kd_tree<psi::tree_traits<P,K>>>(data,name,verify,start);
    if(name=="psi_orth")return psi_query<D,psi::orth_tree<psi::orth_tree_traits<P,O,psi::box_leaf_aug<B>,psi::box_interior_aug<B>>>>(data,name,verify,start);
    if(name=="psi_spac_z")return psi_query<D,psi::p_tree<psi::tree_traits<P,Z>>>(data,name,verify,start);
    if(name=="psi_spac_h")return psi_query<D,psi::p_tree<psi::tree_traits<P,H>>>(data,name,verify,start);
#elif defined(SQ_PGM)
    if(name=="pgm") {
        using Tree=pgm::MultidimensionalPGMIndex<D,std::uint64_t,64>;
        using P=typename Tree::value_type;
        using Code=mortonnd::MortonNDBmi<D,std::uint64_t>;
        constexpr unsigned shift=32-(64/D-1);
        auto cell=[]<std::size_t... A>(const Point<D>& p,std::index_sequence<A...>){return P((std::uint64_t(p[A])>>shift)...);};
        auto coarse=[&](const Point<D>& p){return cell(p,std::make_index_sequence<D>{});};
        std::vector<P> cells;cells.reserve(data.points.size());
        std::vector<std::pair<std::uint64_t,Id>> order;order.reserve(data.points.size());
        for(Id i=0;i<data.points.size();++i){auto p=coarse(data.points[i]);cells.push_back(p);order.emplace_back(std::apply([](auto... x){return Code::Encode(x...);},p),i);}
        std::sort(order.begin(),order.end());
        std::unique_ptr<Tree> tree;if(!cells.empty())tree=std::make_unique<Tree>(cells.begin(),cells.end());
        return measure<true,D,Id>(data,name,verify,seconds(start),[&](auto& b,auto& hits){
            if(!tree)return;
            for(auto it=tree->range(coarse(b.lo),coarse(b.hi));it!=tree->end();++it){
                const auto id=order.at(it.source_position()).second;
                if(b.contains(data.points[id]))hits.push_back(id);
            }
        },[](Id i){return i;},"coarse_box_then_exact_box_filter");
    }
#elif defined(SQ_NANOFLANN)
    if(name=="nanoflann"){
        Cloud<D> cloud{data.points};
        using T=nanoflann::KDTreeSingleIndexAdaptor<nanoflann::L2_Simple_Adaptor<double,Cloud<D>>,Cloud<D>,D,Id>;
        T tree(D,cloud,nanoflann::KDTreeSingleIndexAdaptorParams(16));
        std::vector<nanoflann::ResultItem<Id,double>> candidates;
        return measure<true,D,Id>(data,name,verify,seconds(start),[&](auto& b,auto& hits){
            if(data.points.empty())return;
            std::array<double,D> center;double r2=0;
            for(unsigned a=0;a<D;++a){double half=(double(b.hi[a])-b.lo[a])/2;center[a]=b.lo[a]+half;r2+=half*half;}
            // Native search is a strict floating radius query. Expand beyond
            // rounding error and exactly filter the original integer points.
            r2=std::nextafter(r2*(1+16*std::numeric_limits<double>::epsilon()),INFINITY);
            candidates.clear();tree.radiusSearch(center.data(),r2,candidates,nanoflann::SearchParameters(0,false));
            for(auto& p:candidates)if(b.contains(data.points[p.first]))hits.push_back(p.first);
        },[](Id i){return i;},"enclosing_radius_then_box_filter");
    }
#elif defined(SQ_THST)
    if constexpr(D==2){
        std::vector<ThstPoint> points;points.reserve(data.points.size());for(auto& p:data.points)points.emplace_back(p[0],p[1],points.size());
        auto thst=[&](auto& tree){return measure<true,2,ThstPoint>(data,name,verify,seconds(start),[&](auto& b,auto& hits){
            std::int64_t lo[]={b.lo[0],b.lo[1]},hi[]={b.hi[0],b.hi[1]};
            if(!points.empty())tree.query(spatial::within<2>(lo,hi),std::back_inserter(hits));
        },[](auto& p){return p.id;});};
        if(name=="thst_quadtree"){
            const std::int64_t lo[]={0,0},hi[]={UINT32_MAX,UINT32_MAX};
            spatial::QuadTree<std::int64_t,ThstPoint,2,ThstIndexable> tree(lo,hi,points.begin(),points.end());return thst(tree);
        }
        if(name=="thst_rtree"){
            spatial::RTree<std::int64_t,ThstPoint,2,8,4,ThstIndexable> tree(points.begin(),points.end());return thst(tree);
        }
    }
#endif
    throw std::runtime_error("Unsupported competitor or dimension");
}
} // namespace square
