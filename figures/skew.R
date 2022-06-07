library(dplyr)
library(ggplot2)
library(ggsci)
library(tidyr)

csv_path <- "./data"
source("utils.R")




df <- p4db_read(csv_file="exp_ycsb_skew.csv") %>%
        mutate(facet_label="YCSB", facet_id=1, hot_prob=ycsb_hot_prob)


p <- df %>%
    filter(metric == 'total_commits') %>%
    ggplot(aes(
        x=hot_prob,
        y=throughput,
        color=type_label,
        shape=type_label
    )) +
    theme(
        legend.position="top",
        legend.margin=margin(0, r=-3, t=-2, b=-3, unit='mm'),
        axis.title.y=element_text(margin=margin(0, r=-0.5, unit='mm')),
        legend.text=element_text(size=10),
        legend.key.size = unit(0.7, "line"),
        legend.spacing.y = unit(0, 'mm'),
    ) +
    guides(color=guide_legend(nrow=2, byrow=TRUE)) +
    geom_line() +
    geom_point() +
    scale_x_continuous("% Hot Txns", labels=function(x) sprintf("%d%%", x)) +
    scale_y_continuous(name="Throughput [txn/sec]", labels=addUnits) +
    scale_color_manual(
        name="",
        values=pal_jco()(10)[c(3,4,1,2)],
        breaks=c("No-Switch (NO_WAIT)", "No-Switch (WAIT_DIE)")
    ) +
    scale_shape_manual(
        name="",
        values=c(15,3,16,17),
        breaks=c("No-Switch (NO_WAIT)", "No-Switch (WAIT_DIE)")
    )

print(p)
ggsave(file="out/exp_skew.pdf", plot=p, device=cairo_pdf, width=210*0.3, height=297*0.21, units="mm")



p <- df %>%
    filter(metric == 'total_commits') %>%
    ungroup() %>%
    group_by(cc_scheme, facet_label, hot_prob) %>% # first x axis then facet variable
    arrange(cc_scheme, use_switch, lm_on_switch, .by_group=T) %>%
    mutate(speedup=throughput/throughput[1]) %>%
    filter(use_switch == 'true' | lm_on_switch=="true") %>%
    ggplot(aes(
        x=hot_prob,
        y=speedup,
        color=type_label,
        shape=type_label
    )) +
    theme(
        plot.title=element_text(hjust = 0.5),
        plot.subtitle=element_text(hjust = 0.5),
        legend.position="top",
        legend.margin=margin(0, l=-14, b=-3, t=-2, unit='mm'), # l=-38
        legend.text=element_text(size=10),
        legend.key.size = unit(0.7, "line"),
        legend.spacing.y = unit(0, 'mm'),
        #legend.spacing.x = unit(0.25, 'mm'),
        axis.title.y=element_text(margin=margin(0, r=-0.5, unit='mm')),
    ) +
    guides(color=guide_legend(nrow=2, byrow=TRUE)) +
    geom_line() +
    geom_point() +
    geom_hline(yintercept=1, linetype="longdash", color = "red", alpha=0.5) +
    scale_x_continuous("% Hot Txns", labels=function(x) sprintf("%d%%", x)) +
    scale_y_continuous(name="Speedup ~ w/o", labels=function(x){
        sprintf("%gX", x)
    }) +
    scale_color_manual(name="", values=pal_jco()(10)[c(1,2,3,4)])+
    scale_shape_manual(name="", values=c(16,17,15,3))
print(p)
ggsave(file="out/exp_skew_A2.pdf", plot=p, device=cairo_pdf, width=210*0.3, height=297*0.21, units="mm")

