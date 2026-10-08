#ifndef FUSION_CORE_SHA1_H
#define FUSION_CORE_SHA1_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool sha1_file(const char *path, uint8_t digest[20], char *error, size_t error_size);
void sha1_hex(const uint8_t digest[20], char output[41]);

#endif
