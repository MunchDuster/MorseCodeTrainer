#include <math.h>		// sin()
#include <stddef.h>		// NULL
#include <stdio.h>		// FILE, perror(), printf()
#include <stdint.h>		// uint8_t, uint16_t, etc
#include <stdlib.h>		// EXT_FAILURE, etc

// audio consts - settings
#define NORMAL_VOLUME 0.5			// TODO: make settable as parameter for user preference
#define GAIN_TIME_SECS 0.005		// prevents audio popping

// consts - shorthands
#define SAMPLE_RATE 48000		// matches system default
#define FREQUENCY 880.0			// Concert A5
#define AMPLITUDE 32767.0		// Max 16-bit signed integer
#define PI 3.141592653589793	// 15 dp is approximate max accuracy of double

// 'input stream' to audio handler
FILE *subprocess_stdin = NULL;

// private
void queueAudio(const double duration_seconds, const double volume) {
	const long num_samples = SAMPLE_RATE * duration_seconds;
	const double two_pi = 2.0 * PI;

	for (long i = 0; i < num_samples; i++) {
		// calculate the sine wave value between -1.0 and 1.0
		const double t = (double)i / SAMPLE_RATE;
		double amplitude = volume;

		// gradually increase amplitude at start
		if (t < GAIN_TIME_SECS) {
			amplitude *= (double)t / GAIN_TIME_SECS;
		}
		// gradually decrease amplitude at end
		else if ((duration_seconds - t) < GAIN_TIME_SECS) {
			amplitude *= (double)(duration_seconds - t) / GAIN_TIME_SECS;
		}
		const double sample_value = amplitude * sin(two_pi * FREQUENCY * t);

		// scale to 16-bit signed integer PCM (-32768 to 32767)
		const int16_t pcm_sample = (int16_t)(sample_value * AMPLITUDE);

		// write the 16-bit sample (2 bytes) in little-endian format
		const uint8_t right_byte = pcm_sample & 0xFF;
		const uint8_t left_byte = (pcm_sample >> 8) & 0xFF;
		fputc(right_byte, subprocess_stdin);
		fputc(left_byte, subprocess_stdin);
	}
}
// public
void queueSound(const double duration_seconds) {
	queueAudio(duration_seconds, NORMAL_VOLUME);
}
// public
void queueQuiet(const double duration_seconds) {
	queueAudio(duration_seconds, 0);
}
// public
// must queue audio first to be played
void playSound(void) {
	// explicitly flush the stream to force data through the pipe immediately
	fflush(subprocess_stdin);
}
// public
int startAudioSubprocess(void) {
	// aplay is the audio software
	// '-q' is quiet, no regular logging
	// '-r 48000' is audio rate of 48000Hz (AKA 48000 samples/second)
	// '-c 1' is 1 channel
	// '-f S16_LE' is format of Signed 16-bit Little Endian
	// TODO: Figure out custom audio playing without using aplay, maybe interface directly with ALSA?
	const char*const subprocess = "aplay -q -r 48000 -c 1 -f S16_LE";

	// create the subprocess and open a write ("w") stream to its stdin
	// this executes the command in a child process, keeping its input stream open as a file
	subprocess_stdin = popen(subprocess, "w");
	
	// check if the subprocess failed to start
	if (subprocess_stdin == NULL) {
		perror("Failed to run subprocess");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
// public
int closeAudioSubprocess(void) {
	// close the stream and wait for the child process to terminate
	const int status = pclose(subprocess_stdin);
	
	if (status == -1) {
		perror("Error closing subprocess");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}