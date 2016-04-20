"use strict";

let child_process = require('child_process');

const MAX_CARDINALITY = 7;
const BETA = 0.6;
const R = 3;
const LEARNING_RATE = 0.01;
const N_DISTRIBUTIONS = 10000;
const N_RUNS = 500;
const QUIT_AFTER_N_CORRECT = 200;

let rd = new Float64Array(MAX_CARDINALITY); // Declared outside of function to avoid allocation on every run.

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
    for (let i = 0; i < MAX_CARDINALITY; ++i) {
        rd[i] = ztnbd(i+1, beta, r);
    }
    let sum = 0;
    for (let i = 0; i < MAX_CARDINALITY; ++i)
        sum += rd[i];
}

function initRandomDistribution(rd) {
    let total = 0.0;
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

    let tot = 0;
    for (let i = 0; i < MAX_CARDINALITY; ++i) {
        let x = parseInt(Math.round(Math.random()*MAX_CARDINALITY)) + 1;
        let v = gamma(MAX_CARDINALITY, 1, x);
        rd[i] = v;
        tot += v;
    }
    for (let i = 0; i < MAX_CARDINALITY; ++i)
        rd[i] /= tot;
}

function getInitialArgs(seed1, seed2) {
    if (seed1 === undefined)
        seed1 = parseInt(Math.random()*Math.pow(2,64));
    if (seed2 === undefined)
        seed2 = parseInt(Math.random()*Math.pow(2,64));

    return (
        __dirname + "/languages.txt " +
        seed1 + ' ' + seed2 + ' ' +
        process.argv[2] + ' ' +
        BETA + ' ' +
        R + ' ' +
        LEARNING_RATE + ' ' +
        MAX_CARDINALITY + ' ' +
        N_RUNS + ' ' +
        'summary ' +
        QUIT_AFTER_N_CORRECT + ' '
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
programs.default = function () {
    this.numberOfFails = new Uint32Array(MAX_CARDINALITY); // Will be initialized with zeros

    this.setupDistribution = () => {
        initRandomDistribution(rd);
        //initZtnbDistribution(0.6, 3, rd);
    };

    this.handleLine = (cols) => {
        if (cols.length != MAX_CARDINALITY + 2)
            return;

        for (let i = 0; i < MAX_CARDINALITY; ++i)
            this.numberOfFails[i] += (parseInt(cols[i]) == -1 ? 1 : 0);

        ++numRuns;
        if (numRuns < N_DISTRIBUTIONS) {
            doRun(seed1, seed2);
        }
        else {
            printFinalReport();
            process.exit(0);
        }
    };

    this.printFinalReport = () => {
        for (let i = 0; i < MAX_CARDINALITY; ++i) {
            console.log(i+1, ((1-(this.numberOfFails[i]/N_DISTRIBUTIONS))*100) + '%');
        }
    };
};

programs.compare = function () {
    this.state = 'random';
    this.ztnbd = new Float64Array(MAX_CARDINALITY);
    this.random = new Float64Array(MAX_CARDINALITY);
    initZtnbDistribution(0.6, 3, this.ztnbd);

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

    this.handleLine = (cols) => {
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
            for (let i = 0; i < MAX_CARDINALITY; ++i) {
                let v = parseFloat(cols[i]);
                if (v != -1)
                    succ += v;
            }
            succ /= MAX_CARDINALITY;

            console.log(tot + ',' + succ);
        }
    };
};

let program = new programs.default;

let currentBuffer = "";
let currentBufferIndex = 0;
let numRuns = 0;
let seed1, seed2;
let numberOfFails = new Uint32Array(MAX_CARDINALITY); // Will be initialized with zeros
numbersim.stdout.on('data', (data) => {
    currentBuffer += data;

    for (; currentBufferIndex < currentBuffer.length; ++currentBufferIndex) {
        if (currentBuffer.charAt(currentBufferIndex) == '\n') {
            var left = currentBuffer.substr(0, currentBufferIndex);
            var right = currentBuffer.substr(currentBufferIndex+1);
            let cols = left.split(",");
            currentBuffer = right;
            currentBufferIndex = 0;
            seed1 = parseInt(cols[cols.length-2]);
            seed2 = parseInt(cols[cols.length-1]);
            program.handleLine(cols);
            ++numRuns;
            if (numRuns < N_DISTRIBUTIONS) {
                doRun(seed1, seed2);
            }
            else {
                if (program.printFinalReport)
                     program.printFinalReport();
                process.exit(0);
            }
            break;
        }
    }
});

doRun();
function doRun(seed1, seed2) {
    program.setupDistribution();
    let cmd = getInitialArgs(seed1, seed2) + rd.join(' ') + '\n';
    //console.log("-->", cmd);
    numbersim.stdin.write(cmd, 'utf-8');
}
