#pragma once

#ifdef __unix__

// System's CPU setup

#ifndef ZNNI_NUM_CHIPS
#  define ZNNI_NUM_CHIPS 1
#endif

#ifndef ZNNI_CORES_PER_CHIP
#  define ZNNI_CORES_PER_CHIP 1
#endif

#ifndef ZNNI_THREADS_PER_CORE
#  define ZNNI_THREADS_PER_CORE 1
#endif

// Resources available to ZNNi

#ifndef ZNNI_ALLOW_NUM_CHIPS
#  define ZNNI_ALLOW_NUM_CHIPS 1
#endif

#ifndef ZNNI_ALLOW_CORES_PER_CHIP
#  define ZNNI_ALLOW_CORES_PER_CHIP 1
#endif

#ifndef ZNNI_ALLOW_THREADS_PER_CORE
#  define ZNNI_ALLOW_THREADS_PER_CORE 1
#endif


#include "znn/types.hpp"

//#define _GNU_SOURCE
#include <sched.h>
#include <atomic>
#include <tbb/tbb.h>
#include <zi/utility/singleton.hpp>

namespace znn { namespace fwd { namespace host {

class architectire
{
private:
    cpu_set_t all_threads_;
    cpu_set_t chip_threads_[ZNNI_ALLOW_NUM_CHIPS];
    std::unique_ptr<tbb::task_scheduler_init> tbb_scheduler_;

public:
    cpu_set_t& all_threads()
    {
        return all_threads_;
    }

    cpu_set_t& chip_threads( int c )
    {
        return chip_threads_[c];
    }

    architectire()
    {
        CPU_ZERO(&all_threads_);
        for ( int chip = 0; chip < ZNNI_ALLOW_NUM_CHIPS; ++chip )
        {
            CPU_ZERO(&chip_threads_[chip]);
            for ( int core = 0; core < ZNNI_ALLOW_CORES_PER_CHIP; ++core )
            {
                int cid = chip * ZNNI_CORES_PER_CHIP + core;

                CPU_SET(cid, &all_threads_);
                CPU_SET(cid, &chip_threads_[chip]);

                for ( int i = 1; i < ZNNI_ALLOW_THREADS_PER_CORE; ++i )
                {
                    CPU_SET(cid + i * system_cores(), &all_threads_);
                    CPU_SET(cid + i * system_cores(), &chip_threads_[chip]);
                }
            }
        }

        sched_setaffinity(0, sizeof(all_threads_), &all_threads_);

        tbb_scheduler_ = make_unique<tbb::task_scheduler_init>
            (architectire::available_threads());
    }

    static constexpr int system_chips() noexcept
    {
        return ZNNI_NUM_CHIPS;
    }

    static constexpr int system_cores() noexcept
    {
        return ZNNI_NUM_CHIPS * ZNNI_CORES_PER_CHIP;
    }

    static constexpr int system_threads() noexcept
    {
        return system_cores() * ZNNI_THREADS_PER_CORE;
    }

    static constexpr int available_chips() noexcept
    {
        return ZNNI_ALLOW_NUM_CHIPS;
    }

    static constexpr int available_cores() noexcept
    {
        return ZNNI_ALLOW_NUM_CHIPS * ZNNI_ALLOW_CORES_PER_CHIP;
    }

    static constexpr int available_threads() noexcept
    {
        return available_cores() * ZNNI_ALLOW_THREADS_PER_CORE;
    }

    static int to_real_cpu_thread(int znni_thread) noexcept
    {
        int vthread = znni_thread / architectire::available_cores();
        znni_thread %= architectire::available_cores();

        int chip = znni_thread % ZNNI_ALLOW_NUM_CHIPS;
        znni_thread /= ZNNI_ALLOW_NUM_CHIPS;

        return znni_thread + chip * ZNNI_CORES_PER_CHIP
            + vthread * architectire::system_cores();
    }
};

namespace {
architectire& machine = zi::singleton<architectire>::instance();
}


class thread_distributor
{
private:
    std::atomic<int> counter;
    int total;

public:
    thread_distributor()
        : counter(0)
        , total(architectire::available_threads())
    {}

    thread_distributor( thread_distributor const & ) = delete;
    thread_distributor operator=( thread_distributor const & ) = delete;

    int next()
    {
        int r = counter++;
        return r % total;
    }

};

class thread_pin
{
public:
    int const znni_thread_id;
    int const real_thread_id;

private:
    cpu_set_t old_set;

public:
    explicit thread_pin( thread_distributor & td )
        : znni_thread_id(td.next())
        , real_thread_id(architectire::to_real_cpu_thread(znni_thread_id))
    {
        // sched_getaffinity(0, sizeof(old_set), &old_set);
        // cpu_set_t cpuset;
        // CPU_ZERO(&cpuset);
        // CPU_SET( real_thread_id , &cpuset);
        // sched_setaffinity(0, sizeof(cpuset), &cpuset);
    }

    int location() const
    {
        return real_thread_id;
    }

    ~thread_pin()
    {
        //sched_setaffinity(0, sizeof(old_set), &old_set);
    }

    thread_pin( thread_pin const & ) = delete;
    thread_pin& operator=( thread_pin const & ) = delete;

};

class cpu_pin
{
// private:
//     cpu_set_t old_set;

public:
    explicit cpu_pin( int vcore )
    {
        //sched_getaffinity(0, sizeof(old_set), &old_set);
        // int chip = vcore % architectire::system_cores();
        // chip /= ZNNI_CORES_PER_CHIP;
        // auto& cpuset = machine.chip_threads(chip);
        // sched_setaffinity(0, sizeof(cpuset), &cpuset);
    }

    // ~cpu_pin()
    // {
    //     //sched_setaffinity(0, sizeof(old_set), &old_set);
    // }

    cpu_pin( cpu_pin const & ) = delete;
    cpu_pin& operator=( cpu_pin const & ) = delete;

};


}}} // namespace znn::fwd::host

#else

namespace znn { namespace fwd { namespace host {

class thread_distributor
{
public:
    explicit thread_distributor(int=0,int=0,bool=true)
    {}

    thread_distributor( thread_distributor const & ) = delete;
    thread_distributor operator=( thread_distributor const & ) = delete;

};

class cpu_pin
{
public:
    explicit cpu_pin(int)
    {}

    cpu_pin( cpu_pin const & ) = delete;
    cpu_pin& operator=( cpu_pin const & ) = delete;

};

class thread_pin
{
public:
    explicit thread_pin( thread_distributor & )
    {}

    int location() noexcept
    { return 0; }

    thread_pin( thread_pin const & ) = delete;
    thread_pin& operator=( thread_pin const & ) = delete;

};



}}} // namespace znn::fwd::host

#endif
