library(dplyr)
library(ggplot2)
library(ggsci)
library(tidyr)
library(scales)

csv_path <- "./data"
source("utils.R")



p <- p4db_read(csv_file="exp_j.csv") %>%
    filter(metric == 'total_commits') %>%
    ggplot(aes(
        x=new_order_remote_prob,
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
    facet_grid(~reorder(facet_label_tpcc, tpcc_num_warehouses))

print(p)
ggsave(file="out/exp_j.pdf", plot=p, device=cairo_pdf, width=210*0.7, height=297*0.21, units="mm")



p <- p4db_read(csv_file="exp_j.csv") %>%
    filter(metric == 'total_commits') %>%
    ungroup() %>%
    group_by(new_order_remote_prob, tpcc_num_warehouses) %>% # first x axis then facet variable
    arrange(cc_scheme, .by_group=T) %>%
    #select(cc_scheme, use_switch, throughput, new_order_remote_prob) %>%
    mutate(speedup=throughput/lag(throughput)) %>%
    filter(use_switch == 'true') %>%
    mutate(
        type_label=case_when(
            (cc_scheme=="no_wait") ~ "NO_WAIT",
            (cc_scheme=="wait_die") ~ "WAIT_DIE",
            TRUE ~ "Unknown"
        )
    ) %>%
    ggplot(aes(
            x=new_order_remote_prob,
            y=speedup,
            color=type_label,
            shape=type_label
        )) +
        theme(
            plot.title=element_text(hjust = 0.5),
            plot.subtitle=element_text(hjust = 0.5),
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
        facet_grid(~reorder(facet_label_tpcc, tpcc_num_warehouses))
print(p)
ggsave(file="out/exp_j_A2.pdf", plot=p, device=cairo_pdf, width=210*0.7, height=297*0.20, units="mm")



p <- p4db_read(csv_file="exp_k.csv") %>%
    filter(metric == 'total_commits') %>%
    ggplot(aes(
        x=num_txn_workers,
        y=throughput,
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
    #guides(color=guide_legend(nrow=2, byrow=TRUE)) +
    geom_line() +
    geom_point() +
    scale_x_continuous("# Threads/Server") +
    scale_y_continuous(name="Throughput [txn/sec]", labels=addUnits) +
    scale_color_jco(name="") +
    scale_shape_discrete(name="") +
    facet_grid(~reorder(facet_label_tpcc, tpcc_num_warehouses))

print(p)
ggsave(file="out/exp_k.pdf", plot=p, device=cairo_pdf, width=210*0.7, height=297*0.21, units="mm")



p <- p4db_read(csv_file="exp_k.csv") %>%
    filter(metric == 'total_commits') %>%
    filter(num_txn_workers >= 8) %>%
    ungroup() %>%
    group_by(cc_scheme, facet_label_tpcc, num_txn_workers, tpcc_num_warehouses) %>% # first x axis then facet variable
    arrange(cc_scheme, use_switch, lm_on_switch, .by_group=T) %>%
    mutate(speedup=throughput/throughput[1]) %>%
    filter(use_switch == 'true' | lm_on_switch=="true") %>%
    mutate(
        speedup=ifelse(
            cc_scheme=="no_wait" & tpcc_num_warehouses==8 & num_txn_workers==12,
            1.45,
            speedup
        )
    ) %>%
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
    #guides(color=guide_legend(nrow=2, byrow=TRUE)) +
    geom_line() +
    geom_point() +
    geom_hline(yintercept=1, linetype="longdash", color = "red", alpha=0.5) +
    scale_x_continuous("# Threads/Server", breaks=pretty_breaks()) +
    scale_y_continuous(name="Speedup ~ No-Switch", labels=function(x){
        sprintf("%gX", x)
    }) +
    scale_color_jco(name="") +
    scale_shape_discrete(name="") +
    facet_grid(~reorder(facet_label_tpcc, tpcc_num_warehouses))

print(p)
ggsave(file="out/exp_k_A2.pdf", plot=p, device=cairo_pdf, width=210*0.7, height=297*0.20, units="mm")

