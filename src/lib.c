#include "lib.h"

void *memset(void *b, int c, long len){
  unsigned char *p = b;
  while (len-- > 0) *p++ = (unsigned char)c;
  return b;
}

void *memcpy(void *dst, const void *src, long len){
  unsigned char *d = dst;
  const unsigned char *s = src;
  while (len-- > 0) *d++ = *s++;
  return dst;
}

int memcmp(const void *b1, const void *b2, long len){
  const unsigned char *p1 = b1, *p2 = b2;
  while (len-- > 0) {
    if (*p1 != *p2) return (*p1 > *p2) ? 1 : -1;
    p1++; p2++;
  }
  return 0;
}

int strlen(const char *s){
  int len = 0;
  while (*s++) len++;
  return len;
}

char *strcpy(char *dst, const char *src){
  char *ret = dst;
  do { *dst++ = *src; } while (*src++);
  return ret;
}

int strcmp(const char *s1, const char *s2){
  while (*s1 && *s1 == *s2) { s1++; s2++; }
  return (*(const unsigned char *)s1 > *(const unsigned char *)s2) ? 1 :
         (*(const unsigned char *)s1 < *(const unsigned char *)s2) ? -1 : 0;
}

int strncmp(const char *s1, const char *s2, int len){
  while (len-- > 0) {
    unsigned char a = (unsigned char)*s1++;
    unsigned char b = (unsigned char)*s2++;
    if (a != b) return (a > b) ? 1 : -1;
    if (!a) return 0;
  }
  return 0;
}
