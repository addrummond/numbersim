"use strict";

let child_process = require('child_process');
let PcgRandom = require('./pcg-random');

if (process.argv.length != 4 && process.argv.length != 5) {
    process.stderr.write("Bad arguments\n");
    process.exit(1);
}
let language = process.argv[2];
let program = process.argv[3];
let options = {
    max_cardinality: 7,
    learning_rate: 0.01,
    n_distributions: 1000,
    n_runs: 1000,
    seed1: parseInt(Math.random()*Math.pow(2,64)),
    seed2: parseInt(Math.random()*Math.pow(2,64)),
    distribution: 'ztnbd'
};
if (process.argv.length == 5) {
    try {
        let j = JSON.parse(process.argv[4]);
        for (let k in j) {
            if (! ({}).hasOwnProperty.call(j, k))
                continue;
            options[k] = j[k];
        }
    }
    catch (e) {
        process.stderr.write("Bad JSON passed as third argument |" + process.argv[4] + "|\n");
        process.exit(1);
    }
}

let rand = new PcgRandom(options.seed1, options.seed2);

let rd = new Float64Array(options.max_cardinality-1); // Declared outside of function to avoid allocation on every run.

function factorial(n) {
    let r = 1;
    while (n > 1)
        r *= n--;
    return r;
}

function ztnbd(k, beta, r)
{
    let top = r;
    for (let i = 1; i < k; ++i)
        top *= r + i;
    top /= factorial(k) * (Math.pow(1.0+beta, r) - 1);
    top *= Math.pow(beta/(1.0+beta), k);
    return top;
}

function initZtnbDistribution(beta, r, rd) {
    for (let i = 0; i < options.max_cardinality-1 && i < rd.length; ++i) {
        rd[i] = ztnbd(i+1, beta, r);
    }
}

function initRandomDistribution(rd) {
    let total = rand.number(); // The remaining probability mass for n=options.max_cardinality.
    for (let i = 0; i < rd.length; ++i) {
        let r = rand.number();
        total += r;
        rd[i] = r;
    }

    for (let i = 0; i < rd.length; ++i) {
        rd[i] /= total;
    }
}

function initDistributionFromOptions(options, rd) {
    if (options.distribution == 'ztnbd')
        initZtnbDistribution(0.6, 3, rd);
    else
        initRandomDistribution(rd);
}
function reinitDistributionFromOptions(options, rd) {
    if (options.distribution == 'random')
        initDistributionFromOptions(options, rd);
}

function initDirichletDistribution(rd)
{
    function g(n) {
        return factorial(n-1);
    }

    function gamma(k, theta, x) {
        return (1 / (g(k) * Math.pow(theta,k))) * Math.pow(x, k - 1) * Math.exp(-(x/theta));
    }

    let tot = gamma(options.max_cardinality, 1, x);
    for (let i = 0; i < options.max_cardinality-1 && i < rd.length; ++i) {
        let x = parseInt(Math.round(rand.number()*options.max_cardinality)) + 1;
        let v = gamma(options.max_cardinality, 1, x);
        rd[i] = v;
        tot += v;
    }
    for (let i = 0; i < options.max_cardinality-1; ++i)
        rd[i] /= tot;
}

function distributionDiff(d1, d2) {
    let tot = 0;
    let d1Ptot = 0, d2Ptot = 0;
    for (let i = 0; i < d1.length; ++i) {
        d1Ptot += d1[i];
        d2Ptot += d2[i];

        tot += d1[i] * Math.log(d1[i]/d2[i]);
    }
    let d1Last = (1 - d1Ptot);
    let d2Last = (1 - d2Ptot);
    tot += d1Last * Math.log(d1Last/d2Last);

    return tot;
}

function getInitialArgs(seed1, seed2, mode, stopAfterNCorrect) {
    return (
        __dirname + "/languages.txt " +
        seed1 + ' ' + seed2 + ' ' +
        language + ' ' +
        options.learning_rate + ' ' +
        options.max_cardinality + ' ' +
        options.n_runs + ' ' +
        mode + ' ' +
        (stopAfterNCorrect || '0') + ' '
    );
}

let numbersim = child_process.spawn(__dirname + "/csim/numbersim");
numbersim.stdout.setEncoding('utf-8');
numbersim.stderr.setEncoding('utf-8');

numbersim.on('error', function (err) {
    console.log("Child process span error", err);
    process.exit(1);
});
numbersim.on('exit', function (code) {
    if (code != 0)
        console.log("Child process exited with error (code", code, ")");
    else
        console.log("Child process exited");
});
numbersim.stdin.on('error', function (err) {
    console.log("Child process stdin errror", err);
    process.exit(1);
});
numbersim.stdout.on('error', function (err) {
    console.log("Child process stdin errror", err);
    process.exit(1);
});
numbersim.stderr.pipe(process.stderr);

let programs = { };

programs.single = function () {
    this.mode = 'full';

    this.getMaxNumLines = () => {
        return options.n_runs;
    };

    this.setupDistribution = () => {
        initZtnbDistribution(0.6, 3, rd);
    };

    this.handleLine = (cols, numRuns) => {
        console.log(cols.join(','));
    };

    this.printFinalReport = () => { };
};

programs.compare = function () {
    this.mode = 'summary';

    this.getQuitAfterN = () => 50;

    this.ztnbd = new Float64Array(options.max_cardinality-1);
    initZtnbDistribution(0.6, 3, this.ztnbd);

    this.diffToN = { };
    
    this.getMaxNumLines = () => {
        return options.n_distributions;
    };

    this.setupDistribution = () => {
        initRandomDistribution(rd);
    };

    this.handleLine = (cols, numLines) => {
        let allCorrectAfter = parseInt(cols[cols.length-3]);
        let diff = distributionDiff(this.ztnbd, rd);
        this.diffToN[diff] = allCorrectAfter;

        initRandomDistribution(rd);
    };

    this.printFinalReport = () => {
        for (let k in this.diffToN) {
            if (! ({ }).hasOwnProperty.call(this.diffToN, k))
                continue;
            
            console.log(k + "," + this.diffToN[k]);
        }
    };
}

programs.multisim = function () {
    this.mode = 'range_summary';    

    this.percentages = new Array(options.max_cardinality + 1);
    for (let i = 0; i < this.percentages.length; ++i)
        this.percentages[i] = { };

    this.getMaxNumLines = () => {
        return options.n_distributions;
    };

    this.setupDistribution = () => {
        initDistributionFromOptions(options, rd);
    };

    this.handleLine = (cols, numLines) => {
        for (let i = 0; i < cols.length - 2; ++i) { // -2 to skip random seeds at end
            let c = cols[i];
            let ranges = c.split(':');
            for (let j = 0; j < ranges.length; ++j) {
                if (this.percentages[i][ranges[j]])
                    ++(this.percentages[i][ranges[j]]);
                else
                    this.percentages[i][ranges[j]] = 1;
            }
        }
        reinitDistributionFromOptions(options, rd);
    };

    this.printFinalReport = () => {
        // Currently we just print percentages for all cardinalities.
        let percentage_all_right = this.percentages[this.percentages.length-1];

        let keys = Object.keys(percentage_all_right);
        keys = keys.sort();
        let start = 0;
        for (let i = 0; i < options.n_runs; ++i) {
            let total = 0;
            let max_y = 0;
            let max_y_j = 0;
            for (let j = start; j < keys.length; ++j) {
                let pr = keys[j].split('-');
                let x = parseInt(pr[0]);
                let y = parseInt(pr[1]);

                if (y > max_y) {
                    max_y = y;
                    max_y_j = j;
                }

                if (x > i) {
                    break;
                }
                if (i <= y) {
                    total += percentage_all_right[keys[j]];
                }
            }
            start = max_y_j;

            console.log(total / options.n_distributions);
        }
    };
};

let progf = programs[program];
if (progf === undefined) {
    process.stderr.write("Bad program name\n");
    process.exit(1);
}
progf = new progf;

let currentBuffer = "";
let currentBufferIndex = 0;
let numLines = 0;
let maxNumLines = progf.getMaxNumLines();
let mode = progf.mode;
let seed1 = null, seed2 = null;
let numberOfFails = new Uint32Array(options.max_cardinality); // Will be initialized with zeros
numbersim.stdout.on('data', (data) => {
    currentBuffer += data;

    for (; currentBufferIndex < currentBuffer.length; ++currentBufferIndex) {
        if (currentBuffer.charAt(currentBufferIndex) == '\n') {
            var left = currentBuffer.substr(0, currentBufferIndex);
            var right = currentBuffer.substr(currentBufferIndex+1);
            currentBuffer = right;
            currentBufferIndex = 0;

            if (left.match(/^\s*$/))
                continue;

            let cols = left.split(",");
            
            if (left.indexOf('->') == -1) {
                seed1 = parseInt(cols[cols.length-2]);
                seed2 = parseInt(cols[cols.length-1]);
            }
            progf.handleLine(cols, numLines);
            ++numLines;
            if (numLines < maxNumLines) {
                doRun(seed1, seed2);
            }
            else {
                if (progf.printFinalReport)
                     progf.printFinalReport();
                process.exit(0);
            }
            break;
        }
    }
});

doRun();
function doRun(seed1, seed2) {
    progf.setupDistribution();
    let quitAfterN = 0;
    if (progf.getQuitAfterN)
        quitAfterN = progf.getQuitAfterN();
    let cmd = getInitialArgs(seed1 ? seed1 : options.seed1, seed2 ? seed2 : options.seed2, mode) + rd.join(' ') + '\n';
    //console.log("--->", cmd);
    numbersim.stdin.write(cmd, 'utf-8');
}
