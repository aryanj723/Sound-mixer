    #include "audio_source.h"
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

RingBuffer* ring_buffer_create(uint32_t capacity) {
    RingBuffer *rb = malloc(sizeof(RingBuffer));
    if (!rb) return NULL;
    
    rb->data = malloc(sizeof(int32_t) * capacity);
    rb->timestamps = malloc(sizeof(double) * capacity);
    if (!rb->data || !rb->timestamps) {
        free(rb->data);
        free(rb->timestamps);
        free(rb);
        return NULL;
    }
    
    rb->head = 0;
    rb->tail = 0;
    rb->size = capacity;
    pthread_mutex_init(&rb->lock, NULL);
    return rb;
}

void ring_buffer_destroy(RingBuffer *rb) {
    if (!rb) return;
    pthread_mutex_destroy(&rb->lock);
    free(rb->data);
    free(rb->timestamps);
    free(rb);
}

int ring_buffer_push_with_timestamp(RingBuffer *rb, int32_t sample, double timestamp) {
    pthread_mutex_lock(&rb->lock);
    uint32_t next = (rb->head + 1) % rb->size;
    if (next == rb->tail) {
        pthread_mutex_unlock(&rb->lock);
        return -1; // Full
    }
    rb->data[rb->head] = sample;
    rb->timestamps[rb->head] = timestamp;
    rb->head = next;
    pthread_mutex_unlock(&rb->lock);
    return 0;
}

int ring_buffer_peek_n(RingBuffer *rb, uint32_t n_offset, int32_t *sample, double *timestamp) {
    pthread_mutex_lock(&rb->lock);
    if (rb->head == rb->tail) {
        pthread_mutex_unlock(&rb->lock);
        return -1; // Empty
    }
    uint32_t idx = (rb->tail + n_offset) % rb->size;
    if (idx == rb->head) {
        pthread_mutex_unlock(&rb->lock);
        return -1; // Beyond available data
    }
    *sample = rb->data[idx];
    if (timestamp) *timestamp = rb->timestamps[idx];
    pthread_mutex_unlock(&rb->lock);
    return 0;
}

uint32_t ring_buffer_available(RingBuffer *rb) {
    pthread_mutex_lock(&rb->lock);
    uint32_t count = (rb->head >= rb->tail) ? (rb->head - rb->tail) : (rb->size - (rb->tail - rb->head));
    pthread_mutex_unlock(&rb->lock);
    return count;
}

AudioSource* audio_source_create(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;

    BadCodecHeader header;
    if (fread(&header, 1, sizeof(header), f) != sizeof(header)) {
        fclose(f);
        return NULL;
    }

    if (strncmp(header.magic, MAGIC_STRING, MAGIC_SIZE) != 0) {
        fclose(f);
        return NULL;
    }

    AudioSource *source = malloc(sizeof(AudioSource));
    if (!source) {
        fclose(f);
        return NULL;
    }
    
    source->file = f;
    source->header = header;
    source->buffer = *ring_buffer_create(BUFFER_CAPACITY);
    source->sample_period = 1.0 / header.sample_rate;
    source->last_written_timestamp = -1.0; // Unset
    source->volume = 1.0f;
    pthread_mutex_init(&source->volume_lock, NULL);
    source->active = 0; // Start as inactive
    source->thread_started = 0; // Thread not started yet
    source->thread = 0;

    return source;
}

void audio_source_start(AudioSource *source) {
    if (!source || source->thread_started) return;
    
    source->active = 1;
    source->thread_started = 1;
    pthread_create(&source->thread, NULL, audio_source_thread_func, source);
}

void audio_source_destroy(AudioSource *source) {
    if (!source) return;
    
    source->active = 0;
    if (source->thread_started) {
        pthread_join(source->thread, NULL);
    }
    fclose(source->file);
    // ring_buffer_destroy for the embedded struct
    free(source->buffer.data);
    free(source->buffer.timestamps);
    pthread_mutex_destroy(&source->buffer.lock);
    pthread_mutex_destroy(&source->volume_lock);
    free(source);
}

static int32_t read_sample(FILE *f, uint16_t bits) {
    if (bits == 8) {
        int8_t s;
        if (fread(&s, sizeof(s), 1, f) != 1) return 0;
        return (int32_t)s;
    } else if (bits == 16) {
        int16_t s;
        if (fread(&s, sizeof(s), 1, f) != 1) return 0;
        return (int32_t)s;
    } else if (bits == 32) {
        int32_t s;
        if (fread(&s, sizeof(s), 1, f) != 1) return 0;
        return s;
    }
    return 0;
}

void* audio_source_thread_func(void *arg) {
    AudioSource *source = (AudioSource*)arg;
    
    while (source->active) {
        BadCodecBlockHeader block_header;
        if (fread(&block_header, 1, sizeof(block_header), source->file) != sizeof(block_header)) {
            // EOF or error
            break;
        }

        double block_time = (double)block_header.seconds + (double)block_header.microseconds / 1000000.0;
        uint32_t bytes_to_read = block_header.block_size - sizeof(block_header);
        uint32_t bytes_per_sample = source->header.bits_per_sample / 8;
        uint32_t num_samples = bytes_to_read / bytes_per_sample;

        // In a real system, we'd check against a master clock here.
        // For now, we just push samples into the buffer.
        // If the buffer is full, we wait.
        
        for (uint32_t i = 0; i < num_samples; i++) {
            int32_t sample = read_sample(source->file, source->header.bits_per_sample);
            double sample_time = block_time + i * source->sample_period;
            
            while (ring_buffer_push_with_timestamp(&source->buffer, sample, sample_time) != 0 && source->active) {
                usleep(1000); // 1ms wait if buffer is full
            }
            
            // Update last written timestamp
            source->last_written_timestamp = sample_time;
        }
    }
    
    return NULL;
}

int ring_buffer_get_bracketing_samples(RingBuffer *rb, double target_time,
                                       int32_t *sample_before, double *time_before,
                                       int32_t *sample_after, double *time_after) {
    pthread_mutex_lock(&rb->lock);
    
    if (rb->head == rb->tail) {
        pthread_mutex_unlock(&rb->lock);
        return -1; // Empty
    }
    
    // Find the last sample with timestamp <= target_time
    uint32_t before_idx = rb->tail;
    uint32_t max_iter = 0;
    while (rb->timestamps[before_idx] <= target_time) {
        uint32_t next_idx = (before_idx + 1) % rb->size;
        if (next_idx == rb->head) {
            // We've reached the end of available data
            break;
        }
        if (rb->timestamps[next_idx] > target_time) {
            break;
        }
        before_idx = next_idx;
        max_iter++;
        if (max_iter > rb->size) {
            pthread_mutex_unlock(&rb->lock);
            return -1; // Prevent infinite loop
        }
    }
    
    // Check if the found sample is actually before or at target_time
    if (rb->timestamps[before_idx] > target_time) {
        // All samples are after target_time
        pthread_mutex_unlock(&rb->lock);
        return -1;
    }
    
    uint32_t after_idx = (before_idx + 1) % rb->size;
    if (after_idx == rb->head) {
        // No sample after the found one
        pthread_mutex_unlock(&rb->lock);
        return -1;
    }
    
    *sample_before = rb->data[before_idx];
    *time_before = rb->timestamps[before_idx];
    *sample_after = rb->data[after_idx];
    *time_after = rb->timestamps[after_idx];
    
    pthread_mutex_unlock(&rb->lock);
    return 0;
}

void ring_buffer_drop_before(RingBuffer *rb, double cutoff_time) {
    pthread_mutex_lock(&rb->lock);
    
    if (rb->head == rb->tail) {
        pthread_mutex_unlock(&rb->lock);
        return;
    }
    
    // Advance tail until we find a sample with timestamp >= cutoff_time
    while (rb->tail != rb->head && rb->timestamps[rb->tail] < cutoff_time) {
        rb->tail = (rb->tail + 1) % rb->size;
    }
    
    pthread_mutex_unlock(&rb->lock);
}

double audio_source_get_interpolated_sample(AudioSource *source, double target_time) {
    int32_t sample_before, sample_after;
    double time_before, time_after;
    
    // Drop old samples
    ring_buffer_drop_before(&source->buffer, target_time);
    
    if (ring_buffer_get_bracketing_samples(&source->buffer, target_time,
                                          &sample_before, &time_before,
                                          &sample_after, &time_after) != 0) {
        // Not enough data for interpolation
        return 0.0;
    }
    
    // Linear interpolation
    double fraction = (target_time - time_before) / (time_after - time_before);
    double interpolated = (double)sample_before + fraction * ((double)sample_after - (double)sample_before);
    
    // Apply volume
    float volume;
    pthread_mutex_lock(&source->volume_lock);
    volume = source->volume;
    pthread_mutex_unlock(&source->volume_lock);
    
    return interpolated * volume;
}

void audio_source_set_volume(AudioSource *source, float volume) {
    pthread_mutex_lock(&source->volume_lock);
    source->volume = volume;
    pthread_mutex_unlock(&source->volume_lock);
}
