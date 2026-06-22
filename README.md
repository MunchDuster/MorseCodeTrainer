# What is this?
This is C program for a console application to practice morse code on Linux using the `aplay` audio player utility.

# Learning stages
1. Letters
	Loop of listening to Letters and typing in letter heard
	Keep going until all letters correct 3 times consecutively
2. Words
	Full words, gets progressively more difficult
3. Sentences
	(Implement later) Full sentences, gets progresively more difficult

# What is the environment?
Linux desktop, Ubuntu based

# How does it work?
Use linux audio program 'aplay' as subprocess and pipe in audio bytes,
opens and closes aplay when sound is needed to keep things simple.

COMPILE
`clang -Wall -Wextra -Werror main.c -o morser -lm`

RUN
`./morser`
