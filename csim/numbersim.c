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
    double assocs[MAX_MARKERS * MAX_CARDINALITY];
    double learning_rate;
    uint32_t thresholds[MAX_CARDINALITY];
    pcg32_random_t rand_state;
} state_t;

static unsigned length_of_assocs_array(const language_t *l, uint_fast32_t max_cue)
{
    return l->num_markers * (max_cue + 1);
}

static double get_assoc(const state_t *state, uint_fast32_t cue, unsigned marker_index)
{
    return state->assocs[(cue * state->language.num_markers) + marker_index];
}

static void add_to_assoc(state_t *state, uint_fast32_t cue, unsigned marker_index, double v)
{
    state->assocs[(cue * state->language.num_markers) + marker_index] += v;
}

static void update_state_helper(state_t *state, unsigned marker_index, uint_fast32_t cardinality, double l)
{
    double vax = 0;
    for (unsigned i = 0; i <= cardinality; ++i) {
        vax += get_assoc(state, i, marker_index);
    }

    double delta_v = state->learning_rate * (l - vax);

    for (unsigned i = 0; i <= cardinality; ++i) {
        //printf("ADDING card=%i, marker=%i, raw_index=%u, +%f [%f]\n", i, marker_index, (i * state->language.num_markers) + marker_index, delta_v, l);
        add_to_assoc(state, i, marker_index, delta_v);
    }
}

static void update_state(state_t *state, unsigned marker_index, uint_fast32_t cardinality)
{
    //printf("UPD marker=%u, card=%u\n", marker_index, cardinality);
    for (unsigned i = 0; i < state->language.num_markers; ++i) {
        update_state_helper(state, i, cardinality, i == marker_index ? 1.0 : 0.0);
    }
}

static void output_headings(const state_t *state)
{
    printf("trial");
    for (unsigned i = 0; i < state->max_cue; ++i) {
        for (unsigned j = 0; j < state->language.num_markers; ++j) {
            printf(",%i->%s", i+1, state->language.markers[j]);
        }
    }
    printf(",seed1,seed2\n");
}

static void output_line(const state_t *state, int marker_index, uint_fast32_t cardinality)
{
    printf("%s %i", marker_index == - 1 ? "" : state->language.markers[marker_index], cardinality+1);
    for (unsigned i = 0; i < state->max_cue; ++i) {
        for (unsigned j = 0; j < state->language.num_markers; ++j) {
            double sum = 0;
            for (unsigned k = 0; k <= i; ++k)
                sum += get_assoc(state, k, j);
            printf(",%f", sum);
        }
    }
    printf(",%llu,%llu\n", state->rand_state.state, state->rand_state.inc);
}

static void run_trials(state_t *state, uint_fast64_t n)
{
    output_headings(state);

    uint_fast32_t card = 0;
    int marker_index = -1;
    for (uint_fast64_t i = 0; i < n; ++i) {
        output_line(state, marker_index, card);

        uint32_t r = pcg32_random_r(&(state->rand_state));

        // Determine the cardinality of the cue based on the random number.
        for (card = 0; card < state->max_cue && r >= state->thresholds[card]; ++card);

        // Get the appropriate marker for that cardinality.
        marker_index = state->language.n_to_marker[card];
        assert(marker_index >= 0);

        update_state(state, marker_index, card);
    }
}

//
// If unsigned long long is smaller than uint64_t, the following array will be
// declared with a negative size, which will give rise to a compile-time error.
//
struct ASSERT_UNSIGNED_LONG_LONG_IS_AT_LEAST_64_BIT_STRUCT {
    int ASSERT_UNSIGNED_LONG_LONG_IS_AT_LEAST_64_BIT[(int)sizeof(unsigned long long) - (int)sizeof(uint64_t)];
};

// Example invocation:
//
//     ./numbersim 4321 1234 english 0.6 3 0.01 7 200
//
int main(int argc, char **argv)
{
    //
    // Arguments (all required):
    //
    //     1)    Name of file containing language data.
    //     2)    First random seed (decimal integer between 0 and (2^64)-1 inclusive)
    //     3)    Second reandom seed (decimal integer between 0 and (2^64)-1 inclusive)
    //     4)    Language name
    //     5)    beta (param to ztnbd, value is 0.6 in original exp; this value ignored unless arg 9 is "ztnbd")
    //     6)    r (param to ztnbd, value is 3 in original exp; this value ignored unless arg 9 is "ztnbd")
    //     7)    learning rate (typical value is 0.01)
    //     8)    Maximum cue cardinality (7 in original experiment).
    //     9)    Number of trials to run (decimal integer between 0 and (2^64)-1 inclusive)
    //
    //     If argument (10) is "ztnbd", then no further arguments should be given,
    //     and the distribution of cardinalities is given by a zero-terminated
    //     negative binomial distribution parameterized by arguments (4)-(5).
    //
    //     Otherwise, argument 10 should be the first in a series of floating point
    //     values. The length of this series must be identical to the maximum
    //     cue cardinality. The values are intepreted as p values specifying a
    //     probability distribution.
    //
    //

    if (argc < 11) {
        fprintf(stderr, "Not enough arguments\n");
        exit(1);
    }

    state_t state;

    const char *language_file_name = argv[1];

    // Get random seed from first and second arguments.
    uint64_t seed1, seed2;
    if (sscanf(argv[2], "%llu", &seed1) < 1) {
        fprintf(stderr, "Error parsing first random seed (first argument)\n");
        exit(1);
    }
    if (sscanf(argv[3], "%llu", &seed2) < 1) {
        fprintf(stderr, "Error parsing second random seed (second argument)\n");
        exit(1);
    }

    seed2 &= 1; // Ensure that seed2 is odd, as required by pcg library.
    pcg32_srandom_r(&(state.rand_state), seed1, seed2);

    const char *language_name = argv[4];

    double beta, r, learning_rate;
    if (sscanf(argv[5], "%lf", &beta) < 1) {
        fprintf(stderr, "Error parsing beta (fourth argument)\n");
        exit(1);
    }
    if (sscanf(argv[6], "%lf", &r) < 1) {
        fprintf(stderr, "Error parsing r (fifth argument)\n");
        exit(1);
    }
    if (sscanf(argv[7], "%lf", &learning_rate) < 1) {
        fprintf(stderr, "Error parsing learning_rate (sixth argument)\n");
        exit(1);
    }

    uint_fast32_t max_cue;
    if (sscanf(argv[8], "%u", &max_cue) < 1) {
        fprintf(stderr, "Error parsing max_cue (seventh argument)\n");
        exit(1);
    }
    if (max_cue == 0) {
        fprintf(stderr, "max_cue (seventh argument) must be greater than 0\n");
        exit(1);
    }
    if (max_cue > MAX_CARDINALITY) {
        fprintf(stderr, "Value of max_cue (seventh argument) is too big.\n");
        exit(1);
    }

    uint_fast64_t num_trials;
    if (sscanf(argv[9], "%llu", &num_trials) < 1) {
        fprintf(stderr, "Error parsing number of trials (eighth argument)\n");
        exit(1);
    }

    if (! strcmp(argv[10], "ztnbd")) {
        if (argc > 11) {
            fprintf(stderr, "Unrecognized trailing arguments following zrnbd\n");
            exit(1);
        }
        for (unsigned i = 0; i < max_cue; ++i) {
            double p = ztnbd(i+1, beta, r);
            assert(p >= 0 && p <= 1);
            p *= UINT32_MAX;
            state.thresholds[i] = (uint32_t)p;
            if (i > 0) {
                state.thresholds[i] += state.thresholds[i-1];
            }
        }
    }
    else {
        if (argc != 10 + max_cue) {
            fprintf(stderr, "Incorrect number of p values for probability distribution (%u given, %u required)\n", argc-10, max_cue);
            exit(1);
        }
        double total = 0;
        for (unsigned i = 0; i < 0 + max_cue; ++i) {
            double p;
            if (sscanf(argv[i+10], "%lf", &p) < 1) {
                fprintf(stderr, "Error parsing probability value.\n");
                exit(1);
            }
            total += p;
            state.thresholds[i] = (uint32_t)(UINT32_MAX * p);
            if (i > 0) {
                state.thresholds[i] += state.thresholds[i-1];
            }
        }
        if (fabs(total - 1.0) > 0.01) {
            fprintf(stderr, "p values do not sum to 1\n");
            exit(1);
        }
    }

    get_languages(language_file_name, languages);
    //test_print_languages(languages);

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

    memcpy(&state.language, lang, sizeof(language_t));
    state.max_cue = max_cue;
    unsigned al = length_of_assocs_array(lang, max_cue);
    for (unsigned i = 0; i < al; ++i)
        state.assocs[i] = 0.0;
    state.learning_rate = learning_rate;

    run_trials(&state, num_trials);

    return 0;
}
