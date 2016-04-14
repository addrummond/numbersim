#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <config.h>
#include <parser.h>
#include <pcg_basic.h>
#include <assert.h>
#include <string.h>

// Terminated by language with empty string as name.
static language_t languages[MAX_LANGUAGES];

static unsigned factorial(uint_fast32_t n)
{
    assert(n > 0);

    uint_fast32_t nn = n;
    while (--nn > 0)
        n *= nn;
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

typedef struct state {
    language_t language;
    uint_fast32_t max_cue;
    double *assocs;
    double learning_rate;
    double ztnbd_beta;
    double ztnbd_r;
} state_t;

static unsigned length_of_assocs_array(const language_t *l, uint_fast32_t max_cue)
{
    return l->num_markers * max_cue;
}

static double get_assoc(const state_t *state, uint_fast32_t cue, unsigned marker_index)
{
    return state->assocs[(cue * state->language.num_markers) + marker_index];
}

static void add_to_assoc(const state_t *state, uint_fast32_t cue, unsigned marker_index, double v)
{
    state->assocs[(cue * state->language.num_markers) + marker_index] += v;
}

static void update_state_helper(state_t *state, unsigned marker_index, uint_fast32_t cardinality, double l)
{
    double vax = 0;
    for (unsigned i = 0; i < cardinality; ++i) {
        vax += get_assoc(state, i, marker_index);
    }

    double delta_v = state->learning_rate * (l - vax);

    for (unsigned i = 0; i < cardinality; ++i) {
        add_to_assoc(state, cardinality, marker_index, delta_v);
    }
}

static void update_state(state_t *state, unsigned marker_index, uint_fast32_t cardinality)
{
    for (unsigned i = 0; i < state->language.num_markers; ++i) {
        const char *m = state->language.markers[i];
        update_state_helper(state, i, cardinality, i == marker_index);
    }
}

static void output_headings(const state_t *state)
{
    for (unsigned i = 0; i < state->max_cue; ++i) {
        for (unsigned j = 0; j < state->language.num_markers; ++j) {
            if (! (j == 0 && i == 0))
                printf(",");
            printf("%i->%s", i+1, state->language.markers[j]);
        }
    }
    printf("\n");
}

static void output_state(const state_t *state)
{
    for (unsigned i = 0; i < state->max_cue; ++i) {
        for (unsigned j = 0; j < state->language.num_markers; ++j) {
            if (! (j == 0 && i == 0))
                printf(",");
            printf("%f", get_assoc(state, i, j));
        }
    }
    printf("\n");
}

static void run_trials(state_t *state, unsigned n)
{
    output_headings(state);

    uint32_t thresholds[state->max_cue];

    for (unsigned i = 1; i <= state->max_cue; ++i) {
        double p = ztnbd(i, state->ztnbd_beta, state->ztnbd_r);
        assert(p >= 0 && p <= 1);
        p *= UINT32_MAX;
        thresholds[i] = (uint32_t)rint(p);
    }

    for (unsigned i = 0; i < n; ++i) {
        output_state(state);

        uint32_t r = pcg32_boundedrand(1.0);

        // Determine the cardinality of the cue based on the random number.
        unsigned card;
        for (card = 0; card < state->max_cue && r < thresholds[card]; ++card);

        // Get the appropriate marker for that cardinality.
        int marker_index = state->language.n_to_marker[card];
        if (marker_index == -1)
            marker_index = state->language.default_marker_index;

        update_state(state, marker_index, card);
    }
}

// Example invocation:
//
//     ./numbersim 4321 1234 english 0.6 3 0.01 7 200
//
int main(int argc, char **argv)
{
    if (sizeof(unsigned long long) < sizeof(uint64_t)) {
        fprintf(stderr, "WARNING: Seeds cannot be parsed as 64-bit integers -- unsigned long long is too small\n");
    }

    //
    // Arguments (all required):
    //
    //     1) First random seed (unsigned 64-bit integer in decimal)
    //     2) Second reandom seed (unsigned 64-bit integer in decimal)
    //     3) Language name
    //     4) beta (param to ztnbd, value is 0.6 in original exp)
    //     5) r (param to ztnbd, value is 3 in original exp)
    //     6) learning rate (typical value is 0.01)
    //     7) Maximum cue cardinality (7 in original experiment).
    //     8) Number of trials to run.
    //

    if (argc != 9) {
        fprintf(stderr, "Bad arguments\n");
        exit(1);
    }

    // Get random seed from first and second arguments.
    uint64_t seed1, seed2;
    if (sscanf(argv[1], "%llu", &seed1) < 1) {
        fprintf(stderr, "Error parsing first random seed (first argument)\n");
        exit(1);
    }
    if (sscanf(argv[2], "%llu", &seed2) < 1) {
        fprintf(stderr, "Error parsing second random seed (second argument)\n");
        exit(1);
    }

    seed2 += !(seed2 % 2); // Ensure that seed2 is odd, as required by pcg library.
    pcg32_srandom(seed1, seed2);

    const char *language_name = argv[3];

    double beta, r, learning_rate;
    if (sscanf(argv[4], "%lf", &beta) < 1) {
        fprintf(stderr, "Error parsing beta (fourth argument)\n");
        exit(1);
    }
    if (sscanf(argv[5], "%lf", &r) < 1) {
        fprintf(stderr, "Error parsing r (fifth argument)\n");
        exit(1);
    }
    if (sscanf(argv[6], "%lf", &learning_rate) < 1) {
        fprintf(stderr, "Error parsing learning_rate (sixth argument)\n");
        exit(1);
    }

    uint_fast32_t max_cue;
    if (sscanf(argv[7], "%u", &max_cue) < 1) {
        fprintf(stderr, "Error parsing max_cue (seventh argument)\n");
        exit(1);
    }

    unsigned num_trials;
    if (sscanf(argv[8], "%u", &num_trials) < 1) {
        fprintf(stderr, "Error parsing number of trials (eighth argument)\n");
        exit(1);
    }

    get_languages("languages.txt", languages);
    test_print_languages(languages);

    // Find the specified language.
    language_t *lang = NULL;
    for (language_t *l = languages; l->name[0] != '\0'; ++l) {
        if (! strcmp(l->name, language_name)) {
            lang = l;
            break;
        }
    }
    if (! lang) {
        fprintf(stderr, "Could not find language %s\n", language_name);
        exit(1);
    }

    state_t state;
    memcpy(&state.language, lang, sizeof(language_t));
    state.max_cue = max_cue;
    unsigned al = length_of_assocs_array(lang, max_cue);
    state.assocs = malloc(sizeof(state.assocs[0]) * al);
    for (unsigned i = 0; i < al; ++i)
        state.assocs[i] = 0.0;
    state.learning_rate = learning_rate;
    state.ztnbd_beta = beta;
    state.ztnbd_r = r;

    run_trials(&state, num_trials);

    free(state.assocs);

    return 0;
}
