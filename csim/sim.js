"use strict";

let child_process = require('child_process');

const MAX_CARDINALITY = 7;
const BETA = 0.6;
const R = 3;
const LEARNING_RATE = 0.01;
const N_DISTRIBUTIONS = 100000;
const N_RUNS = 500;
const QUIT_AFTER_N_CORRECT = 200;

let rd = new Array(MAX_CARDINALITY); // Declared outside of function to avoid allocation on every run.
function initRandomDistribution() {
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

function getInitialArgs(seed1, seed2) {
    if (seed1 === undefined)
        seed1 = parseInt(Math.random()*Math.pow(2,64));
    if (seed2 === undefined)
        seed2 = parseInt(Math.random()*Math.pow(2,64));

    return (
        "languages.txt " +
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

let numbersim = child_process.spawn(__dirname + "/numbersim");
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
            handleLine(cols);
            break;
        }
    }

    function handleLine(cols) {
        if (cols.length != MAX_CARDINALITY + 2)
            return;

        for (let i = 0; i < MAX_CARDINALITY; ++i)
            numberOfFails[i] += (parseInt(cols[i]) == -1 ? 1 : 0);

        seed2 = parseInt(cols[cols.length-1]);
        seed1 = parseInt(cols[cols.length-2]);

        ++numRuns;
        if (numRuns < N_DISTRIBUTIONS) {
            doRun(seed1, seed2);
        }
        else {
            printFinalReport();
            process.exit(0);
        }
    }
});

function printFinalReport()
{
    for (let i = 0; i < MAX_CARDINALITY; ++i) {
        console.log(i+1, ((1-(numberOfFails[i]/N_DISTRIBUTIONS))*100) + '%');
    }
}

doRun();
function doRun(seed1, seed2) {
    initRandomDistribution();
    let cmd = getInitialArgs(seed1, seed2) + rd.join(' ') + '\n';
    //console.log("-->", cmd);
    numbersim.stdin.write(cmd, 'utf-8');
}
