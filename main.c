#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "badcodec.h"
#include "audio_source.h"
#include "mixer_engine.h"
#include "volume_control.h"

// Function to create a simple sine wave test file
void create_test_badcodec_file(const char *filename, uint32_t sample_rate, 
                               uint16_t bits_per_sample, double frequency, 
                               double duration, double start_time) {
    FILE *f = fopen(filename, "wb");
    if (!f) {
        printf("Error creating file: %s\n", filename);
        return;
    }

    // Write header
    BadCodecHeader header;
    memset(&header, 0, sizeof(header));
    memcpy(header.magic, MAGIC_STRING, MAGIC_SIZE);
    header.sample_rate = sample_rate;
    header.bits_per_sample = bits_per_sample;
    fwrite(&header, 1, sizeof(header), f);

    uint32_t num_samples = (uint32_t)(duration * sample_rate);
    uint32_t samples_per_block = 1024;
    uint32_t bytes_per_sample = bits_per_sample / 8;
    
    for (uint32_t block_start = 0; block_start < num_samples; block_start += samples_per_block) {
        uint32_t samples_in_block = (block_start + samples_per_block <= num_samples) ? 
                                   samples_per_block : num_samples - block_start;
        
        // Write block header
        BadCodecBlockHeader block_header;
        block_header.seconds = (uint32_t)start_time;
        block_header.microseconds = (uint32_t)((start_time - (uint32_t)start_time) * 1000000);
        block_header.block_size = sizeof(block_header) + (samples_in_block * bytes_per_sample);
        fwrite(&block_header, 1, sizeof(block_header), f);
        
        // Write samples
        for (uint32_t i = 0; i < samples_in_block; i++) {
            double t = (double)(block_start + i) / sample_rate;
            double sample_value = sin(2 * M_PI * frequency * t);
            
            // Scale based on bit depth
            int32_t sample;
            if (bits_per_sample == 8) {
                sample = (int8_t)(sample_value * 127);
                fwrite(&sample, 1, 1, f);
            } else if (bits_per_sample == 16) {
                sample = (int16_t)(sample_value * 32767);
                fwrite(&sample, 2, 1, f);
            } else if (bits_per_sample == 32) {
                sample = (int32_t)(sample_value * 2147483647);
                fwrite(&sample, 4, 1, f);
            }
        }
    }
    
    fclose(f);
    printf("Created test file: %s (%.1f Hz, %d bits, %.2f sec)\n", 
           filename, frequency, bits_per_sample, duration);
}

// Function to create volume control file
void create_volume_file() {
    FILE *f = fopen("volumes.txt", "w");
    if (!f) {
        printf("Error creating volumes.txt\n");
        return;
    }
    
    // Set initial volumes: source 0: 100%, source 1: 50%, source 2: 25%
    fprintf(f, "2024-01-01T00:00:00.000 0 100\n");
    fprintf(f, "2024-01-01T00:00:00.000 1 50\n");
    fprintf(f, "2024-01-01T00:00:00.000 2 25\n");
    
    fclose(f);
    printf("Created volume control file: volumes.txt\n");
}

// Function to verify output file
void verify_output_file(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        printf("Error opening output file: %s\n", filename);
        return;
    }
    
    // Read header
    BadCodecHeader header;
    if (fread(&header, 1, sizeof(header), f) != sizeof(header)) {
        printf("Error reading header from output file\n");
        fclose(f);
        return;
    }
    
    if (strncmp(header.magic, MAGIC_STRING, MAGIC_SIZE) != 0) {
        printf("Invalid magic in output file\n");
        fclose(f);
        return;
    }
    
    printf("Output file info:\n");
    printf("  Sample rate: %u Hz\n", header.sample_rate);
    printf("  Bits per sample: %u\n", header.bits_per_sample);
    
    // Count blocks and samples
    uint32_t block_count = 0;
    uint32_t sample_count = 0;
    
    while (!feof(f)) {
        BadCodecBlockHeader block_header;
        if (fread(&block_header, 1, sizeof(block_header), f) != sizeof(block_header)) {
            break;
        }
        
        block_count++;
        uint32_t bytes_to_read = block_header.block_size - sizeof(block_header);
        uint32_t bytes_per_sample = header.bits_per_sample / 8;
        uint32_t samples_in_block = bytes_to_read / bytes_per_sample;
        sample_count += samples_in_block;
        
        // Skip the samples
        fseek(f, bytes_to_read, SEEK_CUR);
    }
    
    printf("  Blocks: %u\n", block_count);
    printf("  Total samples: %u\n", sample_count);
    printf("  Duration: %.2f seconds\n", (double)sample_count / header.sample_rate);
    
    fclose(f);
}

int main() {
    printf("=== Audio Mixer Test Program ===\n\n");
    
    // Create test files
    printf("Creating test audio files...\n");
    create_test_badcodec_file("source0.bad", 44100, 16, 440.0, 5.0, 1600000000.0); // A4 note
    create_test_badcodec_file("source1.bad", 48000, 16, 880.0, 5.0, 1600000000.0); // A5 note
    create_test_badcodec_file("source2.bad", 16000, 16, 220.0, 5.0, 1600000000.0); // A3 note
    
    // Create volume control file
    create_volume_file();
    
    // Create and configure mixer
    printf("\nCreating mixer...\n");
    MixerEngine *mixer = mixer_create(44100, 16, "output.bad");
    
    // Create audio sources
    printf("Creating audio sources...\n");
    AudioSource *source0 = audio_source_create("source0.bad");
    AudioSource *source1 = audio_source_create("source1.bad");
    AudioSource *source2 = audio_source_create("source2.bad");
    
    if (!source0 || !source1 || !source2) {
        printf("Error creating audio sources\n");
        return 1;
    }
    
    // Add sources to mixer
    mixer_add_source(mixer, source0);
    mixer_add_source(mixer, source1);
    mixer_add_source(mixer, source2);
    
    // Set initial volumes
    audio_source_set_volume(source0, 1.0f);   // 100%
    audio_source_set_volume(source1, 0.5f);   // 50%
    audio_source_set_volume(source2, 0.25f);  // 25%
    
    // Start volume control thread
    pthread_t volume_thread;
    pthread_create(&volume_thread, NULL, volume_control_thread_func, mixer);
    
    // Start mixer
    printf("Starting mixer...\n");
    mixer_start(mixer);
    
    // Let it run for a few seconds
    printf("Mixing for 5 seconds...\n");
    sleep(5);
    
    // Stop mixer
    printf("Stopping mixer...\n");
    mixer_stop(mixer);
    
    // Stop volume control
    mixer->active = 0;
    pthread_join(volume_thread, NULL);
    
    // Cleanup
    printf("Cleaning up...\n");
    audio_source_destroy(source0);
    audio_source_destroy(source1);
    audio_source_destroy(source2);
    mixer_destroy(mixer);
    
    // Verify output
    printf("\nVerifying output...\n");
    verify_output_file("output.bad");
    
    printf("\n=== Test Complete ===\n");
    printf("Check output.bad for mixed audio.\n");
    printf("You can modify volumes.txt to change volume levels during playback.\n");
    
    return 0;
}
