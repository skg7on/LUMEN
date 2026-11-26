/* Compiles and links the public backend header as C11, proving the C ABI
 * boundary is usable from a C translation unit. */

#include <lumen/backend.h>

#include <stddef.h>

int main(void) {
  const lumen_backend_api_v1 *cpu;

  if (LUMEN_BACKEND_ABI_MAJOR != 1u) {
    return 1;
  }

  cpu = lumen_cpu_backend_api();
  if (cpu == NULL) {
    return 2;
  }
  if (cpu->struct_size != (uint32_t)sizeof(lumen_backend_api_v1)) {
    return 3;
  }
  if (lumen_backend_api_validate(cpu) != LUMEN_STATUS_OK) {
    return 4;
  }

  {
    lumen_context_t context = NULL;
    if (cpu->backend_open(cpu, &context) != LUMEN_STATUS_OK) {
      return 5;
    }
    if (context == NULL) {
      return 6;
    }
    if (cpu->context_close(context) != LUMEN_STATUS_OK) {
      return 7;
    }
  }

  return 0;
}
