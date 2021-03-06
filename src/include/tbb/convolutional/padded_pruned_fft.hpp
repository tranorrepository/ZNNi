#pragma once

#include "../../types.hpp"
#include "../../assert.hpp"
#include "../../memory.hpp"
#include "../../layer.hpp"
#include "../host_layer.hpp"
#include "padded_pruned_fft/fft.hpp"
#include "base.hpp"
#include <tbb/tbb.h>


namespace znn { namespace fwd { namespace tbb {


class padded_pruned_fft_convolutional_layer
    : public cpu_convolutional_layer_base
    , public host_layer
{
private:
    padded_pruned_fft_transformer* fft_;

public:

    padded_pruned_fft_convolutional_layer( long_t n, long_t fin, long_t fout,
                                           vec3i const & is, vec3i const & ks,
                                           real * km = nullptr,
                                           real* bs = nullptr )
        : cpu_convolutional_layer_base( n, fin, fout, is, ks, km, bs )
        , fft_(padded_pruned_fft_plans.get(is,ks))
    { }


private:
    void do_input_fft( real* in, complex* out ) const noexcept
    {
        fft_->forward_image(in,out);
    }

    void do_input_fft_padded( real* in,
                              complex* out,
                              real* tmp) const noexcept
    {
        std::memcpy( tmp, in, fft_->image_memory());

        // append zeros
        long_t zero_bytes = fft_->image_scratch_memory() - fft_->image_memory();

        std::memset( tmp + fft_->image_elements(), 0, zero_bytes );

        fft_->forward_image(tmp,out);
    }

    void do_output_ifft( complex* in, real* out,
                         real bias, real* out_scratch ) const noexcept
    {
        fft_->backward(in,out_scratch);
        real scale = fft_->get_scale();

        long_t off = fft_->result_offset();

        for ( long_t i = 0; i < out_image_len; ++i )
        {
            out[i] = out_scratch[i+off] / scale + bias;
        }
    }

    void mul_to( complex* a, complex* b, complex* r, long_t n ) const noexcept
    {
        // Complex8f ac, bc;

        // long_t i = 0;

        // for ( ; i < n; i += 4 )
        // {
        //     ac.load(reinterpret_cast<float*>(&a[i]));
        //     bc.load(reinterpret_cast<float*>(&b[i]));

        //     //ac *= bc;

        //     ac.store(reinterpret_cast<float*>(&r[i]));
        // }

        // i -= 4;

        for ( long_t i = 0; i < n; ++i )
        {
            r[i] = a[i] * b[i];
        }

    }

    void mul_add_to( complex* a, complex* b,
                     complex* r, long_t n ) const noexcept
    {
        // Complex8f ac, bc, rc;

        // long_t i = 0;

        // for ( ; i < n; i += 4 )
        // {
        //     ac.load(reinterpret_cast<float*>(&a[i]));
        //     bc.load(reinterpret_cast<float*>(&b[i]));
        //     rc.load(reinterpret_cast<float*>(&r[i]));

        //     // ac *= bc;
        //     // rc += ac;

        //     rc.store(reinterpret_cast<float*>(&r[i]));
        // }

        // i -= 4;

        for ( long_t i = 0; i < n; ++i )
        {
            r[i] += a[i] * b[i];
        }
    }

    void process_single_kernel( long_t in_no,
                                long_t out_no,
                                real* iscratch,
                                complex* oscratch,
                                complex* inputs,
                                complex* outputs ) const noexcept
    {
        real* kernel = kernels.get() + (out_no*num_inputs + in_no) * kernel_len;

        // copy the kernel to the scratch
        std::memcpy( iscratch, kernel, fft_->kernel_memory());

        // append zeros
        long_t zero_bytes = fft_->kernel_scratch_memory()
            - fft_->kernel_memory();

        std::memset( iscratch + fft_->kernel_elements(), 0, zero_bytes );

        // transform the kernel
        fft_->forward_kernel( iscratch, oscratch );

        // loop over the batch
        long_t n_elements = fft_->transform_elements();

        long_t input_stride  = num_inputs  * n_elements;
        long_t output_stride = num_outputs * n_elements;

        complex* input  = inputs  + in_no  * n_elements;
        complex* output = outputs + out_no * n_elements;

        // figure out an optimal way to saturate the cores

        long_t n_cores = std::thread::hardware_concurrency() / 2;

        // simple strategy.. all batches on the same core
        if ( (n_cores <= num_outputs) || (batch_size == 1) )
        {
            for ( long_t k = 0; k < batch_size; ++k )
            {
                complex* a = input  + k * input_stride ;
                complex* r = output + k * output_stride;

                if ( in_no == 0 )
                    mul_to(a, oscratch, r, n_elements);
                else
                    mul_add_to(a, oscratch, r, n_elements);

            }
        }
        else
        {
            long_t tasks_per_core   = (n_cores + num_outputs - 1) / num_outputs;
            long_t batches_per_core = batch_size / tasks_per_core;

            batches_per_core
                = std::max(batches_per_core, static_cast<long_t>(1));

            tasks_per_core
                = ( batch_size + batches_per_core - 1 ) / batches_per_core;

            ::tbb::task_group tg;

            for ( long_t t = 0; t < tasks_per_core; ++t )
            {
                auto fn = [=]()
                {
                    for ( long_t k = t * batches_per_core;
                          k < std::min( (t+1) * batches_per_core, batch_size );
                          ++k )
                    {
                        complex* a = input  + k * input_stride ;
                        complex* r = output + k * output_stride;

                        if ( in_no == 0 )
                            this->mul_to(a, oscratch, r, n_elements);
                        else
                            this->mul_add_to(a, oscratch, r, n_elements);
                    }
                };

                if ( t < tasks_per_core - 1 )
                {
                    tg.run(fn);
                }
                else
                {
                    tg.run_and_wait(fn);
                }
            }
        }


    }

private:
    host_array<complex> transform_inputs( host_array<real> in ) const
    {
        long_t relements = fft_->image_elements();
        long_t celements = fft_->transform_elements();

        auto itransforms
            = get_array<complex>(batch_size * num_inputs * celements);

        // create the list of all transforms to be processed
        std::vector<std::pair<real*, complex*>>
            all_transforms(num_inputs*batch_size);

        ::tbb::concurrent_queue<std::pair<real*, complex*>*> queue;

        for ( long_t i = 0, off = 0; i < batch_size; ++i )
            for ( long_t j = 0; j < num_inputs; ++j, ++off )
            {
                all_transforms[off].first  = in.get() + relements*off;
                all_transforms[off].second = itransforms.get() + celements*off;
                queue.push(&all_transforms[off]);
            }

        auto fn = [&,this]()
            {
                host_array<real> scratch;;
                if ( this->fft_->needs_padding() )
                {
                    scratch = get_array<real>
                        (this->fft_->image_scratch_elements());
                }

                std::pair<real*, complex*>* which;

                if ( !this->fft_->needs_padding() )
                {
                    while ( queue.try_pop(which) )
                    {
                        this->do_input_fft(which->first,
                                           which->second);
                    }
                }
                else
                {
                    while ( queue.try_pop(which) )
                    {
                        this->do_input_fft_padded(which->first,
                                                  which->second,
                                                  scratch.get());
                    }
                }
            };


        ::tbb::task_group tg;

        long_t num_tasks = std::thread::hardware_concurrency();
        num_tasks = std::min(num_tasks, batch_size*num_inputs);

        for ( long_t i = 0; i < num_tasks - 1; ++i )
        {
            tg.run(fn);
        }

        tg.run_and_wait(fn);

        return itransforms;
    }

    host_array<complex> collect_outputs( host_array<complex> itransforms ) const
    {
        long_t celements = fft_->transform_elements();

        auto otransforms
            = get_array<complex>(batch_size * num_outputs * celements);

        std::vector< ::tbb::concurrent_queue<long_t>> queues(num_inputs);

        for ( long_t i = 0; i < num_outputs; ++i )
        {
            queues[0].push(i);
        }

        auto fn = [&,this]()
            {
                auto oscratch = get_array<complex>(this->fft_->transform_elements());
                auto iscratch = get_array<real>(this->fft_->kernel_scratch_elements());

                long_t out_no;

                for ( long_t i = 0; i < num_inputs; ++i )
                {
                    while ( queues[i].try_pop(out_no) )
                    {
                        this->process_single_kernel( i, out_no,
                                                     iscratch.get(),
                                                     oscratch.get(),
                                                     itransforms.get(),
                                                     otransforms.get() );
                        if ( i < num_inputs - 1 )
                        {
                            queues[i+1].push(out_no);
                        }
                    }
                }
            };


        long_t num_tasks = std::thread::hardware_concurrency();
        num_tasks = std::min(num_tasks, num_outputs);

        ::tbb::task_group tg;

        for ( long_t i = 0; i < num_tasks - 1; ++i )
        {
            tg.run(fn);
        }

        tg.run_and_wait(fn);

        return otransforms;
    }

    host_array<real> process_outputs( host_array<complex> otransforms ) const
    {
        long_t celements = fft_->transform_elements();

        auto result = get_array<real>(total_output_len);

        // create the list of all transforms to be processed
        std::vector<std::tuple<real, complex*, real*>>
            all_transforms(num_outputs*batch_size);

        ::tbb::concurrent_queue<std::tuple<real, complex*, real*>*> queue;

        for ( long_t i = 0, off = 0; i < batch_size; ++i )
            for ( long_t j = 0; j < num_outputs; ++j, ++off )
            {
                std::get<0>(all_transforms[off]) = biases.get()[j];
                std::get<1>(all_transforms[off])
                    = otransforms.get() + celements * off;
                std::get<2>(all_transforms[off])
                    = result.get() + out_image_len * off;
                queue.push(&all_transforms[off]);
            }


        auto fn = [&,this]()
        {
            auto scratch
            = get_array<real>(this->fft_->result_scratch_elements());

            std::tuple<real, complex*, real*>* which;

            while ( queue.try_pop(which) )
            {
                this->do_output_ifft( std::get<1>(*which),
                                      std::get<2>(*which),
                                      std::get<0>(*which),
                                      scratch.get() );
            }
        };

        ::tbb::task_group tg;

        long_t num_tasks = std::thread::hardware_concurrency();
        num_tasks = std::min(num_tasks, batch_size*num_inputs);

        for ( long_t i = 0; i < num_tasks - 1; ++i )
        {
            tg.run(fn);
        }

        tg.run_and_wait(fn);

        return result;
    }

public:
    host_array<real> forward( host_array<real> in ) const override
    {
        auto   in_transforms  = transform_inputs(std::move(in));
        auto   out_transforms = collect_outputs(std::move(in_transforms));
        return process_outputs(std::move(out_transforms));
    }


};



}}} // namespace znn::fwd::tbb
