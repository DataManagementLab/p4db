theme_set(
    theme_bw(
        base_size=12,
    )
)

read_csv <- function(filename) {
    directory = ifelse(exists("csv_path"), csv_path, "./data")
    read.csv(file.path(directory, filename))
}


GROUP_BY_COLS = c(
    "workload", "num_nodes", "num_txn_workers", "cc_scheme", "use_switch", "metric", "switch_no_conflict", "lm_on_switch", "ycsb_opti_test", # default
    "ycsb_hot_prob", "ycsb_hot_size", "ycsb_table_size", "ycsb_write_prob", "ycsb_remote_prob", #ycsb
    "smallbank_remote_prob", "smallbank_table_size", "smallbank_hot_prob", "smallbank_hot_size", #smallbank
    "tpcc_num_warehouses", "new_order_remote_prob" #tpcc
)



p4db_read <- function(df, csv_file) {
    read_csv(csv_file) %>%
        rowwise() %>%
        mutate(
            lm_on_switch = ifelse("lm_on_switch" %in% names(.), lm_on_switch, "false"),
            hot_commit_ratio = ifelse("hot_commit_ratio" %in% names(.), hot_commit_ratio, 0),
            txn_latency = ifelse("txn_latency" %in% names(.), txn_latency, 0),
            ycsb_opti_test = ifelse("ycsb_opti_test" %in% names(.), ycsb_opti_test, "false")
        ) %>%
        gather(key="metric", value="total_sep", total_aborts, total_commits) %>%
        mutate(
            throughput=total_sep/(avg_duration/1e6)
        ) %>%

        group_by_at(c("run", GROUP_BY_COLS)) %>%
        summarise(
            throughput=sum(throughput),
            rate=total_sep/total_txns,
            hot_commit_ratio=mean(hot_commit_ratio),
            txn_latency=mean(txn_latency)
        ) %>%

        group_by_at(GROUP_BY_COLS) %>%
        summarise(
            throughput=max(throughput),
            rate=median(rate),
            hot_commit_ratio=median(hot_commit_ratio),
            txn_latency=median(txn_latency)
        ) %>%

        mutate(
            type_label=case_when(
                (use_switch=="false" & lm_on_switch=="false" & cc_scheme=="no_wait") ~ "No-Switch (NO_WAIT)",
                (use_switch=="false" & lm_on_switch=="false" & cc_scheme=="wait_die") ~ "No-Switch (WAIT_DIE)",
                (use_switch=="true" & lm_on_switch=="false" & cc_scheme=="no_wait") ~ "P4DB (NO_WAIT)",
                (use_switch=="true" & lm_on_switch=="false" & cc_scheme=="wait_die") ~ "P4DB (WAIT_DIE)",
                (use_switch=="false" & lm_on_switch=="true" & cc_scheme=="no_wait") ~ "LM-Switch (NO_WAIT)",
                (use_switch=="false" & lm_on_switch=="true" & cc_scheme=="wait_die") ~ "LM-Switch (WAIT_DIE)",
                TRUE ~ "Unknown"
            ),
            facet_label_ycsb=case_when(
                (ycsb_write_prob==50) ~ "Workload A",
                (ycsb_write_prob==5) ~ "Workload B",
                (ycsb_write_prob==0) ~ "Workload C",
                TRUE ~ "Unknown"
            ),
            facet_label_smallbank=sprintf("%dx%d Hot Customers", num_nodes, smallbank_hot_size),
            facet_label_tpcc=sprintf("%d Warehouses", tpcc_num_warehouses)
        ) %>%
        return
}






# Converts axis to 1k, 84M, 25B, 15T
library(data.table)
addUnits <- function(n) {
    #labels <- case_when(
    #    n < 1e3 ~ paste0(n, ''),         # less than thousands
    #    n < 1e6 ~ paste0(round(n/1e3, 1), 'K'),    # in thousands
    #    n < 1e9 ~ paste0(round(n/1e6, 1), 'M'),    # in millions
    #    n < 1e12 ~ paste0(round(n/1e9, 1), 'B'),   # in billions
    #    n < 1e15 ~ paste0(round(n/1e12, 1), 'T'),  # in trillions
    #    TRUE ~ 'too big!'
    #)


    factors <- case_when(
        n < 1e3 ~ as.double(n),
        n < 1e6 ~ n/1e3,
        n < 1e9 ~ n/1e6,
        n < 1e12 ~ n/1e9,
        n < 1e15 ~ n/1e12,
        TRUE ~ as.double(0)
    )
    units <- case_when(
        n < 1e3 ~ '',
        n < 1e6 ~ 'K',
        n < 1e9 ~ 'M',
        n < 1e12 ~ 'B',
        n < 1e15 ~ 'T',
        TRUE ~ '???'
    )

    digits <- integer(length(n))
    while (TRUE) {
        rounded <- round(factors, digits)
        need_prec <- as.logical((shift(rounded, 1)==rounded) | (shift(rounded, -1) == rounded))
        if (!any(need_prec, na.rm=TRUE)) {
            break
        }
        digits <- digits + ifelse(is.na(need_prec), 0, 1)
    }

    return(paste0(rounded, units))
}
