#include <string.h>

size_t strlen(const char* s) {
  size_t len = 0;
  if (s == NULL) {
    return 0;
  }
  while (s[len] != '\0') {
    len++;
  }
  return len;
}

size_t strnlen(const char* s, size_t maxlen) {
  size_t len = 0;
  if (s == NULL) {
    return 0;
  }
  while (len < maxlen && s[len] != '\0') {
    len++;
  }
  return len;
}

int strcmp(const char* s1, const char* s2) {
  if (s1 == NULL || s2 == NULL) {
    if (s1 == s2) {
      return 0;
    }
    return (s1 == NULL) ? -1 : 1;
  }

  while (*s1 != '\0' && *s1 == *s2) {
    s1++;
    s2++;
  }
  return (int)((unsigned char)*s1 - (unsigned char)*s2);
}

int strncmp(const char* s1, const char* s2, size_t n) {
  if (n == 0) {
    return 0;
  }
  if (s1 == NULL || s2 == NULL) {
    if (s1 == s2) {
      return 0;
    }
    return (s1 == NULL) ? -1 : 1;
  }

  for (size_t i = 0; i < n; i++) {
    if (s1[i] != s2[i]) {
      return (int)((unsigned char)s1[i] - (unsigned char)s2[i]);
    }
    if (s1[i] == '\0') {
      return 0;
    }
  }
  return 0;
}

char* strcpy(char* dest, const char* src) {
  if (dest == NULL || src == NULL) {
    return dest;
  }

  size_t i = 0;
  while (src[i] != '\0') {
    dest[i] = src[i];
    i++;
  }
  dest[i] = '\0';
  return dest;
}

char* strncpy(char* dest, const char* src, size_t n) {
  if (dest == NULL || src == NULL || n == 0) {
    return dest;
  }

  size_t i = 0;
  while (i < n && src[i] != '\0') {
    dest[i] = src[i];
    i++;
  }
  while (i < n) {
    dest[i] = '\0';
    i++;
  }
  return dest;
}

size_t strlcpy(char* dst, const char* src, size_t size) {
  if (src == NULL) {
    if (dst != NULL && size > 0) {
      dst[0] = '\0';
    }
    return 0;
  }

  size_t src_len = strlen(src);
  if (dst == NULL || size == 0) {
    return src_len;
  }

  size_t copy_len = (src_len >= size) ? size - 1 : src_len;
  for (size_t i = 0; i < copy_len; i++) {
    dst[i] = src[i];
  }
  dst[copy_len] = '\0';

  return src_len;
}
