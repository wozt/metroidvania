/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/assets/gba.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const uint8_t bytes[] = {0x34, 0x12, 0x78, 0x56, 0x9A, 0xBC};
    FusionBinaryReader reader;
    fusion_binary_reader_init(&reader, bytes, sizeof bytes);
    uint16_t u16;
    uint32_t u32;
    assert(fusion_binary_read_u16(&reader, &u16) && u16 == 0x1234);
    assert(fusion_binary_read_u32(&reader, &u32) && u32 == 0xBC9A5678u);
    assert(fusion_binary_remaining(&reader) == 0);
    assert(!fusion_binary_read_u8(&reader, (uint8_t *)&u16));

    const uint8_t literals[] = {0x10, 4, 0, 0, 0, 1, 2, 3, 4};
    uint8_t output[16] = {0};
    size_t written = 0;
    assert(fusion_gba_lz77_uncompress(literals, sizeof literals, output, sizeof output,
                                      &written));
    assert(written == 4 && !memcmp(output, "\1\2\3\4", 4));

    const uint8_t overlap[] = {0x10, 7, 0, 0, 0x40, 'A', 0x30, 0};
    assert(fusion_gba_lz77_uncompress(overlap, sizeof overlap, output, sizeof output, &written));
    assert(written == 7 && !memcmp(output, "AAAAAAA", 7));

    const uint8_t run_length[] = {0x30, 8, 0, 0, 0x82, 'Z', 0x02, 'a', 'b', 'c'};
    assert(fusion_gba_rl_uncompress(run_length, sizeof run_length, output, sizeof output,
                                    &written));
    assert(written == 8 && !memcmp(output, "ZZZZZabc", 8));

    assert(!fusion_gba_lz77_uncompress(literals, 5, output, sizeof output, &written));
    const uint8_t bad_distance[] = {0x10, 3, 0, 0, 0x80, 0, 0};
    assert(!fusion_gba_lz77_uncompress(bad_distance, sizeof bad_distance, output, sizeof output,
                                       &written));
    const uint8_t bad_rle[] = {0x30, 2, 0, 0, 0x80, 'x'};
    assert(!fusion_gba_rl_uncompress(bad_rle, sizeof bad_rle, output, sizeof output, &written));
    puts("Native GBA binary and compression tests passed");
    return 0;
}
