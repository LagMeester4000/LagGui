#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// TODO: Should len be a size_t?
unsigned int xcrc32(const unsigned char* buf, int len, unsigned int init);

#ifdef __cplusplus
}
#endif
