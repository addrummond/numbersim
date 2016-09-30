require("reshape2");
require("ggplot2");

cbbPalette <- c("#56B4E9", "#D55E00", "#000000", "#E69F00", "#009E73", "#F0E442", "#0072B2", "#CC79A7");

seeds <- '\'{"seed1":200, "seed2": 300}\'';

make_json <- function (distribution) {
    paste('{"seed1":200,"seed2":300,"distribution":"', distribution, '"}', sep="");
}

languages <- c(
    "1,o",
    "12,o",
    "123,o",
    "1234,o",
    "12345,o",
    "2,o",
    "3,o",
    "4,o",
    "5,o",
    "2,3,o",
    "3,4,o",
    "4,5,o",
    "2,3,4,o",
    "3,4,5,o",
    "1,2,o",
    "1,23,o",
    "1,234,o",
    "1,2345,o"
);
attested <- c(
    TRUE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    TRUE,
    TRUE,
    TRUE,
    TRUE
);
names(attested) <- languages;

gen_data <- function (distribution) {
    for (l in languages) {
        cmd <- paste("node sim.js '", l, "' multisim '", make_json(distribution), "' > ", "'multisim_", l, ".csv'", sep="");
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
    melted$attested <<- ifelse(attested[melted$language], "attested", "unattested");
}

plot_data <- function () {
    ggplot(data=melted, aes(x=trial, y=fraction)) + geom_line(aes(id=language, color=language, linetype=language)) +
    xlab("Trial") +
    ylab("Fraction of learners successful") +
    scale_linetype_manual(values = c(rep("solid", 8), rep("longdash", 8), rep("dotted", 8))) +
    scale_colour_manual(values=c(cbbPalette,cbbPalette,cbbPalette));
}