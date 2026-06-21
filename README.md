WHO
Made by MunchDuster, 2026

WHAT
Program to practice morse code
Three stages
1. Letters
	Loop of listening to Letters and typing in letter heard
	Keep going until all letters correct 3 times consecutively
2. Words
	Full words, gets progressively more difficult
3. Sentences
	(Implement later) Full sentences, gets progresively more difficult

ENVIRONMENT
Linux desktop, Ubuntu based

HOW
Use linux audio program 'aplay' as subprocess and pipe in audio bytes
Basically minimize imports and keep it simple
TODO: Figure out custom audio playing without using aplay

COMPILE
`clang -Wall -Wextra -Werror main.c -o morser -lm`

RUN
`./morser`
