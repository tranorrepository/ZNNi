#pragma once

#include "znn/util/io.hpp"
#include "znn/util/znnhelper.hpp"
#include "znn/host/v1/mfp.hpp"
#include "znn/host/v1/crop.hpp"
#include "znn/host/v1/fft_conv.hpp"
#include "znn/host/v1/maxfilter.hpp"
#include "znn/tensor/tensor.hpp"

#include <string>

namespace znn {namespace fwd{

std::vector<std::unique_ptr<host::v1::host_layer>>
create_multiscale_b1(const vec3i & outsz)
{
  std::vector<std::unique_ptr<host::v1::host_layer>> layers;

  vec3i fov(9, 109, 109);
  vec3i insz = outsz + fov - vec3i::one;

  // crop
  layers.push_back(std::unique_ptr<host::v1::host_layer>
                   (new host::v1::crop
                    (1, 1, insz, vec3i(0,32,32))));
  insz -= vec3i(0, 64, 64);

  // conv1a
  float conv1a_k[1*24*1*3*3];
  float conv1a_b[24];
  read_from_file<float>("./0421_VD2D3D-MS/conv1a/filters",conv1a_k,1*24*1*3*3);
  fix_dims(conv1a_k, 1, 24, 1, 3, 3);
  read_from_file<float>("./0421_VD2D3D-MS/nconv1a/biases",conv1a_b,24);
  layers.push_back(std::unique_ptr<host::v1::host_layer>
                   (new host::v1::fft_conv
                    (1, 1, 24,
                     insz, vec3i(1,3,3),
                     conv1a_k, conv1a_b, activation::relu)));
  insz -= vec3i(0, 2, 2);

  // conv1b
  float conv1b_k[24*24*1*3*3];
  float conv1b_b[24];
  read_from_file<float>("./0421_VD2D3D-MS/conv1b/filters",conv1b_k,24*24*1*3*3);
  fix_dims(conv1b_k, 24, 24, 1, 3, 3);
  read_from_file<float>("./0421_VD2D3D-MS/nconv1b/biases",conv1b_b,24);
  layers.push_back(std::unique_ptr<host::v1::host_layer>
                   (new host::v1::fft_conv
                    (1, 24, 24,
                     insz, vec3i(1,3,3),
                     conv1b_k, conv1b_b, activation::relu)));
  insz -= vec3i(0, 2, 2);

  // conv1c
  float conv1c_k[24*24*1*2*2];
  float conv1c_b[24];
  read_from_file<float>("./0421_VD2D3D-MS/conv1c/filters",conv1c_k,24*24*1*2*2);
  fix_dims(conv1c_k, 24, 24, 1, 2, 2);
  read_from_file<float>("./0421_VD2D3D-MS/nconv1c/biases",conv1c_b,24);
  layers.push_back(std::unique_ptr<host::v1::host_layer>
                   (new host::v1::fft_conv
                    (1, 24, 24,
                     insz, vec3i(1,2,2),
                     conv1c_k, conv1c_b, activation::relu)));
  insz -= vec3i(0, 1, 1);

  // pool1 using mfp
  layers.push_back(std::unique_ptr<host::v1::host_layer>
                   (new host::v1::mfp
                    (1, 24, insz, vec3i(1,2,2))));
  insz /= vec3i(1, 2, 2);

  // conv2a
  float conv2a_k[24*36*1*3*3];
  float conv2a_b[36];
  read_from_file<float>("./0421_VD2D3D-MS/conv2a/filters",conv2a_k,24*36*1*3*3);
  fix_dims(conv2a_k, 24, 36, 1, 3, 3);
  read_from_file<float>("./0421_VD2D3D-MS/nconv2a/biases",conv2a_b,36);
  layers.push_back(std::unique_ptr<host::v1::host_layer>
                    (new host::v1::fft_conv
                     (4, 24, 36,
                      insz, vec3i(1,3,3),
                      conv2a_k, conv2a_b, activation::relu)));
  insz -= vec3i(0, 2, 2);

  // conv2b
  float conv2b_k[36*36*1*3*3];
  float conv2b_b[36];
  read_from_file<float>("./0421_VD2D3D-MS/conv2b/filters",conv2b_k,36*36*1*3*3);
  fix_dims(conv2b_k, 36, 36, 1, 3, 3);
  read_from_file<float>("./0421_VD2D3D-MS/nconv2b/biases",conv2b_b,36);
  layers.push_back(std::unique_ptr<host::v1::host_layer>
                    (new host::v1::fft_conv
                     (4, 36, 36,
                      insz, vec3i(1,3,3),
                      conv2b_k, conv2b_b, activation::relu)));
  insz -= vec3i(0, 2, 2);

  // pool2 using maxfilter
  layers.push_back(std::unique_ptr<host::v1::host_layer>
                   (new host::v1::maxfilter
                    (4, 36, insz, vec3i(1,2,2))));
  insz -= vec3i(0, 1, 1);

  // conv3a-p1
  float conv3a_k[36*36*2*3*3];
  float conv3a_b[36];
  read_from_file<float>("./0421_VD2D3D-MS/conv3a-p1/filters",conv3a_k,36*36*2*3*3);
  fix_dims(conv3a_k, 36, 36, 2, 3, 3);
  read_from_file<float>("./0421_VD2D3D-MS/nconv3a-p1/biases",conv3a_b,36);
  layers.push_back(std::unique_ptr<host::v1::host_layer>
                    (new host::v1::fft_conv
                     (4, 36, 36,
                      insz, vec3i(2,3,3),
                      conv3a_k, conv3a_b, activation::relu)));
  insz -= vec3i(1, 2, 2);

  // conv3b-p1
  float conv3b_k[36*36*2*3*3];
  float conv3b_b[36];
  read_from_file<float>("./0421_VD2D3D-MS/conv3b-p1/filters",conv3b_k,36*36*2*3*3);
  fix_dims(conv3b_k, 36, 36, 2, 3, 3);
  read_from_file<float>("./0421_VD2D3D-MS/nconv3b-p1/biases",conv3b_b,36);
  layers.push_back(std::unique_ptr<host::v1::host_layer>
                    (new host::v1::fft_conv
                     (4, 36, 36,
                      insz, vec3i(2,3,3),
                      conv3b_k, conv3b_b, activation::relu)));
  insz -= vec3i(1, 2, 2);

  // pool3-p1
  layers.push_back(std::unique_ptr<host::v1::host_layer>
                   (new host::v1::maxfilter
                    (4, 36, insz, vec3i(2,2,2))));
  insz -= vec3i(1, 1, 1);

  // conv4a-p1
  float conv4a_k[36*48*2*3*3];
  float conv4a_b[48];
  read_from_file<float>("./0421_VD2D3D-MS/conv4a-p1/filters",conv4a_k,36*48*2*3*3);
  fix_dims(conv4a_k, 36, 48, 2, 3, 3);
  read_from_file<float>("./0421_VD2D3D-MS/nconv4a-p1/biases",conv4a_b,48);
  layers.push_back(std::unique_ptr<host::v1::host_layer>
                    (new host::v1::fft_conv
                     (4, 36, 48,
                      insz, vec3i(2,3,3),
                      conv4a_k, conv4a_b, activation::relu)));
  insz -= vec3i(1, 2, 2);

  // conv4b-p1
  float conv4b_k[48*48*2*3*3];
  float conv4b_b[48];
  read_from_file<float>("./0421_VD2D3D-MS/conv4b-p1/filters",conv4b_k,48*48*2*3*3);
  fix_dims(conv4b_k, 48, 48, 2, 3, 3);
  read_from_file<float>("./0421_VD2D3D-MS/nconv4b-p1/biases",conv4b_b,48);
  layers.push_back(std::unique_ptr<host::v1::host_layer>
                    (new host::v1::fft_conv
                     (4, 48, 48,
                      insz, vec3i(2,3,3),
                      conv4b_k, conv4b_b, activation::relu)));
  insz -= vec3i(1, 2, 2);

  // pool4 using maxfilter
  layers.push_back(std::unique_ptr<host::v1::host_layer>
                   (new host::v1::maxfilter
                    (4, 48, insz, vec3i(2,2,2))));
  insz -= vec3i(1, 1, 1);

  // conv5a-p1
  float conv5a_k[48*60*2*3*3];
  float conv5a_b[60];
  read_from_file<float>("./0421_VD2D3D-MS/conv5a-p1/filters",conv5a_k,48*60*2*3*3);
  fix_dims(conv5a_k, 48, 60, 2, 3, 3);
  read_from_file<float>("./0421_VD2D3D-MS/nconv5a-p1/biases",conv5a_b,60);
  layers.push_back(std::unique_ptr<host::v1::host_layer>
                   (new host::v1::fft_conv
                    (4, 48, 60,
                     insz, vec3i(2,3,3),
                     conv5a_k, conv5a_b, activation::relu)));
  insz -= vec3i(1, 2, 2);

  // conv5b-p1
  float conv5b_k[60*60*2*3*3];
  float conv5b_b[60];
  read_from_file<float>("./0421_VD2D3D-MS/conv5b-p1/filters",conv5b_k,60*60*2*3*3);
  fix_dims(conv5b_k, 60, 60, 2, 3, 3);
  read_from_file<float>("./0421_VD2D3D-MS/nconv5b-p1/biases",conv5b_b,60);
  layers.push_back(std::unique_ptr<host::v1::host_layer>
                   (new host::v1::fft_conv
                    (4, 60, 60,
                     insz, vec3i(2,3,3),
                     conv5b_k, conv5b_b, activation::relu)));
  insz -= vec3i(1, 2, 2);

  // convx-p1
  float convx_k[60*200*1*1*1];
  float convx_b[200];
  memset(convx_b, 0, 200 * sizeof(float));
  read_from_file<float>("./0421_VD2D3D-MS/convx-p1/filters",convx_k,60*200*1*1*1);
  fix_dims(convx_k, 60, 200, 1, 1, 1);
  layers.push_back(std::unique_ptr<host::v1::host_layer>
                   (new host::v1::fft_conv
                    (4, 60, 200,
                     insz, vec3i(1,1,1),
                     convx_k, convx_b, activation::none)));

  return layers;
}

  }} // namespace znn::fwd