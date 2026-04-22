#include "volume_control.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>

void* volume_control_thread_func(void *arg) {
    MixerEngine *mixer = (MixerEngine*)arg;
    const char *filename = "volumes.txt";
    
    // Simple mock: poll the file for changes
    // In a real system, we might use inotify
    while (mixer->active) {
        FILE *f = fopen(filename, "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                // Expected format: "2022-12-31T01:59:59.999 0 13"
                // Parse ISO 8601 timestamp (simplified)
                int year, month, day, hour, minute, second, millisecond;
                int source_idx, volume_percent;
                
                // Try to parse the full ISO 8601 format
                if (sscanf(line, "%d-%d-%dT%d:%d:%d.%d %d %d",
                          &year, &month, &day, &hour, &minute, 
                          &second, &millisecond, &source_idx, 
                          &volume_percent) == 9) {
                    if (source_idx >= 0 && source_idx < (int)mixer->num_sources) {
                        // Clamp volume to 0-100
                        if (volume_percent < 0) volume_percent = 0;
                        if (volume_percent > 100) volume_percent = 100;
                        
                        audio_source_set_volume(mixer->sources[source_idx], 
                                              (float)volume_percent / 100.0f);
                    }
                }
                // Also support simplified format without timestamp for testing
                else if (sscanf(line, "%d %d", &source_idx, &volume_percent) == 2) {
                    if (source_idx >= 0 && source_idx < (int)mixer->num_sources) {
                        // Clamp volume to 0-100
                        if (volume_percent < 0) volume_percent = 0;
                        if (volume_percent > 100) volume_percent = 100;
                        
                        audio_source_set_volume(mixer->sources[source_idx], 
                                              (float)volume_percent / 100.0f);
                    }
                }
            }
            fclose(f);
        }
        sleep(1); // Poll every second
    }
    return NULL;
}
