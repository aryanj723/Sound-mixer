#ifndef AUDIO_SOURCE_H
#define AUDIO_SOURCE_H

#include <stdint.h>
#include <pthread.h>
#include <stdio.h>
#include "badcodec.h"

#define BUFFER_CAPACITY 65536 // Number of samples in the ring buffer

typedef struct {
    int32_t *data;
    double *timestamps; // Timestamp for each sample (seconds)
    uint32_t head; // Write index
    uint32_t tail; // Read index
    uint32_t size;
    pthread_mutex_t lock;
} RingBuffer;

typedef struct {
    FILE *file;
    BadCodecHeader header;
    RingBuffer buffer;
    
    // The sample period (seconds)
    double sample_period;
    // The timestamp of the most recent sample written to the buffer
    double last_written_timestamp;
    float volume; // 0.0 to 1.0
    pthread_mutex_t volume_lock;
    
    int active;
    int thread_started; // Flag to track if thread has been started
    pthread_t thread;
} AudioSource;

RingBuffer* ring_buffer_create(uint32_t capacity);
void ring_buffer_destroy(RingBuffer *rb);
int ring_buffer_push_with_timestamp(RingBuffer *rb, int32_t sample, double timestamp);
int ring_buffer_peek_n(RingBuffer *rb, uint32_t n_offset, int32_t *sample, double *timestamp);
uint32_t ring_buffer_available(RingBuffer *rb);
// Get the two samples that bracket the target time
// Returns 0 if successful, -1 if not enough data
int ring_buffer_get_bracketing_samples(RingBuffer *rb, double target_time, 
                                       int32_t *sample_before, double *time_before,
                                       int32_t *sample_after, double *time_after);
// Drop samples older than given time
void ring_buffer_drop_before(RingBuffer *rb, double cutoff_time);

AudioSource* audio_source_create(const char *filename);
void audio_source_destroy(AudioSource *source);
void audio_source_start(AudioSource *source); // New function to start thread
void* audio_source_thread_func(void *arg);

// Get interpolated sample at target_time
// Returns interpolated sample value (accounting for volume)
double audio_source_get_interpolated_sample(AudioSource *source, double target_time);

// Set volume (0.0 to 1.0)
void audio_source_set_volume(AudioSource *source, float volume);

#endif // AUDIO_SOURCE_H
