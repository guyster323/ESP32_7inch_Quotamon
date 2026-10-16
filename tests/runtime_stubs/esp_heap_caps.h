#pragma once
#include <cstddef>
constexpr unsigned MALLOC_CAP_SPIRAM=1, MALLOC_CAP_INTERNAL=2, MALLOC_CAP_8BIT=4;
struct multi_heap_info_t {
    size_t total_free_bytes, minimum_free_bytes, largest_free_block;
};
void heap_caps_get_info(multi_heap_info_t*, unsigned);
