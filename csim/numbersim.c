#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>

#define MAX_LANGUAGES 50
#define LANGUAGE_NAME_MAX_LENGTH 50
#define MARKER_MAX_LENGTH 10
#define MAX_MARKERS 10
#define MAX_CARDINALITY 100

typedef struct language {
    char name[LANGUAGE_NAME_MAX_LENGTH+1];
    char markers[MARKER_MAX_LENGTH+1][MAX_MARKERS+1];
    char n_to_marker[MAX_CARDINALITY];
    // Used if marker[][] is empty string.
    char default_marker[MARKER_MAX_LENGTH+1];
} language_t;
language_t languages[MAX_LANGUAGES];

void get_languages(const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (! f) {
        fprintf(stderr, "Error opening %s\n", filename);
        exit(1);
    }

    language_t current_language;
    unsigned current_name_index = 0;
    char *current_marker = current_language.markers[0];
    unsigned current_marker_index = 0;
    unsigned markers_index = 0;
    char current_num[MAX_CARDINALITY/10 + 1];
    unsigned current_num_index = 0;
    char state = 'i';
    for (;;) {
        int ci = fgetc(f);
        if (ci != EOF && ci < 0)
            goto err;
        char c = (char)ci;

        if (c == '\n' || ci == EOF) {

        }
        else if (state == 'i' && isalpha(c)) {
            if (current_name_index + 1 >= LANGUAGE_NAME_MAX_LENGTH) {
                fprintf(stderr, "Language name too long in %s\n", filename);
                exit(1);
            }
            current_language.name[current_name_index++] = c;
        }
        else if (state == 'i' && isspace(c)) {
            current_language.name[current_name_index] = '\0';
            state = 's';
        }
        else if (state == 's' && isspace(c)) {
            ;
        }
        else if (state == 's' && isalpha(c)) {
            current_marker[current_marker_index++] = c;
            state = 'm';
        }
        else if (state == 's') {
            fprintf(stderr, "Bad syntax in %s\n", filename);
            exit(1);
        }
        else if (state == 'm' && isalpha(c)) {
            if (current_marker_index + 1 >= MARKER_MAX_LENGTH) {
                fprintf(stderr, "Marker name too long in %s\n", filename);
                exit(1);
            }

            current_marker[current_marker_index++] = c;
        }
        else if (state == 'm' && isspace(c)) {
            current_marker[marker_index] = '\0';
            state = 't';
        }
        else if (state == 'm') {
            fprintf(stderr, "Unexpected character in %s\n", filename);
            exit(1);
        }
        else if (state == 't' && isspace(c)) {
            ;
        }
        else if (state == 't' && isdigit(c)) {
            current_num[current_num_index++] = c;
            state = 'n';
        }
        else if (state == 't' && c == '*') {
            current_language.default_marker = current_language.markers[current_marker_index];
            state = 's';
        }
        else if (state == 't') {
            fprintf(stderr, "Unexpected character in %s\n", filename);
            exit(1);
        }
        else if (state == 'n' && isdigit(c)) {
            if (current_num_index + 1 > sizeof(current_num)/sizeof(char))) {
                fprintf(stderr, "Cardinality too big in %s\n", filename);
                exit(1);
            }
            current_num[current_num_index++] = c;
        }
        else if (state == 'n' && isspace(c) || c == '\n') {
            current_num[current_num_index] = '\0';
            int n = atoi(current_num);
            current_language[n_to_marker] = marker_index;
        }
        else if (state == 'n') {
            fprintf(stderr, "Unexpected character in %s\n", filename);
            exit(1);
        }
    }

err:
    fprintf(stderr, "Error reading %s\n", filename);
    exit(1);
}

unsigned factorial(uint_fast32_t n)
{
    uint_fast32_t nn = n-1;
    do {
        n *= nn;
    } while (--nn > 0)
    return n;
}

double ztnbd(uint_fast32_t k, double beta, double r)
{
    double top = r;
    for (uint_fast32_t i = 1; i < k; ++i)
        top *= r + (double)i;
    top /= factorial(k) * (pow(1.0+beta, r) - 1);
    top *= pow(beta/(1.0+beta), k);
    return top;
}

typedef struct state {
    const char *markers[];
    unsigned num_markers;
    uint_fast32_t max_cue;
    double assocs[];
} state_t;]

double get_assoc(const state_t *state, unsigned cue, unsigned marker_index)
{
    return state->assocs[(cue * num_markers) + marker_index];
}

void update_state(state_t *state, )
