library(dplyr)
library(ggplot2)
library(ggsci)
library(tidyr)

csv_path <- "./data"
source("utils.R")

df <- rbind(
    p4db_read(csv_file="exp_a.csv") %>%
        filter(ycsb_write_prob==50 & cc_scheme=="no_wait") %>%
        mutate(facet_label="YCSB", facet_id=1)
    ,
    p4db_read(csv_file="exp_e.csv") %>%
        filter(smallbank_hot_size==5 & cc_scheme=="no_wait") %>%
        mutate(facet_label="SmallBank", facet_id=2)
    ,
    p4db_read(csv_file="exp_i.csv") %>%
        filter(tpcc_num_warehouses==8 & cc_scheme=="no_wait") %>%
        mutate(facet_label="TPC-C", facet_id=3)
) %>%
    mutate(
        color_label=case_when(
            (use_switch=="false") ~ "No-Switch",
            (use_switch=="true") ~ "P4DB",
            TRUE ~ "Unknown"
        ),
        color_id=case_when(
            (use_switch=="false") ~ 0,
            (use_switch=="true") ~ 1,
            TRUE ~ NaN
        )
    )


p <- df %>%
    filter(metric == 'total_commits') %>%
    ggplot() +
    theme(
        axis.title.x=element_blank(),
        axis.text.x=element_blank(),
        axis.ticks.x=element_blank(),
        axis.title.y=element_text(hjust=0),
        legend.position="top",
        legend.margin=margin(0, b=-3, unit='mm'),
        legend.text=element_text(size=10),
        legend.key.size=unit(0.7, "line"),
        legend.spacing.y=unit(0, 'mm')
    ) +
    geom_col(aes(
        x=use_switch,
        y=throughput,
        fill=reorder(color_label, color_id),
    ), show.legend=T, position=position_dodge()) +
    scale_x_discrete("") +
    scale_y_continuous(name="Throughput [txn/sec]", labels=addUnits) +
    scale_fill_jco(name="") +
    facet_wrap(~reorder(facet_label, facet_id), scales="free_y")
print(p)
ggsave(file="out/frontpage.pdf", plot=p, device=cairo_pdf, width=210*0.5, height=297*0.16, units="mm")




p <- df %>%
    filter(metric == 'total_commits') %>%
    ungroup() %>%
    group_by(cc_scheme, facet_label) %>% # first x axis then facet variable
    arrange(cc_scheme, use_switch, lm_on_switch, .by_group=T) %>%
    mutate(speedup=throughput/throughput[1]) %>%
    filter(use_switch == 'true' | lm_on_switch=="true") %>%
    ggplot() +
    theme(
        legend.position="top",
        legend.margin=margin(0, unit='cm')
    ) +
    geom_col(aes(
        x=reorder(facet_label, facet_id),
        y=speedup,
        fill=reorder(facet_label, facet_id)
    ), show.legend=F, position=position_dodge()) +
    geom_hline(yintercept=1, linetype="longdash", color = "red", alpha=0.5) +
    scale_x_discrete("") +
    scale_y_continuous(name="Speedup", labels=function(x){
        sprintf("%gX", x)
    }) +
    scale_fill_manual(name="", values=pal_jco()(10)[4:10])
print(p)
ggsave(file="out/frontpage_A2.pdf", plot=p, device=cairo_pdf, width=210*0.3, height=297*0.19, units="mm")