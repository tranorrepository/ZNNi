#include "znn/util/network2d.hpp"
#include "znn/util/network.hpp"
#include "znn/device/v1/cudnn_conv.hpp"
#include "znn/device/v1/cudnn_no_precomp_gemm_conv.hpp"
#include "znn/device/v1/fft_conv.hpp"
#include "znn/device/v1/cudnn_pool.hpp"
#include "znn/device/v1/maxout.hpp"
#include "znn/device/v1/cudnn_maxfilter.hpp"
#include "znn/device/v1/cudnn_mfp.hpp"

#include <zi/time.hpp>

#include <string>
#include <fstream>
#include <sstream>

// RUN THE FOLLOWING
// ./bin/benchmark_device m36 4
// ./bin/benchmark_device m56 4
// ./bin/benchmark_device m76 4
// ./bin/benchmark_device m96 4

using namespace znn::fwd;

std::string net_name;
vec3i       os;
long_t      max_memory = static_cast<long_t>(11) * 1024 * 1024 * 1024; // GB
long_t      batch_size = 8;

template<typename Conv>
inline void benchmark_network( network_descriptor & ndesc,
                               long_t rounds,
                               std::ofstream & rout )
{
    znni_network net(ndesc, batch_size, os);

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
                layers.push_back(make_unique<Conv>
                              (l.batch_size,
                               l.descriptor.num_inputs,
                               l.descriptor.num_outputs,
                               l.in_size,
                               l.descriptor.k_or_w_size,
                               l.random_kernels().data(),
                               l.random_biases().data()));

            }
            else if ( l.descriptor.type == layer_type::pooling )
            {
                layers.push_back(make_unique<device::v1::cudnn_mfp>
                                 (l.batch_size,
                                  l.descriptor.num_inputs,
                                  l.in_size,
                                  l.descriptor.k_or_w_size));
            }
            else if ( l.descriptor.type == layer_type::maxfilter )
            {
                layers.push_back(make_unique<device::v1::cudnn_maxfilter>
                                 (l.batch_size,
                                  l.descriptor.num_inputs,
                                  l.in_size,
                                  l.descriptor.k_or_w_size));
            }
            else
            {
                layers.push_back(make_unique<device::v1::maxout>
                                 (l.batch_size,
                                  l.descriptor.num_inputs,
                                  l.descriptor.num_inputs
                                  /l.descriptor.num_outputs,
                                  l.in_size));
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


    bool workable = true;
    for ( auto & l: layers )
    {
        if ( workable )
        {
            auto r = l->workable();
            workable = workable && r.first;
        }
    }

    if ( !workable )
    {
        rout << "[network_information] " << net_name
             << " :: " << os << " :: IS NOT WORKABLE!" << std::endl;
        return;
    }

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

        rout << "[network_average] " << net_name
             << " :: " << os << " :: " << total << std::endl;

        double voxels = net.out_voxels();

        rout << "[network_throughput] " << net_name
             << " :: " << os << " :: " << (voxels/total) << std::endl;
    }

}

template<class Conv>
void benchmark( std::string const & rep, long_t rounds )
{
    std::string net_path    = "../networks/" + net_name + ".znni";
    std::string report_path = "../reports/" + net_name + ".device." + rep + ".report";

    std::ofstream ofs;
    ofs.open (report_path.c_str(), std::ofstream::out | std::ofstream::app);

    ofs << "--------------------------------------------\n\n";

    std::cout << net_path << "\n";

    network_descriptor nd(net_path);

    for ( long_t i = 4; i < 400; i += 4 )
    {
        os = vec3i(i*8,i*8,i);
        std::cout << "Testing: " << os << std::endl;
        std::cout << "   frag: " << nd.fragmentation() << std::endl;
        std::cout << "    mod: " << (os % nd.fragmentation()) << std::endl;
        if ( os % nd.fragmentation() == vec3i::zero )
        {
            benchmark_network<Conv>(nd, rounds, ofs);
        }
    }
}

int main(int argc, char *argv[])
{
    net_name = std::string(argv[1]);

    long_t rounds = 4;
    if ( argc > 2 ) rounds = atoi(argv[2]);

    //benchmark<device::v1::cudnn_conv>("cudnn", rounds);
    //benchmark<device::v1::fft_conv>("fft", rounds);

    benchmark<device::v1::cudnn_no_precomp_gemm_conv>
        ("cudnn_no_precomp_gemm_conv", rounds);

}
