g++ -c $1.cpp -std=c++11 -pthread -o $1.o -O2 -DNDEBUG -I../include -I../..  -DyockZNN_NO_PRECOMP_GEMM  -I/usr/local/cuda/include -I/usr/people/zlateski/cuda/include
g++ -L/usr/people/zlateski/cuda/lib -L/usr/local/cuda/lib64 utils.o $1.o -lpthread -lcudart -lcudnn -lcufft -lfftw3f -lcublas -o $1_gpu
