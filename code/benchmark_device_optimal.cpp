#include "znn/device/common/cudnn.hpp"
#include "znn/device/common/fft/transformer.hpp"

#include "znn/util/network.hpp"
#include "znn/device/v1/cudnn_conv.hpp"
#include "znn/device/v1/cudnn_no_precomp_gemm_conv.hpp"
#include "znn/device/v1/fft_conv.hpp"
#include "znn/device/v1/cudnn_pool.hpp"
#include "znn/device/v1/cudnn_mfp.hpp"

#include <zi/time.hpp>

#include <string>
#include <fstream>
#include <sstream>

// RUN THE FOLLOWING
// ./benchmark_device m37 4
// ./benchmark_device m57 4
// ./benchmark_device m77 4
// ./benchmark_device m97 4


using namespace znn::fwd;

std::string net_name;
vec3i       os;
long_t      max_memory = static_cast<long_t>(11) * 1024 * 1024 * 1024; // GB

inline void benchmark_network( network_descriptor & ndesc,
                               long_t rounds,
                               std::ofstream & rout )
{
    znni_network net(ndesc, 1, os);

    rout << "## " << net_name << " :: starting benchmark for output size "
         << os << std::endl;

    std::vector<std::unique_ptr<device::v1::device_layer>> layers;

    long_t lnum = 0;

    long_t rm = 0;
    long_t wm = 0;

    try
    {
        for ( auto & l: net.layers() )
        {
            if ( l.descriptor.type == layer_type::convolutional )
            {
                // find the best network
                std::unique_ptr<device::v1::device_layer> to_add;
                double best = std::numeric_limits<double>::max();

                try
                {
                    auto attempt = make_unique<device::v1::cudnn_conv>
                        (l.batch_size,
                         l.descriptor.num_inputs,
                         l.descriptor.num_outputs,
                         l.in_size,
                         l.descriptor.k_or_w_size,
                         l.random_kernels().data(),
                         l.random_biases().data());

                    auto r = attempt->workable();
                    if (r.first && (r.second < best))
                    {
                        best = r.second;
                        to_add = std::move(attempt);
                    }

                    rout << "   ## cudnn_conv test " << lnum << ' '
                         << ( r.first ? "OK" : "NO" )
                         << ' ' << r.second << std::endl;

                }
                catch (...)
                {
                    // whatever
                }

                try
                {
                    auto attempt = make_unique<device::v1::fft_conv>
                        (l.batch_size,
                         l.descriptor.num_inputs,
                         l.descriptor.num_outputs,
                         l.in_size,
                         l.descriptor.k_or_w_size,
                         l.random_kernels().data(),
                         l.random_biases().data());

                    auto r = attempt->workable();
                    if (r.first && (r.second < best))
                    {
                        best = r.second;
                        to_add = std::move(attempt);
                    }

                    rout << "   ## fft_conv   test " << lnum << ' '
                         << ( r.first ? "OK" : "NO" )
                         << ' ' << r.second << std::endl;
                }
                catch (...)
                {
                    // whatever
                }


                if ( !to_add )
                {
                    try
                    {
                        auto attempt
                            = make_unique<device::v1::cudnn_no_precomp_gemm_conv>
                            (l.batch_size,
                             l.descriptor.num_inputs,
                             l.descriptor.num_outputs,
                             l.in_size,
                             l.descriptor.k_or_w_size,
                             l.random_kernels().data(),
                             l.random_biases().data());

                        auto r = attempt->workable();
                        if (r.first && (r.second < best))
                        {
                            to_add = std::move(attempt);
                            best = r.second;
                        }

                        rout << "   ## ngemm_conv test " << lnum << ' '
                             << ( r.first ? "OK" : "NO" )
                             << ' ' << r.second << std::endl;

                    }
                    catch (...)
                    {
                        // whatever
                    }
                }

                if ( to_add )
                {
                    layers.push_back(std::move(to_add));
                }
                else
                {
                    rout << "[network_information] " << net_name
                         << " :: " << os << " :: IS NOT WORKABLE!" << std::endl;
                    return;
                }

            }
            else
            {
                layers.push_back(make_unique<device::v1::cudnn_mfp>
                                 (l.batch_size,
                                  l.descriptor.num_inputs,
                                  l.in_size,
                                  l.descriptor.k_or_w_size));
            }

            ++lnum;

            rm += layers.back()->resident_memory();
            wm = std::max(wm, layers.back()->working_memory());
        }
    }
    catch ( std::exception & e )
    {
        rout << "[network_exception] " << net_name
             << " :: " << os << " :: "
             << " threw an exception: " << e.what() << std::endl;
        return;
    }

    rout << "[network_requirements] " << net_name
         << " :: " << os << " :: "
         << "RM: " << rm/1024/1024
         << " MB WM: " << wm/1024/1024 << " MB" << std::endl;


    // bool workable = true;
    // for ( auto & l: layers )
    // {
    //     if ( workable )
    //     {
    //         auto r = l->workable();
    //         workable = workable && r.first;
    //     }
    // }

    // if ( !workable )
    // {
    //     rout << "[network_information] " << net_name
    //          << " :: " << os << " :: IS NOT WORKABLE!" << std::endl;
    //     return;
    // }

    if ( rm + wm < max_memory )
    {
        double total = 0;
        zi::wall_timer wtn;
        zi::wall_timer wtl;

        for ( long_t i = 0; i < rounds; ++i )
        {
            lnum = 0;
            auto inh = net.get_random_sample();

            wtn.reset();

            device_tensor<float,5> in(net.in_shape());
            in = inh;

            for ( auto & l: layers )
            {
                wtl.reset();
                in = l->forward(std::move(in));
                double t = wtl.elapsed<double>();

                rout << "[layer_measurement] " << net_name
                     << " :: " << os << " :: " << lnum
                     << " :: " << t << std::endl;

                ++lnum;
            }

            host_tensor<float,5> result(in.shape()[0], in.shape()[1],
                                        in.shape()[2], in.shape()[3],
                                        in.shape()[4]);

            result = in;
            double t = wtn.elapsed<double>();

            rout << "[network_measurement] " << net_name
                     << " :: " << os
                     << " :: " << t << std::endl;

            total += t;

            ++lnum;
        }

        total /= rounds;

    // rout << "## " << net_name << " :: starting benchmark for output size "
    //      << os << std::endl;

        rout << "[network_average] " << net_name
             << " :: " << os << " :: " << total << std::endl;

        double voxels = net.out_voxels();

        rout << "[network_throughput] " << net_name
             << " :: " << os << " :: " << (voxels/total) << std::endl;
    }

}

void benchmark( std::string const & rep, long_t rounds )
{
    std::string net_path    = "../networks/" + net_name + ".znni";
    std::string report_path = "../reports/" + net_name + ".device." + rep + ".report";

    std::ofstream ofs;
    ofs.open (report_path.c_str(), std::ofstream::out | std::ofstream::app);

    ofs << "--------------------------------------------\n\n";

    std::cout << net_path << "\n";

    network_descriptor nd(net_path);

    for ( long_t i = 8; i < 400; i += 8 )
    {
        os = vec3i(i,i,i);
        benchmark_network(nd, rounds, ofs);
    }
}

int main(int argc, char *argv[])
{
    net_name = std::string(argv[1]);

    long_t rounds = 4;
    if ( argc > 2 ) rounds = atoi(argv[2]);

    benchmark("optimal", rounds);
}