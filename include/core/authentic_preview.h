#ifndef FUSION_AUTHENTIC_PREVIEW_H
#define FUSION_AUTHENTIC_PREVIEW_H

#include <stdbool.h>
#include <stddef.h>

bool authentic_arrival_preview_generate(const char *metroid_path,
                                        const char *aria_path,
                                        const char *output_directory,
                                        char *error, size_t error_size);

#endif
