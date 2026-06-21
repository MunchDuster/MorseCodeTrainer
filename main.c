#include <stdio.h>		// perror(), printf()
#include <stdlib.h>		// exit(), EXIT_FAILURE
#include <unistd.h>		// fork()
#include <sys/wait.h>	// waitpid()
#include <math.h>		// sin()
#include <stdint.h>		// uint8_t, uint16_t, etc
#include <stdbool.h>	// bool
#include <string.h>		// memset()
#include <time.h>   	// time()

// audio consts
#define SAMPLE_RATE 48000		// matches system default
#define FREQUENCY 880.0			// Concert A5
#define AMPLITUDE 32767.0		// Max 16-bit signed integer
#define PI 3.141592653589793	// 15 dp is approximate max accuracy of double

// morse code timings following international standard
#define WORDS_PER_SECOND 20.0		// TODO: make settable as parameter for better practice of fast speeds
#define DIT_DURATION_SECS (1.2 / WORDS_PER_SECOND)
#define DAH_DURATIONS_SECS (DIT_DURATION_SECS * 3.0)
#define BIT_INTERVAL_DURATION_SECS (DIT_DURATION_SECS)
#define NORMAL_VOLUME 0.5			// TODO: make settable as parameter for user preference
#define GAIN_TIME_SECS 0.005		// prevents audio popping
#define COUNTS_DISPLAY_ROWS 10		// prevent list too long, TODO: make param
#define MAX_WORD_LENGTH 256
// TODO: word letter interval, word interval

// learning consts
#define CONSECUTIVE_CORRECT_THRESHOLD 5 // TODO: make settable as parameter for user preference

// input index is the character and output string of dits and dahs
// not the most memory efficient but easy to encode correctly and fairly simple to decode
const char*const letters_morse[256] = {
	['a']  = ".-",
	['b']  = "-...",
	['c']  = "-.-.",
	['d']  = "-..",
	['e']  = ".",
	['f']  = "..-.",
	['g']  = "--.",
	['h']  = "....",
	['i']  = "..",
	['j']  = ".---",
	['k']  = "-.-",
	['l']  = ".-..",
	['m']  = "--",
	['n']  = "-.",
	['o']  = "---",
	['p']  = ".--.",
	['q']  = "--.-",
	['r']  = ".-.",
	['s']  = "...",
	['t']  = "-",
	['u']  = "..-",
	['v']  = "...-",
	['w']  = ".--",
	['x']  = "-..-",
	['y']  = "-.--",
	['z']  = "--..",

	['0']  = "-----",
	['1']  = ".----",
	['2']  = "..---",
	['3']  = "...--",
	['4']  = "....-",
	['5']  = ".....",
	['6']  = "-....",
	['7']  = "--...",
	['8']  = "---..",
	['9']  = "----.",

	['&']  = ".-...",
	['\''] = ".----.",
	['@']  = ".--.-.",
	[')']  = "-.--.-",
	['(']  = "-.--.",
	[':']  = "---...",
	[',']  = "--..--",
	['=']  = "-...-",
	['!']  = "-.-.--",
	['.']  = ".-.-.-",
	['-']  = "-....-",
	['%']  = "-----",
	['+']  = ".-.-.",
	['"']  = ".-..-.",
	['?']  = "..--..",
	['/']  = "-..-.",
};
struct lesson_contents {
	int text_count;
	const char** texts;
};
struct lesson {
	char* description;
	char* source_file_name;
};
struct stage {
	int lesson_count;
	const char* const name;
	struct lesson* lessons;
};
struct lesson_contents* load_words_file(const char*const filename) {
	if (filename == NULL) {
		fprintf(stderr, "NO FILENAME\n");
		return NULL;
	}
	if (access(filename, R_OK) != EXIT_SUCCESS) {
		fprintf(stderr, "File not accessible: %s\n", filename);
		return NULL;
	}
	FILE* file_read_stream = fopen(filename, "r");
	if (file_read_stream == NULL) {
        fprintf(stderr, "Failed to open file '%s'\n", filename);
        return NULL;
    }

    char ch;
	char word[MAX_WORD_LENGTH];

	// initially only keep one word
	char** words = calloc(1, sizeof(char*));
	int words_capacity = 1;
	
	int letter_index = 0;
	int words_index = 0;
    while ((ch = fgetc(file_read_stream)) != EOF) {
		if (ch != '\n') {
			word[letter_index] = ch;
			letter_index++;
			if (letter_index > MAX_WORD_LENGTH) {
				perror("word too long!");
			    fclose(file_read_stream);
				return NULL;
			}
			continue;
		}
		// word finished
		word[letter_index] = '\0';
		// letter_index++;
		words[words_index] = calloc(letter_index, sizeof(char));
		letter_index = 0;

		if (words_index >= words_capacity) {
			words_capacity *= 2;
			// make new larger array and copy over
			words = realloc(words, sizeof(char*)*words_capacity);
			if (words == NULL) {
				perror("failed copying words into larger array");
    			fclose(file_read_stream);
				return NULL;
			}
		}
		strcpy(words[words_index], word);
		words_index++;
    }

    fclose(file_read_stream);

	struct lesson_contents* result = calloc(1, sizeof(struct lesson_contents));
	result->text_count = words_index;
	result->texts = (const char**)words; // letters within words wont change from here on out
    return result;
}
static const struct lesson stage1_lessons[] = {
    {"simple letters", "data/letters_easy.txt"},
    {"more letters", "data/letters_mid.txt"},
    {"hard letters", "data/letters_hard.txt"},
    {"all letters", "data/letters_alphabet.txt"},
    {"numbers", "data/letters_numbers.txt"}
};
static const struct lesson stage2_lessons[] = {
    {"small words - 2 letters", "data/words_2-letters.txt"},
	{"small words - 3 letters", "data/words_3-letters.txt"},
	{"small words - 4 letters", "data/words_4-letters.txt"},
	{"words - 5 letters", "data/words_5-letters.txt"},
	{"long words", "data/words_long.txt"}
};
const struct stage stages[] = {
	{5, "Letters", (struct lesson*)stage1_lessons},
	{5, "Words", (struct lesson*)stage2_lessons}
	// TODO: stage 3: Sentences
};

// 'input stream' to audio handler
FILE *subprocess_stdin = NULL;

int min(int a, int b) {
	if (a > b) return b;
	return a;
}

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
	queueAudio(duration_seconds, NORMAL_VOLUME);
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
	for (int i = 0; i < min(length, COUNTS_DISPLAY_ROWS); i++) {
		for (int c = i; c < length; c += COUNTS_DISPLAY_ROWS) {
			printf("%s\t%d\t", texts[c], counts[c]);
		}
		printf("\n");
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
		printf("Listen... ");
		fflush(stdout);
		int success;
		success = send_message(message);
		if (success != EXIT_SUCCESS) {
			return success;
		}
		
		// check that user interprets message correctly
		printf("\rWhat was said? (empty to play again) ");

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
			send_message(message);

			// look for other in texts
			int other_index = -1;
			for (int i = 0; i < texts_still_learning; i++) {
				if (i == index) continue;
				bool match = interpretation[0] == texts[i][0];
				for (int c = 0; interpretation[c] != '\0' && texts[i][c] != '\0' && interpretation[c] != '\n'; c++) {
					if (interpretation[c] != texts[i][c]) {
						match = false;
						break;
					}
				}
				if (match) {
					other_index = i;
					break;
				}
			}

			consecutive_correct_counts[index] = (uint8_t)0;
			// if confused for another, reset the other as well
			if (other_index != -1) {
				consecutive_correct_counts[other_index] = (uint8_t)0;
			}
			press_to_continue();
		}
		else {
			consecutive_correct_counts[index]++;
			bool completedText = consecutive_correct_counts[index] >= CONSECUTIVE_CORRECT_THRESHOLD;
			if (completedText) {
				if (texts_still_learning > 1) {
					// swap out text with last to ensure all 0 to N-1 are valid
					const int lastIndex = texts_still_learning - 1;
					const char* temp_text = texts[index];
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
	}
	clear_console();
	return EXIT_SUCCESS;
}
bool press_to_continue_or_skip(void) {
	printf("[press enter to continue or space to skip]");
	if (getchar() == ' ') {
		while (getchar() != '\n') ; // read all other chars
		return true;
	}
	return false;
}
int play(void) {
	bool skip;
	for (int stage_index = 0; stage_index < 2; stage_index++) {
		const struct stage stage = stages[stage_index];
		printf("Stage %d: %s\n", stage_index + 1, stage.name);
		skip = press_to_continue_or_skip();
		if (skip) continue;

		for (int lesson_index = 0; lesson_index < stage.lesson_count; lesson_index++) {
			const struct lesson lesson = stage.lessons[lesson_index];
			printf("Stage %d: Lesson %d: %s\n", stage_index + 1, lesson_index + 1, lesson.description);
			skip = press_to_continue_or_skip();
			if (skip) continue;
			struct lesson_contents* lesson_contents = load_words_file(lesson.source_file_name);
			if (lesson_contents == NULL) {
				perror("could not load lesson contents from file");
				return EXIT_FAILURE;
			}
			const int count = lesson_contents->text_count;
			const char** texts = lesson_contents->texts;
			int success = learnText(texts, count);
			if (EXIT_SUCCESS != success) {
				return success;
			}
			printf("Completed lesson\n");
		}
		printf("Completed section\n");
	}

	return EXIT_SUCCESS;
}

int main(void) {
	srand(time(NULL)); // seed random number with curtime
	int success = play();
	return success;
}
