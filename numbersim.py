import csv
from itertools import *
import math
import random

MAX_NUMEROSITY = 7

def powerset_tups(s):
    return (tuple(s[j] for j in range(len(s)) if (i & (1 << j))) for i in range(1 << len(s)))

def ztnbd(k, beta, r):
    """Zero-truncated negative binomial distribution.

       Definition of p^T_k from p. 12 of https://www.soa.org/files/edu/edu-exam-c-tables-disc-dist.pdf
    """

    top = r
    for i in range(1, k):
        top *= (r+i)
    top /= math.factorial(k) * (math.pow(1+beta,r) - 1)
    top *= math.pow(beta/(1+beta), k)
    return top

class State:
    def __init__(self, atomic_cues, markers):
        self.markers = markers
        self.assocs = { }
        self.cue_combos = list(islice(powerset_tups(atomic_cues), 1, None))
        for c in self.cue_combos:
            for m in markers:
                self.assocs[(c,m)] = 0

K = 0.1

def update_state(st, trial):
    cues, marker = trial
    for i in range(len(cues)):
        cue = cues[i]
        others = tuple((x for x in cues if x != i))

        vax = st.assocs[((cue,), marker)] + st.assocs[(others, marker)]
        delta_va = K * (1 - vax)
        delta_vx = K * (1 - vax)

        st.assocs[(cues, marker)] = vax
        st.assocs[((cue,), marker)] += delta_va
        st.assocs[(others, marker)] += delta_vx

def run_trials(st, trials, output_file_name):
    rows = [ [ ':'.join(c) + '--' + m for c in st.cue_combos for m in st.markers ] ]

    def add_row():
        r = [ ]
        for c in st.cue_combos:
            for m in st.markers:
                r.append(st.assocs[(c,m)])
        rows.append(r)

    for t in trials:
        add_row()
        update_state(st, t)
    add_row()

    with open(output_file_name, 'w', encoding='utf-8') as csvfile:
        w = csv.writer(csvfile)
        for r in rows:
            w.writerow(r)

def marker_for_n(language, n):
    if language == 'english':
        return 's' if n == 1 else 'pl'

def gen_trials(language, n):
    ns = [ ]
    for i in range(1, MAX_NUMEROSITY+1):
        ns.append(int(round(ztnbd(i, 0.6, 3) * n)))

    trials = [ ]
    for i in range(1, len(ns)+1):
        ntrials = ns[i-1]
        for k in range(ntrials):
            cues = tuple((str(x) for x in range(1,i+1)))
            marker = marker_for_n(language, i)
            trials.append((cues, marker))

    remainder = n - len(trials)
    if remainder < 0:
        trials = trials[:remainder]
    elif remainder > 0:
        for i in range(remainder):
            num = int(round(random.random() * MAX_NUMEROSITY))
            cues = tuple((str(x) for x in range(1,num+1)))
            marker = marker_for_n(language, num)
            trials.append((cues, marker))

    assert(len(trials) == n)

    return trials

if __name__ == '__main__':
    cues = [str(x) for x in range(1, MAX_NUMEROSITY+1)]
    markers = ['s', 'pl']
    trials = gen_trials('english', 100)
    st = State(cues, markers)
    run_trials(st, trials, 'test.csv')
