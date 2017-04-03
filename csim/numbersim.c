//
// Example invocations:
//
//     numbersim languages.txt 4321 1234 english 0 0 0.01 7 1000 summary 100 0.143 0.143 0.143 0.143 0.143 0.143 0.143
//     numbersim
//
// If invoked with no arguments, arguments are read line-by-line from stdin.
// The corresponding output for each line is written to stdout immediately after
// each line is read.
//
// Arguments (all required):
//
//     1)    Name of file containing language data.
//     2)    First random seed (decimal integer between 0 and (2^64)-1 inclusive)
//     3)    Second reandom seed (decimal integer between 0 and (2^64)-1 inclusive)
//     4)    Language name
//     5)    learning rate (typical value is 0.01)
//     6)    Maximum cue cardinality (7 in original experiment).
//     7)    Number of trials to run (decimal integer between 0 and (2^64)-1 inclusive)
//     8)    Output mode (either 'full', 'summary' or 'range_summary')
//     9)    If output mode is "summary', quit after all markers have been correct
//           for at least this number of trials. If 0, never quit early.
//           This value is ignored for other output modes.
//
//     Argument (10) should be the first in a series of floating point
//     values. The length of this series must be one less than the maximum
//     cue cardinality. The values are intepreted as p values specifying a
//     probability distribution. The leftover probability mass is interpreted
//     as the probability of a cue with at least the cardinality specified by
//     argument (6).
//

#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <config.h>
#include <parser.h>
#include <pcg_basic.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

typedef enum output_mode {
    OUTPUT_MODE_FULL,
    OUTPUT_MODE_SUMMARY,
    OUTPUT_MODE_RANGE_SUMMARY
} output_mode_t;

typedef struct state {
    language_t language;
    uint_fast64_t n_trials;
    uint_fast32_t max_cue;
    double assocs[MAX_CARDINALITY][MAX_MARKERS];
    double compound_cue_assocs[MAX_CARDINALITY][MAX_MARKERS];
    double learning_rate;
    uint32_t thresholds[MAX_CARDINALITY];
    pcg32_random_t rand_state;
    output_mode_t output_mode;
    uint_fast64_t marker_has_been_correct_for_last[MAX_CARDINALITY];
    uint_fast64_t all_markers_have_been_correct_for_last;
    // Array of bitfields for each cardinality.
    uint8_t *correct_at[MAX_CARDINALITY];
    unsigned quit_after_n_correct;
} state_t;

static void update_state_for_marker(state_t *state, unsigned marker_index, uint_fast32_t cardinality, double l)
{
    double vax = 0;
    for (unsigned i = 0; i <= cardinality; ++i) {
        vax += state->assocs[i][marker_index];
    }

    double delta_v = state->learning_rate * (l - vax);

    for (unsigned i = 0; i <= cardinality; ++i) {
        //printf("ADDING card=%i, marker=%i, raw_index=%u, +%f [%f]\n", i, marker_index, (i * state->language.num_markers) + marker_index, delta_v, l);
        state->assocs[i][marker_index] += delta_v;
    }
}

static bool update_state(state_t *state, unsigned marker_index, uint_fast32_t cardinality)
{
    for (unsigned i = 0; i < state->language.num_markers; ++i) {
        update_state_for_marker(state, i, cardinality, i == marker_index ? 1.0 : 0.0);
    }

    unsigned correct_for = 0;
    for (unsigned i = 0; i < state->max_cue; ++i) {
        double max_sum = 0.0;
        unsigned max_sum_marker_index;
        for (unsigned j = 0; j < state->language.num_markers; ++j) {
            double sum = 0.0;
            for (unsigned k = 0; k <= i; ++k)
                sum += state->assocs[k][j];
            if (sum > max_sum) {
                max_sum = sum;
                max_sum_marker_index = j;
            }
            state->compound_cue_assocs[i][j] = sum;
        }
        if (max_sum_marker_index == state->language.n_to_marker[i]) {
            ++correct_for;
            ++(state->marker_has_been_correct_for_last[i]);
            state->correct_at[i][state->n_trials / 8] |= (1 << (state->n_trials % 8));
        }
        else {
            state->marker_has_been_correct_for_last[i] = 0;
        }
    }

    if (correct_for == state->max_cue) {
        ++(state->all_markers_have_been_correct_for_last);
    }
    else {
        state->all_markers_have_been_correct_for_last = 0;
    }

    if (state->output_mode == OUTPUT_MODE_SUMMARY) {
        // Check if we should quit now.
        return (state->quit_after_n_correct == 0 ||
                (state->all_markers_have_been_correct_for_last < state->quit_after_n_correct));
    }
    else {
        return true; // Continue running.
    }
}

static void output_headings(const state_t *state)
{
    printf("trial");
    for (unsigned i = 0; i < state->max_cue; ++i) {
        for (unsigned j = 0; j < state->language.num_markers; ++j) {
            printf(",%i->%s", i+1, state->language.markers[j]);
        }
        printf(",%i_correct", i+1);
    }
    printf(",seed1,seed2\n");
}

static void output_line(const state_t *state, int marker_index, uint_fast32_t cardinality)
{
    printf("%s %i", (marker_index == - 1 ? "" : state->language.markers[marker_index]), cardinality+1);
    for (unsigned i = 0; i < state->max_cue; ++i) {
        for (unsigned j = 0; j < state->language.num_markers; ++j) {
            printf(",%f", state->compound_cue_assocs[i][j]);
        }
        printf(",%llu", state->marker_has_been_correct_for_last[i]);
    }
    printf(",%llu,%llu\n", state->rand_state.state, state->rand_state.inc);
}

static void output_summary(const state_t *state)
{
    // Output the number of trials which it took to get the right marker
    // for each cardinality for a sequence of at least the specified number of
    // trials.
    for (unsigned i = 0; i < state->max_cue; ++i) {
        if (i != 0)
            printf(",");

        uint_fast64_t correct_for = state->marker_has_been_correct_for_last[i];
        if (correct_for < state->quit_after_n_correct)
            // This marker was never correct for the required number of trials.
            printf("%llu", state->n_trials);
        else
            printf("%llu", state->n_trials - state->marker_has_been_correct_for_last[i]);
    }

    // For all cardinalities.
    if (state->all_markers_have_been_correct_for_last < state->quit_after_n_correct)
        printf(",%llu", state->n_trials);
    else
        printf(",%llu", state->n_trials - state->all_markers_have_been_correct_for_last);

    // Output seed state for random number generator (so that subsequent runs
    // can use them as the starting point).
    printf(",%llu,%llu\n", state->rand_state.state, state->rand_state.inc);
}

static void output_range_summary(const state_t *state)
{
    // We want to print numbers in ranges with a constant number of digits
    // to make sorting easier. So we need to find the number of digits
    // required to print state->n_trials - 1. See:
    //     http://stackoverflow.com/a/7200873/376854
    // for an explanation of this cute trick.
    unsigned num_digits = snprintf(0, 0, "%llu", state->n_trials);

    // For each cardinality, output the number of simulations which got
    // it right for each n trials (using ranges to make the output more compact).
    for (unsigned i = 0; i < state->max_cue; ++i) {
        if (i != 0)
            printf(",");
        
        uint_fast64_t start = 0;
        uint_fast64_t num_ranges = 0;
        uint_fast64_t j;
        uint_fast8_t v = 0;
        for (j = 0; j < state->n_trials; ++j) {
            v = state->correct_at[i][j/8];
            v >>= (j % 8);
            v &= 1;
            if (! v) {
                if (j - start > 1) {
                    if (num_ranges != 0)
                        printf(":");
                    printf("%0*llu-%0*llu", num_digits, start, num_digits, j-1);
                    ++num_ranges;
                }
                start = j;
            }
        }
        if (v) {
             if (num_ranges > 0)
                 printf(":");
             printf("%0*llu-%0*llu", num_digits, start, num_digits, j-1);
        }
    }
    printf(",");
    // Same thing but right for every cardinality for n trials.
    uint_fast64_t start = 0;
    uint_fast64_t num_ranges = 0;
    uint_fast64_t j;
    uint_fast8_t v = 0;
    for (j = 0; j < state->n_trials; ++j) {
        v = 1;
        for (unsigned c = 0; c < state->max_cue; ++c) {
            uint_fast8_t vv = state->correct_at[c][j/8];
            vv >>= (j % 8);
            vv &= 1;
            v &= vv;
        }
        if (! v) {
            if (j - start > 1) {
                if (num_ranges != 0)
                    printf(":");
                printf("%0*llu-%0*llu", num_digits, start, num_digits, j-1);
                ++num_ranges;
            }
            start = j;
        } 
    }
    if (v) {
        if (num_ranges > 0)
            printf(":");
        printf("%0*llu-%0*llu", num_digits, start, num_digits, j-1);
    }

    // Output seed state for random number generator (so that subsequent runs
    // can use them as the starting point).
    printf(",%llu,%llu\n\n", state->rand_state.state, state->rand_state.inc);
}

static void run_trials(state_t *state, uint_fast64_t n)
{
    if (state->output_mode == OUTPUT_MODE_FULL)
        output_headings(state);

    uint_fast32_t card = 0;
    int marker_index = -1;
    for (; state->n_trials < n; ++(state->n_trials)) {
        if (state->output_mode == OUTPUT_MODE_FULL)
            output_line(state, marker_index, card);

        uint32_t r = pcg32_random_r(&(state->rand_state));

        // Determine the cardinality of the cue based on the random number.
        for (card = 0; card < state->max_cue - 1 && r >= state->thresholds[card]; ++card);

        // Get the appropriate marker for that cardinality.
        marker_index = state->language.n_to_marker[card];
        assert(marker_index >= 0);

        if (! update_state(state, marker_index, card))
            break;
    }

    if (state->output_mode == OUTPUT_MODE_SUMMARY)
        output_summary(state);
    else if (state->output_mode == OUTPUT_MODE_RANGE_SUMMARY)
        output_range_summary(state);

    fflush(stdout);

    for (unsigned i = 0; i < MAX_CARDINALITY; ++i)
        free(state->correct_at[i]);
}

#define ARGS_STRING_MAX_LENGTH (1024*8)
#define MAX_ARGS 100

static unsigned string_to_arg_array(char *str, char *arg_array[])
{
    arg_array[0] = str;

    bool in_space = false;
    unsigned aai = 1;
    unsigned i;
    for (i = 0; str[i] && aai < MAX_ARGS; ++i) {
        if (in_space) {
            if (! isspace(str[i])) {
                arg_array[aai++] = str + i;
                in_space = false;
            }
        }
        else {
            if (isspace(str[i])) {
                str[i] = '\0';
                in_space = true;
            }
        }
    }

    if (str[i]) {
        fprintf(stderr, "Too many arguments (max is %u)\n", ARGS_STRING_MAX_LENGTH);
        exit(1);
    }

    return aai;
}

//
// If unsigned long long is smaller than uint64_t, the following array will be
// declared with a negative size, which will give rise to a compile-time error.
//
struct ASSERT_UNSIGNED_LONG_LONG_IS_AT_LEAST_64_BIT_STRUCT {
    int ASSERT_UNSIGNED_LONG_LONG_IS_AT_LEAST_64_BIT[(int)sizeof(unsigned long long) - (int)sizeof(uint64_t)];
};

// Guaranteed to be initialized to all zeroes. This will have the consequence
// that the name of the first language will initially be the empty string.
static language_t languages[MAX_LANGUAGES];

static void run_given_arguments(int num_args, char **args)
{
    if (num_args < 12) {
        fprintf(stderr, "Not enough arguments\n");
        exit(2);
    }

    static state_t state;
    state.n_trials = 0;

    const char *language_file_name = args[0];

    // Get random seed from first and second arguments.
    uint64_t seed1, seed2;
    if (sscanf(args[1], "%llu", &seed1) < 1) {
        fprintf(stderr, "Error parsing first random seed '%s' (second argument)\n", args[1]);
        exit(3);
    }
    if (sscanf(args[2], "%llu", &seed2) < 1) {
        fprintf(stderr, "Error parsing second random seed '%s' (third argument)\n", args[2]);
        exit(4);
    }

    seed2 |= 1; // Ensure that seed2 is odd, as required by pcg library.
    pcg32_srandom_r(&(state.rand_state), seed1, seed2);

    const char *language_name = args[3];

    if (sscanf(args[4], "%lf", &(state.learning_rate)) < 1) {
        fprintf(stderr, "Error parsing learning_rate (fifth argument)\n");
        exit(7);
    }
    if (state.learning_rate <= 0) {
        fprintf(stderr, "Bad value for learning_rate (must be > 0)\n");
        exit(8);
    }

    if (sscanf(args[5], "%u", &(state.max_cue)) < 1) {
        fprintf(stderr, "Error parsing max_cue (sixth argument)\n");
        exit(9);
    }
    if (state.max_cue == 0) {
        fprintf(stderr, "max_cue (sixth argument) must be greater than 0\n");
        exit(10);
    }
    if (state.max_cue > MAX_CARDINALITY) {
        fprintf(stderr, "Value of max_cue (sixth argument) is too big.\n");
        exit(11);
    }

    uint_fast64_t num_trials;
    if (sscanf(args[6], "%llu", &num_trials) < 1) {
        fprintf(stderr, "Error parsing number of trials (seventh argument)\n");
        exit(12);
    }

    for (unsigned i = 0; i < MAX_CARDINALITY; ++i) {
        size_t sz = ((num_trials / 8) + 1) * sizeof(uint8_t);
        state.correct_at[i] = malloc(sz);
        memset(state.correct_at[i], 0, sz);
    }

    const char *output_mode_string = args[7];
    if (! strcmp(output_mode_string, "full")) {
        state.output_mode = OUTPUT_MODE_FULL;
    }
    else if (! strcmp(output_mode_string, "summary")) {
        state.output_mode = OUTPUT_MODE_SUMMARY;
    }
    else if (! strcmp(output_mode_string, "range_summary")) {
        state.output_mode = OUTPUT_MODE_RANGE_SUMMARY;
    }
    else {
        fprintf(stderr, "Bad value for output_mode (eighth argument, should be \"summary\" or \"full\")");
        exit(13);
    }

    if (sscanf(args[8], "%u", &(state.quit_after_n_correct)) < 1) {
        fprintf(stderr, "Bad value for quit_after_n_correct (ninth argument)\n");
        exit(14);
    }

    const unsigned DIST_ARGI = 9;

    if (num_args != DIST_ARGI + state.max_cue - 1) {
        fprintf(stderr, "Incorrect number of p values for probability distribution (%u given, %u required)\n", num_args-DIST_ARGI, state.max_cue-1);
        exit(16);
    }
    for (unsigned i = 0; i < 0 + state.max_cue - 1; ++i) {
        double p;
        if (sscanf(args[i+DIST_ARGI], "%lf", &p) < 1) {
            fprintf(stderr, "Error parsing probability value.\n");
            exit(17);
        }
        state.thresholds[i] = (uint32_t)(UINT32_MAX * p);
        if (i > 0) {
            state.thresholds[i] += state.thresholds[i-1];
        }
    }

    // Find the specified language.
    language_t *lang = NULL;
    for (unsigned i = 0; i < 2; ++i) {
        for (language_t *l = languages; l->name[0] != '\0'; ++l) {
            if (! strcmp(l->name, language_name)) {
                lang = l;
                break;
            }
        }
        if (lang)
            break;
        else
            get_languages(language_file_name, languages);
    }
    if (! lang) {
        fprintf(stderr, "Could not find language %s\n", language_name);
        exit(19);
    }
    memcpy(&state.language, lang, sizeof(language_t));

    double *as = (double *)(state.assocs);
    for (unsigned i = 0; i < sizeof(state.assocs)/sizeof(state.assocs[0][0]); ++i)
        as[i] = (((double)pcg32_random_r(&(state.rand_state)))/(double)UINT32_MAX)*1.0;
    double *cas = (double *)state.compound_cue_assocs;
    for (unsigned i = 0; i < sizeof(state.compound_cue_assocs)/sizeof(state.compound_cue_assocs[0][0]); ++i)
        cas[i] = (((double)pcg32_random_r(&(state.rand_state)))/(double)UINT32_MAX)*1.0;
    memset(state.marker_has_been_correct_for_last, 0, sizeof(state.marker_has_been_correct_for_last));
    state.all_markers_have_been_correct_for_last = 0;

    run_trials(&state, num_trials);
}

int main(int argc, char *argv[])
{
    // No need to free this as it is used until process exits.
    char *buf = malloc(ARGS_STRING_MAX_LENGTH * sizeof(char));

    if (argc > 1) {
        run_given_arguments(argc - 1, argv + 1);
        printf("\n");
        fflush(stdout);
    }
    else {
        for (;;) {
            size_t sz = ARGS_STRING_MAX_LENGTH * sizeof(char);
            int bytes_read = getline(&buf, &sz, stdin);
            if (bytes_read < 0) {
                if (feof(stdin)) {
                    exit(0);
                }
                else {
                    fprintf(stderr, "Read error\n");
                    exit(20);
                }
            }
            else if (bytes_read > 0) {
                static char *args[MAX_ARGS];
                unsigned num_args = string_to_arg_array(buf, args);
                run_given_arguments(num_args, args);
                printf("\n");
                fflush(stdout);
            }
        }
    }

    return 0;
}
