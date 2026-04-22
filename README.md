# Audio Mixer Project

A simplified audio mixer implementation that mixes multiple audio sources with independent volume control, sample rate conversion, and real-time playback simulation.

## Features

- **Multiple Audio Sources**: Supports up to 3 audio sources
- **Volume Control**: Independent volume control for each source (0-100% in 1% increments)
- **Sample Rate Conversion**: Linear interpolation for mixing sources with different sample rates
- **Supported Formats**:
  - Sample rates: 8kHz, 16kHz, 44.1kHz, 48kHz, 96kHz
  - Bit depths: 8, 16, 32 bits per sample
- **Real-time Processing**: Threaded architecture for responsive mixing
- **BadCodec Format**: Reads and writes audio in the custom BadCodec format

## Architecture

The mixer is built with a modular architecture:

```
main.c - Test program and file generation
├── audio_source.[ch] - Individual audio source management
├── mixer_engine.[ch] - Main mixing engine
├── volume_control.[ch] - Volume adjustment handling
└── badcodec.h - BadCodec file format definitions
```

### Key Components:

1. **AudioSource**: Manages individual audio streams with ring buffers
2. **MixerEngine**: Combines multiple sources with interpolation
3. **VolumeControl**: Handles dynamic volume adjustments
4. **RingBuffer**: Thread-safe buffer for real-time audio data

## Building

```bash
make clean && make
```

This will create the `audio_mixer` executable.

## Running

```bash
./audio_mixer
```

The program will:
1. Create test audio files (sine waves at different frequencies)
2. Create a volume control file (`volumes.txt`)
3. Mix the audio sources for 5 seconds
4. Write the output to `output.bad`

## Volume Control

Volume adjustments can be made during runtime by editing `volumes.txt`:

### Format 1: Full ISO 8601 timestamp
```
2024-01-01T12:00:00.000 0 75   # Set source 0 to 75% volume
2024-01-01T12:00:01.000 1 50   # Set source 1 to 50% volume
2024-01-01T12:00:02.000 2 25   # Set source 2 to 25% volume
```

### Format 2: Simplified (source volume)
```
0 100   # Set source 0 to 100% volume
1 50    # Set source 1 to 50% volume
2 25    # Set source 2 to 25% volume
```

## BadCodec File Format

The BadCodec format is a simple binary format:

### Header (16 bytes):
- Magic string: "bad-codec\0" (10 bytes)
- Sample rate (uint32_t, little-endian)
- Bits per sample (uint16_t, little-endian)

### Block Structure:
- Timestamp (seconds + microseconds, uint32_t each)
- Block size in bytes (uint32_t, includes header)
- Audio samples (variable length based on bit depth)

## Testing

The test program creates three sine wave sources:
- `source0.bad`: 440 Hz (A4) at 44.1kHz, 16-bit
- `source1.bad`: 880 Hz (A5) at 48kHz, 16-bit  
- `source2.bad`: 220 Hz (A3) at 16kHz, 16-bit

These are mixed with initial volumes: 100%, 50%, 25% respectively.

### Automated Tests

```bash
# Basic test
make test

# Volume change test
make voltest
```

## Git Repository

The repository includes:
- `.gitignore` - Excludes build artifacts and generated files
- `.gitattributes` - Ensures proper line ending handling

To clone and build:
```bash
git clone <repository-url>
cd <repository-directory>
make
```

## Implementation Details

### Sample Rate Conversion
Uses linear interpolation to align samples from different sources:
```c
interpolated = sample_before + fraction * (sample_after - sample_before)
```

### Volume-weighted Sum
```c
output = sum(source_sample[i] * volume[i])
```

### Threading Model
- Each audio source has its own reader thread
- Mixer runs in a separate output thread
- Volume control runs in a monitoring thread
- All threads synchronize through mutex-protected ring buffers

## Requirements Met

- [x] Support 3 audio feeds
- [x] Volume-weighted sum output
- [x] Volume range 0-100% in 1% increments
- [x] Independent volume control
- [x] Support multiple sample rates (8k, 16k, 44.1k, 48k, 96kHz)
- [x] Support multiple bit depths (8, 16, 32 bits)
- [x] Linear interpolation for sample rate conversion
- [x] Continuous output without perceptible delay
- [x] Threaded architecture
- [x] Modular design with swappable components

## Limitations & Future Improvements

1. **Current limitations**:
   - Fixed to 3 audio sources (configurable via `MAX_SOURCES`)
   - Simple linear interpolation (could use more sophisticated resampling)
   - Basic error handling

2. **Potential improvements**:
   - Add more audio filters (EQ, compression, etc.)
   - Support for network streaming
   - GUI for real-time control
   - Better real-time scheduling
   - Support for more audio formats

## License

This project is for demonstration purposes as part of a coding assessment.