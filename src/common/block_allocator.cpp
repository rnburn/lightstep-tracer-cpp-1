#include "common/block_allocator.h"

#include <cassert>
#include <cstdlib>
#include <new>
#include <iostream>

#include "common/lock_free_list.h"

namespace lightstep {
//--------------------------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------------------------
BlockAllocator::BlockAllocator(size_t block_size, size_t max_blocks)
    : block_size_{block_size},
      max_blocks_{max_blocks},
      data_{static_cast<char*>(std::malloc(block_size_ * max_blocks_))} {
        assert(block_size >= sizeof(Node));
}

//--------------------------------------------------------------------------------------------------
// destructor
//--------------------------------------------------------------------------------------------------
BlockAllocator::~BlockAllocator() noexcept { std::free(data_); }

//--------------------------------------------------------------------------------------------------
// allocate
//--------------------------------------------------------------------------------------------------
void* BlockAllocator::allocate() {
  if (max_blocks_ == 0) {
    auto result = std::malloc(block_size_);
    if (result == nullptr) {
      throw std::bad_alloc{};
    }
    return result;
  }
  auto node = ListPopFront(free_list_);
  /* auto cptr = reinterpret_cast<char*>(node); */
  /* if (cptr != nullptr && (cptr < data_ || cptr >= data_ + max_blocks_ * block_size_)) { */
  /*   std::cerr << "pop: invalid ptr\n"; */
  /*   std::cerr << "cptr = " << static_cast<void*>(cptr) << std::endl; */
  /*   std::cerr << "delta = " << std::distance(data_, cptr) << "\n"; */
  /*   std::terminate(); */
  /* } */
  if (node != nullptr) {
    return static_cast<void*>(node);
  }
  int64_t index = reserve_index_++;
  if (index >= static_cast<int64_t>(max_blocks_)) {
    throw std::bad_alloc{};
  }
  return static_cast<void*>(data_ + index * block_size_);
}

//--------------------------------------------------------------------------------------------------
// deallocate
//--------------------------------------------------------------------------------------------------
void BlockAllocator::deallocate(void* ptr) noexcept {
  assert(ptr != nullptr);
  if (max_blocks_ == 0) {
    return std::free(ptr);
  }
  /* auto cptr = reinterpret_cast<char*>(ptr); */
  /* if (cptr < data_ || cptr >= data_ + max_blocks_ * block_size_) { */
  /*   std::cerr << "push: invalid ptr\n"; */
  /*   std::terminate(); */
  /* } */
  ListPushFront(free_list_, *reinterpret_cast<Node*>(ptr));
}
}  // namespace lightstep
