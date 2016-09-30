"use strict";

let child_process = require('child_process');

if (process.argv.length != 4 && process.argv.length != 5) {
    process.stderr.write("Bad arguments\n");
    process.exit(1);
}
let language = process.argv[2];
let program = process.argv[3];
let options = {
    max_cardinality: 7,
    beta: 0.6,
    r: 3,
    learning_rate: 0.01,
    n_distributions: 10000,
    n_runs: 1000,
    quit_after_n_correct: 200
};
if (process.argv.length == 5) {
    try {
        let j = JSON.parse(process.argv[4]);
        for (k in j) {
            if (! ({}).hasOwnProperty.call(j, k))
                continue;
            options[k] = j[k];
        }
    }
    catch (e) {
        process.stderr.write("Bad JSON passed as third argument\n");
        process.exit(1);
    }
}

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
    let total = Math.random(); // The remaining probability mass for n=options.max_cardinality.
    for (let i = 0; i < rd.length; ++i) {
        let r = Math.random();
        total += r;
        rd[i] = r;
    }

    for (let i = 0; i < rd.length; ++i) {
        rd[i] /= total;
    }
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
        let x = parseInt(Math.round(Math.random()*options.max_cardinality)) + 1;
        let v = gamma(options.max_cardinality, 1, x);
        rd[i] = v;
        tot += v;
    }
    for (let i = 0; i < options.max_cardinality-1; ++i)
        rd[i] /= tot;
}

function getInitialArgs(seed1, seed2, mode) {
    if (seed1 == null)
        seed1 = parseInt(Math.random()*Math.pow(2,64));
    if (seed2 == null)
        seed2 = parseInt(Math.random()*Math.pow(2,64));

    return (
        __dirname + "/languages.txt " +
        seed1 + ' ' + seed2 + ' ' +
        language + ' ' +
        options.learning_rate + ' ' +
        options.max_cardinality + ' ' +
        options.n_runs + ' ' +
        mode + ' ' +
        options.quit_after_n_correct + ' '
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

/*
programs.default = function () {
    this.gotItIn = new Array(options.max_cardinality);
    for (let i = 0; i < this.gotItIn.length; ++i) {
        this.gotItIn[i] = { };
    }
    this.gotAllIn = { };

    this.mode = 'summary';

    this.getMaxNumLines = () => {
        return options.n_distributions;
    };

    this.setupDistribution = () => {
        //initRandomDistribution(rd);
        initZtnbDistribution(0.6, 3, rd);
    };

    this.handleLine = (cols, numLines) => {
        if (cols.length != options.max_cardinality + 2)
            return;

        let got_all = true;
        let max = 0;
        for (let i = 0; i < options.max_cardinality; ++i) {
            let v = parseInt(cols[i]);
            if (v != -1) {
                if (this.gotItIn[i][v] === undefined)
                    this.gotItIn[i][v] = 1;
                else
                    ++(this.gotItIn[i][v]);
                
                if (v > max)
                    max = v;
            }
            else {
                got_all = false;
            }
        }
        if (got_all) {
            if (this.gotAllIn[max] === undefined)
                this.gotAllIn[max] = 1;
            else
                ++(this.gotAllIn[max]);
        };
    };

    this.printFinalReport = () => {
        let percentages = new Array(options.max_cardinality+1);
        for (let i = 0; i < options.n_runs; ++i) {
            // Calculate what percentage of learners got it correct by trial n
            // for (a) each individual cardinality and (b) all cardinalities.
            for (let c = 0; c < options.max_cardinality; ++c) {
                let ks = Object.keys(this.gotItIn[c]);
                ks = ks.filter((x) => x <= i);
                let n = ks.reduce((tot, k) => tot + this.gotItIn[c][k], 0);
                percentages[c] = n / options.n_distributions;
            }
            let ks = Object.keys(this.gotAllIn);
            ks = ks.filter((x) => x <= i);
            let n = ks.reduce((tot, k) => tot + this.gotAllIn[k], 0);
            percentages[options.max_cardinality] = n / options.n_distributions;

            console.log(percentages.join(','));
        }
    };
};
*/

programs.xxx = function () {
    this.mode = 'range_summary';

    this.percentages = new Array(options.max_cardinality + 1);
    for (let i = 0; i < this.percentages.length; ++i)
        this.percentages[i] = { };

    this.getMaxNumLines = () => {
        return options.n_distributions;
    };

    this.setupDistribution = () => {
        initZtnbDistribution(0.6, 3, rd);
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
    };

    this.printFinalReport = () => {
        let keys = Object.keys(this.percentages[this.percentages.length-1]);
        keys = keys.sort();
        let start = 0;
        for (let i = 0; i < options.n_distributions; ++i) {
            let total = 0;
            for (let j = start; j < keys.length; ++j) {
                let pr = keys[j].split('-');
                let x = parseInt(pr[0]);
                let y = parseInt(pr[1]);
                if (x > i) {
                    start = j;
                    break;
                }
                if (i <= y) {
                    total += this.percentages[this.percentages.length-1][keys[j]];
                }
            }
            console.log(total / options.n_distributions);
        }

        //console.log(this.percentages);
    };
};

programs.compare = function () {
    this.state = 'random';
    this.ztnbd = new Float64Array(options.max_cardinality-1);
    this.random = new Float64Array(options.max_cardinality-1);
    initZtnbDistribution(0.6, 3, this.ztnbd);

    this.mode = 'summary';

    this.getMaxNumLines = () => {
        return options.n_distributions;
    };

    this.setupDistribution = () => {
        if (this.state == 'random') {
            rd = this.random;
            initRandomDistribution(rd);
            this.state = 'ztnbd';
        }
        else {
            rd = this.ztnbd;
            this.state = 'random';
        }
    };

    this.handleLine = (cols, numLines) => {
        if (this.state == 'random') {
            // Compute distance from ztnbd.
            let tot = 0;
            for (let i = 0; i < this.random.length; ++i) {
                let diff = this.ztnbd[i] - this.random[i];
                tot += diff*diff;
            }
            tot = Math.sqrt(tot);

            // Compute average success rate.
            let succ = 0;
            for (let i = 0; i < options.max_cardinality; ++i) {
                let v = parseFloat(cols[i]);
                if (v != -1)
                    succ += v;
            }
            succ /= options.max_cardinality;

            console.log(tot + ',' + succ);
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
            var left = currentBuffer.substr(1, currentBufferIndex);
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
    let cmd = getInitialArgs(seed1, seed2, mode) + rd.join(' ') + '\n';
    //console.log("--->", cmd);
    numbersim.stdin.write(cmd, 'utf-8');
}
