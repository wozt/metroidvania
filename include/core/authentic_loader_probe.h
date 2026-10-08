#ifndef FUSION_AUTHENTIC_LOADER_PROBE_H
#define FUSION_AUTHENTIC_LOADER_PROBE_H

#include <stdbool.h>
#include <stddef.h>

bool authentic_loader_probe_run(const char *metroid_path,
                                 const char *aria_path,
                                 char *error, size_t error_size);

bool authentic_roundtrip_probe_run(const char *metroid_path,
                                   const char *aria_path,
                                   char *error, size_t error_size);

#endif
