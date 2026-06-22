#include <stdio.h>		// perror(), printf()
#include <stdlib.h>		// exit(), EXIT_FAILURE
#include <unistd.h>		// R_OK
#include <sys/wait.h>	// waitpid()
#include <stdint.h>		// uint8_t, uint16_t, etc
#include <stdbool.h>	// bool
#include <string.h>		// memset()
#include <time.h>   	// time()

#include "audio_handler.h"
#include "morse_data.h"
#include "lessons.h"

// morse code timings following international standard
#define WORDS_PER_SECOND 20.0		// TODO: make settable as parameter for better practice of fast speeds
#define DIT_DURATION_SECS (1.2 / WORDS_PER_SECOND)
#define DAH_DURATIONS_SECS (DIT_DURATION_SECS * 3.0)
#define BIT_INTERVAL_DURATION_SECS (DIT_DURATION_SECS)

#define COUNTS_DISPLAY_ROWS 10		// prevent list too long, TODO: make param

// TODO: word letter interval, word interval
// TODO: limit amount of texts practicing (longer word lists get quite long but varied pratice is good)
// TODO: add permanent practice saving (be able to auto-continue where left off, maybe also anki-style retention setup)
// TODO: stage4? (sending messages)
// TODO: stage5? (two-way dialogues/communications)

// learning consts
#define CONSECUTIVE_CORRECT_THRESHOLD 5 // TODO: make settable as parameter for user preference

int min(const int a, const int b) {
	if (a > b) return b;
	return a;
}
void sleep_double(const double duration_seconds) {
	const int duration_microseconds = (int)(duration_seconds*1000000);
	usleep(duration_microseconds);
}
int send_message(const char*const message) {
	int success;
	success = startAudioSubprocess();
	if (success != EXIT_SUCCESS) {
		return success;
	}
	queueQuiet(DAH_DURATIONS_SECS); // start with quiet to let audio program start and get ready
	for (int i = 0; message[i] != '\0'; i++) {
		// TODO: handle spaces for multiple words to have correct timing
		const char letter = message[i];
		const char*const morse = get_morse(letter);
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
int random_range(const int maxExclusive) {
	return (rand() % maxExclusive);
}
void clear_console(void) {
	printf("\e[2J\e[H\n"); // clears and resets cursor position
}

int learnText(const char**const texts, const int texts_length) {
	uint8_t consecutive_correct_counts[texts_length];
	memset(consecutive_correct_counts, 0, texts_length * sizeof(uint8_t));

	int texts_still_learning = texts_length;
	int index = 0;
	while (texts_still_learning > 0) {
		index = random_range(texts_still_learning);
		const char*const message = texts[index];
		playSound:
		clear_console();
		print_counts(texts_still_learning, texts, consecutive_correct_counts);
		printf("Listen... ");
		fflush(stdout);
		const int success = send_message(message);
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
			const bool completedText = consecutive_correct_counts[index] >= CONSECUTIVE_CORRECT_THRESHOLD;
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
int main(void) {
	srand(time(NULL)); // seed random number with curtime
	bool skip;
	for (int stage_index = 0; stage_index < 2; stage_index++) {
		const struct stage stage = get_stage(stage_index);
		printf("Stage %d: %s\n", stage_index + 1, stage.name);
		skip = press_to_continue_or_skip();
		if (skip) continue;

		for (int lesson_index = 0; lesson_index < stage.lesson_count; lesson_index++) {
			const struct lesson lesson = stage.lessons[lesson_index];
			printf("Stage %d: Lesson %d: %s\n", stage_index + 1, lesson_index + 1, lesson.description);
			skip = press_to_continue_or_skip();
			if (skip) continue;
			const struct lesson_contents*const lesson_contents = load_words_file(lesson.source_file_name);
			if (lesson_contents == NULL) {
				perror("could not load lesson contents from file");
				return EXIT_FAILURE;
			}
			const int count = lesson_contents->text_count;
			const char**const texts = lesson_contents->texts;
			const int success = learnText(texts, count);
			if (EXIT_SUCCESS != success) {
				return success;
			}
			printf("Completed lesson\n");
		}
		printf("Completed section\n");
	}
	return EXIT_SUCCESS;
}
