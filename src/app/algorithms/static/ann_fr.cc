#include "app/algorithms/static/ann_fr.hh"
#include "app/algorithms/static/adapter.hh"
namespace diskreport::sfc::app::algorithms::sfc {
using AnnFRAlgo = Algo<AnnFR>;
REGISTER_IMPL_NAMED(AnnFRAlgo, "ann_fr");
}
