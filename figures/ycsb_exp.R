library(dplyr)

library(ggplot2)
library(ggsci)
library(tidyr)
library(ggrepel)
library(scales)
# install.packages("remotes")
#remotes::install_github("coolbutuseless/ggpattern")
library(ggpattern)
library(shadowtext)

csv_path <- "./data"
source("utils.R")


p <- p4db_read(csv_file="exp_a.csv") %>%
    filter(metric == 'total_commits') %>%
    ggplot() +
        theme(
            legend.position="top",
            legend.margin=margin(0, unit='cm'),
            axis.title.x=element_blank(),
            axis.text.x=element_blank(),
            axis.ticks.x=element_blank()
        ) +
        geom_col(aes(
            x=use_switch,
            y=throughput,
            fill=type_label
        ), show.legend=T, position=position_dodge()) +
        scale_x_discrete("") +
        scale_y_continuous(name="Throughput [txn/sec]", labels=addUnits) +
        scale_fill_jco(name="") +
        facet_grid(~facet_label_ycsb)

print(p)
ggsave(file="out/exp_a.pdf", plot=p, device=cairo_pdf, width=210*0.7, height=297*0.19, units="mm")

exp_a <- p4db_read(csv_file="exp_a.csv")
get_tp <- function(use_switch_, cc_scheme_, ycsb_write_prob_) {
    (exp_a %>% filter(
        metric == 'total_commits',
        use_switch == use_switch_,
        cc_scheme == cc_scheme_,
        ycsb_write_prob == ycsb_write_prob_
    ))$throughput
}

df <- p4db_read(csv_file="exp_cycles_pp.csv") %>%
    filter(metric == 'total_commits') %>%
    mutate(
        throughput=get_tp(use_switch, cc_scheme, ycsb_write_prob)
    )


df <- rbind(
    df %>% mutate(throughput_type='hot', throughput_=throughput*hot_commit_ratio/100),
    df %>% mutate(throughput_type='cold', throughput_=throughput*(100-hot_commit_ratio)/100)
)


bars = do.call(paste, expand.grid(unique(df$cc_scheme), unique(df$use_switch)))
len = length(bars)
width = 2.0
inner_offsets = ((0:(len-1)) - (len-1)/2) * width

x_breaks = 1:length(unique(df$use_switch))
x_labels = unique(df$use_switch)

get_x <- function (use_switch, x_label) {
    x = which(x_labels == use_switch)
    pos = which(bars == x_label)
    return (x + inner_offsets[[pos]])
}

df <- df %>%
    rowwise() %>%
    mutate(x_label=paste(cc_scheme, use_switch)) %>%
    mutate(
        x_pos=get_x(use_switch, x_label),
        col_alpha=ifelse(throughput_type=="hot", "Hot Txns", "Cold Txns")
    ) %>%
    ungroup() %>%
    mutate(
        info_label=case_when(
            (use_switch=="true"&cc_scheme=="no_wait"&throughput_type=="hot") ~ 'Hot Txns',
            (use_switch=="false"&cc_scheme=="no_wait"&throughput_type=="cold") ~ 'Cold Txns',
            TRUE ~ ""
        ),
        info_y=ifelse(throughput_type=="hot", throughput_ * 0.75, (throughput_+throughput*hot_commit_ratio/100) * 0.93)
    )


p <- df %>%
    ggplot() +
    theme(
        axis.title.x=element_blank(),
        axis.text.x=element_blank(),
        axis.ticks.x=element_blank(),
        axis.title.y=element_text(hjust=-1),
        legend.position="top",
        legend.margin=margin(0, b=-3, unit='mm'),
        legend.text=element_text(size=10),
        legend.key.size=unit(0.7, "line"),
        legend.spacing.y=unit(0, 'mm')
    ) +
    geom_col_pattern(aes(
        x=x_pos,
        y=throughput_,
        fill=type_label,
        alpha=col_alpha,
        pattern=throughput_type
    ), show.legend=T, position="stack", width=width,
        pattern_fill = "black",
        pattern_alpha = 0.3,
        pattern_angle = rep(c(rep(145, 4), rep(35, 4)), 3),
        pattern_density = 0.0001,
        pattern_size = 0.4,
        pattern_spacing = 0.025
    ) +
    geom_shadowtext(aes(
        x=x_pos,
        y=throughput_,
        label=sprintf("%0.2f%%", ifelse(throughput_type=="hot", hot_commit_ratio, 100-hot_commit_ratio))
    ), position=position_stack(vjust=0.5), size=2.50, fontface="plain", angle=20, color="white", bg.colour='black', bg.r=0.15) + # bold
    scale_pattern_manual(
        name="",
        labels = c("hot" = "Hot Txns", "cold" = "Cold Txns"),
        values = c("hot" = "stripe", "cold" = "stripe")
    ) +
    guides(
        fill = guide_legend(override.aes = list(pattern = "none"), nrow=2, byrow=TRUE, order=1),
        pattern = guide_legend(override.aes = list(fill = "white", pattern_angle=c(35, 145)), nrow=2, byrow=TRUE)
    ) +
    scale_alpha_discrete(name="", range=c(0.65, 1), guide='none') +
    scale_x_discrete("") +
    scale_y_continuous(name="Throughput [txn/sec]", labels=addUnits) +
    scale_fill_manual(name="", values=pal_jco()(10)[c(3,4,1,2)]) +
    facet_grid(~facet_label_ycsb)

print(p)
ggsave(file="out/exp_a_A3.pdf", plot=p, device=cairo_pdf, width=210*0.7, height=297*0.19, units="mm")


p <- p4db_read(csv_file="exp_b.csv") %>%
    filter(metric == 'total_commits') %>%
    ggplot(aes(
        x=ycsb_remote_prob,
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
    facet_grid(~facet_label_ycsb)

print(p)
ggsave(file="out/exp_b.pdf", plot=p, device=cairo_pdf, width=210*0.7, height=297*0.21, units="mm")



p <- p4db_read(csv_file="exp_b.csv") %>%
    filter(metric == 'total_commits') %>%
    ungroup() %>%
    group_by(cc_scheme, facet_label_ycsb, ycsb_remote_prob) %>% # first x axis then facet variable
    arrange(cc_scheme, use_switch, lm_on_switch, .by_group=T) %>%
    mutate(speedup=throughput/throughput[1]) %>%
    filter(use_switch == 'true' | lm_on_switch=="true") %>%
    ggplot(aes(
        x=ycsb_remote_prob,
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
    guides(color=guide_legend(nrow=2, byrow=TRUE)) +
    geom_line() +
    geom_point() +
    geom_hline(yintercept=1, linetype="longdash", color = "red", alpha=0.5) +
    scale_x_continuous("% Distributed Txns", labels=function(x) sprintf("%d%%", x)) +
    scale_y_continuous(name="Speedup ~ No-Switch", labels=function(x){
        sprintf("%gX", x)
    }) +
    scale_color_manual(name="", values=pal_jco()(10)[c(3,4,1,2)]) +
    scale_shape_manual(name="", values=c(15,3,16,17)) +
    facet_grid(~facet_label_ycsb)
print(p)
ggsave(file="out/exp_b_A2.pdf", plot=p, device=cairo_pdf, width=210*0.7, height=297*0.21, units="mm")





p <- p4db_read(csv_file="exp_c.csv") %>%
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
    facet_grid(~facet_label_ycsb)

print(p)
ggsave(file="out/exp_c.pdf", plot=p, device=cairo_pdf, width=210*0.7, height=297*0.21, units="mm")



p <- p4db_read(csv_file="exp_c.csv") %>%
    filter(metric == 'total_commits') %>%
    filter(num_txn_workers >= 8) %>%
    ungroup() %>%
    group_by(cc_scheme, facet_label_ycsb, num_txn_workers, ycsb_write_prob) %>% # first x axis then facet variable
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
    guides(color=guide_legend(nrow=2, byrow=TRUE)) +
    geom_line() +
    geom_point() +
    geom_hline(yintercept=1, linetype="longdash", color = "red", alpha=0.5) +
    scale_x_continuous("# Threads/Server", breaks=pretty_breaks()) +
    scale_y_continuous(name="Speedup ~ No-Switch", labels=function(x){
        sprintf("%gX", x)
    }) +
    scale_color_manual(name="", values=pal_jco()(10)[c(3,4,1,2)]) +
    scale_shape_manual(name="", values=c(15,3,16,17)) +
    facet_grid(~facet_label_ycsb)

print(p)
ggsave(file="out/exp_c_A2.pdf", plot=p, device=cairo_pdf, width=210*0.7, height=297*0.21, units="mm")



