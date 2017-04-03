require("reshape2");
require("ggplot2");

cbbPalette <- c("#56B4E9", "#D55E00", "#000000", "#E69F00", "#009E73", "#F0E442", "#0072B2", "#CC79A7");

make_json <- function (distribution) {
    paste('{"seed1":2000,"seed2":3000,"distribution":"', distribution, '"}', sep="");
}

languages <- c(
    "sing:pl",
    "sing:dual:pl",
    "dual:nondual",
    "sing:pauc:pl"
);
attested <- c(
    TRUE,
    TRUE,
    FALSE,
    FALSE
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
    xlim(0,300) +
    scale_linetype_manual(values = c(rep("solid", 8), rep("longdash", 8), rep("dotted", 8))) +
    scale_colour_manual(values=c(cbbPalette,cbbPalette,cbbPalette));
}