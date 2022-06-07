library(dplyr)
library(ggplot2)
library(ggsci)
library(tidyr)

csv_path <- "./data"
source("utils.R")




df_ycsb_ <- rbind(
    p4db_read(csv_file="exp_c.csv") %>% filter(use_switch=="true"),
    p4db_read(csv_file="exp_d.csv")
) %>% filter(ycsb_write_prob == 50) %>% mutate(facet_id=1,facet_label="YCSB A") %>%
    filter(cc_scheme=="no_wait" & metric == 'total_commits') %>%
    mutate(
        ptype=case_when(
            (switch_no_conflict=="true") ~ "Optimal Data Layout",
            (switch_no_conflict=="false") ~ "Worst Data Layout",
            TRUE ~ "Unknown"
        )
    )

df_smallbank_ <- rbind(
    p4db_read(csv_file="exp_g.csv") %>% filter(use_switch=="true"),
    p4db_read(csv_file="exp_h.csv")
) %>% filter(smallbank_hot_size == 5) %>% mutate(facet_id=2,facet_label="SmallBank 8x5 Hot") %>%
    filter(cc_scheme=="no_wait" & metric == 'total_commits') %>%
    mutate(
        ptype=case_when(
            (switch_no_conflict=="true") ~ "Optimal Data Layout",
            (switch_no_conflict=="false") ~ "Worst Data Layout",
            TRUE ~ "Unknown"
        )
    )

df_tpcc_ <- rbind(
    p4db_read(csv_file="exp_k.csv") %>% filter(use_switch=="true"),
    p4db_read(csv_file="exp_l.csv")
) %>% filter(tpcc_num_warehouses == 8) %>% mutate(facet_id=3,facet_label="TPC-C 8WH") %>%
    filter(cc_scheme=="no_wait" & metric == 'total_commits') %>%
    mutate(
        ptype=case_when(
            (switch_no_conflict=="true") ~ "Optimal Data Layout",
            (switch_no_conflict=="false") ~ "Worst Data Layout",
            TRUE ~ "Unknown"
        )
    )

p <- rbind(df_ycsb_, df_smallbank_, df_tpcc_) %>%
    ggplot(aes(
        x=num_txn_workers,
        y=throughput,
        color=ptype,
        shape=ptype
    )) +
    theme(
        legend.position="top",
        legend.margin=margin(0, b=-2, unit='mm')
    ) +
    geom_line() +
    geom_point() +
    scale_x_continuous("# Threads/Server") +
    scale_y_continuous(name="Throughput [txn/sec]", labels=addUnits) +
    scale_color_jco(name="") +
    scale_shape_discrete(name="") +
    facet_wrap(~reorder(facet_label, facet_id), scales="free_y")

print(p)
ggsave(file="out/partitioning_comparison.pdf", plot=p, device=cairo_pdf, width=210*0.7, height=297*0.20, units="mm")


df_ycsb_ <- df_ycsb_ %>% filter(ycsb_write_prob==50)
df_smallbank_ <- df_smallbank_ %>% filter(smallbank_hot_size==5)
df_tpcc_ <- df_tpcc_ %>% filter(tpcc_num_warehouses == 8)


library(ggpubr)

margin <- margin(0, unit='mm')

df_ycsb <- p4db_read(csv_file="exp_partcomp_cycles_pp.csv") %>%
    filter(
        workload=="ycsb",
        cc_scheme=="no_wait",
        metric == 'total_commits'
   ) %>%
    mutate(
        ptype=case_when(
            (switch_no_conflict=="true") ~ "Optimal Data Layout",
            (switch_no_conflict=="false") ~ "Worst Data Layout",
            TRUE ~ "Unknown"
        ),
        facet_id=1,
        facet_label="YCSB A",
        txn_latency=(1/(2.2*1e9)*txn_latency)*1e6
    )

min_txn_latency1 = min(df_ycsb$txn_latency)
coeff_txn_latency1 = max(df_ycsb_$throughput)/(max(df_ycsb$txn_latency)-min_txn_latency1)
df_ycsb$adj_txn_latency = (df_ycsb$txn_latency-min_txn_latency1) * coeff_txn_latency1

p_ycsb <- ggplot() +
    geom_line(df_ycsb_, mapping=aes(x=num_txn_workers, y=throughput, color=ptype)) +
    geom_point(df_ycsb_, mapping=aes(x=num_txn_workers, y=throughput, color=ptype, shape=ptype)) +
    geom_line(df_ycsb, mapping=aes(x=num_txn_workers, y=adj_txn_latency, color=ptype, linetype="dashed", alpha=0.7)) +
    theme(
        legend.position="top",
        axis.title.y.right=element_blank(),
        legend.margin=margin(0, b=0, unit='mm'),
        plot.margin = margin
    ) +
    scale_linetype_manual(
        name="",
        values = c("dashed"),
        labels = c("Transaction Latency")
    ) +
    scale_x_continuous("") +
    scale_y_continuous(
        name="Throughput [txn/sec]",
        labels=addUnits,
        sec.axis = sec_axis(trans=~./coeff_txn_latency1+min_txn_latency1)
    ) +
    scale_color_jco(name="") +
    scale_alpha(guide = 'none') +
    scale_shape_discrete(name="") +
    facet_wrap(~reorder(facet_label, facet_id), scales="free_y")


df_smallbank <- p4db_read(csv_file="exp_partcomp_cycles_pp.csv") %>%
    filter(
        workload=="smallbank",
        cc_scheme=="no_wait",
        metric == 'total_commits'
    ) %>%
    mutate(
        ptype=case_when(
            (switch_no_conflict=="true") ~ "Optimal Data Layout",
            (switch_no_conflict=="false") ~ "Worst Data Layout",
            TRUE ~ "Unknown"
        ),
        facet_id=1,
        facet_label="SmallBank 8x5 Hot",
        txn_latency=(1/(2.2*1e9)*txn_latency)*1e6
    )

min_txn_latency2 = min(df_smallbank$txn_latency)
coeff_txn_latency2 = max(df_smallbank_$throughput)/(max(df_smallbank$txn_latency)-min_txn_latency2)
df_smallbank$adj_txn_latency = (df_smallbank$txn_latency-min_txn_latency2) * coeff_txn_latency2

p_smallbank <- ggplot(df_smallbank, aes(x=num_txn_workers)) +
    geom_line(df_smallbank_, mapping=aes(x=num_txn_workers, y=throughput, color=ptype)) +
    geom_point(df_smallbank_, mapping=aes(x=num_txn_workers, y=throughput, color=ptype, shape=ptype)) +
    geom_line(aes(y=adj_txn_latency, color=ptype, linetype="dashed", alpha=0.7)) +
    theme(
        legend.position="top",
        axis.title.y=element_blank(),
        axis.title.y.right=element_blank(),
        plot.margin = margin
    ) +
    scale_linetype_manual(
        name="",
        values = c("dashed"),
        labels = c("Transaction Latency")
    ) +
    scale_x_continuous("# Threads/Server") +
    scale_y_continuous(
        name="",
        labels=addUnits,
        sec.axis = sec_axis(trans=~./coeff_txn_latency2+min_txn_latency2, name="")
    ) +
    scale_color_jco(name="") +
    scale_alpha(guide = 'none') +
    scale_shape_discrete(name="") +
    facet_wrap(~reorder(facet_label, facet_id), scales="free_y")



df_tpcc <- p4db_read(csv_file="exp_partcomp_cycles_pp.csv") %>%
    filter(
        workload=="tpcc",
        cc_scheme=="no_wait",
        metric == 'total_commits'
    ) %>%
    mutate(
        ptype=case_when(
            (switch_no_conflict=="true") ~ "Optimal Data Layout",
            (switch_no_conflict=="false") ~ "Worst Data Layout",
            TRUE ~ "Unknown"
        ),
        facet_id=1,
        facet_label="TPC-C 8WH",
        txn_latency=(1/(2.2*1e9)*txn_latency)*1e6
    )

min_txn_latency3 = min(df_tpcc$txn_latency)
coeff_txn_latency3 = max(df_tpcc_$throughput)/(max(df_tpcc$txn_latency)-min_txn_latency3)
df_tpcc$adj_txn_latency = (df_tpcc$txn_latency-min_txn_latency3) * coeff_txn_latency3

p_tpcc <- ggplot(df_tpcc, aes(x=num_txn_workers)) +
    geom_line(df_tpcc_, mapping=aes(x=num_txn_workers, y=throughput, color=ptype)) +
    geom_point(df_tpcc_, mapping=aes(x=num_txn_workers, y=throughput, color=ptype, shape=ptype)) +
    geom_line(aes(y=adj_txn_latency, color=ptype, linetype="dashed", alpha=0.7)) +
    theme(
        legend.position="top",
        axis.title.y.left = element_blank(),
        plot.margin = margin
    ) +
    scale_linetype_manual(
        name="",
        values = c("dashed"),
        labels = c("Transaction Latency")
    ) +
    scale_x_continuous("") +
    scale_y_continuous(
        name="",
        labels=addUnits,
        sec.axis = sec_axis(trans=~./coeff_txn_latency3+min_txn_latency3, name="Latency [µs]", labels=function(x) sprintf("%.0f", x))
    ) +
    scale_color_jco(name="") +
    scale_alpha(guide = 'none') +
    scale_shape_discrete(name="") +
    facet_wrap(~reorder(facet_label, facet_id), scales="free_y")




p <- ggarrange(p_ycsb, p_smallbank, p_tpcc, ncol=3, nrow=1, common.legend = TRUE, legend="top")
print(p)

ggsave(file="out/partitioning_comparison_lat.pdf", plot=p, device=cairo_pdf, width=210*0.7, height=297*0.17, units="mm")