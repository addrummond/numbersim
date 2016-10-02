This repository contains code for running the simulations described in 

[Malouf, R., F. Ackerman & S. Seyfarth. 2015. Explaining the Number Hierarchy. Proceedings of the 37th Annual Cognitive Science Society Meeting, 1500-1506.](https://www.researchgate.net/publication/275657562_Explaining_the_Number_Hierarchy)

The core of the simulation is the C program in `csim`. Build as follows:

    cd csim
    make numbersim

Various "languages" (i.e. number systems) are defined and named in the file `languages.txt`.

This program is called by the NodeJS wrapper script sim.js. E.g.:

    node sim.js '1,o' multisim '{"seed1":200,"seed2":300}'

Random numbers are generated using the PCG algorithm.

There is code for producing plots in `plot.R`. 