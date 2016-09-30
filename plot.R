require("reshape2");
require("ggplot2")

cbbPalette <- c("#000000", "#E69F00", "#56B4E9", "#009E73", "#F0E442", "#0072B2", "#D55E00", "#CC79A7");

seeds <- '\'{"seed1":200, "seed2": 300}\'';

languages <- c(
    "1,o",
    "12,o",
    "123,o",
    "1234,o",
    "2,o",
    "234,o",
    "1,2345,o"
);

gen_data <- function () {
    for (l in languages) {
        cmd <- paste("node sim.js '", l, "' multisim ", seeds, " > ", "'multisim_", l, ".csv'", sep="");
        cat(paste(cmd, "\n", sep=""));
        system(cmd);
    }
}

data <- NULL;
melted <- NULL;

load_data <- function () {
    cols <- Map(function (l) { read.csv(paste("multisim_", l, ".csv", sep="")); }, languages);
    data <<- do.call(cbind, append(list(seq.int(nrow(cols[[1]]))), cols));
    colnames(data) <<- append(c("trial"), languages);
    melted <<- melt(data, id.vars="trial", value.name="fraction", variable.name="language");
}

plot_data <- function () {
    ggplot(data=melted, aes(x=trial, y=fraction, color=language)) + geom_line() +
    xlab("Trial") +
    ylab("Fraction of learners successful") +
    scale_colour_manual(values=cbbPalette);
}