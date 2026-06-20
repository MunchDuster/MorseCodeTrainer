#include <stdio.h>		// perror(), printf()
#include <stdlib.h>		// exit(), EXIT_FAILURE
#include <unistd.h>		// fork()
#include <sys/wait.h>	// waitpid()
#include <math.h>		// sin()
#include <stdint.h>		// uint8_t, uint16_t, etc
#include <stdbool.h>	// bool
#include <string.h>		// memset()

// audio consts
#define SAMPLE_RATE 48000		// matches system default
#define FREQUENCY 880.0			// Concert A5
#define AMPLITUDE 32767.0		// Max 16-bit signed integer
#define PI 3.141592653589793	// 15 dp is approximate max accuracy of double

// morse code timings following international standard
#define WORDS_PER_SECOND 15.0		// TODO: make settable as parameter for better practice of fast speeds
#define DIT_DURATION_SECS (1.2 / WORDS_PER_SECOND)
#define DAH_DURATIONS_SECS (DIT_DURATION_SECS * 3.0)
#define BIT_INTERVAL_DURATION_SECS (DIT_DURATION_SECS)
#define GAIN_TIME_SECS 0.01		// prevents audio popping
// TODO: word letter interval, word interval

// learning consts
#define CONSECUTIVE_CORRECT_THRESHOLD 3 // TODO: make settable as parameter for user preference

// input index is the character and output string of dits and dahs
// not the most memory efficient but easy to encode correctly and fairly simple to decode
char* letters_morse[256] = {
	['a'] = ".-",
	['b'] = "-...",
	['c'] = "-.-.",
	['d'] = "-..",
	['e'] = ".",
	['f'] = "..-.",
	['g'] = "--.",
	['h'] = "....",
	['i'] = "..",
	['j'] = ".---",
	['k'] = "-.-",
	['l'] = ".-..",
	['m'] = "--",
	['n'] = "-.",
	['o'] = "---",
	['p'] = ".--.",
	['q'] = "--.-",
	['r'] = ".-.",
	['s'] = "...",
	['t'] = "-",
	['u'] = "..-",
	['v'] = "...-",
	['w'] = ".--",
	['x'] = "-..-",
	['y'] = "-.--",
	['z'] = "--..",
	['0'] = "-----",
	['1'] = ".----",
	['2'] = "..---",
	['3'] = "...--",
	['4'] = "....-",
	['5'] = ".....",
	['6'] = "-....",
	['7'] = "--...",
	['8'] = "---..",
	['9'] = "----.",
};
// 'input stream' to audio handler
FILE *subprocess_stdin = NULL;

int startAudioSubprocess(void) {
	// aplay is the audio software
	// '-q' is quiet, no regular logging
	// '-r 48000' is audio rate of 48000Hz (AKA 48000 samples/second)
	// '-c 1' is 1 channel
	// '-f S16_LE' is format of Signed 16-bit Little Endian
	const char* const subprocess = "aplay -q -r 48000 -c 1 -f S16_LE";

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
int closeAudioSubprocess(void) {
	// close the stream and wait for the child process to terminate
	int status = pclose(subprocess_stdin);
	
	if (status == -1) {
		perror("Error closing subprocess");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
void queueAudio(double duration_seconds, double volume) {
	long num_samples = SAMPLE_RATE * duration_seconds;
	double two_pi = 2.0 * PI;

	for (long i = 0; i < num_samples; i++) {
		// calculate the sine wave value between -1.0 and 1.0
		double t = (double)i / SAMPLE_RATE;
		double amplitude = volume;

		// gradually increase amplitude at start
		if (t < GAIN_TIME_SECS) {
			amplitude *= (double)t / GAIN_TIME_SECS;
		}
		// gradually decrease amplitude at end
		else if ((duration_seconds - t) < GAIN_TIME_SECS) {
			amplitude *= (double)(duration_seconds - t) / GAIN_TIME_SECS;
		}
		double sample_value = amplitude * sin(two_pi * FREQUENCY * t);

		// scale to 16-bit signed integer PCM (-32768 to 32767)
		int16_t pcm_sample = (int16_t)(sample_value * AMPLITUDE);

		// write the 16-bit sample (2 bytes) in little-endian format
		uint8_t right_byte = pcm_sample & 0xFF;
		uint8_t left_byte = (pcm_sample >> 8) & 0xFF;
		fputc(right_byte, subprocess_stdin);
		fputc(left_byte, subprocess_stdin);
	}
}
void queueSound(double duration_seconds) {
	queueAudio(duration_seconds, 1);
}
void queueQuiet(double duration_seconds) {
	queueAudio(duration_seconds, 0);
}
// must queue audio first to be played
void playSound(void) {
	// explicitly flush the stream to force data through the pipe immediately
	fflush(subprocess_stdin);
}
void sleep_double(double duration_seconds) {
	int duration_microseconds = (int)(duration_seconds*1000000);
	usleep(duration_microseconds);
}
int send_message(const char* const message) {
	int success;
	success = startAudioSubprocess();
	if (success != EXIT_SUCCESS) {
		return success;
	}
	queueQuiet(DAH_DURATIONS_SECS); // start with quiet to let audio program start and get ready
	for (int i = 0; message[i] != '\0'; i++) {
		// TODO: handle spaces for multiple words to have correct timing
		const char letter = message[i];
		const char* morse = letters_morse[(int)letter];
		if (morse == NULL) {
			perror("morse for letter '");
			perror(&letter);
			perror("' has no associated morse\n");
			return EXIT_FAILURE;
		}

		for (int m = 0; morse[m] != '\0'; m++) {
			const char bit = morse[m];
			if (bit == '.') {
				queueSound(DIT_DURATION_SECS);
			}
			else if (bit == '-') {
				queueSound(DAH_DURATIONS_SECS);
			}
			else {
				perror("Unrecognised morse bit '");
				perror(&bit);
				perror("'\n");
				return EXIT_FAILURE;
			}
			queueQuiet(BIT_INTERVAL_DURATION_SECS);
		}
		// finish waiting for letter end
		queueQuiet(BIT_INTERVAL_DURATION_SECS);
		queueQuiet(BIT_INTERVAL_DURATION_SECS);
	}
	playSound();
	success = closeAudioSubprocess();
	if (success != EXIT_SUCCESS) {
		return success;
	}
	return EXIT_SUCCESS;
}
void press_to_continue(void) {
	printf("[press enter to continue]");
    getchar(); 
}
void print_counts(const int length, const char*const*const texts, const uint8_t*const counts) {
	printf("Current scores\n");
	for (int i = 0; i < length; i++) {
		printf("%s\t%d\n", texts[i], counts[i]);
	}
}
// random number from 0 to maxExclusive - 1
int random_range(int maxExclusive) {
	return (rand() % maxExclusive);
}
void clear_console(void) {
	printf("\e[2J\e[H\n"); // clears and resets cursor position
}

int learnText(const char** texts, const int texts_length) {
	uint8_t consecutive_correct_counts[texts_length];
	memset(consecutive_correct_counts, 0, texts_length * sizeof(uint8_t));

	int texts_still_learning = texts_length;
	int index = 0;
	while (texts_still_learning > 0) {
		index = random_range(texts_still_learning);
		const char* message = texts[index];
		playSound:
		clear_console();
		print_counts(texts_still_learning, texts, consecutive_correct_counts);
		printf("Listen...\n");
		int success;
		success = send_message(message);
		if (success != EXIT_SUCCESS) {
			return success;
		}
		
		// check that user interprets message correctly
		printf("What was said? (empty to play again) ");

		char interpretation[1000];
		while (fgets(interpretation, sizeof(interpretation), stdin) == NULL) {
			perror("input too long, ignoring");
		}
		if (interpretation[0]=='\n') {
			goto playSound;
		}
		
		// compare interpretation to actual
		bool wasCorrect = true;
		for (int c = 0; message[c] != '\0' && interpretation[c] != '\0'; c++) {
			if (message[c] != interpretation[c]) {
				wasCorrect = false;
				break;
			}
		}
		if (!wasCorrect) {
			printf("INCORRECT\n");
			printf("You guessed:\t %s", interpretation);
			printf("Correct was:\t %s\n", message);
			consecutive_correct_counts[index] = 0;
		}
		else {
			printf("CORRECT\n");
			consecutive_correct_counts[index]++;
			bool completedText = consecutive_correct_counts[index] >= CONSECUTIVE_CORRECT_THRESHOLD;
			if (completedText) {
				if (texts_still_learning > 1) {
					// swap out text with last to ensure all 0 to N-1 are valid
					const int lastIndex = texts_still_learning - 1;
					const char*const temp_text = texts[index];
					texts[index] = texts[lastIndex];
					texts[lastIndex] = temp_text;
					// and swap counts to keep alignment
					const int temp_count = consecutive_correct_counts[index];
					consecutive_correct_counts[index] = consecutive_correct_counts[lastIndex];
					consecutive_correct_counts[lastIndex] = temp_count;
				}
				texts_still_learning--;
			}
		}
		press_to_continue();
	}
	clear_console();
	return EXIT_SUCCESS;
}
bool press_to_continue_or_skip(void) {
	printf("[press enter to continue or space to skip]");
	if (getchar() == ' ') {
		getchar(); // read enter
		return true;
	}
	return false;
}
int playStage1(void) {
	bool skip;

	// stage 1: part 1: simple letters
	printf("Entering stage 1: Part 1: Easy letters\n");
	skip = press_to_continue_or_skip();
	if (!skip) {
		const int simple_letters_count = 8;
		const char* simple_letters[] = {"e","t","i","a","n","m","o","s"};
		int success = learnText(simple_letters, simple_letters_count);
		if (EXIT_SUCCESS != success) {
			return success;
		}
		printf("completed stage1 part1!\n");
	}

	// stage 1: part 2: more letters
	printf("Entering stage 1: Part 2: More letters\n");
	skip = press_to_continue_or_skip();
	if (!skip) {
		const int simple_letters_count = 16;
		const char* letters[] = {"e","t","i","a","n","m","o","s", "g","d","k","r","u","w","c","p"};
		int success = learnText(letters, simple_letters_count);
		if (EXIT_SUCCESS != success) {
			return success;
		}
		printf("completed stage1 part2!\n");
	}

	printf("completed stage1!\n");
	return EXIT_SUCCESS;
}
int main(void) {
	int success = playStage1();
	return success;
}
