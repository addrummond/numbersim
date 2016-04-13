import csv
from itertools import *
import math
import random
import sys

LANGUAGE = 'english' if len(sys.argv) < 2 else sys.argv[1]
print("Language:", LANGUAGE)
OUTPUT_FILE = 'test.csv' if len(sys.argv) < 3 else sys.argv[2]
print("Output file:", OUTPUT_FILE)

MAX_NUMEROSITY = 7
K = 0.01
BETA=0.6
SIZE=3

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
        self.atomic_cues = atomic_cues
        for cue in self.atomic_cues:
            for m in markers:
                self.assocs[(cue,m)] = 0

def update_state(st, trial):
    tcues, marker = trial

    def upd(m, l):
        vax = 0
        for cue in tcues:
            vax += st.assocs[(cue, m)]

        delta_v = K * ((l*1) - vax)

        for cue in tcues:
            st.assocs[(cue, m)] += delta_v

    for m in st.markers:
        upd(m, 1 if m == marker else 0)

def run_trials(st, trials, output_file_name):
    rows = [ [ 'cues', 'marker' ] + [ '{' + ''.join(st.atomic_cues[:i+1]) + '}-' + m for i in range(len(st.atomic_cues)) for m in st.markers ] ]

    def add_row(t):
        r = [ '{' + ''.join(t[0]) + '}', t[1] ]
        for i in range(len(st.atomic_cues)):
            for m in st.markers:
                r.append(sum(st.assocs[(st.atomic_cues[j],m)] for j in range(i+1)))
        rows.append(r)

    for t in trials:
        add_row(t)
        update_state(st, t)
    add_row(('',''))

    with open(output_file_name, 'w', encoding='utf-8') as csvfile:
        w = csv.writer(csvfile)
        for r in rows:
            w.writerow(r)

def marker_for_n(language, n):
    if language == 'english':
        return 's' if n == 1 else 'pl'
    elif language == 'dnd':
        return 'd' if n == 2 else '!d'

def gen_trials(language, n):
    ns = [ ]
    for i in range(1, MAX_NUMEROSITY+1):
        ns.append(int(round(ztnbd(i, BETA, SIZE) * n)))

    trials = [ ]
    for i in range(1, len(ns)+1):
        ntrials = ns[i-1]
        cues = tuple((str(x) for x in range(1,i+1)))
        assert len(cues) > 0 and len(cues) <= MAX_NUMEROSITY
        for k in range(ntrials):
            marker = marker_for_n(language, i)
            trials.append((cues, marker))

    remainder = n - len(trials)
    if remainder < 0:
        trials = trials[:remainder]
    elif remainder > 0:
        for i in range(remainder):
            num = int(round(random.random() * (MAX_NUMEROSITY-1)))+1
            cues = tuple((str(x) for x in range(1,num+1)))
            assert len(cues) > 0 and len(cues) <= MAX_NUMEROSITY
            marker = marker_for_n(language, num)
            trials.append((cues, marker))

    assert(len(trials) == n)

    random.shuffle(trials)
    return trials

if __name__ == '__main__':
    cues = [str(x) for x in range(1, MAX_NUMEROSITY+1)]
    markers = list(set([marker_for_n(LANGUAGE, n+1) for n in range(MAX_NUMEROSITY)]))
    trials = gen_trials(LANGUAGE, 1000)
    st = State(cues, markers)
    run_trials(st, trials, OUTPUT_FILE)
