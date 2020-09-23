/*
 * Copyright (c) 2020, NVIDIA CORPORATION.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <rmm/detail/error.hpp>
#include <rmm/mr/device/device_memory_resource.hpp>

namespace rmm {
namespace mr {
/**
 * @brief Resource that uses `Upstream` to allocate memory and limits the total
 * allocations possible.
 *
 * An instance of this resource can be constructed with an existing, upstream
 * resource in order to satisfy allocation requests, but any existing allocations
 * will be untracked.
 *
 * @tparam Upstream Type of the upstream resource used for
 * allocation/deallocation.
 */
template <typename Upstream>
class limiting_resource_adaptor final : public device_memory_resource {
 public:
  /**
   * @brief Construct a new limiting resource adaptor using `upstream` to satisfy
   * allocation requests and limiting the total allocation amount possible.
   *
   * @throws `rmm::logic_error` if `upstream == nullptr`
   *
   * @param upstream The resource used for allocating/deallocating device memory
   * @param allocation_limit Maximum memory allowed for this allocator.
   */
  limiting_resource_adaptor(Upstream* upstream,
                            std::size_t allocation_limit_ = std::numeric_limits<std::size_t>::max(),
                            std::size_t allocation_alignment_ = 256)
    : upstream_{upstream},
      allocation_limit{allocation_limit_},
      allocation_alignment(allocation_alignment_),
      used(0)
  {
    RMM_EXPECTS(nullptr != upstream, "Unexpected null upstream resource pointer.");
  }

  limiting_resource_adaptor()                                 = delete;
  ~limiting_resource_adaptor()                                = default;
  limiting_resource_adaptor(limiting_resource_adaptor const&) = delete;
  limiting_resource_adaptor(limiting_resource_adaptor&&)      = default;
  limiting_resource_adaptor& operator=(limiting_resource_adaptor const&) = delete;
  limiting_resource_adaptor& operator=(limiting_resource_adaptor&&) = default;

  /**
   * @brief Return pointer to the upstream resource.
   *
   * @return Upstream* Pointer to the upstream resource.
   */
  Upstream* get_upstream() const noexcept { return upstream_; }

  /**
   * @brief Checks whether the upstream resource supports streams.
   *
   * @return true The upstream resource supports streams
   * @return false The upstream resource does not support streams.
   */
  bool supports_streams() const noexcept override { return upstream_->supports_streams(); }

  /**
   * @brief Query whether the resource supports the get_mem_info API.
   *
   * @return bool true if the upstream resource supports get_mem_info, false otherwise.
   */
  bool supports_get_mem_info() const noexcept override
  {
    return upstream_->supports_get_mem_info();
  }

  std::size_t space_free() const { return allocation_limit - used; }

 private:
  /**
   * @brief Allocates memory of size at least `bytes` using the upstream
   * resource as long as it fits inside the allocation limit.
   *
   * The returned pointer has at least 256B alignment.
   *
   * @throws `rmm::bad_alloc` if the requested allocation could not be fulfilled
   * by the upstream resource.
   *
   * @param bytes The size, in bytes, of the allocation
   * @param stream Stream on which to perform the allocation
   * @return void* Pointer to the newly allocated memory
   */
  void* do_allocate(std::size_t bytes, cudaStream_t stream) override
  {
    void* p = nullptr;

    std::size_t proposed_size = rmm::detail::align_up(bytes, allocation_alignment);
    if (proposed_size + used <= allocation_limit) {
      used += proposed_size;
      try {
        p = upstream_->allocate(bytes, stream);
      } catch (std::exception const& e) {
        RMM_LOG_ERROR("[A][Stream {}][Upstream {}B][FAILURE {}]",
                      reinterpret_cast<void*>(stream),
                      bytes,
                      e.what());
        throw;
      }
    } else {
      throw rmm::cuda_error{std::string{"CUDA error: Unable to allocate memory due to limit"}};
    }

    return p;
  }

  /**
   * @brief Free allocation of size `bytes` pointed to by `p`
   *
   * @throws Nothing.
   *
   * @param p Pointer to be deallocated
   * @param bytes Size of the allocation
   * @param stream Stream on which to perform the deallocation
   */
  void do_deallocate(void* p, std::size_t bytes, cudaStream_t stream) override
  {
    std::size_t allocated_size = rmm::detail::align_up(bytes, allocation_alignment);
    used -= allocated_size;
    upstream_->deallocate(p, bytes, stream);
  }

  /**
   * @brief Compare the upstream resource to another.
   *
   * @throws Nothing.
   *
   * @param other The other resource to compare to
   * @return true If the two resources are equivalent
   * @return false If the two resources are not equal
   */
  bool do_is_equal(device_memory_resource const& other) const noexcept override
  {
    if (this == &other)
      return true;
    else {
      limiting_resource_adaptor<Upstream> const* cast =
        dynamic_cast<limiting_resource_adaptor<Upstream> const*>(&other);
      if (cast != nullptr)
        return upstream_->is_equal(*cast->get_upstream());
      else
        return upstream_->is_equal(other);
    }
  }

  /**
   * @brief Get free and available memory from upstream resource.
   *
   * @throws `rmm::cuda_error` if unable to retrieve memory info.
   *
   * @param stream Stream on which to get the mem info.
   * @return std::pair contaiing free_size and total_size of memory
   */
  std::pair<size_t, size_t> do_get_mem_info(cudaStream_t stream) const override
  {
    auto ret = upstream_->get_mem_info(stream);
    return {std::min(ret.first, space_free()), std::max(ret.second, allocation_limit)};
  }

  // maximum bytes this allocator is allowed to allocate.
  std::size_t allocation_limit;

  // number of currently-allocated bytes
  std::size_t used;

  // todo: should be some way to ask the upstream...
  std::size_t allocation_alignment;

  Upstream* upstream_;  ///< The upstream resource used for satisfying
                        ///< allocation requests
};

/**
 * @brief Convenience factory to return a `limiting_resource_adaptor` around the
 * upstream resource `upstream`.
 *
 * @tparam Upstream Type of the upstream `device_memory_resource`.
 * @param upstream Pointer to the upstream resource
 * @param limit Maximum amount of memory to allocate
 */
template <typename Upstream>
limiting_resource_adaptor<Upstream> make_limiting_adaptor(Upstream* upstream,
                                                          size_t allocation_limit)
{
  return limiting_resource_adaptor<Upstream>{upstream, allocation_limit};
}

}  // namespace mr
}  // namespace rmm