/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/assets/rom.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    char digest[41];
    const uint8_t abc[] = {'a', 'b', 'c'};
    assert(fusion_rom_sha1_hex(abc, sizeof abc, digest));
    assert(strcmp(digest, "a9993e364706816aba3e25717850c26c9cd0d89d") == 0);
    assert(!fusion_rom_sha1_hex(NULL, 1, digest));

    /* FIPS 180-4 vectors: empty, "abc", the two-block 448-bit message and
     * a 55/56/64-byte padding boundary set. */
    char digest256[65];
    assert(fusion_sha256_hex(NULL, 0, digest256));
    assert(!strcmp(digest256,
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    assert(fusion_sha256_hex(abc, sizeof abc, digest256));
    assert(!strcmp(digest256,
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    const char *two_blocks =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    assert(fusion_sha256_hex((const uint8_t *)two_blocks, strlen(two_blocks),
                             digest256));
    assert(!strcmp(digest256,
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
    uint8_t a_bytes[64];
    memset(a_bytes, 'a', sizeof a_bytes);
    assert(fusion_sha256_hex(a_bytes, 55, digest256));
    assert(!strcmp(digest256,
        "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318"));
    assert(fusion_sha256_hex(a_bytes, 56, digest256));
    assert(!strcmp(digest256,
        "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a"));
    assert(fusion_sha256_hex(a_bytes, 64, digest256));
    assert(!strcmp(digest256,
        "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb"));
    assert(!fusion_sha256_hex(NULL, 1, digest256));

    const FusionAssetGameSpec *aria = fusion_asset_game_from_id("aria");
    assert(aria && aria->expected_size == 8u * 1024u * 1024u);
    assert(strcmp(aria->sha1, "abd71fe01ebb201bcc133074db1dd8c5253776c7") == 0);
    assert(fusion_asset_game_from_id("unknown") == NULL);

    uint8_t verified_marker = 0;
    FusionRom verified = {
        .data = &verified_marker,
        .size = aria->expected_size,
        .game = FUSION_ASSET_GAME_ARIA,
    };
    char error[256];
    const char *manifest_path = "fusion-assets-rom-manifest.json";
    assert(fusion_rom_write_manifest(&verified, manifest_path,
                                     error, sizeof error));
    FILE *manifest = fopen(manifest_path, "rb");
    assert(manifest);
    char content[256];
    size_t content_size = fread(content, 1, sizeof content - 1, manifest);
    assert(ferror(manifest) == 0);
    assert(fclose(manifest) == 0);
    content[content_size] = '\0';
    assert(!strcmp(content,
        "{\n"
        "  \"game\": \"Castlevania: Aria of Sorrow USA\",\n"
        "  \"sha1\": \"abd71fe01ebb201bcc133074db1dd8c5253776c7\",\n"
        "  \"file\": \"Castlevania - Aria of Sorrow (USA).gba\"\n"
        "}\n"));
    assert(remove(manifest_path) == 0);
    const FusionAssetGameSpec *metroid =
        fusion_asset_game_spec(FUSION_ASSET_GAME_METROID);
    assert(metroid);
    verified.game = FUSION_ASSET_GAME_METROID;
    verified.size = metroid->expected_size;
    assert(fusion_rom_write_manifest(&verified, manifest_path,
                                     error, sizeof error));
    manifest = fopen(manifest_path, "rb");
    assert(manifest);
    content_size = fread(content, 1, sizeof content - 1, manifest);
    assert(ferror(manifest) == 0);
    assert(fclose(manifest) == 0);
    content[content_size] = '\0';
    assert(!strcmp(content,
        "{\n"
        "  \"game\": \"Metroid: Zero Mission USA\",\n"
        "  \"sha1\": \"5de8536afe1f0078ee6fe1089f890e8c7aa0a6e8\",\n"
        "  \"file\": \"Metroid - Zero Mission (USA).gba\"\n"
        "}\n"));
    assert(remove(manifest_path) == 0);
    verified.size = 1;
    assert(!fusion_rom_write_manifest(&verified, manifest_path,
                                      error, sizeof error));
    assert(strstr(error, "invalid verified ROM") != NULL);
    assert(fusion_asset_game_spec((FusionAssetGame)-1) == NULL);

    FusionRom rom = {0};
    assert(!fusion_rom_open(FUSION_ASSET_GAME_ARIA, "does-not-exist.gba", &rom,
                            error, sizeof error));
    assert(strstr(error, "cannot open") != NULL);
    FILE *bad = fopen("fusion-assets-short.gba", "wb");
    assert(bad);
    assert(fputc(0, bad) != EOF);
    assert(fclose(bad) == 0);
    assert(!fusion_rom_open(FUSION_ASSET_GAME_ARIA, "fusion-assets-short.gba", &rom,
                            error, sizeof error));
    assert(strstr(error, "size 1") != NULL);
    assert(remove("fusion-assets-short.gba") == 0);
    puts("Native asset ROM tests passed");
    return 0;
}
