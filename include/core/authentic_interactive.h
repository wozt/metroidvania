#ifndef FUSION_AUTHENTIC_INTERACTIVE_H
#define FUSION_AUTHENTIC_INTERACTIVE_H

#include <stdbool.h>
#include <stddef.h>

/* SDL3 native-ROM proof: one live mGBA engine; M switches worlds.
 * Destination characters remain their native entities (no guest swap). */
bool authentic_interactive_run(const char *metroid_path, const char *aria_path,
                              char *error, size_t error_size);

#endif
