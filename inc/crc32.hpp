#pragma once

#ifdef __cplusplus
extern "C" {
#endif

unsigned int xcrc32(const unsigned char* buf, int len, unsigned int init);

#ifdef __cplusplus
}
#endif
