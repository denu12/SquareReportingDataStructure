// ------------------------------------------------------------------------------
// Copyright Christopher Kormanyos 2024 - 2025.
// Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/math/special_functions/gamma.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>

auto main() -> int {
  // Configure a multiprecision binary floating-point type with approximately
  // 100 decimal digits of precision and expression templates enabled.
  // Note that the popular type cpp_bin_float_100 has already been preconfigured
  // and aliased in the multiprecision headers.

  using big_float_type = boost::multiprecision::cpp_bin_float_100;

  // In the next few lines, compute and compare sqrt(pi) with tgamma(1/2)
  // using the 100-digit multiprecision type.

  const big_float_type sqrt_pi{sqrt(boost::math::constants::pi<big_float_type>())};

  const big_float_type half{big_float_type(1) / 2};

  const big_float_type gamma_half{boost::math::tgamma(half)};

  std::stringstream strm{};

  // 1.772453850905516027298167483341145182797549456122387128213807789852911284591032181374950656738544665

  strm << std::setprecision(std::numeric_limits<big_float_type>::digits10)
       << "sqrt_pi   : " << sqrt_pi << "\ngamma_half: " << gamma_half;

  std::cout << strm.str() << std::endl;
}