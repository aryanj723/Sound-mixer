#!/bin/bash

# Test script for audio mixer volume control
# This script demonstrates dynamic volume changes during playback

echo "=== Audio Mixer Volume Test ==="
echo ""

# Create initial volume file
echo "Creating initial volume settings..."
echo "0 100" > volumes.txt
echo "1 50" >> volumes.txt
echo "2 25" >> volumes.txt

echo "Initial volumes:"
echo "  Source 0: 100%"
echo "  Source 1: 50%"
echo "  Source 2: 25%"
echo ""

# Start the mixer in background
echo "Starting audio mixer..."
./audio_mixer &
MIXER_PID=$!

echo "Mixer started with PID: $MIXER_PID"
echo ""

# Wait a bit for mixer to start
sleep 2

# Test 1: Change volumes after 1 second
echo "Test 1: Changing volumes after 1 second..."
echo "0 75" > volumes.txt
echo "1 100" >> volumes.txt
echo "2 50" >> volumes.txt
echo "  New volumes: Source0=75%, Source1=100%, Source2=50%"
sleep 1

# Test 2: Change volumes again
echo "Test 2: Changing volumes again..."
echo "0 25" > volumes.txt
echo "1 75" >> volumes.txt
echo "2 100" >> volumes.txt
echo "  New volumes: Source0=25%, Source1=75%, Source2=100%"
sleep 1

# Test 3: Mute one source
echo "Test 3: Muting source 1..."
echo "0 100" > volumes.txt
echo "1 0" >> volumes.txt
echo "2 100" >> volumes.txt
echo "  New volumes: Source0=100%, Source1=0% (muted), Source2=100%"
sleep 1

# Wait for mixer to complete
echo ""
echo "Waiting for mixer to complete..."
wait $MIXER_PID

echo ""
echo "=== Test Complete ==="
echo "Check output.bad for mixed audio with volume changes."