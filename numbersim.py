import csv
from itertools import *

def powerset_tups(s):
    return (tuple(s[j] for j in range(len(s)) if (i & (1 << j))) for i in range(1 << len(s)))

class State:
    def __init__(self, atomic_cues, markers):
        self.markers = markers
        self.assocs = { }
        self.cue_combos = list(islice(powerset_tups(atomic_cues), 1, None))
        for c in self.cue_combos:
            for m in markers:
                self.assocs[(c,m)] = 0

def update_state(st, trial):
    cues, marker = trial
    for i in range(len(cues)):
        cue = cues[i]
        others = tuple((x for x in cues if x != i))

        vax = st.assocs[((cue,), marker)] + st.assocs[(others, marker)]
        delta_va = 1 - vax
        delta_vx = 1 - vax

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

CUES = ['c1', 'c2', 'c3']
MARKERS = ['foo', 'bar', 'amp']
TRIALS = [
    (('c1', 'c2'), 'foo'),
    (('c2',), 'bar'),
    (('c3',), 'amp')
]
st = State(CUES, MARKERS)
run_trials(st, TRIALS, 'test.csv')
