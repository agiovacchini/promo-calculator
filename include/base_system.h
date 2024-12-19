//
//  base_system.h
//  PromoCalculator
//
//  Created by Andrea Giovacchini on 08/01/14.
//  Copyright (c) 2014 Andrea Giovacchini. All rights reserved.
//

#ifndef __PromoCalculator__BaseSystem__
#define __PromoCalculator__BaseSystem__

#include "base_types.h"
#include <boost/asio.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sstream>

#include <boost/thread/thread.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/regex.hpp>

#include "department.h"
#include "Item.h"
#include "barcodes.h"
#include "promotion.h"
#include "cart.h"
#include "base/archive_map.h"

using boost::asio::ip::tcp;
using boost::property_tree::ptree;
using boost::property_tree::read_json;
using boost::property_tree::write_json;


class BaseSystem {
    string base_path;
    string ini_file_name_name;
    uint32_t node_id;
    src::severity_logger_mt<boost::log::trivial::severity_level> my_logger_bs;
    bool base_system_running;
    bool dummy_rcs;
    bool carts_price_changes_while_shopping;
    bool mainReturnSeparateLinkedBarcode{};
    std::map<string, string> configuration_map;


    ArchiveMap<Department> departments_map;
    ArchiveMap<Item> items_map;
    ArchiveMap<Barcodes> barcodes_map;
    ArchiveMap<Promotion> promotions_map;

    std::map<uint64_t, Cart> carts_map;
    std::map<uint64_t, uint64_t> all_loy_cards_map;
    boost::regex ean13;
    boost::regex ean13_price_req;
    boost::regex upc;
    boost::regex ean8;
    boost::regex loy_card;
    boost::regex loy_card_no_check;
    std::string var_folder_name;
    std::string cart_folder_name;
    uint32_t varCheckDelaySeconds{};
    const std::string archives_dir = "archives/";

public:
    BaseSystem(string p_base_path, string p_ini_file_name);

    string getBasePath() const;

    void setBasePath(string pBasePath);

    long loadConfiguration();

    long setConfigValue(string conf_map_key, string tree_search_key, boost::property_tree::ptree *config_tree);

    void printConfiguration();

    string getConfigValue(string p_param_name);

    void readDepartmentArchive(string p_file_name);

    void dumpDepartmentArchive(string p_file_name);

    void readItemArchive(string p_file_name);

    void dumpItemArchive(string p_file_name);

    void readBarcodesArchive(string p_file_name);

    void dumpBarcodesArchive(string p_file_name);

    void readPromotionsArchive(string p_file_name);

    void dumpPromotionsArchive(string p_file_name);

    void readArchives();

    void dumpArchivesFromMemory();

    void loadCartsInProgress();

    string getCartsList();

    void checkForVariationFiles();

    ItemCodePrice decodeBarcode(uint64_t r_code);

    int64_t getItemPrice(Item *p_item, uint64_t p_barcode, unsigned int p_bcode_type);

    string salesActionsFromWebInterface(int p_action, std::map<std::string, std::string> p_url_params_map);

    uint32_t newCart(unsigned int p_action);

    Cart *getCart(uint32_t p_cart_number);

    static bool persistCarts();

    Item getItemByIntCode(uint64_t p_intcode);

    std::string fromLongToStringWithDecimals(int64_t p_value);
};


#endif /* defined(__PromoCalculator__BaseSystem__) */
