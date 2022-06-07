#pragma once

#include "db/database.hpp"
#include "db/defs.hpp"
#include "db/types.hpp"
#include "switch.hpp"
#include "table/table.hpp"


namespace benchmark {
namespace tpcc {


struct TPCCTableInfo {
    template <typename T>
    using Table_t = StructTable<T>;

    // sizeof(char[])+1 because of \0
    struct Warehouse {
        static constexpr auto TABLE_NAME = "warehouse";
        // using PartitionInfo_t = PartitionInfo<PartitionType::REPLICATED>;
        using PartitionInfo_t = PartitionInfo<PartitionType::RANGE>;

        uint64_t w_id;       // W_ID SMALLINT DEFAULT '0' NOT NULL
        char w_name[11];     // W_NAME VARCHAR(16) DEFAULT NULL
        char w_street_1[21]; // W_STREET_1 VARCHAR(32) DEFAULT NULL
        char w_street_2[21]; // W_STREET_2 VARCHAR(32) DEFAULT NULL
        char w_city[21];     // W_CITY VARCHAR(32) DEFAULT NULL
        char w_state[3];     // W_STATE VARCHAR(2) DEFAULT NULL
        char w_zip[10];      // W_ZIP VARCHAR(9) DEFAULT NULL
        int64_t w_tax;       // W_TAX FLOAT DEFAULT NULL   // numeric<4, 3>
        int64_t w_ytd;       // W_YTD FLOAT DEFAULT NULL   // numeric<12, 2>
        // PRIMARY KEY (W_ID)

        static constexpr auto pk(uint64_t w_id) { // maybe return special pk type which has info for partition_info extractable
            return p4db::key_t{w_id};
        }
        constexpr auto pk() const {
            return pk(w_id);
        }

        void print() {}
    };

    struct District {
        static constexpr auto TABLE_NAME = "district";
        // using PartitionInfo_t = PartitionInfo<PartitionType::REPLICATED>;
        using PartitionInfo_t = PartitionInfo<PartitionType::RANGE>;

        uint64_t d_id;        // D_ID TINYINT DEFAULT '0' NOT NULL
        uint64_t d_w_id;      // D_W_ID SMALLINT DEFAULT '0' NOT NULL REFERENCES WAREHOUSE (W_ID)
        char d_name[11];      // D_NAME VARCHAR(16) DEFAULT NULL
        char d_street_1[21];  // D_STREET_1 VARCHAR(32) DEFAULT NULL
        char d_street_2[21];  // D_STREET_2 VARCHAR(32) DEFAULT NULL
        char d_city[21];      // D_CITY VARCHAR(32) DEFAULT NULL
        char d_state[3];      // D_STATE VARCHAR(2) DEFAULT NULL
        char d_zip[10];       // D_ZIP VARCHAR(9) DEFAULT NULL
        int64_t d_tax;        // D_TAX FLOAT DEFAULT NULL
        int64_t d_ytd;        // D_YTD FLOAT DEFAULT NULL
        uint64_t d_next_o_id; // D_NEXT_O_ID INT DEFAULT NULL
        // PRIMARY KEY (D_W_ID, D_ID)
        // D_W_ID Foreign Key, references W_ID

        static constexpr auto pk(uint64_t d_w_id, uint64_t d_id) {
            return p4db::key_t{d_w_id * DISTRICTS_PER_WAREHOUSE + d_id};
        }
        constexpr auto pk() const {
            return pk(d_w_id, d_id);
        }

        void print() {}
    };

    struct Customer {
        static constexpr auto TABLE_NAME = "customer";
        // using PartitionInfo_t = PartitionInfo<PartitionType::REPLICATED>;
        using PartitionInfo_t = PartitionInfo<PartitionType::RANGE>;

        uint64_t c_id;           // C_ID INTEGER DEFAULT '0' NOT NULL
        uint64_t c_d_id;         // C_D_ID TINYINT DEFAULT '0' NOT NULL
        uint64_t c_w_id;         // C_W_ID SMALLINT DEFAULT '0' NOT NULL
        char c_first[17];        // C_FIRST VARCHAR(32) DEFAULT NULL
        char c_middle[3];        // C_MIDDLE VARCHAR(2) DEFAULT NULL
        char c_last[17];         // C_LAST VARCHAR(32) DEFAULT NULL
        char c_street_1[21];     // C_STREET_1 VARCHAR(32) DEFAULT NULL
        char c_street_2[21];     // C_STREET_2 VARCHAR(32) DEFAULT NULL
        char c_city[21];         // C_CITY VARCHAR(32) DEFAULT NULL
        char c_state[3];         // C_STATE VARCHAR(2) DEFAULT NULL
        char c_zip[10];          // C_ZIP VARCHAR(9) DEFAULT NULL
        char c_phone[17];        // C_PHONE VARCHAR(32) DEFAULT NULL
        datetime_t c_since;      // C_SINCE TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL
        char c_credit[3];        // C_CREDIT VARCHAR(2) DEFAULT NULL
        int64_t c_credit_lim;    // C_CREDIT_LIM FLOAT DEFAULT NULL
        int64_t c_discount;      // C_DISCOUNT FLOAT DEFAULT NULL
        int64_t c_balance;       // C_BALANCE FLOAT DEFAULT NULL
        int64_t c_ytd_payment;   // C_YTD_PAYMENT FLOAT DEFAULT NULL
        uint64_t c_payment_cnt;  // C_PAYMENT_CNT INTEGER DEFAULT NULL
        uint64_t c_delivery_cnt; // C_DELIVERY_CNT INTEGER DEFAULT NULL
        char c_data[501];        // C_DATA VARCHAR(500)
        // PRIMARY KEY (C_W_ID, C_D_ID, C_ID)
        // UNIQUE (C_W_ID, C_D_ID, C_LAST, C_FIRST)
        // FOREIGN KEY (C_D_ID, C_W_ID), references (D_ID, D_W_ID)

        static constexpr auto pk(uint64_t c_w_id, uint64_t c_d_id, uint64_t c_id) {
            return p4db::key_t{(c_w_id * DISTRICTS_PER_WAREHOUSE + c_d_id) * CUSTOMER_PER_DISTRICT + c_id};
        }
        constexpr auto pk() const {
            return pk(c_w_id, c_d_id, c_id);
        }

        void print() {}


        // CREATE INDEX IDX_CUSTOMER ON CUSTOMER (C_W_ID,C_D_ID,C_LAST)
    };

    struct Item {
        static constexpr auto TABLE_NAME = "item";
        using PartitionInfo_t = PartitionInfo<PartitionType::REPLICATED>;
        // using PartitionInfo_t = PartitionInfo<PartitionType::RANGE>;

        uint64_t i_id;
        uint64_t i_im_id;
        char i_name[25];
        int64_t i_price;
        char i_data[51];
        // PRIMARY KEY (I_ID)

        static constexpr p4db::key_t pk(uint64_t i_id) {
            return p4db::key_t{i_id};
        }

        void print() {}
    };

    struct Stock {
        static constexpr auto TABLE_NAME = "stock";
        // using PartitionInfo_t = PartitionInfo<PartitionType::REPLICATED>;
        using PartitionInfo_t = PartitionInfo<PartitionType::RANGE>;


        uint64_t s_i_id;                 // S_I_ID INTEGER DEFAULT '0' NOT NULL REFERENCES ITEM (I_ID)
        uint64_t s_w_id;                 // S_W_ID SMALLINT DEFAULT '0 ' NOT NULL REFERENCES WAREHOUSE (W_ID)
        uint64_t s_quantity;             // S_QUANTITY INTEGER DEFAULT '0' NOT NULL
        std::array<char[25], 10> s_dist; // S_DIST_01 ... S_DIST_10 VARCHAR(32) DEFAULT NULL
        uint64_t s_ytd;                  // S_YTD INTEGER DEFAULT NULL
        uint64_t s_order_cnt;            // S_ORDER_CNT INTEGER DEFAULT NULL
        uint64_t s_remote_cnt;           // S_REMOTE_CNT INTEGER DEFAULT NULL
        char s_data[66];                 // S_DATA VARCHAR(64) DEFAULT NULL
        // PRIMARY KEY (S_W_ID,S_I_ID)
        // S_W_ID Foreign Key, references W_ID
        // S_I_ID Foreign Key, references I_ID

        static constexpr p4db::key_t pk(uint64_t s_w_id, uint64_t s_i_id) {
            return p4db::key_t{s_w_id * NUM_ITEMS + s_i_id};
        }

        void print() {}
    };

    struct History {
        static constexpr auto TABLE_NAME = "history";
        using PartitionInfo_t = PartitionInfo<PartitionType::REPLICATED>;

        uint64_t h_c_id;   // H_C_ID INTEGER DEFAULT NULL
        uint64_t h_c_d_id; // H_C_D_ID TINYINT DEFAULT NULL
        uint64_t h_c_w_id; // H_C_W_ID SMALLINT DEFAULT NULL
        uint64_t h_d_id;   // H_D_ID TINYINT DEFAULT NULL
        uint64_t h_w_id;   // H_W_ID SMALLINT DEFAULT '0' NOT NULL
        datetime_t h_date; // H_DATE TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL
        int64_t h_amount;  // H_AMOUNT FLOAT DEFAULT NULL
        char h_data[25];   // H_DATA VARCHAR(32) DEFAULT NULL
        // FOREIGN KEY (H_C_ID, H_C_D_ID, H_C_W_ID) references (C_ID, C_D_ID, C_W_ID)
        // FOREIGN KEY (H_D_ID, H_W_ID) references (D_ID, D_W_ID)

        void print() {}
    };

    struct NewOrder {
        static constexpr auto TABLE_NAME = "new_order";
        using PartitionInfo_t = PartitionInfo<PartitionType::REPLICATED>;

        uint64_t no_o_id; // NO_O_ID INTEGER DEFAULT '0' NOT NULL
        uint64_t no_d_id; // NO_D_ID TINYINT DEFAULT '0' NOT NULL
        uint64_t no_w_id; // NO_W_ID SMALLINT DEFAULT '0' NOT NULL
        // Primary Key: (NO_W_ID, NO_D_ID, NO_O_ID)
        // FOREIGN KEY (NO_O_ID, NO_D_ID, NO_W_ID) REFERENCES (O_ID, O_D_ID, O_W_ID)
        void print() {}
    };

    struct Order {
        static constexpr auto TABLE_NAME = "order";
        using PartitionInfo_t = PartitionInfo<PartitionType::REPLICATED>;


        uint64_t o_id;         // O_ID INTEGER DEFAULT '0' NOT NULL
        uint64_t o_d_id;       // O_D_ID TINYINT DEFAULT '0' NOT NULL
        uint64_t o_w_id;       // O_W_ID SMALLINT DEFAULT '0' NOT NULL
        uint64_t o_c_id;       // O_C_ID INTEGER DEFAULT NULL
        datetime_t o_entry_d;  // O_ENTRY_D TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL
        uint64_t o_carrier_id; // O_CARRIER_ID INTEGER DEFAULT NULL
        uint64_t o_ol_cnt;     // O_OL_CNT INTEGER DEFAULT NULL
        uint64_t o_all_local;  // O_ALL_LOCAL INTEGER DEFAULT NULL
        // PRIMARY KEY (O_W_ID,O_D_ID,O_ID)
        // UNIQUE (O_W_ID,O_D_ID,O_C_ID,O_ID)
        // CONSTRAINT O_FKEY_C FOREIGN KEY (O_D_ID, O_W_ID, O_C_ID) REFERENCES CUSTOMER (C_D_ID, C_W_ID, C_ID)

        void print() {}
        // CREATE INDEX IDX_ORDERS ON ORDERS (O_W_ID,O_D_ID,O_C_ID)
    };

    struct OrderLine {
        static constexpr auto TABLE_NAME = "order_line";
        using PartitionInfo_t = PartitionInfo<PartitionType::REPLICATED>;


        uint64_t ol_o_id;         // OL_O_ID INTEGER DEFAULT '0' NOT NULL
        uint64_t ol_d_id;         // OL_D_ID TINYINT DEFAULT '0' NOT NULL
        uint64_t ol_w_id;         // OL_W_ID SMALLINT DEFAULT '0' NOT NULL
        uint64_t ol_number;       // OL_NUMBER INTEGER DEFAULT '0' NOT NULL
        uint64_t ol_i_id;         // OL_I_ID INTEGER DEFAULT NULL
        uint64_t ol_supply_w_id;  // OL_SUPPLY_W_ID SMALLINT DEFAULT NULL
        datetime_t ol_delivery_d; // OL_DELIVERY_D TIMESTAMP DEFAULT NULL
        uint64_t ol_quantity;     // OL_QUANTITY INTEGER DEFAULT NULL
        int64_t ol_amount;        // OL_AMOUNT FLOAT DEFAULT NULL
        char ol_dist_info[25];    // OL_DIST_INFO VARCHAR(32) DEFAULT NULL
        // PRIMARY KEY (OL_W_ID,OL_D_ID,OL_O_ID,OL_NUMBER)
        // FOREIGN KEY (OL_O_ID, OL_D_ID, OL_W_ID) REFERENCES (O_ID, O_D_ID, O_W_ID
        // FOREIGN KEY (OL_I_ID, OL_SUPPLY_W_ID) REFERENCES (S_I_ID, S_W_ID)

        void print() {}
        // CREATE INDEX IDX_ORDER_LINE_TREE ON ORDER_LINE (OL_W_ID,OL_D_ID,OL_O_ID);
    };


    Table_t<Warehouse>* warehouse;
    Table_t<District>* district;
    Table_t<Item>* item;
    Table_t<Customer>* customer;
    Table_t<History>* history;
    Table_t<Stock>* stock;
    Table_t<Order>* order;
    Table_t<NewOrder>* new_order;
    Table_t<OrderLine>* order_line;

    TPCCSwitchInfo p4_switch;

    using tables = parameter_pack<
        Table_t<Warehouse>, Table_t<District>, Table_t<Item>, Table_t<Customer>,
        Table_t<History>, Table_t<Stock>, Table_t<Order>, Table_t<NewOrder>, Table_t<OrderLine>>;

    void link_tables(Database& db) {
        db.get_casted(Warehouse::TABLE_NAME, warehouse);
        db.get_casted(District::TABLE_NAME, district);
        db.get_casted(Item::TABLE_NAME, item);
        db.get_casted(Customer::TABLE_NAME, customer);
        db.get_casted(History::TABLE_NAME, history);
        db.get_casted(Stock::TABLE_NAME, stock);
        db.get_casted(Order::TABLE_NAME, order);
        db.get_casted(NewOrder::TABLE_NAME, new_order);
        db.get_casted(OrderLine::TABLE_NAME, order_line);
    }
};

} // namespace tpcc
} // namespace benchmark