#include <stdio.h>		// perror(), printf()
#include <stdlib.h>		// exit(), EXIT_FAILURE
#include <unistd.h>		// fork()
#include <sys/wait.h>	// waitpid()
#include <math.h>		// sin()

#define SAMPLE_RATE 48000		// matches system default
#define FREQUENCY 440.0			// Concert A4
#define DIT_DURATION_SECS 0.2	// TODO: make settable as parameter for better practice of fast speeds
#define AMPLITUDE 32767.0		// Max 16-bit signed integer
#define PI 3.141592653589793	// 15 dp is approximate max accuracy of double

// morse code timings following international standard
#define DAH_DURATIONS_SECS (DIT_DURATION_SECS * 3)
#define BIT_INTERVAL_DURATION_SECS (DIT_DURATION_SECS)
// TODO: word letter interval, word interval

int playSound(double duration_seconds) {
	// aplay is the audio software
	// '-q' is quiet, no regular logging
	// '-r 48000' is audio rate of 48000Hz (AKA 48000 samples/second)
	// '-c 1' is 1 channel
	// '-f S16_LE' is format of Signed 16-bit Little Endian
	const char* const subprocess = "aplay -q -r 48000 -c 1 -f S16_LE";

	// create the subprocess and open a write ("w") stream to its stdin
	// this executes the command in a child process, keeping its input stream open as a file
	FILE *subprocess_stdin = popen(subprocess, "w");
	
	// check if the subprocess failed to start
	if (subprocess_stdin == NULL) {
		perror("Failed to run subprocess");
		return EXIT_FAILURE;
	}

	long num_samples = SAMPLE_RATE * duration_seconds;
	double two_pi = 2.0 * PI;
	double t = 0;
	for (long i = 0; i < num_samples; i++) {
		// calculate the sine wave value between -1.0 and 1.0
		t += 1.0 / SAMPLE_RATE;
		double sample_value = sin(two_pi * FREQUENCY * t);

		// scale to 16-bit signed integer PCM (-32768 to 32767)
		int16_t pcm_sample = (int16_t)(sample_value * AMPLITUDE);

		// write the 16-bit sample (2 bytes) in little-endian format
		fputc(pcm_sample & 0xFF, subprocess_stdin);
		fputc((pcm_sample >> 8) & 0xFF, subprocess_stdin);
	}
	
	// explicitly flush the stream to force data through the pipe immediately
	fflush(subprocess_stdin);

	// close the stream and wait for the child process to terminate
	int status = pclose(subprocess_stdin);
	
	if (status == -1) {
		perror("Error closing subprocess");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
void sleep_double(double duration_seconds) {
	int duration_microseconds = (int)(duration_seconds*100000);
	usleep(duration_microseconds);
}
int main(void) {
	// letter 'F'
	playSound(DIT_DURATION_SECS);
	sleep_double(BIT_INTERVAL_DURATION_SECS);
	playSound(DIT_DURATION_SECS);
	sleep_double(BIT_INTERVAL_DURATION_SECS);
	playSound(DAH_DURATIONS_SECS);
	sleep_double(BIT_INTERVAL_DURATION_SECS);
	playSound(DIT_DURATION_SECS);
	sleep_double(BIT_INTERVAL_DURATION_SECS);
	return EXIT_SUCCESS;
}
