#pragma once

#include <cstdint>
#include <cstddef>
#include <complex>
#include <mutex>
#include <memory>
#include <functional>
#include <zi/vl/vl.hpp>

#include <map>
#include <list>
#include <vector>


namespace znn { namespace fwd {

typedef long double ldouble;

#if defined(ZNN_USE_LONG_DOUBLE_PRECISION)
typedef long double real;
#elif defined(ZNN_USE_DOUBLE_PRECISION)
typedef double real;
#else
typedef float real;
#endif

typedef std::complex<real>   cplx;
typedef std::complex<real>   complex;

typedef int64_t long_t;

typedef zi::vl::vec<long_t,2> vec2i;
typedef zi::vl::vec<long_t,3> vec3i;
typedef zi::vl::vec<long_t,4> vec4i;

typedef std::size_t size_t;

typedef std::lock_guard<std::mutex> guard;


}} // namespace znn::fwd
