// File-format and box-to-ball adapters; PGM iterator fixes are documented separately.
#pragma once

std::pair<Point,Point> query_box(const Data& d, std::size_t q) {
    std::uint64_t lo=0, hi=UINT64_MAX;
    while(lo<hi) { auto m=lo+(hi-lo)/2; if(Wide(m)*m<d.radii[q])lo=m+1;else hi=m; }
    Point lower(d.d),upper(d.d);
    for(unsigned a=0;a<d.d;++a) {
        auto c=d.centers[q][a];
        lower[a]=lo>c?0:c-lo;
        upper[a]=lo>UINT32_MAX-c?UINT32_MAX:c+lo;
    }
    return {lower,upper};
}
template<class P> bool inside_point(const Data& d,std::size_t q,const P& p) {
    Wide sum=0;
    for(unsigned a=0;a<d.d;++a) {
        auto delta=std::int64_t(p[a])-d.centers[q][a];
        auto v=std::uint64_t(delta<0?-delta:delta);sum+=Wide(v)*v;
    }
    return sum<=d.radii[q];
}
template<class Points> bool verify_points(const Data& d,std::size_t q,const Points& hits) {
    std::vector<Point> actual,expected;
    for(const auto& p:hits) { Point value(d.d);for(unsigned a=0;a<d.d;++a)value[a]=p[a];actual.push_back(value); }
    for(std::size_t i=0;i<d.points.size();++i)if(d.contains(i,q))expected.push_back(d.points[i]);
    std::sort(actual.begin(),actual.end());std::sort(expected.begin(),expected.end());
    return actual==expected;
}
const char* membership_contract="original_box_api_with_integer_ball_filter";

#if defined(ADDITIONAL_PHTREE)
#include "phtree/phtree_multimap.h"
const char* output_contract="point_ids_only";
const char* output_representation="native_phtree_values";
template<unsigned D> Query make_index(Data& d,const std::string&) {
    using Tree=improbable::phtree::PhTreeMultiMap<D,std::size_t>;
    using P=improbable::phtree::PhPoint<D>;
    auto tree=std::make_shared<Tree>();
    for(std::size_t i=0;i<d.points.size();++i) { P p;std::copy(d.points[i].begin(),d.points[i].end(),p.begin());tree->emplace(p,i); }
    auto hits=std::make_shared<std::vector<std::size_t>>();
    return {[&,tree,hits](std::size_t q) {
        hits->clear();auto [lower,upper]=query_box(d,q);P lo,hi;
        std::copy(lower.begin(),lower.end(),lo.begin());std::copy(upper.begin(),upper.end(),hi.begin());
        for(auto it=tree->begin_query({lo,hi});it!=tree->end();++it)
            if(d.contains(*it,q))hits->push_back(*it);
    },[hits]{return hits->size();},[&,hits](std::size_t q){return verify_ids(d,q,*hits);}};
}
#elif defined(ADDITIONAL_PGM)
#include "pgm/pgm_index_variants.hpp"
const char* output_contract="point_ids_only";
const char* output_representation="pgm_iterator_original_ids";

template<unsigned D> Query make_index(Data& d,const std::string&) {
    using Tree=pgm::MultidimensionalPGMIndex<D,std::uint64_t,64>;
    using P=typename Tree::value_type;
    using Morton=mortonnd::MortonNDBmi<D,std::uint64_t>;
    constexpr unsigned shift=32-(64/D-1);
    auto cell=[](const Point& p) {
        P v;[&]<std::size_t... A>(std::index_sequence<A...>){((std::get<A>(v)=std::uint64_t(p[A])>>shift),...);}(std::make_index_sequence<D>{});return v;
    };
    std::vector<P> cells;std::vector<std::pair<std::uint64_t,std::size_t>> records;
    for(std::size_t i=0;i<d.points.size();++i) {
        auto p=cell(d.points[i]);cells.push_back(p);
        records.emplace_back(std::apply([](auto... x){return Morton::Encode(x...);},p),i);
    }
    // Mirror native Morton-key order to attach IDs; equal keys may use any order.
    std::sort(records.begin(),records.end());
    auto ids=std::make_shared<std::vector<std::size_t>>();
    for(auto [key,id]:records)ids->push_back(id);
    auto tree=std::make_shared<Tree>(cells.begin(),cells.end());
    auto hits=std::make_shared<std::vector<std::size_t>>();
    return {[&,tree,hits,ids,cell](std::size_t q) {
        hits->clear();auto [lower,upper]=query_box(d,q);
        for(auto it=tree->range(cell(lower),cell(upper));it!=tree->end();++it) {
            auto id=ids->at(it.source_position());
            if(d.contains(id,q))hits->push_back(id);
        }
    },[hits]{return hits->size();},[&,hits](std::size_t q){return verify_ids(d,q,*hits);}};
}
#elif defined(ADDITIONAL_PSI)
#include "psi/kd_tree.h"
#include "psi/orth_tree.h"
#include "psi/p_tree.h"
#include "psi/dependence/splitter.h"
const char* output_contract="integer_points";
const char* output_representation="native_psi_augmented_points";
struct PsiId {
    using id_type=std::size_t;using curve_code_type=std::uint64_t;
    curve_code_type code=0;id_type id=0;
    void set_member(curve_code_type value){code=value;}
    bool operator<(const PsiId& b)const{return code==b.code?id<b.id:code<b.code;}
    bool operator==(const PsiId& b)const{return id==b.id;}
    friend std::ostream& operator<<(std::ostream& out,const PsiId& p){return out<<p.id;}
};
template<unsigned D,class Tree> Query psi_index(Data& d) {
    using P=psi::aug_point<std::int64_t,D,PsiId>;
    auto points=std::make_shared<parlay::sequence<P>>(d.points.size());
    for(std::size_t i=0;i<d.points.size();++i) {for(unsigned a=0;a<D;++a)(*points)[i][a]=d.points[i][a];(*points)[i].aug.id=i;}
    auto tree=std::make_shared<Tree>();tree->build(parlay::make_slice(*points));
    auto candidates=std::make_shared<parlay::sequence<P>>(d.points.size());
    auto hits=std::make_shared<std::vector<P>>();
    return {[&,tree,points,candidates,hits](std::size_t q) {
        hits->clear();auto [lower,upper]=query_box(d,q);typename Tree::box_type box;
        for(unsigned a=0;a<D;++a) {box.first[a]=lower[a];box.second[a]=upper[a];}
        auto [count,logger]=tree->range_query(box,parlay::make_slice(*candidates));
        for(std::size_t i=0;i<count;++i)if(inside_point(d,q,(*candidates)[i]))hits->push_back((*candidates)[i]);
    },[hits]{return hits->size();},[&,hits](std::size_t q){return verify_points(d,q,*hits);}};
}
template<unsigned D> Query make_index(Data& d,const std::string& algorithm) {
    using P=psi::aug_point<std::int64_t,D,PsiId>;using B=psi::point_traits<P>;
    using K=psi::orthogonal_split_rule<psi::max_stretch_dim<P>,psi::object_median<P>>;
    using O=psi::orthogonal_split_rule<psi::rotate_dim<P>,psi::spatial_median<P>>;
    using Z=psi::spatial_filling_curve<psi::morton_curve<P>>;
    using H=psi::spatial_filling_curve<psi::hilbert_curve<P>>;
    if(algorithm=="psi_kd")return psi_index<D,psi::kd_tree<psi::tree_traits<P,K>>>(d);
    if constexpr(D<=8) if(algorithm=="psi_orth")return psi_index<D,psi::orth_tree<psi::orth_tree_traits<P,O,psi::box_leaf_aug<B>,psi::box_interior_aug<B>>>>(d);
    if constexpr(D<=3) if(algorithm=="psi_spac_z")return psi_index<D,psi::p_tree<psi::tree_traits<P,Z>>>(d);
    if constexpr(D<=3) if(algorithm=="psi_spac_h")return psi_index<D,psi::p_tree<psi::tree_traits<P,H>>>(d);
    throw std::runtime_error("Unknown PSI algorithm");
}
#elif defined(ADDITIONAL_SPATIALINDEX)
#include <spatialindex/SpatialIndex.h>
const char* output_contract="point_ids_only";
const char* output_representation="native_libspatialindex_ids";
struct SpatialStream:SpatialIndex::IDataStream {
    Data& data;std::size_t next=0;
    explicit SpatialStream(Data& d):data(d){}
    SpatialIndex::IData* getNext()override {
        if(!hasNext())return nullptr;
        std::vector<double> p(data.points[next].begin(),data.points[next].end());
        SpatialIndex::Region box(p.data(),p.data(),data.d);
        return new SpatialIndex::RTree::Data(0,nullptr,box,next++);
    }
    bool hasNext()override{return next<data.points.size();}
    std::uint32_t size()override{return data.points.size();}
    void rewind()override{next=0;}
};
struct SpatialVisitor:SpatialIndex::IVisitor {
    const Data& data;std::size_t query=0;std::vector<std::size_t> hits;
    explicit SpatialVisitor(const Data& d):data(d){}
    void visitNode(const SpatialIndex::INode&)override{}
    void visitData(const SpatialIndex::IData& p)override {auto id=p.getIdentifier();if(data.contains(id,query))hits.push_back(id);}
    void visitData(std::vector<const SpatialIndex::IData*>& values)override {for(auto p:values)visitData(*p);}
};
Query create(Data& d,const std::string& algorithm) {
    using namespace SpatialIndex;
    auto storage=std::shared_ptr<IStorageManager>(StorageManager::createNewMemoryStorageManager());
    auto variant=algorithm=="lsi_linear"?RTree::RV_LINEAR:algorithm=="lsi_quadratic"?RTree::RV_QUADRATIC:RTree::RV_RSTAR;
    id_type identifier;SpatialStream stream(d);
    auto tree=std::shared_ptr<ISpatialIndex>(algorithm=="lsi_str" && !d.points.empty()
        ? RTree::createAndBulkLoadNewRTree(RTree::BLM_STR,stream,*storage,.7,16,16,d.d,RTree::RV_RSTAR,identifier)
        : RTree::createNewRTree(*storage,variant==RTree::RV_RSTAR?.7:.5,16,16,d.d,variant,identifier));
    if(algorithm!="lsi_str")for(std::size_t i=0;i<d.points.size();++i) {
        std::vector<double> p(d.points[i].begin(),d.points[i].end());Region box(p.data(),p.data(),d.d);tree->insertData(0,nullptr,box,i);
    }
    auto visitor=std::make_shared<SpatialVisitor>(d);
    return {[&,storage,tree,visitor](std::size_t q) {
        visitor->query=q;visitor->hits.clear();auto [lower,upper]=query_box(d,q);
        std::vector<double> lo(lower.begin(),lower.end()),hi(upper.begin(),upper.end());
        Region box(lo.data(),hi.data(),d.d);tree->intersectsWithQuery(box,*visitor);
    },[visitor]{return visitor->hits.size();},[&,visitor](std::size_t q){return verify_ids(d,q,visitor->hits);}};
}
#endif
#if !defined(ADDITIONAL_SPATIALINDEX)
Query create(Data& d,const std::string& algorithm) {
    if(d.d==2)return make_index<2>(d,algorithm);
    if(d.d==3)return make_index<3>(d,algorithm);
#if defined(ADDITIONAL_PHTREE) || defined(ADDITIONAL_PSI) || defined(ADDITIONAL_PGM)
    switch(d.d) {
#define DIMENSION(N) case N: return make_index<N>(d,algorithm);
    DIMENSION(4) DIMENSION(5) DIMENSION(6) DIMENSION(7) DIMENSION(8)
    DIMENSION(9) DIMENSION(10) DIMENSION(11) DIMENSION(12) DIMENSION(13)
    DIMENSION(14) DIMENSION(15) DIMENSION(16) DIMENSION(32)
#if !defined(ADDITIONAL_PGM)
    DIMENSION(34)
#endif
#if defined(ADDITIONAL_PSI)
    DIMENSION(96) DIMENSION(100) DIMENSION(128)
#endif
#undef DIMENSION
    }
#endif
    throw std::runtime_error("Unsupported dimension for additional backend");
}
#endif
