//  Copyright (c) 2019-2021 Hartmut Kaiser
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/config.hpp>
#if !defined(HPX_COMPUTE_DEVICE_CODE)
#include <hpx/hpx.hpp>
#include <hpx/hpx_init.hpp>
#include <hpx/modules/collectives.hpp>
#include <hpx/modules/testing.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

constexpr char const* reduce_basename = "/test/reduce/";
constexpr char const* reduce_direct_basename = "/test/reduce_direct/";

void test_one_shot_use()
{
    std::uint32_t num_localities = hpx::get_num_localities(hpx::launch::sync);
    std::uint32_t this_locality = hpx::get_locality_id();

    // test functionality based on future<> of local result
    for (int i = 0; i != 10; ++i)
    {
        hpx::future<std::uint32_t> value =
            hpx::make_ready_future(hpx::get_locality_id());

        if (this_locality == 0)
        {
            hpx::future<std::uint32_t> overall_result =
                hpx::reduce_here(reduce_basename, std::move(value),
                    std::plus<std::uint32_t>{}, num_localities, i);

            std::uint32_t sum = 0;
            for (std::uint32_t j = 0; j != num_localities; ++j)
            {
                sum += j;
            }
            HPX_TEST_EQ(sum, overall_result.get());
        }
        else
        {
            hpx::future<void> overall_result =
                hpx::reduce_there(reduce_basename, std::move(value), i);
            overall_result.get();
        }
    }

    // test functionality based on immediate local result value
    for (int i = 0; i != 10; ++i)
    {
        std::uint32_t value = hpx::get_locality_id();

        if (this_locality == 0)
        {
            hpx::future<std::uint32_t> overall_result =
                hpx::reduce_here(reduce_direct_basename, value,
                    std::plus<std::uint32_t>{}, num_localities, i);

            std::uint32_t sum = 0;
            for (std::uint32_t j = 0; j != num_localities; ++j)
            {
                sum += j;
            }
            HPX_TEST_EQ(sum, overall_result.get());
        }
        else
        {
            hpx::future<void> overall_result =
                hpx::reduce_there(reduce_direct_basename, std::move(value), i);
            overall_result.get();
        }
    }
}

void test_multiple_use()
{
    std::uint32_t num_localities = hpx::get_num_localities(hpx::launch::sync);
    std::uint32_t this_locality = hpx::get_locality_id();

    auto reduce_client = hpx::create_reducer(reduce_basename, num_localities);

    // test functionality based on future<> of local result
    for (int i = 0; i != 10; ++i)
    {
        hpx::future<std::uint32_t> value =
            hpx::make_ready_future(hpx::get_locality_id());

        if (this_locality == 0)
        {
            hpx::future<std::uint32_t> overall_result = hpx::reduce_here(
                reduce_client, std::move(value), std::plus<std::uint32_t>{});

            std::uint32_t sum = 0;
            for (std::uint32_t j = 0; j != num_localities; ++j)
            {
                sum += j;
            }
            HPX_TEST_EQ(sum, overall_result.get());
        }
        else
        {
            hpx::future<void> overall_result =
                hpx::reduce_there(reduce_client, std::move(value));
            overall_result.get();
        }
    }

    auto reduce_direct_client =
        hpx::create_reducer(reduce_direct_basename, num_localities);

    // test functionality based on immediate local result value
    for (int i = 0; i != 10; ++i)
    {
        std::uint32_t value = hpx::get_locality_id();

        if (this_locality == 0)
        {
            hpx::future<std::uint32_t> overall_result = hpx::reduce_here(
                reduce_direct_client, value, std::plus<std::uint32_t>{});

            std::uint32_t sum = 0;
            for (std::uint32_t j = 0; j != num_localities; ++j)
            {
                sum += j;
            }
            HPX_TEST_EQ(sum, overall_result.get());
        }
        else
        {
            hpx::future<void> overall_result =
                hpx::reduce_there(reduce_direct_client, std::move(value));
            overall_result.get();
        }
    }
}

int hpx_main()
{
    test_one_shot_use();
    test_multiple_use();

    return hpx::finalize();
}

int main(int argc, char* argv[])
{
    std::vector<std::string> const cfg = {"hpx.run_hpx_main!=1"};

    hpx::init_params init_args;
    init_args.cfg = cfg;

    HPX_TEST_EQ(hpx::init(argc, argv, init_args), 0);
    return hpx::util::report_errors();
}
#endif
