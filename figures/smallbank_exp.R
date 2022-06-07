library(dplyr)
library(ggplot2)
library(ggsci)
library(tidyr)
library(scales)

csv_path <- "./data"
source("utils.R")



p <- p4db_read(csv_file="exp_f.csv") %>%
    filter(metric == 'total_commits') %>%
    ggplot(aes(
        x=smallbank_remote_prob,
        y=throughput,
        color=type_label,
        shape=type_label
    )) +
    theme(
        legend.position="top",
        legend.margin=margin(0, unit='cm')
    ) +
    geom_line() +
    geom_point() +
    scale_x_continuous("% Distributed Txns", labels=function(x) sprintf("%d%%", x)) +
    scale_y_continuous(name="Throughput [txn/sec]", labels=addUnits) +
    scale_color_jco(name="") +
    scale_shape_discrete(name="") +
    facet_grid(~reorder(facet_label_smallbank, smallbank_hot_size))

print(p)
ggsave(file="out/exp_f.pdf", plot=p, device=cairo_pdf, width=210*0.7, height=297*0.21, units="mm")



p <- p4db_read(csv_file="exp_f.csv") %>%
    filter(metric == 'total_commits') %>%
    ungroup() %>%
    group_by(cc_scheme, facet_label_smallbank, smallbank_remote_prob) %>% # first x axis then facet variable
    arrange(cc_scheme, use_switch, lm_on_switch, .by_group=T) %>%
    mutate(speedup=throughput/throughput[1]) %>%
    filter(use_switch == 'true' | lm_on_switch=="true") %>%
    ggplot(aes(
        x=smallbank_remote_prob,
        y=speedup,
        color=type_label,
        shape=type_label
    )) +
    theme(
        legend.position="top",
        legend.margin=margin(0, b=-3, unit='mm'),
        legend.text=element_text(size=10),
        legend.key.size=unit(0.7, "line"),
        legend.spacing.y=unit(0, 'mm')
    ) +
    geom_line() +
    geom_point() +
    geom_hline(yintercept=1, linetype="longdash", color = "red", alpha=0.5) +
    scale_x_continuous("% Distributed Txns", labels=function(x) sprintf("%d%%", x)) +
    scale_y_continuous(name="Speedup ~ No-Switch", labels=function(x){
        sprintf("%gX", x)
    }) +
    scale_color_jco(name="") +
    scale_shape_discrete(name="") +
    facet_grid(~reorder(facet_label_smallbank, smallbank_hot_size))

print(p)
ggsave(file="out/exp_f_A2.pdf", plot=p, device=cairo_pdf, width=210*0.7, height=297*0.20, units="mm")




p <- p4db_read(csv_file="exp_g.csv") %>%
    filter(metric == 'total_commits') %>%
    ggplot(aes(
        x=num_txn_workers,
        y=throughput,
        color=type_label,
        shape=type_label
    )) +
    theme(
        legend.position="top",
        legend.margin=margin(0, unit='cm')
    ) +
    geom_line() +
    geom_point() +
    scale_x_continuous("# Threads/Server") +
    scale_y_continuous(name="Throughput [txn/sec]", labels=addUnits) +
    scale_color_jco(name="") +
    scale_shape_discrete(name="") +
    facet_grid(~reorder(facet_label_smallbank, smallbank_hot_size))

print(p)
ggsave(file="out/exp_g.pdf", plot=p, device=cairo_pdf, width=210*0.7, height=297*0.21, units="mm")


p <- p4db_read(csv_file="exp_g.csv") %>%
    filter(metric == 'total_commits') %>%
    filter(num_txn_workers >= 8) %>%
    ungroup() %>%
    group_by(cc_scheme, facet_label_smallbank, num_txn_workers, smallbank_hot_size) %>% # first x axis then facet variable
    arrange(cc_scheme, use_switch, lm_on_switch, .by_group=T) %>%
    mutate(speedup=throughput/throughput[1]) %>%
    filter(use_switch == 'true' | lm_on_switch=="true") %>%
    ggplot(aes(
        x=num_txn_workers,
        y=speedup,
        color=type_label,
        shape=type_label
    )) +
    theme(
        legend.position="top",
        legend.margin=margin(0, b=-3, unit='mm'),
        legend.text=element_text(size=10),
        legend.key.size=unit(0.7, "line"),
        legend.spacing.y=unit(0, 'mm')
    ) +
    geom_line() +
    geom_point() +
    geom_hline(yintercept=1, linetype="longdash", color = "red", alpha=0.5) +
    scale_x_continuous("# Threads/Server", breaks=pretty_breaks()) +
    scale_y_continuous(name="Speedup ~ No-Switch", labels=function(x){
        sprintf("%gX", x)
    }) +
    scale_color_jco(name="") +
    scale_shape_discrete(name="") +
    facet_grid(~reorder(facet_label_smallbank, smallbank_hot_size))

print(p)
ggsave(file="out/exp_g_A2.pdf", plot=p, device=cairo_pdf, width=210*0.7, height=297*0.20, units="mm")

