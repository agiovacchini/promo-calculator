//
//  BaseSystem.cpp
//  PromoCalculator
//
//  Created by Andrea Giovacchini on 08/01/14.
//  Copyright (c) 2014 Andrea Giovacchini. All rights reserved.
//
#include <cstdlib>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <boost/tokenizer.hpp>


#include <boost/spirit/include/qi.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/filesystem.hpp>
#include <utility>

#include "base_types.h"

#include "reply.hpp"
#include "request_handler.hpp"

#include "base_system.h"

static uint32_t next_cart_number;
boost::mutex cartnumber_mutex;
namespace qi = boost::spirit::qi;
namespace fs = boost::filesystem;

qi::rule<std::string::const_iterator, std::string()> quoted_string = '"' >> *(qi::char_ - '"') >> '"';
qi::rule<std::string::const_iterator, std::string()> valid_characters = qi::char_ - '"' - ',';
qi::rule<std::string::const_iterator, std::string()> item = *(quoted_string | valid_characters);
qi::rule<std::string::const_iterator, std::vector<std::string>()> csv_parser = item % ',';

using namespace std;

BaseSystem::BaseSystem(string p_base_path, string p_ini_file_name) {
    using namespace logging::trivial;

    this->base_path = std::move(p_base_path);
    this->ini_file_name_name = std::move(p_ini_file_name);
    this->node_id = 0;
    this->base_system_running = false;
    this->dummy_rcs = false;
    this->carts_price_changes_while_shopping = false;
    this->ean13 = "\\d{13}";
    this->ean13_price_req = "2\\d{12}";
    this->upc = "\\d{12}";
    this->ean8 = "\\d{8}";
    this->loy_card = "260\\d{9}";
    this->loy_card_no_check = "260\\d{8}";

    if (this->loadConfiguration() == 0) {
        this->printConfiguration();

        this->base_system_running = true;

        this->readArchives();

        this->loadCartsInProgress();

        boost::thread newThread(boost::bind(&BaseSystem::checkForVariationFiles, this));

        boost::this_thread::sleep(boost::posix_time::milliseconds(10000));

        BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - System initialized.";
    } else {
        BOOST_LOG_SEV(my_logger_bs, fatal) << "- BS - Bad configuration error, aborting start";
    }
}

string BaseSystem::getConfigValue(string p_param_name) {
    return configuration_map[p_param_name];
}

string BaseSystem::getBasePath() const {
    return this->base_path;
}

void BaseSystem::setBasePath(string pBasePath) {
    this->base_path = std::move(pBasePath);
}

long BaseSystem::loadConfiguration() {
    BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - Config file: " << this->base_path << this->ini_file_name_name;
    BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - Config load start";
    long rc = 0;
    ptree pConfigTree;
    read_ini(this->base_path + this->ini_file_name_name, pConfigTree);


    try {
        rc = rc + setConfigValue("MainStoreChannel", "Main.StoreChannel", &pConfigTree);
        rc = rc + setConfigValue("MainStoreLoyChannel", "Main.StoreLoyChannel", &pConfigTree);
        rc = rc + setConfigValue("MainStoreId", "Main.StoreId", &pConfigTree);
        rc = rc + setConfigValue("MainVarCheckDelaySeconds", "Main.VarCheckDelaySeconds", &pConfigTree);
        rc = rc + setConfigValue("MainReturnSeparateLinkedBarcode", "Main.ReturnSeparateLinkedBarcode", &pConfigTree);
        rc = rc + setConfigValue("MainDummyRCS", "Main.DummyRCS", &pConfigTree);
        rc = rc + setConfigValue("CartsPriceChangesWhileShopping", "Carts.PriceChangesWhileShopping", &pConfigTree);
        rc = rc + setConfigValue("SelfScanScanInDir", "SelfScan.ScanInDir", &pConfigTree);
        rc = rc + setConfigValue("SelfScanScanOutDir", "SelfScan.ScanOutDir", &pConfigTree);
        rc = rc + setConfigValue("LoyCardPrefix", "Loy.CardPrefix", &pConfigTree);
        rc = rc + setConfigValue("LoyMaxCardsPerTransaction", "Loy.MaxCardsPerTransaction", &pConfigTree);
        rc = rc + setConfigValue("LoyOnlyOneShoppingSessionPerCard", "Loy.OnlyOneShoppingSessionPerCard", &pConfigTree);
        rc = rc + setConfigValue("BarcodesType01", "Barcodes.Type", &pConfigTree);
        rc = rc + setConfigValue("NodeId", "Node.Id", &pConfigTree);
        rc = rc + setConfigValue("WebAddress", "Web.Address", &pConfigTree);
        rc = rc + setConfigValue("WebPort", "Web.Port", &pConfigTree);
        rc = rc + setConfigValue("WebThreads", "Web.Threads", &pConfigTree);
        configuration_map["MainArchivesDir"] = configuration_map["MainStoreChannel"] + "/" + configuration_map[
                                                  "MainStoreId"] + "/";

        this->node_id = pConfigTree.get<uint32_t>("Node.Id");

        if (atoi(configuration_map["MainDummyRCS"].c_str()) == 1) {
            this->dummy_rcs = true;
        } else {
            this->dummy_rcs = false;
        }

        if (atoi(configuration_map["CartsPriceChangesWhileShopping"].c_str()) == 1) {
            this->carts_price_changes_while_shopping = true;
        } else {
            this->carts_price_changes_while_shopping = false;
        }

        if (atoi(configuration_map["MainReturnSeparateLinkedBarcode"].c_str()) == 1) {
            this->mainReturnSeparateLinkedBarcode = true;
        } else {
            this->mainReturnSeparateLinkedBarcode = false;
        }
        this->var_folder_name = this->base_path + archives_dir + configuration_map["MainArchivesDir"] + "VARS";
        this->cart_folder_name = this->base_path + archives_dir + configuration_map["MainArchivesDir"] + "CARTS";
        this->varCheckDelaySeconds = atol(configuration_map["MainVarCheckDelaySeconds"].c_str()) * 1000UL;
    } catch (std::exception const &e) {
        BOOST_LOG_SEV(my_logger_bs, lt::fatal) << "- BS - Config error: " << e.what();
        return RC_ERR;
    }
    BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - Config load end";
    return rc;
}

long BaseSystem::setConfigValue(string conf_map_key, string tree_search_key, boost::property_tree::ptree *config_tree) {
    configuration_map[conf_map_key] = config_tree->get<std::string>(tree_search_key);
    return RC_OK;
}

void BaseSystem::printConfiguration() {
    typedef std::map<string, string>::iterator configurationRows;

    BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - Config print start";
    BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - Node Id: " << this->node_id;
    for (configurationRows iterator = configuration_map.begin(); iterator != configuration_map.end(); iterator++) {
        BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - Key: " << iterator->first << ", value: " << iterator->second;
    }
    BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - Config print end";
}

void BaseSystem::readDepartmentArchive(string p_file_name) {
    //http://stackoverflow.com/questions/18365463/how-to-parse-csv-using-boostspirit

    // Tokenizer
    typedef boost::tokenizer<boost::escaped_list_separator<char>, std::string::const_iterator, std::string> Tokenizer;
    boost::escaped_list_separator<char> seps('\\', ',', '\"');
    string archiveFileName = this->base_path + archives_dir + configuration_map["MainArchivesDir"] + p_file_name;
    std::string line;
    std::ifstream archiveFile(archiveFileName);
    bool r = false;
    int column = 0;
    Department tempDepartment;

    if (!archiveFile) {
        BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - File " + archiveFileName + " not found";
        exit(-1);
    } else {
        BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - File " + archiveFileName + " found";
    }

    while (std::getline(archiveFile, line)) {
        // Boost Spirit Qi
        std::string::const_iterator s_begin = line.begin();
        std::string::const_iterator s_end = line.end();
        std::vector<std::string> result;

        r = parse(s_begin, s_end, csv_parser, result);
        assert(r == true);
        assert(s_begin == s_end);
        column = 0;

        for (auto i: result) {
            switch (column) {
                case 0:
                    tempDepartment.setCode(strtoull(i.c_str(), nullptr, 10));
                    break;
                case 1:
                    tempDepartment.setParentCode(strtoull(i.c_str(), nullptr, 10));
                    break;
                case 2:
                    tempDepartment.setDescription(i);
                    break;
                default:
                    break ;
            }
            column++;
        }
        departments_map.addElement(tempDepartment);
    }
    BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - Finished loading file " << p_file_name;
}

void BaseSystem::readItemArchive(string p_file_name) {
    //http://stackoverflow.com/questions/18365463/how-to-parse-csv-using-boostspirit

    // Tokenizer
    typedef boost::tokenizer<boost::escaped_list_separator<char>, std::string::const_iterator, std::string> Tokenizer;
    boost::escaped_list_separator<char> seps('\\', ',', '\"');

    string archiveFileName = this->base_path + archives_dir + configuration_map["MainArchivesDir"] + p_file_name;
    std::ifstream archiveFile(archiveFileName);
    string tmp;
    bool r;
    int column = 0;
    Item tempItm;

    if (!archiveFile) {
        BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - " << "File " + archiveFileName + " not found";
        exit(-1);
    } else {
        BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - " << "File " + archiveFileName + " found";
    }

    std::string line;
    while (std::getline(archiveFile, line)) {
        // Boost Spirit Qi
        std::string::const_iterator s_begin = line.begin();
        std::string::const_iterator s_end = line.end();
        std::vector<std::string> result;

        r = parse(s_begin, s_end, csv_parser, result);
        assert(r == true);
        assert(s_begin == s_end);
        column = 0;

        for (auto i: result) {
            switch (column) {
                case 0:
                    tmp = std::string(i);
                    tempItm.setCode(strtoul(tmp.c_str(), nullptr, 10));
                    break;
                case 1:
                    tempItm.setDescription(i);
                    break;
                case 2:
                    tempItm.setDepartmentCode(strtoull(i.c_str(), nullptr, 10));
                    break;
                case 3:
                    tempItm.setPrice(strtol(i.c_str(), nullptr, 10));
                    break;
                case 4:
                    tempItm.setLinkedBarCode(strtoull(i.c_str(), nullptr, 10));
                    break;
                default:
                    break ;
            }
            column++;
        }
        //itemsMap.insert(std::pair<uint64_t, Item>(tempItm.getCode(), std::move(tempItm)));
        items_map.addElement(tempItm);
    }
    BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - Finished loading file " << p_file_name;
}

void BaseSystem::readBarcodesArchive(string p_file_name) {
    //http://stackoverflow.com/questions/18365463/how-to-parse-csv-using-boostspirit

    ItemCodePrice itmCodePrice;
    // Tokenizer
    typedef boost::tokenizer<boost::escaped_list_separator<char>, std::string::const_iterator, std::string> Tokenizer;
    boost::escaped_list_separator<char> seps('\\', ',', '\"');

    string archiveFileName = this->base_path + archives_dir + configuration_map["MainArchivesDir"] + p_file_name;
    std::ifstream archiveFile(archiveFileName);
    string tmp = "";
    bool r = false;
    int column = 0;
    uint64_t bcdWrk = 0;
    Barcodes tempBarcode;

    if (!archiveFile) {
        BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - File " + archiveFileName + " not found";
        exit(-1);
    } else {
        BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - File " + archiveFileName + " loaded";
    }

    std::string line;
    while (std::getline(archiveFile, line)) {
        // Boost Spirit Qi
        std::string::const_iterator s_begin = line.begin();
        std::string::const_iterator s_end = line.end();
        std::vector<std::string> result;

        r = parse(s_begin, s_end, csv_parser, result);
        assert(r == true);
        assert(s_begin == s_end);
        column = 0;
        for (auto i: result) {
            switch (column) {
                case 0:
                    tmp = std::string(i);
                    bcdWrk = strtoull(tmp.c_str(), nullptr, 10);
                    itmCodePrice = decodeBarcode(bcdWrk);
                    tempBarcode.setCode(itmCodePrice.barcode);
                    break;
                case 1:
                    tempBarcode.setItemCode(strtoull(i.c_str(), nullptr, 10));
                    break;
            }
            column++;
        }
        //barcodesMap.insert(std::pair<uint64_t, Barcodes>(tempBarcode.getCode(), std::move(tempBarcode)));
        barcodes_map.addElement(tempBarcode);
    }
    BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - " << "Finished loading file " << p_file_name;
}


void BaseSystem::readPromotionsArchive(string p_file_name) {
    //http://stackoverflow.com/questions/18365463/how-to-parse-csv-using-boostspirit

    // Tokenizer
    typedef boost::tokenizer<boost::escaped_list_separator<char>, std::string::const_iterator, std::string> Tokenizer;
    boost::escaped_list_separator<char> seps('\\', ',', '\"');

    string archiveFileName = this->base_path + archives_dir + configuration_map["MainArchivesDir"] + p_file_name;
    std::ifstream archiveFile(archiveFileName);
    string tmp = "";
    bool r = false;
    int column = 0;
    Promotion tempPromotion;

    if (!archiveFile) {
        BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - File " + archiveFileName + " not found";
        exit(-1);
    } else {
        BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - File " + archiveFileName + " loaded";
    }

    std::string line;
    while (std::getline(archiveFile, line)) {
        // Boost Spirit Qi
        std::string::const_iterator s_begin = line.begin();
        std::string::const_iterator s_end = line.end();
        std::vector<std::string> result;

        r = parse(s_begin, s_end, csv_parser, result);
        assert(r == true);
        assert(s_begin == s_end);
        column = 0;
        for (auto i: result) {
            switch (column) {
                case 0:
                    tempPromotion.setPromoCode(strtoull(i.c_str(), nullptr, 10));
                    break;
                case 1:
                    tempPromotion.setCode(strtoull(i.c_str(), nullptr, 10));
                    break;
                case 2:
                    tempPromotion.setDiscount(strtoull(i.c_str(), nullptr, 10));
                    break;
                case 3:
                    tempPromotion.setDiscountType(strtoull(i.c_str(), nullptr, 10));
                    break;
                case 4:
                    tempPromotion.setDescription(std::string(i));
                    break;
            }
            column++;
        }
        promotions_map.addElement(tempPromotion);
        cout << tempPromotion.toStr() << endl;
    }
    BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - " << "Finished loading file " << p_file_name;
}


void BaseSystem::readArchives() {
    this->readDepartmentArchive("DEPARTMENTS.CSV");
    this->readItemArchive("ITEMS.CSV");
    this->readBarcodesArchive("BARCODES.CSV");
    this->readPromotionsArchive("PROMOTIONS.CSV");
}

void BaseSystem::dumpArchivesFromMemory() {
    this->departments_map.dumpToFile(
        this->base_path + archives_dir + configuration_map["MainArchivesDir"] + "DEPARTMENTS_DUMP.CSV");
    this->items_map.dumpToFile(this->base_path + archives_dir + configuration_map["MainArchivesDir"] + "ITEMS_DUMP.CSV");
    this->barcodes_map.dumpToFile(
        this->base_path + archives_dir + configuration_map["MainArchivesDir"] + "BARCODES_DUMP.CSV");
    this->promotions_map.dumpToFile(
        this->base_path + archives_dir + configuration_map["MainArchivesDir"] + "PROMOTIONS_DUMP.CSV");
}

ItemCodePrice BaseSystem::decodeBarcode(uint64_t r_code) {
    std::stringstream tempStringStream;
    std::string barcodeWrkStr;
    ItemCodePrice rValues = {0, 0, 0, 0};

    tempStringStream.str(std::string());
    tempStringStream.clear();
    tempStringStream << r_code;

    rValues.type = BCODE_NOT_RECOGNIZED;

    if (regex_match(tempStringStream.str(), loy_card)) {
        rValues.type = BCODE_LOYCARD;
    } else {
        if (regex_match(tempStringStream.str(), loy_card_no_check)) {
            rValues.type = BCODE_LOYCARD;
        } else {
            if (regex_match(tempStringStream.str(), ean13)) {
                if (regex_match(tempStringStream.str(), ean13_price_req)) {
                    rValues.type = BCODE_EAN13_PRICE_REQ;
                } else {
                    rValues.type = BCODE_EAN13;
                }
            } else {
                if (regex_match(tempStringStream.str(), upc)) {
                    rValues.type = BCODE_UPC;
                } else {
                    if (regex_match(tempStringStream.str(), ean8)) {
                        rValues.type = BCODE_EAN8;
                    }
                }
            }
        }
    }

    switch (rValues.type) {
        case BCODE_EAN13_PRICE_REQ:
            barcodeWrkStr = tempStringStream.str().substr(0, 7) + "000000";
            rValues.barcode = atoll(barcodeWrkStr.c_str());
            break;
        default:
            rValues.barcode = r_code;
            break;
    }

    if (barcodes_map[rValues.barcode].getCode() == rValues.barcode) {
        rValues.code = barcodes_map[rValues.barcode].getItemCode();
    }

    return rValues;
}

void BaseSystem::checkForVariationFiles() {
    src::severity_logger_mt<boost::log::trivial::severity_level> my_logger_var;
    std::string var_file_name;
    bool vars_found = false;
    std::string line;
    std::string key;
    std::string value;
    int column = 0;
    bool r = false;
    char row_type = 0, row_action = 0;
    Item itmTemp;

    bool updated_items = false;
    bool updated_depts = false;
    bool updated_vats = false;

    while (base_system_running) {
        vars_found = false;
        BOOST_LOG_SEV(my_logger_var, lt::info) << "- BS - Checking for variation files";
        if (!fs::exists(var_folder_name)) {
            BOOST_LOG_SEV(my_logger_var, lt::info) << "- BS - No " << this->var_folder_name << " subfolder found";
        } else {
            fs::recursive_directory_iterator it(var_folder_name);
            fs::recursive_directory_iterator endit;
            while (it != endit) {
                //std::cout << it->path().stem().string() << " " << it->path().extension() << std::endl ;
                if (is_regular_file(*it) && it->path().extension() == ".VAR") {
                    var_file_name = it->path().filename().string();
                    BOOST_LOG_SEV(my_logger_var, lt::info) << "- BS - Var file found : " << var_file_name;

                    std::ifstream varFileToLoad(var_folder_name + "/" + var_file_name);

                    while (std::getline(varFileToLoad, line)) {
                        std::istringstream is_line(line);

                        std::string::const_iterator s_begin;
                        std::string::const_iterator s_end;
                        std::vector<std::string> result;
                        s_begin = line.begin();
                        s_end = line.end();

                        r = parse(s_begin, s_end, csv_parser, result);
                        assert(r == true);
                        assert(s_begin == s_end);
                        column = 0;

                        for (auto i: result) {
                            if (column == 0) {
                                row_type = i[0];
                            } else {
                                if (column == 1) {
                                    row_action = i[0];
                                } else {
                                    switch (row_type) {
                                        case 'I': {
                                            updated_items = true;
                                            switch (column) {
                                                case 2:
                                                    itmTemp.setCode(strtoull(i.c_str(), nullptr, 10));
                                                    break;
                                                case 3:
                                                    itmTemp.setDescription(i);
                                                    break;
                                                case 4:
                                                    itmTemp.setDepartmentCode(strtoull(i.c_str(), nullptr, 10));
                                                    break;
                                                case 5:
                                                    itmTemp.setPrice(strtol(i.c_str(), nullptr, 10));
                                                    break ;
                                                default:
                                                    break ;
                                            }
                                        }
                                    }
                                }
                            }
                            column++;
                        }
                        switch (row_type) {
                            case 'I': {
                                if (itmTemp.getCode() > 0) {
                                    items_map[itmTemp.getCode()] = itmTemp;
                                }
                                break ;
                            }
                            default: {
                                break;
                            }
                        }
                    }

                    BOOST_LOG_SEV(my_logger_var, lt::info) << "- BS - Renaming Var file " << var_file_name << " in " <<
 var_file_name + ".OLD";
                    fileMove(var_folder_name + "/" + var_file_name, var_folder_name + "/" + var_file_name + ".OLD");
                    vars_found = true;
                }
                ++it;
            }
        }

        if (!vars_found) {
            BOOST_LOG_SEV(my_logger_var, lt::info) << "- BS - No variation files found";
        } else {
            bool updatedBarcodes = false;
            if (updated_items == true) {
                if (fileMove(this->base_path + archives_dir + configuration_map["MainArchivesDir"] + "ITEMS.CSV",
                             this->base_path + archives_dir + configuration_map["MainArchivesDir"] + "ITEMS.OLD")) {
                    BOOST_LOG_SEV(my_logger_var, lt::info) << "- BS - Dumping ITEMS: Rename old file ok";
                    BOOST_LOG_SEV(my_logger_var, lt::info) << "- BS - Dumping ITEMS: Start";
                    this->items_map.dumpToFile(
                        this->base_path + archives_dir + configuration_map["MainArchivesDir"] + "ITEMS.CSV");
                    BOOST_LOG_SEV(my_logger_var, lt::info) << "- BS - Dumping ITEMS: End ";
                } else {
                    BOOST_LOG_SEV(my_logger_var, lt::error) << "- BS - Dumping ITEMS: Rename old file failed";
                }
            }
            if (updatedBarcodes == true) {
                if (fileMove(this->base_path + archives_dir + configuration_map["MainArchivesDir"] + "BARCODES.CSV",
                             this->base_path + archives_dir + configuration_map["MainArchivesDir"] + "BARCODES.OLD")) {
                    BOOST_LOG_SEV(my_logger_var, lt::info) << "- BS - Dumping BARCODES: Rename old file ok";
                    BOOST_LOG_SEV(my_logger_var, lt::info) << "- BS - Dumping BARCODES: Start";
                    this->barcodes_map.dumpToFile(
                        this->base_path + archives_dir + configuration_map["MainArchivesDir"] + "BARCODES.CSV");
                    BOOST_LOG_SEV(my_logger_var, lt::info) << "- BS - Dumping BARCODES: End ";
                } else {
                    BOOST_LOG_SEV(my_logger_var, lt::error) << "- BS - Dumping BARCODES: Rename old file failed";
                }
            }
            if (updated_depts == true) {
                if (fileMove(this->base_path + archives_dir + configuration_map["MainArchivesDir"] + "DEPARTMENTS.CSV",
                             this->base_path + archives_dir + configuration_map["MainArchivesDir"] + "DEPARTMENTS.OLD")) {
                    BOOST_LOG_SEV(my_logger_var, lt::info) << "- BS - Dumping DEPARTMENTS: Rename old file ok";
                    BOOST_LOG_SEV(my_logger_var, lt::info) << "- BS - Dumping DEPARTMENTS: Start";
                    this->departments_map.dumpToFile(
                        this->base_path + archives_dir + configuration_map["MainArchivesDir"] + "DEPARTMENTS.CSV");
                    BOOST_LOG_SEV(my_logger_var, lt::info) << "- BS - Dumping DEPARTMENTS: End ";
                } else {
                    BOOST_LOG_SEV(my_logger_var, lt::error) << "- BS - Dumping DEPARTMENTS: Rename old file failed";
                }
            }
            if (updated_vats == true) {
                //todo
            }
        }
        boost::this_thread::sleep(boost::posix_time::milliseconds(varCheckDelaySeconds));
    }
    std::cout << "Esco\n";
}

void BaseSystem::loadCartsInProgress() {
    std::string key;
    std::string value;
    std::string tmp;
    std::stringstream temp_string_stream;
    std::string barcode_wrk_str;
    bool r = false;

    next_cart_number = 1;

    if (!fs::exists(cart_folder_name)) {
        BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - " << "No " << cart_folder_name << " subfolder found";
        exit(-1);
    }

    if (fs::is_directory(cart_folder_name)) {
        std::map<uint64_t, Cart>::iterator itCarts;
        std::vector<std::string> result;
        std::string::const_iterator s_end;
        std::string::const_iterator s_begin;
        Department temp_department;
        Item temp_itm;
        Cart *my_cart = nullptr;
        int column = 0;
        ItemCodePrice itm_code_price{};
        uint32_t r_qty = 0;
        uint64_t r_code = 0;
        char r_object = ' ';
        char r_action = ' ';
        uint32_t next_cart_number_tmp = 0;
        uint32_t current_tmp_cart_number = 0;
        std::string line;
        BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - " << "CARTS subfolder found";
        fs::recursive_directory_iterator it(cart_folder_name);
        fs::recursive_directory_iterator endit;
        while (it != endit) {
            r_action = ' ';
            r_code = 0;
            r_qty = 0;
            if (fs::is_regular_file(*it) && it->path().extension() == ".transaction_in_progress") {
                current_tmp_cart_number = atol(it->path().stem().string().c_str());

                next_cart_number_tmp = next_cart_number;
                next_cart_number = current_tmp_cart_number;
                BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - " << "==================================";
                BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - " << "File tmpTrans: " << it->path().filename() <<
 " num: " << current_tmp_cart_number << " next: " << next_cart_number;

                newCart(GEN_CART_LOAD);

                next_cart_number = next_cart_number_tmp;

                itCarts = carts_map.find(current_tmp_cart_number);
                if (itCarts != carts_map.end()) {
                    my_cart = &(itCarts->second);
                }
                std::ifstream tmp_transacton_file_to_load(
                    this->base_path + archives_dir + configuration_map["MainArchivesDir"] + "CARTS/" + it->path().
                    filename().string());

                while (std::getline(tmp_transacton_file_to_load, line)) {
                    std::istringstream is_line(line);
                    s_begin = line.begin();
                    s_end = line.end();
                    result.clear();
                    r = parse(s_begin, s_end, csv_parser, result);
                    assert(r == true);
                    assert(s_begin == s_end);
                    column = 0;
                    for (auto i: result) {
                        BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - column: " << column << ",i: " << i;
                        switch (column) {
                            case 0:
                                //timeStamp
                                break;
                            case 1:
                                r_object = i[0];
                                break;
                            case 2:
                                r_action = i[0];
                                break;
                            default:
                                switch (r_object) {
                                    case 'I':
                                    case 'L':
                                        switch (column) {
                                            case 3:
                                                r_code = strtoull(i.c_str(), nullptr, 10);
                                                break;
                                            case 4:
                                                r_qty = atol(i.c_str());
                                                break;
                                            default:
                                                break ;
                                        }
                                        break ;
                                    case 'K':
                                        switch (r_action) {
                                            case 'D':
                                                switch (column) {
                                                    case 3:
                                                        temp_department.setCode(strtoull(i.c_str(), nullptr, 10));
                                                        break;
                                                    case 4:
                                                        temp_department.setParentCode(strtoull(i.c_str(), nullptr, 10));
                                                        break;
                                                    case 5:
                                                        temp_department.setDescription(i);
                                                        break;
                                                }
                                                break;
                                            case 'I':
                                                switch (column) {
                                                    case 3:
                                                        tmp = std::string(i);
                                                        temp_itm.setCode(strtoull(i.c_str(), nullptr, 10));
                                                        break;
                                                    case 4:
                                                        temp_itm.setDescription(i);
                                                        break;
                                                    case 5:
                                                        temp_itm.setDepartmentCode(strtoull(i.c_str(), nullptr, 10));
                                                        break;
                                                    case 6:
                                                        temp_itm.setPrice(strtol(i.c_str(), nullptr, 10));
                                                        break;
                                                    case 7:
                                                        temp_itm.setLinkedBarCode(strtoull(i.c_str(), nullptr, 10));
                                                        break;
                                                    default:
                                                        break;
                                                }
                                                break ;
                                        }
                                        break ;
                                    default:
                                        break ;
                                }
                        }
                        column++;
                    }

                    my_cart->getNextRequestId();
                    switch (r_object) {
                        case 'I':
                            if (r_action == 'A') {
                                BOOST_LOG_SEV(my_logger_bs,
                                              lt::info) << "- BS - Debug recupero riga carrello, IA rcode: " << r_code <<
 " qty:" << r_qty << ", addr: " << &items_map[itm_code_price.code];
                                itm_code_price = decodeBarcode(r_code);
                                itm_code_price.price = my_cart->getItemPrice(
                                    &items_map[itm_code_price.code], r_code, itm_code_price.type,
                                    carts_price_changes_while_shopping);
                                my_cart->addItemByBarcode(items_map[itm_code_price.code], r_code, r_qty, itm_code_price.price,
                                                         0);
                            } else {
                                BOOST_LOG_SEV(my_logger_bs,
                                              lt::info) << "- BS - Debug recupero riga carrello, IV rcode: " << r_code <<
 r_code << " qty:" << r_qty;
                                itm_code_price = decodeBarcode(r_code);
                                itm_code_price.price = my_cart->getItemPrice(
                                    &items_map[itm_code_price.code], r_code, itm_code_price.type,
                                    carts_price_changes_while_shopping);
                                my_cart->removeItemByBarcode(items_map[itm_code_price.code], r_code, itm_code_price.price, 0);
                            }
                            break;
                        case 'L':
                            if (r_action == 'A') {
                                BOOST_LOG_SEV(my_logger_bs,
                                              lt::info) << "- BS - Debug recupero riga carrello, LA rcode: " << r_code;
                                my_cart->addLoyCard(r_code, atoi(configuration_map["LoyMaxCardsPerTransaction"].c_str()));
                                all_loy_cards_map[r_code] = current_tmp_cart_number;
                            } else {
                                BOOST_LOG_SEV(my_logger_bs,
                                              lt::info) << "- BS - Debug recupero riga carrello, LV rcode: " << r_code;
                                my_cart->removeLoyCard(r_code);
                                all_loy_cards_map.erase(r_code);
                            }
                            break;
                        case 'C':
                            if (r_action == 'V') {
                                BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - Debug recupero riga carrello, CV";
                                my_cart->setState(CART_STATE_VOIDED);
                            }
                            break;
                            if (r_action == 'C') {
                                BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - Debug recupero riga carrello, CC";
                                my_cart->setState(CART_STATE_CLOSED);
                            }
                            break;
                        case 'K':
                            switch (r_action) {
                                case 'I':
                                    BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - Debug recupero riga carrello, KI";
                                    if (!carts_price_changes_while_shopping) {
                                        my_cart->updateLocalItemMap(
                                            temp_itm, departments_map[temp_itm.getDepartmentCode()]);
                                    }
                                    break;
                                case 'D':
                                    BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - Debug recupero riga carrello, KD";
                                    break;
                            }
                            break;
                        default:
                            BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - " << "Row action not recognized";
                            break;
                    }
                }

                if ((my_cart->getState() != CART_STATE_VOIDED) && (my_cart->getState() != CART_STATE_CLOSED)) {
                    my_cart->setState(CART_STATE_READY_FOR_ITEM);
                }

                tmp_transacton_file_to_load.close();
                if (current_tmp_cart_number >= next_cart_number) {
                    next_cart_number = current_tmp_cart_number + 1;
                }
            }
            ++it;
        }
    }
}


std::string BaseSystem::fromLongToStringWithDecimals(int64_t p_value) {
    std::ostringstream temp_string_stream, return_stream;
    temp_string_stream.str(std::string());
    temp_string_stream.clear();
    temp_string_stream << p_value;
    BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - PrezziStrani - pValue: " << p_value;
    BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - PrezziStrani - tempStringStream: " << temp_string_stream.str();

    if (temp_string_stream.str().size() < 3) {
        return_stream << "0." << temp_string_stream.str();
    } else {
        return_stream << temp_string_stream.str().substr(0, temp_string_stream.str().size() - 2) << "." << temp_string_stream.
                str().substr(temp_string_stream.str().size() - 2, temp_string_stream.str().size());
    }

    return return_stream.str();
}

int64_t BaseSystem::getItemPrice(Item *p_item, uint64_t p_barcode, unsigned int p_bcode_type) {
    if (p_bcode_type != BCODE_EAN13_PRICE_REQ) {
        return p_item->getPrice();
    } else {
        std::stringstream temp_string_stream;
        temp_string_stream.str(std::string());
        temp_string_stream.clear();
        temp_string_stream << p_barcode;
        std::string barcode_wrk_str = temp_string_stream.str().substr(0, 7) + "000000";
        return atol(temp_string_stream.str().substr(7, 5).c_str());
    }
}

string BaseSystem::salesActionsFromWebInterface(int p_action, std::map<std::string, std::string> p_url_params_map) {
    uint64_t cart_id = 0;
    uint64_t promo_val = 0;
    std::string resp = " ";
    std::ostringstream resp_string_stream;
    std::ostringstream temp_string_stream;
    uint32_t request_id = 0, qty = 0;
    uint64_t barcode = 0;
    std::map<uint64_t, Totals> tmp_totals_map;
    std::map<uint64_t, Cart>::iterator main_iterator;

    ItemCodePrice itm_code_price{};
    long rc = 0;
    resp_string_stream.str(std::string());
    resp_string_stream.clear();

    if (p_action == WEBI_SESSION_INIT) {
        cart_id = this->newCart(GEN_CART_NEW);
        resp_string_stream << R"({"status":0,"deviceReqId":1,"sessionId":)" << cart_id << "}";
        BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - " << "InitResp - Cool";
    } else if (p_action == WEBI_GET_STORE_INFO) {
        resp_string_stream << R"({"status":0,"loyChannel":)" << configuration_map["MainStoreLoyChannel"] << ",\"Channel\":"
                << configuration_map["MainStoreChannel"] << ",\"id\":" << configuration_map["MainStoreId"] << "}";
        BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - " << endl << "WEBI_SESSION_GET_STORE_INFO - Cool - result:" <<
 resp_string_stream.str();
    } else if (p_action == WEBI_GET_CARTS_LIST) {
        resp_string_stream << this->getCartsList();
        BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - WEBI_GET_CARTS_LIST - Cool - result:" << resp_string_stream.
str();
    } else if (p_action == WEBI_ITEM_INFO) {
        if (barcode == 0) {
            barcode = strtoull(p_url_params_map["barcode"].c_str(), nullptr, 10);
        }
        qty = 1;

        itm_code_price = decodeBarcode(barcode);

        itm_code_price.price = getItemPrice(&items_map[itm_code_price.code], barcode, itm_code_price.type);
        if (itm_code_price.type != BCODE_NOT_RECOGNIZED) {
            try {
                if (itm_code_price.code != 0) {
                    resp_string_stream << "{";
                    resp_string_stream << "\"status\":\"0\",\"itemInfo\":{\"itemId\":\"" << barcode <<
                            "\",\"description\":\"" << items_map[itm_code_price.code].getDescription() << "\",\"price\":"
                            << fromLongToStringWithDecimals(itm_code_price.price) << "}";
                    resp_string_stream << "}";
                } else {
                    rc = BCODE_ITEM_NOT_FOUND;
                    if ((rc > 0) && (dummy_rcs)) {
                        rc = 3;
                    }
                    resp_string_stream << "{\"status\":" << rc << "}";
                }
            } catch (std::exception const &e) {
                BOOST_LOG_SEV(my_logger_bs, lt::error) << "- BS - " << "Sales session error: " << e.what();
            }
            BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - " << "WEBI_ITEM_INFO - Cool - rc:" << rc << ", barcode: "
 << barcode << ", qty: " << qty << ",addr: " << &items_map[itm_code_price.code];
        } else {
            rc = BCODE_NOT_RECOGNIZED;
            if (rc > 0 && dummy_rcs) {
                rc = 3;
            }
            resp_string_stream << "{\"status\":" << rc << "}";
        }
    } else if (p_action == WEBI_ACTION_NOT_RECOGNIZED) {
        rc = WEBI_ACTION_NOT_RECOGNIZED;
        if (rc > 0 && dummy_rcs) {
            rc = 3;
        }
        resp_string_stream << "{\"status\":" << rc << "}";
        BOOST_LOG_SEV(my_logger_bs, lt::warning) << "- BS - Web action not recognized :(";
    } else {
        temp_string_stream.str(std::string());
        temp_string_stream.clear();
        temp_string_stream << p_url_params_map["devSessId"];
        std::string strCartId = temp_string_stream.str();
        cart_id = atoll(strCartId.c_str());
        main_iterator = carts_map.find(cart_id);

        if (main_iterator != carts_map.end()) {
            Cart *my_cart = nullptr;
            my_cart = &main_iterator->second;
            request_id = my_cart->getNextRequestId();
            barcode = 0;
            switch (p_action) {
                case WEBI_MANAGE_RESCAN: {
                    bool rescanRequired = false;
                    rc = 0;

                    if (p_url_params_map["required"] == "true") {
                        rescanRequired = true;
                    }

                    my_cart->setRescan(rescanRequired);
                    BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - " << "WEBI_MANAGE_RESCAN - Cool - rc:" << rc <<
 ", required: " << rescanRequired;
                    resp_string_stream << "{\"status\":" << rc << "}";
                    break;
                }
                case WEBI_ITEM_ADD: {
                    if (barcode == 0) {
                        barcode = strtoull(p_url_params_map["barcode"].c_str(), nullptr, 10);
                    }
                    qty = 1;

                    itm_code_price = decodeBarcode(barcode);

                    if (itm_code_price.type != BCODE_NOT_RECOGNIZED) {
                        try {
                            if (itm_code_price.code != 0) {
                                if (!carts_price_changes_while_shopping) {
                                    my_cart->updateLocalItemMap(items_map[itm_code_price.code],
                                                               departments_map[items_map[itm_code_price.code].
                                                                   getDepartmentCode()]);
                                }


                                itm_code_price.price = my_cart->getItemPrice(
                                    &items_map[itm_code_price.code], barcode, itm_code_price.type,
                                    carts_price_changes_while_shopping);

                                resp_string_stream << "{";

                                cout << "Prezzo: " << itm_code_price.price << ", sconto: " << promo_val << ", descr: " <<
                                        promotions_map[itm_code_price.code].getDescription() << std::endl;

                                if (promotions_map[itm_code_price.code].getCode() == itm_code_price.code) {
                                    // cout << "Disc: " << promotionsMap[itmCodePrice.code].toStr() << ", type: " << promotionsMap[itmCodePrice.code].getDiscountType() << std::endl;
                                    switch (promotions_map[itm_code_price.code].getDiscountType()) {
                                        case PROMO_TYPE_DSC_VAL:
                                            promo_val = promotions_map[itm_code_price.code].getDiscount();
                                        // cout << "perc" << endl;
                                            break;
                                        case PROMO_TYPE_DSC_PERC:
                                            promo_val = itm_code_price.price / 100. * promotions_map[itm_code_price.code].
                                                       getDiscount();
                                        // cout << "val" << endl;
                                            break;
                                        case PROMO_TYPE_PRICE_CUT:
                                            promo_val = itm_code_price.price - promotions_map[itm_code_price.code].
                                                       getDiscount();
                                        // cout << "itmCodePrice.price: " << itmCodePrice.price << ", promoVal: " << promoVal << endl;
                                            break;
                                    }
                                }

                                rc = my_cart->addItemByBarcode(items_map[itm_code_price.code], barcode, qty,
                                                              itm_code_price.price, promo_val);


                                if ((rc > 0) && (dummy_rcs)) {
                                    rc = 3;
                                }

                                if ((rc == 0) && (items_map[itm_code_price.code].getLinkedBarCode() > 0)) {
                                    uint64_t barCodeTmp = items_map[itm_code_price.code].getLinkedBarCode();

                                    if (!carts_price_changes_while_shopping) {
                                        my_cart->updateLocalItemMap(items_map[barcodes_map[barCodeTmp].getItemCode()],
                                                                   departments_map[items_map[barcodes_map[barCodeTmp].
                                                                       getItemCode()].getDepartmentCode()]);
                                    }

                                    long rcLinked = my_cart->addItemByBarcode(
                                        items_map[barcodes_map[barCodeTmp].getItemCode()], barCodeTmp, qty,
                                        items_map[barcodes_map[barCodeTmp].getItemCode()].getPrice(), 0);
                                    if ((rcLinked > 0) && (dummy_rcs)) {
                                        rcLinked = 3;
                                    }
                                    if (mainReturnSeparateLinkedBarcode) {
                                        resp_string_stream << "\"addItemResponse\":{\"status\":" << rcLinked <<
                                                ",\"deviceReqId\":" << request_id << ",\"itemId\":\"" << barCodeTmp <<
                                                "\",\"description\":\"" << items_map[barcodes_map[barCodeTmp].
                                                    getItemCode()].getDescription() << "\",\"price\":" <<
                                                fromLongToStringWithDecimals(
                                                    items_map[barcodes_map[barCodeTmp].getItemCode()].getPrice()) <<
                                                ",\"voidFlag\":\"false\",\"quantity\":1,\"itemType\":\"NormalSaleItem\"},";
                                    } else {
                                        itm_code_price.price =
                                                itm_code_price.price + items_map[barcodes_map[barCodeTmp].getItemCode()].
                                                getPrice();
                                    }
                                }
                                resp_string_stream << R"("addItemResponse":{"status":)" << rc << ",\"deviceReqId\":" <<
                                        request_id << R"(,"itemId":")" << barcode << R"(","description":")" << items_map[
                                            itm_code_price.code].getDescription() << R"(","price":)" <<
                                        fromLongToStringWithDecimals(itm_code_price.price) <<
                                        R"(,"voidFlag":"false","quantity":1,"itemType":"NormalSaleItem"})";


                                if (promo_val != 0) {
                                    resp_string_stream << R"(,"promoResponse":{"status":0,"deviceReqId":)" << request_id
                                            << R"(,"promoItemId":")" << promotions_map[itm_code_price.code].getPromoCode()
                                            << R"(","promoDescr":")" << promotions_map[itm_code_price.code].
                                            getDescription() << R"(","promoValue":-)" <<
                                            fromLongToStringWithDecimals(promo_val) <<
                                            R"(,"voidFlag":"false","promoQty":1})";
                                }


                                tmp_totals_map = my_cart->getTotals();

                                resp_string_stream << ",\"getTotalResponse\":{\"status\":" << rc << ",\"deviceReqId\":" <<
                                        request_id << ",\"totalItems\":" << tmp_totals_map[0].items_number <<
                                        ",\"totalAmount\":" << fromLongToStringWithDecimals(tmp_totals_map[0].total_amount)
                                        << ",\"totalDiscounts\":" <<
                                        fromLongToStringWithDecimals(tmp_totals_map[0].total_discount) <<
                                        " ,\"amountToPay\":" << fromLongToStringWithDecimals(
                                            tmp_totals_map[0].total_amount) << "}}";
                            } else {
                                rc = BCODE_ITEM_NOT_FOUND;
                                if (rc > 0 && dummy_rcs) {
                                    rc = 3;
                                }
                                resp_string_stream << "{\"status\":" << rc << ",\"deviceReqId\":0}";
                            }
                        } catch (std::exception const &e) {
                            BOOST_LOG_SEV(my_logger_bs, lt::error) << "- BS - " << "Sales session error: " << e.what();
                        }
                        BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - " << "WEBI_ITEM_ADD - Cool - rc:" << rc <<
 ", barcode: " << barcode << ", qty: " << qty << ",addr: " << &items_map[itm_code_price.code];
                    } else {
                        rc = BCODE_NOT_RECOGNIZED;
                        if (rc > 0 && dummy_rcs) {
                            rc = 3;
                        }
                        resp_string_stream << "{\"status\":" << rc << ",\"deviceReqId\":0}";
                    }

                    break;
                }
                case WEBI_GET_TOTALS: {
                    tmp_totals_map = my_cart->getTotals();
                    resp_string_stream << "{\"status\":" << rc << ",\"deviceReqId\":" << request_id << ",\"totalItems\":"
                            << tmp_totals_map[0].items_number << ",\"totalAmount\":" <<
                            fromLongToStringWithDecimals(tmp_totals_map[0].total_amount) <<
                            R"(,"totalDiscounts":0.0,"amountToPay":)" << fromLongToStringWithDecimals(
                                tmp_totals_map[0].total_amount) << "}";
                    BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - WEBI_GET_TOTALS - Cool - rc:" << rc;
                    break;
                }
                case WEBI_ITEM_VOID: {
                    if (barcode == 0) {
                        barcode = strtoull(p_url_params_map["barcode"].c_str(), nullptr, 10);
                    }
                    itm_code_price = decodeBarcode(barcode);

                    qty = -1;
                    try {
                        if (items_map[itm_code_price.code].getCode() == itm_code_price.code) {
                            itm_code_price.price = my_cart->getItemPrice(&items_map[itm_code_price.code], barcode,
                                                                      itm_code_price.type,
                                                                      carts_price_changes_while_shopping);

                            resp_string_stream << "{";

                            rc = my_cart->removeItemByBarcode(items_map[itm_code_price.code], barcode, itm_code_price.price,
                                                             0);
                            if (rc > 0 && dummy_rcs) {
                                rc = 3;
                            }

                            if (rc == 0 && (items_map[itm_code_price.code].getLinkedBarCode() > 0)) {
                                uint64_t barCodeTmp = items_map[itm_code_price.code].getLinkedBarCode();
                                long rcLinked = my_cart->removeItemByBarcode(
                                    items_map[barcodes_map[barCodeTmp].getItemCode()], barCodeTmp,
                                    items_map[barcodes_map[barCodeTmp].getItemCode()].getPrice(), 0);
                                if ((rcLinked > 0) && (dummy_rcs)) {
                                    rcLinked = 3;
                                }
                                if (mainReturnSeparateLinkedBarcode) {
                                    resp_string_stream << "\"addItemResponse\":{\"status\":" << rcLinked <<
                                            ",\"deviceReqId\":" << request_id << ",\"itemId\":\"" << barCodeTmp <<
                                            "\",\"description\":\"" << items_map[barcodes_map[barCodeTmp].getItemCode()].
                                            getDescription() << "\",\"price\":" << fromLongToStringWithDecimals(
                                                items_map[barcodes_map[barCodeTmp].getItemCode()].getPrice()) <<
                                            ",\"voidFlag\":\"true\",\"quantity\":1,\"itemType\":\"NormalSaleItem\"},";
                                } else {
                                    itm_code_price.price =
                                            itm_code_price.price + items_map[barcodes_map[barCodeTmp].getItemCode()].
                                            getPrice();
                                }
                            }
                            tmp_totals_map = my_cart->getTotals();
                            resp_string_stream << "\"addItemResponse\":{\"status\":" << rc << ",\"deviceReqId\":" <<
                                    request_id << ",\"itemId\":\"" << barcode << "\",\"description\":\"" << items_map[
                                        itm_code_price.code].getDescription() << "\",\"price\":" <<
                                    fromLongToStringWithDecimals(itm_code_price.price) <<
                                    ",\"voidFlag\":\"true\",\"quantity\":1,\"itemType\":\"NormalSaleItem\"}";
                            resp_string_stream << ",\"getTotalResponse\":{\"status\":" << rc << ",\"deviceReqId\":" <<
                                    request_id << ",\"totalItems\":" << tmp_totals_map[0].items_number <<
                                    ",\"totalAmount\":" << fromLongToStringWithDecimals(tmp_totals_map[0].total_amount) <<
                                    ",\"totalDiscounts\":0.0,\"amountToPay\":" << fromLongToStringWithDecimals(
                                        tmp_totals_map[0].total_amount) << "}}";
                        } else {
                            rc = BCODE_ITEM_NOT_FOUND;
                            if ((rc > 0) && (dummy_rcs)) {
                                rc = 3;
                            }
                            resp_string_stream << "{\"status\":" << rc << ",\"deviceReqId\":" << request_id << "}";
                        }
                        BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - WEBI_ITEM_VOID - Cool - rc:" << rc;
                    } catch (std::exception const &e) {
                        BOOST_LOG_SEV(my_logger_bs, lt::error) << "- BS - Sales session error: " << e.what();
                    }
                    break;
                }
                case WEBI_CUSTOMER_ADD: {
                    barcode = strtoull(p_url_params_map["customerId"].c_str(), nullptr, 10);
                    typedef std::map<uint64_t, uint64_t>::iterator loyCardsIteratorType;
                    for (auto &cardIterator: all_loy_cards_map) {
                        if (cardIterator.first == barcode) {
                            if (cardIterator.second == cart_id) {
                                rc = RC_LOY_CARD_ALREADY_PRESENT;
                            } else {
                                if (atoi(configuration_map["LoyOnlyOneShoppingSessionPerCard"].c_str()) == 1) {
                                    rc = RC_LOY_CARD_IN_ANOTHER_TRANSACTION;
                                }
                            }
                        }
                    }

                    if (rc == 0) {
                        all_loy_cards_map[barcode] = cart_id;
                        rc = my_cart->addLoyCard(barcode, atoi(configuration_map["LoyMaxCardsPerTransaction"].c_str()));
                    }

                    if ((rc != 0) && (dummy_rcs)) {
                        rc = 3;
                    }

                    BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - " << "WEBI_ADD_CUSTOMER - Cool - rc:" << rc <<
 ", card: " << barcode;
                    if (rc == RC_OK) {
                        resp_string_stream << "{\"status\":" << rc << ",\"deviceReqId\":" << request_id << "}";
                    } else {
                        resp_string_stream << "{\"status\":" << rc << ",\"deviceReqId\":" << request_id <<
                                R"(,"errorCode":"","errorMessage":"","resultExtension":[]})";
                    }
                    break ;
                }
                case WEBI_CUSTOMER_VOID: {
                    barcode = strtoull(p_url_params_map["customerId"].c_str(), nullptr, 10);
                    rc = my_cart->removeLoyCard(barcode);
                    BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - " << "WEBI_REMOVE_CUSTOMER - Cool - rc:" << rc;
                    if (rc == RC_OK) {
                        all_loy_cards_map.erase(barcode);
                        resp_string_stream << "{\"status\":" << rc << ",\"deviceReqId\":" << request_id << "}";
                    } else {
                        if ((rc > 0) && (dummy_rcs)) {
                            rc = 3;
                        }
                        resp_string_stream << "{\"status\":" << rc << ",\"deviceReqId\":" << request_id <<
                                R"(,"errorCode":"","errorMessage":"","resultExtension":[]})";
                    }

                    break;
                }
                case WEBI_SESSION_END: {
                    rc = my_cart->sendToPos(atol(p_url_params_map["payStationID"].c_str()),
                                           this->configuration_map["SelfScanScanInDir"],
                                           this->configuration_map["MainStoreId"]);
                    if ((rc > 0) && (dummy_rcs)) {
                        rc = 3;
                    }
                    // respStringStream << "{\"status\":" << rc << ",\"deviceReqId\":" << requestId << ",\"sessionId\":" << strCartId << ",\"terminalNum\":" << pUrlParamsMap["payStationID"] << "}" ;
                    resp_string_stream << "{\"status\":" << rc << ",\"deviceReqId\":" << request_id << "}";
                    BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - WEBI_SESSION_END - Cool - rc:" << rc;
                    break;
                }
                case WEBI_GET_ALL_CART: {
                    resp_string_stream << my_cart->getAllCartJson(items_map, false);
                    BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - WEBI_SESSION_GET_ALL_CART - Cool - result:" <<
 resp_string_stream.str();
                    break;
                }
                case WEBI_GET_ALL_CART_WITH_BARCODES: {
                    resp_string_stream << my_cart->getAllCartJson(items_map, true);
                    BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - WEBI_GET_ALL_CART_WITH_BARCODES - Cool - result:"
 << resp_string_stream.str();
                    break;
                }
                case WEBI_SESSION_VOID: {
                    rc = my_cart->voidAllCart();
                    if ((rc > 0) && (dummy_rcs)) {
                        rc = 3;
                    }
                    resp_string_stream << "{\"status\":" << rc << ",\"deviceReqId\":" << request_id << ",\"sessionId\":" <<
                            strCartId << "}";
                    BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - WEBI_SESSION_VOID - Cool - rc:" << rc;
                    break;
                }
                default: {
                    BOOST_LOG_SEV(my_logger_bs, lt::warning) << "- BS - Web action not recognized :(, " << p_action;
                }
            }
        } else {
            rc = CART_NOT_FOUND;
            if ((rc > 0) && (dummy_rcs)) {
                rc = 3;
            }
            resp_string_stream << "{\"status\":" << rc << ",\"sessionId\":" << strCartId << "}";
        }
    }
    BOOST_LOG_SEV(my_logger_bs, lt::info) << "- BS - " << resp_string_stream.str();

    return resp_string_stream.str();
}


Item BaseSystem::getItemByIntCode(const uint64_t p_intcode) {
    return items_map[p_intcode];
}

uint32_t BaseSystem::newCart(const unsigned int p_action) {
    uint32_t thisCartNumber = next_cart_number;

    Cart newCart(this->base_path + archives_dir + configuration_map["MainArchivesDir"], thisCartNumber, p_action,
                 this->dummy_rcs);

    carts_map.insert({thisCartNumber, std::move(newCart)});
    //emplace non disponibile nel GCC di Debian 7
    //cartsMap.emplace( std::piecewise_construct, std::make_tuple(thisCartNumber), std::make_tuple(this->basePath,thisCartNumber,pAction) ) ;

    next_cart_number++;
    return thisCartNumber;
}

bool BaseSystem::persistCarts() {
    return true;
}

string BaseSystem::getCartsList() {
    typedef std::map<uint64_t, Cart>::iterator carts;
    uint64_t cartId = 0;
    bool firstRow = true;

    std::ostringstream tempStringStream;
    tempStringStream.str(std::string());
    tempStringStream.clear();
    tempStringStream << "{\"Carts\":{";

    for (carts iterator = carts_map.begin(); iterator != carts_map.end(); iterator++) {
        if (!firstRow) {
            tempStringStream << ",";
        }
        cartId = iterator->first;
        tempStringStream << "\"cart\":" << cartId;
        firstRow = false;
    }
    tempStringStream << "}}";
    return tempStringStream.str();
}
