// dm_writer
#include <cstdint>
#include "tools/profiler/kernel_profiler.hpp"
#include "internal/firmware_common.h"
#include "api/dataflow/dataflow_api.h"
void kernel_main() {
  int32_t v1 = 0;
  int32_t v2 = 2048;
  int32_t v3 = 1;
  size_t v4 = 0;
  cb_wait_front(get_compile_time_arg_val(1), v3);
  int32_t v5 = get_common_arg_val<uint32_t>(v4);
  auto tensor_accessor_args_8 = TensorAccessorArgs<tensor_accessor::detail::get_tensor_accessor_args_cta_offset<1, 2>(), 0>();
  TensorAccessor v6 = TensorAccessor(tensor_accessor_args_8, v5, v2);
  int32_t v7 = get_read_ptr(get_compile_time_arg_val(1));
  noc_async_write_tile(v1, v6, v7);
  noc_async_write_barrier();
  cb_pop_front(get_compile_time_arg_val(1), v3);
  return;
}

