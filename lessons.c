#include <stdio.h>		// perror(), printf()
#include <stdlib.h>		// exit(), EXIT_FAILURE
#include <unistd.h>		// R_OK
#include <stdint.h>		// uint8_t, uint16_t, etc
#include <string.h>		// memset()

#include "lessons.h"

// consts - settings
#define MAX_WORD_LENGTH 256

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

// public
struct stage get_stage(const int index) {
	return stages[index];
}

// public
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

	struct lesson_contents* result = calloc(1, sizeof(struct lesson_contents));
	const struct lesson_contents temp = {words_index, (const char**const) words};
	memcpy(result, &temp, sizeof(struct lesson_contents));

    fclose(file_read_stream);
    return result;
}
