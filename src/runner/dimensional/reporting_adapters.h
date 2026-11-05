#pragma once
#include "chan_reporting.h"
#include "manual_adapters.hpp"
const char* output_contract="point_ids_only";
const char* output_representation="adapted_original_point_ids";
const char* membership_contract="closed_integer_ball";

template<unsigned D> Query stann_reporting(Data& data) {
    using Tree=diskreport::ddim::StannManuallyAdapted<D>;
    std::vector<typename Tree::Point> points(data.points.size());
    for(std::size_t i=0;i<points.size();++i)std::copy(data.points[i].begin(),data.points[i].end(),points[i].begin());
    auto tree=std::make_shared<Tree>(points);auto hits=std::make_shared<std::vector<std::size_t>>();
    return {[&,tree,hits](std::size_t q){typename Tree::Point center;std::copy(data.centers[q].begin(),data.centers[q].end(),center.begin());hits->clear();tree->ball_squared(center,data.radii[q],[&](auto id){hits->push_back(id);});},
            [hits]{return hits->size();},[&,hits](auto q){return verify_ids(data,q,*hits);}};
}
Query create(Data& data,const std::string& algorithm) {
    if(algorithm=="chan_sss" || algorithm=="chan_sss_ddim_MANUALLY_ADAPTED") {
        auto tree=std::make_shared<ChanReporting>(data);auto hits=std::make_shared<std::vector<std::size_t>>();
        return {[tree,hits](auto q){tree->report(q,*hits);},[hits]{return hits->size();},[&,hits](auto q){return verify_ids(data,q,*hits);}};
    }
    if(algorithm!="stann_fr" && algorithm!="stann_fr_ddim_MANUALLY_ADAPTED")throw std::runtime_error("Unknown reporting adapter");
    switch(data.d) {
#define DIMENSION(N) case N:return stann_reporting<N>(data);
        DIMENSION(2) DIMENSION(3) DIMENSION(4) DIMENSION(5) DIMENSION(6) DIMENSION(7) DIMENSION(8)
        DIMENSION(9) DIMENSION(10) DIMENSION(11) DIMENSION(12) DIMENSION(13) DIMENSION(14) DIMENSION(15) DIMENSION(16)
        DIMENSION(32) DIMENSION(34) DIMENSION(96) DIMENSION(100) DIMENSION(128) DIMENSION(784) DIMENSION(960)
#undef DIMENSION
    }
    throw std::runtime_error("Unsupported STANN dimension");
}
