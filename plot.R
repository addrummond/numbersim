require("reshape2");
require("ggplot2");
require("parallel");
require("foreach");
require("doParallel")

cbbPalette <- c("#56B4E9", "#D55E00", "#000000", "#E69F00", "#009E73", "#F0E442", "#0072B2", "#CC79A7");

languages <- c();
pretty_languages <- c();
attested <- c();

read_languages <- function () {
    langfile <- "languages.txt";
    fh <- file(langfile, open="r")
    i <- 1;
    while (length(line <- readLines(fh, n = 1, warn = FALSE)) > 0) {
        v = strsplit(line, " ");
        if (length(v) > 0 && length(v[[1]]) > 0) {
            l <- v[[1]][1];
            attested[i] <<- startsWith(l, "a_");
            languages[i] <<- l;
            pretty_languages[i] <<- gsub("^a_", "", l);
            i <- i + 1;
        }
    }
    close(fh);
}

gen_data <- function (distribution) {
    read_languages();

    cores <- detectCores() - 1;
    cl <- makeCluster(cores,outfile="");
    registerDoParallel(cl, cores=cores);

    foreach(i = 1:length(languages), .export=c("languages"), .combine=rbind) %dopar% {
        make_json <- function (distribution) {
            paste('{"seed1":2000,"seed2":3000,"distribution":"', distribution, '"}', sep="");
        }

        l <- languages[i];
        cmd <- paste("node sim.js '", l, "' multisim '", make_json(distribution), "' > ", "'multisim_", l, ".csv'", sep="");
        cat(paste(cmd, "\n", sep=""));
        system(cmd);
    }

    stopCluster(cl);
}

data <- NULL;
melted <- NULL;

load_data <- function () {
    read_languages();

    cols <- Map(function (l) { read.csv(paste("multisim_", l, ".csv", sep="")); }, languages);
    data <<- do.call(cbind, append(list(seq.int(nrow(cols[[1]]))), cols));
    colnames(data) <<- append(c("trial"), pretty_languages);
    melted <<- melt(data, id.vars="trial", value.name="fraction", variable.name="language");
    melted$attested <<- ifelse(attested[melted$language], "attested", "unattested");
}

plot_data <- function () {
    ggplot(data=melted, aes(x=trial, y=fraction)) + geom_line(aes(id=language, color=language, linetype=language)) +
    xlab("Trial") +
    ylab("Fraction of learners successful") +
    xlim(0,500) +
    scale_linetype_manual(values = c(rep("solid", 8), rep("longdash", 8), rep("dotted", 8))) +
    scale_colour_manual(values=c(cbbPalette,cbbPalette,cbbPalette));
}