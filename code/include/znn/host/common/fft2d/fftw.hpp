#pragma once

#ifdef ZNN_USE_MKL_FFTW
#  include <fftw/fftw3.h>
#else
#  include <fftw3.h>
#endif

#include <map>
#include <iostream>
#include <unordered_map>
#include <type_traits>
#include <mutex>
#include <zi/utility/singleton.hpp>
#include <tbb/tbb.h>

#include "znn/assert.hpp"
#include "znn/types.hpp"
#include "znn/tensor/tensor.hpp"
#include "znn/host/common/fft2d/base.hpp"

#ifndef ZNN_FFTW_PLANNING_MODE
#  define ZNN_FFTW_PLANNING_MODE (FFTW_ESTIMATE)
#endif

namespace znn { namespace fwd { namespace host {

#if defined(ZNN_USE_LONG_DOUBLE_PRECISION)

#  define FFT_DESTROY_PLAN fftwl_destroy_plan
#  define FFT_CLEANUP      fftwl_cleanup
#  define FFT_PLAN_C2R     fftwl_plan_dft_c2r_3d
#  define FFT_PLAN_R2C     fftwl_plan_dft_r2c_3d

#  define FFT_PLAN_MANY_DFT fftwl_plan_many_dft
#  define FFT_PLAN_MANY_R2C fftwl_plan_many_dft_r2c
#  define FFT_PLAN_MANY_C2R fftwl_plan_many_dft_c2r

#  define FFT_EXECUTE_DFT_R2C fftwl_execute_dft_r2c
#  define FFT_EXECUTE_DFT_C2R fftwl_execute_dft_c2r
#  define FFT_EXECUTE_DFT     fftwl_execute_dft

typedef fftwl_plan    fft_plan   ;
typedef fftwl_complex fft_complex;

#elif defined(ZNN_USE_DOUBLE_PRECISION)

#  define FFT_DESTROY_PLAN fftw_destroy_plan
#  define FFT_CLEANUP      fftw_cleanup
#  define FFT_PLAN_C2R     fftw_plan_dft_c2r_3d
#  define FFT_PLAN_R2C     fftw_plan_dft_r2c_3d

#  define FFT_PLAN_MANY_DFT fftw_plan_many_dft
#  define FFT_PLAN_MANY_R2C fftw_plan_many_dft_r2c
#  define FFT_PLAN_MANY_C2R fftw_plan_many_dft_c2r

#  define FFT_EXECUTE_DFT_R2C fftw_execute_dft_r2c
#  define FFT_EXECUTE_DFT_C2R fftw_execute_dft_c2r
#  define FFT_EXECUTE_DFT     fftw_execute_dft

typedef fftw_plan    fft_plan   ;
typedef fftw_complex fft_complex;

#else

#  define FFT_DESTROY_PLAN fftwf_destroy_plan
#  define FFT_CLEANUP      fftwf_cleanup
#  define FFT_PLAN_C2R     fftwf_plan_dft_c2r_3d
#  define FFT_PLAN_R2C     fftwf_plan_dft_r2c_3d

#  define FFT_PLAN_MANY_DFT fftwf_plan_many_dft
#  define FFT_PLAN_MANY_R2C fftwf_plan_many_dft_r2c
#  define FFT_PLAN_MANY_C2R fftwf_plan_many_dft_c2r

#  define FFT_EXECUTE_DFT_R2C fftwf_execute_dft_r2c
#  define FFT_EXECUTE_DFT_C2R fftwf_execute_dft_c2r
#  define FFT_EXECUTE_DFT     fftwf_execute_dft

typedef fftwf_plan    fft_plan   ;
typedef fftwf_complex fft_complex;

#endif

class padded_pruned_fft2d_transformer: public padded_pruned_fft2d_transformer_base
{
private:
    fft_plan ifwd1;
    fft_plan kfwd1;
    fft_plan fwd2;

    fft_plan bwd1, bwd2;

public:
    ~padded_pruned_fft2d_transformer()
    {
        FFT_DESTROY_PLAN(ifwd1);
        FFT_DESTROY_PLAN(kfwd1);
        FFT_DESTROY_PLAN(fwd2);
        FFT_DESTROY_PLAN(bwd1);
        FFT_DESTROY_PLAN(bwd2);
    }

    padded_pruned_fft2d_transformer( vec2i const & _im,
                                     vec2i const & _fs )
        : padded_pruned_fft2d_transformer_base(_im, _fs)
    {
        host_tensor<real,2>  rp(asize);
        host_tensor<fft_complex,2> cp(csize);

        // Out-of-place
        // Real to complex / complex to real along x direction
        // Repeated along z direction
        // Will need filter.y calls for each y
        {
            int len[]     = { static_cast<int>(asize[0]) };

            ifwd1 = FFT_PLAN_MANY_R2C(
                1, len,
                isize[1], // How many
                rp.data(), NULL,
                isize[1], // Input stride
                1, // Input distance
                cp.data(), NULL,
                csize[1], // Output stride
                1, // Output distance
                ZNN_FFTW_PLANNING_MODE );

            kfwd1 = FFT_PLAN_MANY_R2C(
                1, len,
                ksize[1], // How many
                rp.data(), NULL,
                ksize[1], // Input stride
                1, // Input distance
                cp.data(), NULL,
                csize[1], // Output stride
                1, // Output distance
                ZNN_FFTW_PLANNING_MODE );

            bwd2 = FFT_PLAN_MANY_C2R(
                1, len,
                rsize[1], // How many
                cp.data(), NULL,
                csize[1], // Input stride
                1, // Input distance
                rp.data(), NULL,
                rsize[1], // Output stride
                1, // Output distance
                ZNN_FFTW_PLANNING_MODE );
        }

        // In-place
        // Complex to complex along y direction
        // Single call
        {
            int len[]   = { static_cast<int>(csize[1]) };
            int howmany = static_cast<int>(csize[0]);
            int stride  = static_cast<int>(1);
            int dist    = static_cast<int>(csize[1]);

            fwd2 = FFT_PLAN_MANY_DFT( 1, len, howmany,
                                      cp.data(), NULL, stride, dist,
                                      cp.data(), NULL, stride, dist,
                                      FFTW_FORWARD, ZNN_FFTW_PLANNING_MODE );

            bwd1 = FFT_PLAN_MANY_DFT( 1, len, howmany,
                                      cp.data(), NULL, stride, dist,
                                      cp.data(), NULL, stride, dist,
                                      FFTW_BACKWARD, ZNN_FFTW_PLANNING_MODE );
        }

    }

    void forward_kernel( real* rp, void* cpv )
    {
        fft_complex* cp = reinterpret_cast<fft_complex*>(cpv);
        std::memset(cp, 0, csize[0]*csize[1]*sizeof(fft_complex));

        FFT_EXECUTE_DFT_R2C( kfwd1, rp, cp );
        FFT_EXECUTE_DFT( fwd2, cp, cp );
    }

    void forward_image( real* rp, void* cpv )
    {
        fft_complex* cp = reinterpret_cast<fft_complex*>(cpv);
        std::memset(cp, 0, csize[0]*csize[1]*sizeof(fft_complex));

        FFT_EXECUTE_DFT_R2C( ifwd1, rp, cp );
        FFT_EXECUTE_DFT( fwd2, cp, cp );
    }

    void backward( void* cpv, real* rp )
    {
        fft_complex* cp = reinterpret_cast<fft_complex*>(cpv);
        // In-place complex to complex along z-direction
        FFT_EXECUTE_DFT( bwd1, cp, cp );
        long_t yOff = ksize[1] - 1;
        FFT_EXECUTE_DFT_C2R( bwd2, cp + yOff, rp );
    }

};

}}} // namespace znn::fwd::host


#undef FFT_DESTROY_PLAN
#undef FFT_CLEANUP
#undef FFT_PLAN_R2C
#undef FFT_PLAN_C2R

#undef FFT_PLAN_MANY_DFT
#undef FFT_PLAN_MANY_R2C
#undef FFT_PLAN_MANY_C2R

#undef FFT_EXECUTE_DFT_R2C
#undef FFT_EXECUTE_DFT_C2R
#undef FFT_EXECUTE_DFT
