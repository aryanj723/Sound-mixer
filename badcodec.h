#ifndef BADCODEC_H
#define BADCODEC_H

#include <stdint.h>

#define MAGIC_STRING "bad-codec"
#define MAGIC_SIZE 10

typedef struct {
    char magic[MAGIC_SIZE]; // "bad-codec\0"
    uint32_t sample_rate;
    uint16_t bits_per_sample;
} BadCodecHeader;

typedef struct {
    uint32_t seconds;
    uint32_t microseconds;
    uint32_t block_size; // Includes this 12-byte header
} BadCodecBlockHeader;

#endif // BADCODEC_H
