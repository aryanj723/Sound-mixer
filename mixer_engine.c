#include "mixer_engine.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

MixerEngine* mixer_create(uint32_t sample_rate, uint16_t bits, const char *output_file) {
    MixerEngine *mixer = malloc(sizeof(MixerEngine));
    if (!mixer) return NULL;
    
    mixer->num_sources = 0;
    mixer->output_sample_rate = sample_rate;
    mixer->output_bits_per_sample = bits;
    mixer->master_clock = 0;
    mixer->start_time = 0;
    mixer->active = 0;
    mixer->output_filename = strdup(output_file);
    if (!mixer->output_filename) {
        free(mixer);
        return NULL;
    }
    return mixer;
}

void mixer_add_source(MixerEngine *mixer, AudioSource *source) {
    if (mixer->num_sources < MAX_SOURCES) {
        mixer->sources[mixer->num_sources++] = source;
    }
}

static void write_badcodec_header(FILE *f, uint32_t rate, uint16_t bits) {
    BadCodecHeader h;
    memset(&h, 0, sizeof(h));
    memcpy(h.magic, MAGIC_STRING, MAGIC_SIZE);
    h.sample_rate = rate;
    h.bits_per_sample = bits;
    fwrite(&h, 1, sizeof(h), f);
}

static void write_sample(FILE *f, int32_t sample, uint16_t bits) {
    if (bits == 8) {
        int8_t s = (int8_t)sample;
        fwrite(&s, 1, 1, f);
    } else if (bits == 16) {
        int16_t s = (int16_t)sample;
        fwrite(&s, 2, 1, f);
    } else if (bits == 32) {
        fwrite(&sample, 4, 1, f);
    }
}

// Get the output value from a source at target time
static double get_source_sample_at_time(AudioSource *source, double target_time) {
    return audio_source_get_interpolated_sample(source, target_time);
}

void* mixer_thread_func(void *arg) {
    MixerEngine *mixer = (MixerEngine*)arg;
    FILE *out = fopen(mixer->output_filename, "wb");
    if (!out) return NULL;

    write_badcodec_header(out, mixer->output_sample_rate, mixer->output_bits_per_sample);

    double dt = 1.0 / mixer->output_sample_rate;
    
    // To keep it simple for the first run, we'll write blocks of 1024 samples
    const uint32_t samples_per_block = 1024;

    // Get start time for relative timing
    struct timeval start_time;
    gettimeofday(&start_time, NULL);
    
    while (mixer->active) {
        // Get current absolute time for timestamp
        struct timeval current_time;
        gettimeofday(&current_time, NULL);
        
        // Write Block Header with absolute timestamp
        BadCodecBlockHeader bh;
        bh.seconds = current_time.tv_sec;
        bh.microseconds = current_time.tv_usec;
        bh.block_size = sizeof(bh) + (samples_per_block * (mixer->output_bits_per_sample / 8));
        fwrite(&bh, 1, sizeof(bh), out);

        for (uint32_t i = 0; i < samples_per_block; i++) {
            double mixed_sample = 0;
            for (uint32_t s = 0; s < mixer->num_sources; s++) {
                mixed_sample += get_source_sample_at_time(mixer->sources[s], mixer->master_clock);
            }
            
            // Clamp based on output bit depth
            int32_t output_sample;
            if (mixer->output_bits_per_sample == 8) {
                if (mixed_sample > 127) mixed_sample = 127;
                if (mixed_sample < -128) mixed_sample = -128;
                output_sample = (int32_t)round(mixed_sample);
            } else if (mixer->output_bits_per_sample == 16) {
                if (mixed_sample > 32767) mixed_sample = 32767;
                if (mixed_sample < -32768) mixed_sample = -32768;
                output_sample = (int32_t)round(mixed_sample);
            } else { // 32 bits
                if (mixed_sample > 2147483647.0) mixed_sample = 2147483647.0;
                if (mixed_sample < -2147483648.0) mixed_sample = -2147483648.0;
                output_sample = (int32_t)round(mixed_sample);
            }

            write_sample(out, output_sample, mixer->output_bits_per_sample);
            mixer->master_clock += dt;
        }
        
        // Simulate real-time by sleeping (crude, but for this milestone...)
        usleep((samples_per_block * 1000000) / mixer->output_sample_rate);
    }

    fclose(out);
    return NULL;
}

void mixer_start(MixerEngine *mixer) {
    mixer->active = 1;
    for (uint32_t i = 0; i < mixer->num_sources; i++) {
        audio_source_start(mixer->sources[i]);
    }
    pthread_create(&mixer->thread, NULL, mixer_thread_func, mixer);
}

void mixer_stop(MixerEngine *mixer) {
    mixer->active = 0;
    pthread_join(mixer->thread, NULL);
}

void mixer_destroy(MixerEngine *mixer) {
    free(mixer->output_filename);
    free(mixer);
}
