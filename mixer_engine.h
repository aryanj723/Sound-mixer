#ifndef MIXER_ENGINE_H
#define MIXER_ENGINE_H

#include "audio_source.h"

#define MAX_SOURCES 3

typedef struct {
    AudioSource *sources[MAX_SOURCES];
    uint32_t num_sources;
    
    uint32_t output_sample_rate;
    uint16_t output_bits_per_sample;
    
    double master_clock;
    double start_time; // Earliest timestamp from all sources
    int active;
    pthread_t thread;
    
    char *output_filename;
} MixerEngine;

MixerEngine* mixer_create(uint32_t sample_rate, uint16_t bits, const char *output_file);
void mixer_add_source(MixerEngine *mixer, AudioSource *source);
void mixer_start(MixerEngine *mixer);
void mixer_stop(MixerEngine *mixer);
void mixer_destroy(MixerEngine *mixer);

#endif // MIXER_ENGINE_H
