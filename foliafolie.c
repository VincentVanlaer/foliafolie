/***
 *  Foliafolie - A fast parser to extract lemma and text elements from folia xml
 *  Copyright (C) 2017 Vincent Vanlaer
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>. 
***/

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>


//#define FOLIA_DEBUG

#ifdef FOLIA_DEBUG
#define folia_log(string, ...) fprintf(stderr, string, ##__VA_ARGS__)
#else
#define folia_log(string, ...)
#endif

#define next(loc) i++;folia_log("%c", buffer[i]);goto loc;

#define check_quoted(callback, write_out) if (buffer[i] == '"') { \
	after_quoted = callback; \
	write_out_quoted = write_out; \
    letters_found = 0; \
	next(quoted);}

#define check_letter(letter) if (buffer[i] == letter) { \
	letters_found++; \
} \
else { letters_found = 0; }

#define finished_letters(count, callback) if(letters_found == count) {  folia_log("\nState change to " #callback "\n" ); letters_found = 0; next(callback);  }

const char w_begin[] = "<w ";
const char w_end[] = "</w>";
const char t_begin[] = "<t>";
const char t_ignore_begin[] = "<t ";
const char t_end[] = "</t>";
const char lemma_begin[] = "<lemma ";
const char alt_begin[] = "<alt";
const char alt_end[] = "</alt>";
const char w_class[] = "class=";

void *after_quoted;
bool write_out_quoted;

unsigned int letters_found;
char t_buffer[3];
int t_buffer_next;

unsigned int quoted_count, not_in_w_count, in_w_count, not_in_lemma_count; 


void print_char_something(char c) {
	putc(c, stdout);
}


void sigsegv_handler(int signum, siginfo_t *info, void *ptr) {
	folia_log("\n%d %d %d %d\n", quoted_count, not_in_w_count, in_w_count, not_in_lemma_count);
	exit(0);
}



int main( int argc, char *argv[] )
{
	letters_found = 0;

	struct stat sb;

	char *buffer;
	quoted_count = 0;
	not_in_w_count = 0;
	in_w_count = 0;
	not_in_lemma_count = 0;

	int fd;

	struct sigaction act;

	memset(&act, 0, sizeof(act));

	act.sa_sigaction = sigsegv_handler;
	act.sa_flags = SA_SIGINFO;

	sigaction(SIGSEGV, &act, NULL);

	if (argc == 2) {
		fd = open(argv[1], O_RDONLY);
		fstat(fd, &sb);
		buffer = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
		madvise(buffer, sb.st_size, MADV_SEQUENTIAL);

	}
	else {
		fprintf(stderr, "Streaming is no longer supported due to mmap madness");
		return 1;
	}

	int i = 0;

	goto not_in_w;

	while (true) {

quoted:
#ifdef FOLIA_DEBUG
		quoted_count++;
#endif
		if (buffer[i] == '"') {
			if (write_out_quoted) {
				print_char_something(' ');
			}
			next(*after_quoted)
		}
		
		if (write_out_quoted) {
			print_char_something(buffer[i]);
		}

		next(quoted);

pre_not_in_w:	
		print_char_something('\n');
not_in_w:
#ifdef FOLIA_DEBUG
		not_in_w_count++;
#endif
		//check_quoted(&&not_in_w, false);
		check_letter(w_begin[letters_found]);
		finished_letters(strlen(w_begin), in_w);
		
		next(not_in_w);

in_w:
#ifdef FOLIA_DEBUG
		in_w_count++;
#endif
		check_quoted(&&in_w, false);
		check_letter(w_class[letters_found]);
		finished_letters(strlen(w_class), reading_class);

		next(in_w);

reading_class:

		check_quoted(&&not_in_lemma, true);

		fprintf(stderr, "Couldn't find '\"' after 'class='");
		return 1;

space_pre_not_in_lemma:
		print_char_something(' ');

not_in_lemma:
#ifdef FOLIA_DEBUG
		not_in_lemma_count++;
#endif
//			check_quoted(&&not_in_lemma, false);

		if (buffer[i] == '<') {
			folia_log("\nStarting candidates\n");
			next( not_in_lemma_candidate_start);
		}

		next(not_in_lemma);

not_in_lemma_candidate_start:

		switch (buffer[i]) {
			case 't':
				next(not_in_lemma_candidate_t);
			case 'a':
				letters_found = 2;
				next(not_in_lemma_candidate_a);
			case 'l':
				letters_found = 2;
				next(not_in_lemma_candidate_l);
			case '/':
				letters_found = 2;
				next(not_in_lemma_candidate_w);
			default:
				next(not_in_lemma);
		}

not_in_lemma_candidate_t:

		switch (buffer[i]) {
			case ' ':
				letters_found = 0;
				folia_log("\nCandidate '<t ' found\n");
				next(in_t_ignore);
			case '>':
				letters_found = 0;
				folia_log("\nCandidate '<t>' found\n");
				next(pre_in_t);
			default:
				next(not_in_lemma);
		}

not_in_lemma_candidate_l:

		if (buffer[i] == lemma_begin[letters_found]) {
			if (letters_found == strlen(lemma_begin) - 1) {
				folia_log("\nCandidate '<lemma ' found\n");
				letters_found = 0;
				next(in_lemma);
			}
			
			letters_found++;
			next(not_in_lemma_candidate_l);
		}
		else {
			next(not_in_lemma);
		}

not_in_lemma_candidate_a:

		if (buffer[i] == alt_begin[letters_found]) {
			if (letters_found == strlen(alt_begin) - 1) {
				folia_log("\nCandidate '<alt ' found\n");
				letters_found = 0;
				next(in_alt);
			}
			
			letters_found++;
			next(not_in_lemma_candidate_a);
		}
		else {
			next(not_in_lemma);
		}

not_in_lemma_candidate_w:

		if (buffer[i] == w_end[letters_found]) {
			if (letters_found == strlen(w_end) - 1) {
				folia_log("\nCandidate '</w>' found\n");
				print_char_something('\n');
				letters_found = 0;
				next(not_in_w);
			}
			
			letters_found++;
			next(not_in_lemma_candidate_w);
		}
		else {
			next(not_in_lemma);
		}

in_lemma:
		check_quoted(&&in_lemma, false);
		check_letter(w_class[letters_found]);
		finished_letters(strlen(w_class), reading_class);

		next(in_lemma);

pre_in_t:
		t_buffer[0] = '-';
		t_buffer[1] = 't';
		t_buffer[2] = ':';

		t_buffer_next = 0;

		letters_found = 0;

in_t:
		//fprintf(stderr, "%d %d\n", t_buffer_next, letters_found);
		check_letter(t_end[letters_found]);
		finished_letters(strlen(t_end), space_pre_not_in_lemma);

		print_char_something(t_buffer[t_buffer_next]);
		t_buffer[t_buffer_next] = buffer[i];
		t_buffer_next = ((t_buffer_next + 1) % 3);

		next(in_t);


in_t_preamble_ignore:

		check_quoted(&&in_t_preamble_ignore, false);

		if (buffer[i] == '>') {
		   	next(in_t_ignore);
		}

		next(in_t_preamble_ignore);


in_t_ignore:
		
		check_letter(t_end[letters_found]);
		finished_letters(strlen(t_end), not_in_lemma);

		next(in_t_ignore);
in_alt:

		check_quoted(&&in_alt, false);
		check_letter(alt_end[letters_found]);
		finished_letters(strlen(alt_end), not_in_lemma);

		next(in_alt);

	}
}

