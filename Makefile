CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread -lm
TARGET = audio_mixer
SOURCES = main.c audio_source.c mixer_engine.c volume_control.c
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) -lm

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET) *.bad volumes.txt output.bad

test: $(TARGET)
	./$(TARGET)

voltest: $(TARGET)
	chmod +x test_volumes.sh
	./test_volumes.sh

.PHONY: all clean test voltest
