library(dplyr)
library(ggplot2)
library(ggsci)
library(tidyr)
library(forcats)
library(ggpubr)

csv_path <- "./data"
source("utils.R")


df <- p4db_read(csv_file="exp_ycsb_overhot.csv") %>%
    filter(metric == 'total_commits') %>%
    filter(ycsb_hot_prob == 75) %>%
    filter(ycsb_write_prob == 50) %>% 
    filter(!(use_switch=="false" & ycsb_hot_size %in% c(120:130))) %>%
    mutate(
        switch_capacity=case_when(
            (use_switch=="false") ~ 0,
            (switch_entries<=1000 & switch_entries<=num_nodes*ycsb_hot_size) ~ 1000/num_nodes,
            (switch_entries<=10000 & switch_entries<=num_nodes*ycsb_hot_size) ~ 10000/num_nodes,
            (switch_entries<=65536 & switch_entries<=num_nodes*ycsb_hot_size) ~ 65536/num_nodes,
            (switch_entries<=655360 & switch_entries<=num_nodes*ycsb_hot_size) ~ 655360/num_nodes,
            TRUE ~ NaN, #sprintf("%d hot, %s", ycsb_hot_size, type_label)
        ),
        type_group=case_when(
            (switch_capacity==0) ~ "No-Switch",
            (switch_capacity==125) ~ "Switch-Capacity: 1K Rows",
            (switch_capacity==1250) ~ "Switch-Capacity: 10K Rows",
            (switch_capacity==8192) ~ "Switch-Capacity: 65K Rows",
            (switch_capacity==81920) ~ "Switch-Capacity: 650K Rows",
            TRUE ~ "Unknown",
        )
    ) %>%
    mutate(
        switch_capacity = switch_capacity*num_nodes,
        ycsb_hot_size = ycsb_hot_size*num_nodes,
    ) %>%
    select(facet_label_ycsb, type_label, num_nodes, ycsb_write_prob, ycsb_hot_size, switch_entries, switch_capacity, type_group, throughput)# %>%
    #print(n=Inf)


df <- df %>%
    mutate(
        alpha_value=case_when(
            switch_capacity >= 819200*8 ~ 0.0,
            T ~ 1.0
        )
    )


df <- df %>%
    filter(ycsb_write_prob==50) %>%
    mutate(
        type_group = fct_reorder(type_group, switch_capacity),
        facet_label_ycsb = fct_reorder(facet_label_ycsb, rev(ycsb_write_prob))
    )



X_BREAKS = c(400, 1001, 1e4, 65536, 655360, 8e6)



p_throughput <- df %>%
    ggplot(aes(
        x=ycsb_hot_size,
        y=throughput,
        color=type_group,
        shape=type_group,
        #alpha=alpha_value
    )) +
    theme(
        legend.position="top",
        #legend.margin=margin(0, b=-3, l=-4, unit='mm'),
        legend.margin=margin(0, b=0, l=0, unit='mm'),
        legend.text=element_text(size=10, margin=margin(l=-2, unit='mm')),
        legend.key.size=unit(0.6, "line"),
        legend.spacing.x=unit(1.5, 'mm'),
        legend.spacing.y=unit(0, 'mm'),
        axis.title.y = element_text(size=10),
        axis.title.x = element_text(size=10)
    ) +
    guides(
        color=guide_legend(nrow=2, byrow=TRUE)
    ) +
    geom_line() +
    geom_point() +
    geom_vline(aes(xintercept=ifelse(alpha_value==1.0, switch_capacity, NA), color=type_group), linetype="longdash", alpha=0.5, show.legend=F) + #
    geom_text(aes(x=ifelse(switch_capacity>0 & alpha_value==1.0, switch_capacity, NA), y=ifelse(switch_capacity==1000, 5e6, 6e6), color=type_group), check_overlap=T, vjust="bottom", nudge_x=-0.1, hjust="right", label="Capacity reached", angle=90, size=2.5, alpha=0.75, show.legend=F) +
    #geom_vline(xintercept=unique(df$switch_capacity),  linetype="longdash", alpha=0.5, show.legend=T) +  # color=pal_jco()(10),
    #scale_x_continuous("Hotset-size") +
    scale_x_continuous("Total Hotset-size [log]", trans='log', labels=addUnits, breaks=X_BREAKS) +
    scale_y_continuous(name="Throughput [txn/sec]", labels=addUnits) +
    scale_color_jco(name="") +
    scale_linetype_manual(
        name="",
        values=c("Capacity reached"="longdash"),
        drop=F
    )+
    scale_alpha(guide = 'none', range = c(0, 1)) +
    guides(linetype = guide_legend(override.aes = list(alpha=1))) +
    scale_shape_discrete(name="")
# print(p_throughput)




df <- df %>%
    ungroup() %>%
    group_by(facet_label_ycsb, ycsb_hot_size) %>% 
    filter(any(type_group=="No-Switch")) %>% # filter out groups without No-Switch for speedup
    arrange(ycsb_hot_size, switch_capacity, .by_group=T) %>%
    mutate(speedup=throughput/throughput[1])

p_speedup <- df %>% filter(type_group != "No-Switch") %>%
    ggplot(aes(
        x=ycsb_hot_size,
        y=speedup,
        color=type_group,
        shape=type_group,
        #alpha=alpha_value
    )) +
    theme(
        #legend.position="top",
        #legend.margin=margin(0, b=-3, l=-4, unit='mm'),
        #legend.text=element_text(size=10, margin=margin(l=-2, unit='mm')),
        #legend.key.size=unit(0.7, "line"),
        #legend.spacing.y=unit(0, 'mm')
        axis.title.y = element_text(size=10),
        axis.title.x = element_text(size=10)
        #axis.title.y.left = element_blank()
    ) +
    guides(
        color=guide_legend(nrow=2, byrow=TRUE)
    ) +
    geom_line() +
    geom_point() +
    geom_vline(aes(xintercept=ifelse(alpha_value==1.0, switch_capacity, NA), color=type_group), linetype="longdash", alpha=0.5, show.legend=F) + #
    geom_text(aes(x=ifelse(switch_capacity>0 & alpha_value==1.0, switch_capacity, NA), y=ifelse(switch_capacity==1000, 1.25, 2.75), color=type_group, vjust=ifelse(switch_capacity==1000, "bottom", "bottom")), nudge_x=-0.1, check_overlap=T, hjust="left", label="Capacity reached", angle=90, size=2.5, alpha=0.75, show.legend=F) +
    geom_hline(yintercept=1, linetype="longdash", color = "red", alpha=0.5) +
    #scale_x_continuous("Hotset-size") +
    scale_x_continuous("Total Hotset-size [log]", trans='log', labels=addUnits, breaks=X_BREAKS) +
    scale_y_continuous(name="Speedup ~ No-Switch", labels=function(x){  #
        sprintf("%gX", x)
    }) + #, limits=c(0, NA)) +
    scale_alpha(guide = 'none', range = c(0, 1)) +
    scale_color_manual(name="", values=pal_jco()(10)[-1]) +
    scale_shape_discrete(name="")
#print(p_speedup)



p <- ggarrange(p_throughput, p_speedup, ncol=2, nrow=1, common.legend = TRUE, legend="top", label.x="Hotset-Size [log]")

file <- "out/ycsb_bighotset.pdf"
ggsave(file=file, plot=p, device=cairo_pdf, width=210*0.7, height=297*0.21, units="mm")
system(sprintf("pdfcrop \"%s\" \"%s\"", file, file), wait=T)


