// WHO
// Made by MunchDuster, 2026

// WHAT
// Program to practice morse code
// Three stages
// Stage 1:
// 	Loop of listening to Letters and typing in letter heard
// 	Keep going until all letters correct 3 times consecutively
// Stage 2:
//	(Implement later) Full words, gets progressively more difficult
// Stage 3:
//	(Implement later) Full sentences, gets progresively more difficult

// ENVIRONMENT
// Linux desktop, Ubuntu based

// HOW
// Use linux audio program 'aplay' as subprocess and pipe in audio bytes
// Basically minimize imports and keep it simple
// TODO: Figure out custom audio playing without using aplay

// COMPILE
// clang -Wall -Wextra main.c -o morser -lm

// RUN
// ./morser

#include <stdio.h>		// perror(), printf()
#include <stdlib.h> 	// exit(), EXIT_FAILURE
#include <unistd.h> 	// fork()
#include <sys/wait.h>	// waitpid()
#include <math.h>		// sin()

#define SAMPLE_RATE 48000		// matches system default
#define FREQUENCY 440.0       	// Concert A4
#define DURATION 0.1        	// Play for 5 seconds
#define AMPLITUDE 32767.0     	// Max 16-bit signed integer
#define PI 3.141592653589793	// 15 dp is approximate max accuracy of double

int main(void) {
	// aplay is the audio software
	// '-q' is quiet, no regular logging
	// '-r 48000' is audio rate of 48000Hz (AKA 48000 samples/second)
	// '-c 1' is 1 channel
	// format of Signed 16-bit Little Endian
	const char* const subprocess = "aplay -q -r 48000 -c 1 -f S16_LE";

    // 1. Create the subprocess and open a write ("w") stream to its stdin
    // This executes the "sort" command in a child process
    FILE *subprocess_stdin = popen(subprocess, "w");
    
    // Check if the subprocess failed to start
    if (subprocess_stdin == NULL) {
        perror("Failed to run subprocess");
        return EXIT_FAILURE;
    }

	long num_samples = SAMPLE_RATE * DURATION;
    double two_pi = 2.0 * PI;
	double t = 0;
    for (long i = 0; i < num_samples; i++) {
        // Calculate the sine wave value between -1.0 and 1.0
        t += 1.0 / SAMPLE_RATE;
        double sample_value = sin(two_pi * FREQUENCY * t);

        // Scale to 16-bit signed integer PCM (-32768 to 32767)
        int16_t pcm_sample = (int16_t)(sample_value * AMPLITUDE);

        // Write the 16-bit sample (2 bytes) in little-endian format
        fputc(pcm_sample & 0xFF, subprocess_stdin);
        fputc((pcm_sample >> 8) & 0xFF, subprocess_stdin);
    }
    
    // Explicitly flush the stream to force data through the pipe immediately
    fflush(subprocess_stdin);

    // 3. Close the stream and wait for the child process to terminate
    int status = pclose(subprocess_stdin);
    
    if (status == -1) {
        perror("Error closing subprocess");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
