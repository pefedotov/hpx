//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream> 
#include <string> 
#include <vector>
#include "hpx/futures/future.hpp"

#include <hpx/hpx_init.hpp> 
#include <hpx/local/future.hpp>
#if defined(HPX_HAVE_SYCL)


#include <hpx/async_sycl/sycl_future.hpp> 
#include <CL/sycl.hpp> 

// Check compiler compatability
// Needs to be done AFTER sycl include for HipSYCL
// (intel dpcpp would be fine without the include)
#if defined(SYCL_LANGUAGE_VERSION)
#pragma message("OKAY: Sycl compiler detected!")
#if defined(__INTEL_LLVM_COMPILER)
#pragma message("OKAY: Intel dpcpp detected!")
#elif defined(__HIPSYCL__)
#pragma message("OKAY: HIPSycl compiler detected!")
#else
#warning("HPX-SYCL integration only tested with Intel oneapi and HipSYCL. \
Utilized compiler appears to be neither of those!")
#endif
#else
#error("Compiler does not seem to support SYCL! SYCL_LANGUAGE_VERSION is undefined!")
#endif

// Check for separate compiler host and device passes
#if defined(__SYCL_SINGLE_SOURCE__)
#error("Sycl single source compiler not supported! Use one with multiple passes")
#else
#pragma message("OKAY: Sycl compiler with two or more compile passes detected!")
#endif

#if defined(__SYCL_DEVICE_ONLY__)
#pragma message("Sycl device pass...")
#else
#pragma message("Sycl host pass...")
#endif


#if defined(__HIPSYCL__) 
// Lots of warning within the hipsycl headers
// To compiler with Werror we need to disable those
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-copy"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wgcc-compat"
#pragma clang diagnostic ignored "-Wembedded-directive"
#pragma clang diagnostic ignored "-Wmismatched-tags"
#pragma clang diagnostic ignored "-Wreorder-ctor"
#endif


constexpr size_t vector_size = 2000000;

void VectorAdd(cl::sycl::queue& q, const std::vector<int>& a_vector,
    const std::vector<int>& b_vector, std::vector<int>& add_parallel)
{
    // Kind of superfluous check, but without that macro defined event polling won't work
    // Might as well make sure...
#if defined(HPX_HAVE_MODULE_ASYNC_SYCL)
    std::cerr << "OKAY: HPX_HAVE_MODULE_ASYNC_SYCL is defined!" << std::endl;
#else
    std::cerr << "Error: HPX_HAVE_MODULE_ASYNC_SYCL is not defined!" << std::endl;
    std::terminate();
#endif

    cl::sycl::event my_kernel_event;
    // input range
    cl::sycl::range<1> num_items{a_vector.size()};
    {
      // buffers from host vectors
      cl::sycl::buffer a_buf(a_vector.data(), num_items);
      cl::sycl::buffer b_buf(b_vector.data(), num_items);
      cl::sycl::buffer add_buf(add_parallel.data(), num_items);

      my_kernel_event = q.submit([&](cl::sycl::handler& h) {
          // Tell sycl we'd like to access our buffers here
          cl::sycl::accessor a(a_buf, h, cl::sycl::read_only);
          cl::sycl::accessor b(b_buf, h, cl::sycl::read_only);
          cl::sycl::accessor add(add_buf, h, cl::sycl::write_only, cl::sycl::no_init);
          // run Add kernel
          h.parallel_for(num_items, [=](auto i) { add[i] = a[i] + b[i]; });
          // Note: destruction of the accessors should not cause a device->host
          // memcpy (I think...)
      });
      hpx::future<void> my_kernel_future =
          hpx::sycl::experimental::detail::get_future(q, my_kernel_event);
      const auto event_status_before =
          my_kernel_event.get_info<cl::sycl::info::event::command_execution_status>();
      if (event_status_before != cl::sycl::info::event_command_status::complete)
      {
          std::cerr << "OKAY: Kernel event not complete immediately after launch!" 
              << std::endl;
      } else {
          std::cerr << "ERROR: Kernel event is immediately complete " 
              << "(thus the launch probably is not asynchronous at at all)!" << std::endl;
          std::terminate();
      }
      if (my_kernel_future.is_ready())
      {
          std::cerr << "ERROR: Async kernel launch future is immediately ready " 
              << "(thus probably not asynchronous at at all)!" << std::endl;
          std::terminate();
      } else {
          std::cout<< "OKAY: Kernel hpx::future is NOT ready immediately after launch!" 
              << std::endl;
      }
      auto continuation_future = my_kernel_future.then([](auto && fut) {
          fut.get();
          std::cout << "OKAY: Continuation working!" << std::endl;
          return;
          });
      continuation_future.get();

      const auto event_status_after =
          my_kernel_event.get_info<cl::sycl::info::event::command_execution_status>();
      if (event_status_after == cl::sycl::info::event_command_status::complete)
      {
          std::cerr << "OKAY: Kernel is done!" << std::endl;
      } else {
          std::cerr << "ERROR: Kernel still running after continuation.get()!" << std::endl;
          std::terminate();
      }

      // NOTE about usage:
      // according to the sycl specification (2020) section 3.9.8, the entire
      // thing will synchronize here, due to the buffers being destroyed!
      //
      // Hence this implictly syncs everything, so we should use get on any
      // futures/continuations beforehand
      // (or simply make sure that the sycl buffers (a_buf, b_buf_ add_buf)
      // have a longer lifetime by moving them to another scope. 
    }
}

int hpx_main(int, char**)
{
    std::cout << "Starting HPX main" << std::endl;

    // Enable polling for the future
    hpx::sycl::experimental::detail::register_polling(hpx::resource::get_thread_pool(0));
    std::cout << "SYCL Future polling enabled!\n";

    std::cout << "SYCL language version: " << SYCL_LANGUAGE_VERSION << "\n";


    // Select default sycl device
    cl::sycl::default_selector d_selector;

    // input vectors
    std::vector<int> a(vector_size), b(vector_size),
        add_sequential(vector_size), add_parallel(vector_size);
    for (size_t i = 0; i < a.size(); i++)
    {
        a.at(i) = static_cast<int>(i);
        b.at(i) = static_cast<int>(i);
    }

    try
    {
        // TODO Insert executor once finished
      cl::sycl::queue q(d_selector, cl::sycl::property::queue::in_order{});
        /* queue q(d_selector); */
        std::cout << "Running on device: "
                  << q.get_device().get_info<cl::sycl::info::device::name>() << "\n";
        std::cout << "Vector size: " << a.size() << "\n";
        // TODO Launch with executor
        VectorAdd(q, a, b, add_parallel);
        // TODO Add check for asynchronous launch
        // TODO Synchronize executor
    }
    catch (cl::sycl::exception const& e)
    {
        std::cout << "An exception is caught for vector add.\n";
        std::terminate();
    }

    // Check results
    for (size_t i = 0; i < add_sequential.size(); i++)
        add_sequential.at(i) = a.at(i) + b.at(i);
    for (size_t i = 0; i < add_sequential.size(); i++)
    {
        if (add_parallel.at(i) != add_sequential.at(i))
        {
            std::cerr << "Vector add failed on device.\n ";
            std::terminate();
        }
    }
    std::cout << "OKAY: Vector add results correct!\n";

    static_assert(vector_size >= 6, "vector_size unreasonably small");
    for (size_t i = 0; i < 3; i++)
    {
        std::cout << "[" << i << "]: " << a[i] << " + " << b[i] << " = "
                  << add_parallel[i] << "\n";
    }
    std::cout << "...\n";
    for (size_t i = 3; i > 0; i--)
    {
        std::cout << "[" << vector_size - i << "]: " << a[vector_size - i]
                  << " + " << b[vector_size - i] << " = "
                  << add_parallel[vector_size - i] << "\n";
    }

    a.clear();
    b.clear();
    add_sequential.clear();
    add_parallel.clear();

    // Disable polling
    std::cout << "Disabling SYCL future polling.\n";
    hpx::sycl::experimental::detail::unregister_polling(hpx::resource::get_thread_pool(0));

    std::cout << "Finalizing HPX main" << std::endl;
    return hpx::finalize();
}

int main(int argc, char* argv[])
{
    std::cout << "Starting main" << std::endl;
    return hpx::init(argc, argv);
}
#endif
