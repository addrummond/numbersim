This repository contains code for running the simulations described in 

[Malouf, R., F. Ackerman & S. Seyfarth. 2015. Explaining the Number Hierarchy. Proceedings of the 37th Annual Cognitive Science Society Meeting, 1500-1506.](http://idiom.ucsd.edu/~sseyfart/MaloufAckermanSeyfarth.pdf)

The core of the simulation is the C program in `csim`. Build as follows:

    cd csim
    make numbersim

Various "languages" (i.e. number systems) are defined and named in the file `languages.txt`.

This program is called by the NodeJS wrapper script sim.js. E.g.:

    node sim.js '1,o' multisim '{"seed1":2000,"seed2":3000}'

Random numbers are generated using the PCG algorithm. Results can therefore
be deterministically reproduced for a given random seed. 

There is code for producing plots in `plot.R`. 