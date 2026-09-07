#include <common/memory.h>

#include <stdint.h>

uintptr_t align_up(uintptr_t value, size_t alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

uintptr_t align_down(uintptr_t value, size_t alignment) {
  return value & ~(alignment - 1);
}

void memory_set(void* addr, size_t value, size_t size) {
  if (addr == 0 || size == 0) {
    return;
  }

  uint8_t* byte_ptr = (uint8_t*)addr;
  uint8_t byte_val = (uint8_t)value;

  while (size > 0 && ((uintptr_t)byte_ptr % sizeof(uintptr_t)) != 0) {
    *byte_ptr++ = byte_val;
    size--;
  }

  if (size >= sizeof(uintptr_t)) {
    uintptr_t word_val = 0;
    for (size_t i = 0; i < sizeof(uintptr_t); i++) {
      word_val = (word_val << 8) | byte_val;
    }

    uintptr_t* word_ptr = (uintptr_t*)byte_ptr;
    while (size >= sizeof(uintptr_t)) {
      *word_ptr++ = word_val;
      size -= sizeof(uintptr_t);
    }
    byte_ptr = (uint8_t*)word_ptr;
  }

  while (size > 0) {
    *byte_ptr++ = byte_val;
    size--;
  }
}

void memory_copy(void* addr, const void* src_addr, size_t size) {
  if (addr == 0 || src_addr == 0 || size == 0 || addr == src_addr) {
    return;
  }

  uint8_t* d = (uint8_t*)addr;
  const uint8_t* s = (const uint8_t*)src_addr;

  if ((uintptr_t)d < (uintptr_t)s) {
    if (((uintptr_t)d % sizeof(uintptr_t)) ==
        ((uintptr_t)s % sizeof(uintptr_t))) {
      while (size > 0 && ((uintptr_t)d % sizeof(uintptr_t)) != 0) {
        *d++ = *s++;
        size--;
      }

      uintptr_t* wd = (uintptr_t*)d;
      const uintptr_t* ws = (const uintptr_t*)s;
      while (size >= sizeof(uintptr_t)) {
        *wd++ = *ws++;
        size -= sizeof(uintptr_t);
      }
      d = (uint8_t*)wd;
      s = (const uint8_t*)ws;
    }

    while (size > 0) {
      *d++ = *s++;
      size--;
    }
  } else {
    d += size;
    s += size;
    if (((uintptr_t)d % sizeof(uintptr_t)) ==
        ((uintptr_t)s % sizeof(uintptr_t))) {
      while (size > 0 && ((uintptr_t)d % sizeof(uintptr_t)) != 0) {
        *--d = *--s;
        size--;
      }

      uintptr_t* wd = (uintptr_t*)d;
      const uintptr_t* ws = (const uintptr_t*)s;
      while (size >= sizeof(uintptr_t)) {
        *--wd = *--ws;
        size -= sizeof(uintptr_t);
      }
      d = (uint8_t*)wd;
      s = (const uint8_t*)ws;
    }

    while (size > 0) {
      *--d = *--s;
      size--;
    }
  }
}
