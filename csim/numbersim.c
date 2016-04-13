#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <config.h>
#include <parser.h>

// Terminated by langugae with empty string as name.
static language_t languages[MAX_LANGUAGES];

static unsigned factorial(uint_fast32_t n)
{
    uint_fast32_t nn = n-1;
    do {
        n *= nn;
    } while (--nn > 0);
    return n;
}

static double ztnbd(uint_fast32_t k, double beta, double r)
{
    double top = r;
    for (uint_fast32_t i = 1; i < k; ++i)
        top *= r + (double)i;
    top /= factorial(k) * (pow(1.0+beta, r) - 1);
    top *= pow(beta/(1.0+beta), k);
    return top;
}
/*
typedef struct state {
    char *markers[];
    unsigned num_markers;
    uint_fast32_t max_cue;
    double assocs[];
} state_t;]

double get_assoc(const state_t *state, unsigned cue, unsigned marker_index)
{
    return state->assocs[(cue * num_markers) + marker_index];
}

void update_state(state_t *state, )*/

int main()
{
    get_languages("languages.txt", languages);
    test_print_languages(languages);
}
