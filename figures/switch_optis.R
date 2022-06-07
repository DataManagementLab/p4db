library(dplyr)
library(ggplot2)
library(ggsci)
library(tidyr)

csv_path <- "./data"
source("utils.R")




no_fine_no_fast = (p4db_read(csv_file="exp_ycsb_optis_slow.csv") %>%
        filter(metric=="total_commits"&ycsb_opti_test=="false"&switch_no_conflict=="false"))$throughput
fine_no_fast = (p4db_read(csv_file="exp_ycsb_optis_slow.csv") %>%
        filter(metric=="total_commits"&ycsb_opti_test=="true"&switch_no_conflict=="false"))$throughput


no_fine_fast = (p4db_read(csv_file="exp_ycsb_optis.csv") %>%
        filter(metric=="total_commits"&ycsb_opti_test=="false"&switch_no_conflict=="false"))$throughput
fine_fast = (p4db_read(csv_file="exp_ycsb_optis.csv") %>%
        filter(metric=="total_commits"&ycsb_opti_test=="true"&switch_no_conflict=="false"))$throughput

single_pass = (p4db_read(csv_file="exp_ycsb_optis.csv") %>%
        filter(metric=="total_commits"&ycsb_opti_test=="false"&switch_no_conflict=="true"))$throughput


df <- data.frame(
    x=c("Unoptimized", "+Fast-Recirculate", "+Fine-Locking", "+Declustered"),
    y=c(no_fine_no_fast, no_fine_fast, fine_fast, single_pass)/no_fine_no_fast
)

df$x = factor(df$x, df$x)

p <- ggplot(df) +
    theme(
        legend.position="top",
        legend.margin=margin(0, l=-10, t=-1, b=-3, unit='mm'),
        legend.text=element_text(size=10),
        legend.key.size = unit(0.7, "line"),
        legend.spacing.y = unit(0, 'cm'),
        legend.spacing.x = unit(0.5, 'mm'),
        axis.title.x=element_blank(),
        axis.text.x=element_blank(),
        axis.ticks.x=element_blank(),
        axis.title.y=element_text(margin=margin(0, r=-0.5, unit='mm')),
    ) +
    guides(fill=guide_legend(nrow=2, byrow=TRUE)) +
    geom_col(aes(x=x, y=y, fill=x), show.legend=T) +
    geom_text(aes(x=x, y=y, label=sprintf("%0.2fX", y)), vjust=1.5, size=2.5) +
    scale_x_discrete("") +
    scale_y_continuous("Speedup", labels=function(x){
        sprintf("%gX", x)
    }) +
    scale_fill_jco(name="")


print(p)
ggsave(file="out/exp_m.pdf", plot=p, device=cairo_pdf, width=210*0.3, height=297*0.19, units="mm")