#include "app/algorithms/static/naive.hh"

#include <sys/types.h>

#include <vector>

#include "app/algorithms/static/adapter.hh"
namespace diskreport::sfc::app::algorithms::sfc {
using NaiveImpl = Algo<Naive<u_int32_t>>;
REGISTER_IMPL_NAMED(NaiveImpl, "naive");

}  // namespace diskreport::sfc::app::algorithms::sfc