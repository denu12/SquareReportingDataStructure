#include "app/algorithms/static/esa2026.hh"
#include "app/algorithms/static/adapter.hh"
namespace diskreport::sfc::app::algorithms::sfc {
using Esa1Algo=Algo<Esa2026<1>>; REGISTER_IMPL_NAMED(Esa1Algo,"esa_kiddo");
using Esa2Algo=Algo<Esa2026<2>>; REGISTER_IMPL_NAMED(Esa2Algo,"esa_nabo");
using Esa3Algo=Algo<Esa2026<3>>; REGISTER_IMPL_NAMED(Esa3Algo,"esa_neighbourhood");
using Esa4Algo=Algo<Esa2026<4>>; REGISTER_IMPL_NAMED(Esa4Algo,"esa_vptree");
using Esa5Algo=Algo<Esa2026<5>>; REGISTER_IMPL_NAMED(Esa5Algo,"esa_orthtree");
using Esa6Algo=Algo<Esa2026<6>>; REGISTER_IMPL_NAMED(Esa6Algo,"esa_grid");
using Esa7Algo=Algo<Esa2026<7>>; REGISTER_IMPL_NAMED(Esa7Algo,"esa_sklearn_kd");
using Esa8Algo=Algo<Esa2026<8>>; REGISTER_IMPL_NAMED(Esa8Algo,"esa_sklearn_ball");
using Esa9Algo=Algo<Esa2026<9>>; REGISTER_IMPL_NAMED(Esa9Algo,"esa_snn");
using EsaNanoAlgo=Algo<EsaNanoflann>; REGISTER_IMPL_NAMED(EsaNanoAlgo,"esa_nanoflann");
}
