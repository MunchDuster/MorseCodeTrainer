#include <stdio.h>		// perror(), printf()
#include <stdint.h>		// uint8_t, uint16_t, etc
#include <stdbool.h>	// bool

#include "math.h"

#define COUNTS_DISPLAY_ROWS 10		// prevent list too long, TODO: make param

bool press_to_continue_or_skip(void) {
	printf("[press enter to continue or space to skip]");
	if (getchar() == ' ') {
		while (getchar() != '\n') ; // read all other chars
		return true;
	}
	return false;
}
void clear_console(void) {
	printf("\e[2J\e[H\n"); // clears and resets cursor position
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