//
//  Cart.cpp
//  PromoCalculator
//
//  Created by Andrea Giovacchini on 04/01/14.
//  Copyright (c) 2014 Andrea Giovacchini. All rights reserved.
//

#include "base_types.h"
#include "cart.h"

#include <sstream>
#include <stdio.h>
#include <ctime>
#include <locale>
#include <fstream>
#include <mutex>
//#include <pcap.h>
#include "boost/format.hpp"
#include "boost/filesystem.hpp"
#include "boost/date_time/posix_time/posix_time.hpp"
#include <boost/date_time/posix_time/posix_time_io.hpp>
using namespace std;

std::mutex cart_sendtopos_mutex ;

Cart::Cart( string p_base_path, uint32_t p_number, unsigned int p_action, bool p_dummy_rcs )
{
    std::stringstream temp_string_stream;
    std::ofstream tmp_transaction_file;
    number = p_number ;
    items_number = 0 ;
    next_request_id = 1 ;
    loy_cards_number = 0 ;
    base_path = p_base_path ;
    dummy_rcs = p_dummy_rcs ;
    rescan_required = false ;

    cart_items_map.clear() ;
    items_local_copy_map.clear() ;

    CartRow totalCartRow = { TOTAL, 0 } ;

    totals_map[0].total_amount = 0 ;
    totals_map[0].items_number = 0 ;
    totals_map[0].total_discount = 0;

    cart_items_map[&totals_map[0]] = totalCartRow ;
    cart_file_name = (boost::format("%sCARTS/%010lu.cart") % base_path % number).str() ;
    tmp_transaction_file_name = (boost::format("%sCARTS/%010lu.transaction_in_progress") % base_path % number).str() ;
    tmp_transaction_file.open( tmp_transaction_file_name, fstream::app );
    tmp_transaction_file.close() ;
    
    BOOST_LOG_SEV(my_logger_ca, lt::info) << "- CART " << this->number << " - " << cart_file_name << " - " << tmp_transaction_file_name ;
    
    switch (p_action)
    {
        case GEN_CART_NEW:
            state = CART_STATE_READY_FOR_ITEM ;
            break ;
        case GEN_CART_LOAD:
            state = CART_STATE_TMPFILE_LOADING ;
            break;
        default:
            break;
    }
}


void Cart::setNumber(const uint32_t p_number )
{
    this->number = p_number ;
}

uint32_t Cart::getNumber() const
{
    return this->number ;
}

long Cart::getState() const {
    return this->state;
}

void Cart::setState( unsigned int p_state ) {
    this->state = p_state ;
}

uint32_t Cart::getRequestId() const {
    return this->next_request_id ;
}

uint32_t Cart::getNextRequestId()
{
    this->next_request_id++ ;
    return this->next_request_id ;
}

unsigned int Cart::getLoyCardsNumber() const
{
    return this->loy_cards_number ;
}

void Cart::writeTransactionRow(const string &row ) const {
    std::ofstream tmp_transaction_file;
    tmp_transaction_file.open( tmp_transaction_file_name, fstream::app );
    tmp_transaction_file << to_iso_string(boost::posix_time::microsec_clock::universal_time()) << "," << row << "\n";
    tmp_transaction_file.close() ;
}

long Cart::addLoyCard(const uint64_t p_loy_card_number, unsigned int max_card_number )
{
    long result;
    if ( this->getState() == CART_STATE_TMPFILE_LOADING || this->getState() == CART_STATE_READY_FOR_ITEM )
    {
        if (this->loy_cards_number < max_card_number )
        {
            typedef std::map<unsigned int, uint64_t>::iterator it_type;
            for(it_type iterator = loy_cards_map.begin(); iterator != loy_cards_map.end(); ++iterator) {
                if (iterator->second == p_loy_card_number)
                {
                    return RC_LOY_CARD_ALREADY_PRESENT ;
                }
            }
            loy_cards_map[loy_cards_number] = p_loy_card_number ;
            if (this->getState() == CART_STATE_READY_FOR_ITEM)
            {
                std::stringstream temp_string_stream;
                temp_string_stream.str(std::string());
                temp_string_stream.clear();
                temp_string_stream << "L,A," << p_loy_card_number << "," << "1" ;
                this->writeTransactionRow(temp_string_stream.str() );
            }
            this->loy_cards_number++ ;
            result = RC_OK ;
            
        } else {
            result = RC_LOY_MAX_CARD_NUMBER ;
        }
        result = RC_OK ;
    } else {
        result = this->state ;
    }
    return result ;
}

long Cart::removeLoyCard( uint64_t p_loy_card_number )
{
    long result;
    if ( this->getState() == CART_STATE_TMPFILE_LOADING || this->getState() == CART_STATE_READY_FOR_ITEM )
    {
        typedef std::map<unsigned int, uint64_t>::iterator it_loy_cards;
        std::stringstream temp_string_stream;
        for(it_loy_cards iterator = loy_cards_map.begin(); iterator != loy_cards_map.end(); ++iterator) {
            if (iterator->second == p_loy_card_number)
            {
                loy_cards_map.erase(iterator) ;
                if (this->getState() == CART_STATE_READY_FOR_ITEM)
                {
                    temp_string_stream.str(std::string());
                    temp_string_stream.clear();
                    temp_string_stream << "L,V," << p_loy_card_number << "," << "1" ;
                    this->writeTransactionRow(temp_string_stream.str() );
                }
                this->loy_cards_number-- ;
                result = RC_OK ;
            }
        }
        result = RC_LOY_CARD_NOT_PRESENT ;
    } else {
        result = this->state ;
    }
    return result;
}

bool Cart::updateLocalItemMap(Item p_item, const Department &p_dept)
{
    bool result;
    if (items_local_copy_map.find(p_item.getCode()) != items_local_copy_map.end())
    {
        result = false ;
    } else {
        if (this->getState() == CART_STATE_READY_FOR_ITEM)
        {
            std::stringstream temp_string_stream;
            temp_string_stream.str(std::string());
            temp_string_stream.clear();
            temp_string_stream << "K,D," << p_dept.toStr() ;
            this->writeTransactionRow(temp_string_stream.str() );
            temp_string_stream.str(std::string());
            temp_string_stream.clear();
            temp_string_stream << "K,I," << p_item.toStr() ;
            this->writeTransactionRow(temp_string_stream.str() );
        }
        items_local_copy_map.insert( std::pair(p_item.getCode(), p_item) ) ;
        result = true ;
    }
    return result ;
}

long Cart::getItemPrice(const Item* p_item, const uint64_t p_barcode, const unsigned int p_bcode_type, const bool p_price_changes_while_shopping)
{
    long result;
    if (p_bcode_type != BCODE_EAN13_PRICE_REQ)
    {
        if (!p_price_changes_while_shopping)
        {
            if (items_local_copy_map.find(p_item->getCode()) != items_local_copy_map.end())
            {
                result = items_local_copy_map[p_item->getCode()].getPrice();
            } else {
                result = p_item->getPrice();
            }
        } else {
            result = p_item->getPrice();
        }
    } else {
        std::stringstream temp_string_stream;
        temp_string_stream.str( std::string() ) ;
        temp_string_stream.clear() ;
        temp_string_stream << p_barcode ;
        result = atol(temp_string_stream.str().substr(7,5).c_str()) ;
    }
    return result ;
}


long Cart::addItemByBarcode( const Item& p_item, const uint64_t p_barcode, const int64_t p_price, const int64_t p_discount )
{
    constexpr uint32_t p_qty_item = 1 ;
    return addItemByBarcode(p_item, p_barcode, p_qty_item, p_price, p_discount) ;
}

long Cart::addItemByBarcode( const Item& p_item, const uint64_t p_barcode, const uint32_t p_qty_item, const int64_t p_price, const int64_t p_discount )
{
    long rc;
    if ( this->getState() == CART_STATE_TMPFILE_LOADING || this->getState() == CART_STATE_READY_FOR_ITEM )
    {
        try {
            std::stringstream temp_string_stream;
            const long qty_item = cart_items_map[&p_item].quantity + p_qty_item ;
            const long qty_barcode = barcodes_map[p_barcode] + p_qty_item ;

            totals_map[0].items_number = totals_map[0].items_number + p_qty_item ;
            totals_map[0].total_amount = totals_map[0].total_amount + (p_price - p_discount) * p_qty_item ;
            totals_map[0].total_discount = totals_map[0].total_discount + p_discount * p_qty_item ;

            totals_map[p_item.getDepartmentCode()].items_number = totals_map[p_item.getDepartmentCode()].items_number + p_qty_item ;
            totals_map[p_item.getDepartmentCode()].total_amount = totals_map[p_item.getDepartmentCode()].total_amount + (p_price - p_discount) * p_qty_item ;
            totals_map[p_item.getDepartmentCode()].total_discount = totals_map[p_item.getDepartmentCode()].total_discount + p_discount * p_qty_item ;

            items_number = items_number + p_qty_item ;

            cart_items_map[&p_item] = { ITEM, qty_item } ;
            barcodes_map[p_barcode] = qty_barcode ;

            temp_string_stream.str(std::string());
            temp_string_stream.clear();
            temp_string_stream << this->getState();

            BOOST_LOG_SEV(my_logger_ca, lt::info) << "- CART# " << this->number << " - " <<  "Cart state " << temp_string_stream.str() ;
            if (this->getState() == CART_STATE_READY_FOR_ITEM)
            {
                temp_string_stream.str(std::string());
                temp_string_stream.clear();
                temp_string_stream << "I,A," << p_barcode << "," << p_qty_item ;
                this->writeTransactionRow(temp_string_stream.str() );
            }
        } catch (std::exception const& e)
        {
            BOOST_LOG_SEV(my_logger_ca, lt::error) << "- CART# " << this->number << " - " << "Cart addItemBarcode error: " << e.what();
        }
        rc = RC_OK ;
    } else {
        rc = this->state ;
    }
    return rc;
}

long Cart::removeItemByBarcode( const Item& p_item, const uint64_t p_barcode, const int64_t p_price, const int64_t p_discount )
{
    long rc;
    if ( this->getState() == CART_STATE_TMPFILE_LOADING || this->getState() == CART_STATE_READY_FOR_ITEM )
    {
        if (cart_items_map.find(&p_item) == cart_items_map.end())
        {
            BOOST_LOG_SEV(my_logger_ca, lt::info) << "- CART# " << this->number << " - " << "Not found: " << p_item.getDescription() ;
            rc = RC_ERR ;
        }
        else
        {
            BOOST_LOG_SEV(my_logger_ca, lt::info) << "- CART# " << this->number << " - " << "Found: " << p_item.getDescription() ;
            const long qty_item = cart_items_map[&p_item].quantity - 1;
            const long qty_barcode = barcodes_map[p_barcode] - 1;
            totals_map[0].items_number-- ;
            totals_map[0].total_amount = totals_map[0].total_amount - (p_price - p_discount) ;
            totals_map[0].total_discount = totals_map[0].total_discount - p_discount ;

            totals_map[p_item.getDepartmentCode()].items_number-- ;
            totals_map[p_item.getDepartmentCode()].total_amount = totals_map[p_item.getDepartmentCode()].total_amount - (p_price - p_discount);
            totals_map[p_item.getDepartmentCode()].total_discount = totals_map[p_item.getDepartmentCode()].total_discount - p_discount ;

            items_number-- ;

            if (qty_item < 1)
            {
                BOOST_LOG_SEV(my_logger_ca, lt::info) << "- CART# " << this->number << " - " << "Erasing item" ;
                cart_items_map.erase(&p_item) ;
            }
            else
            {
                cart_items_map[&p_item] = { ITEM, qty_item } ;
            }

            if (qty_barcode < 1)
            {
                barcodes_map.erase(p_barcode) ;
            }
            else
            {
                barcodes_map[p_barcode] = qty_barcode ;
            }

            if (this->getState() == CART_STATE_READY_FOR_ITEM)
            {
                std::stringstream temp_string_stream;
                temp_string_stream.str(std::string());
                temp_string_stream.clear();
                temp_string_stream << "I,V," << p_barcode << "," << "1" ;
                this->writeTransactionRow(temp_string_stream.str() );
            }
        }
        rc = RC_OK ;
    } else {
        rc = this->state ;
    }
    return rc;
}

long Cart::voidAllCart()
{
    long rc;
    if ( this->state!=CART_STATE_VOIDED && this->state!=CART_STATE_CLOSED )
    {
        std::stringstream temp_string_stream;
        this->state = CART_STATE_VOIDED ;
        temp_string_stream.str(std::string());
        temp_string_stream.clear();
        temp_string_stream << "C,V" ;
        this->writeTransactionRow(temp_string_stream.str() );
        rc = RC_OK ;
    } else {
        rc = this->state ;
    }
    return rc;
}

long Cart::printCart()
{
    if ( this->state!=CART_STATE_VOIDED && this->state!=CART_STATE_CLOSED )
    {
        typedef std::map<const void*, CartRow>::iterator item_rows;
        Item* itm_row ;

        BOOST_LOG_SEV(my_logger_ca, lt::info) << "- CART# " << this->number << " - " << "Print start" ;
        for(item_rows iterator = cart_items_map.begin(); iterator != cart_items_map.end(); iterator++) {
            CartRow key = iterator->second ;
            switch (key.type) {
                case ITEM:
                    itm_row = (Item*)iterator->first;
                    //BOOST_LOG_SEV(my_logger_ca, lt::info) << "- CART# " << this->number << " - " << itmRow->getDescription() << " - " << itmRow->getPrice() << " - " << itmRow->getDepartment().getDescription() << " qty: " << key.quantity ;
                    break;
                case DEPT:
                    break;
                case TOTAL:
                    //totalsRow = (Totals*)iterator->first;
                    //BOOST_LOG_SEV(my_logger_ca, lt::info) << "\nTotale: " << totalsRow->totalAmount << " Item Nr.: " << totalsRow->itemsNumber ;
                    break;
                default:
                    break;
            }
            //BOOST_LOG_SEV(my_logger_ca, lt::info) << "\nkey: " << key ;
        }
        BOOST_LOG_SEV(my_logger_ca, lt::info) << "- CART# " << this->number << " - " << "State: " << this->getState();
        BOOST_LOG_SEV(my_logger_ca, lt::info) << "- CART# " << this->number << " - " << "Totals start" ;
        typedef std::map<uint64_t, Totals>::iterator configurationRows;
        
        for(configurationRows iterator = totals_map.begin(); iterator != totals_map.end(); iterator++) {
            BOOST_LOG_SEV(my_logger_ca, lt::info) << "- CART# " << this->number << " - " << "Dept: " << iterator->first << ", value: " << iterator->second.total_amount << ", items: " << iterator->second.items_number ;
        }
        BOOST_LOG_SEV(my_logger_ca, lt::info) << "- CART# " << this->number << " - " << "Totals end" ;
        BOOST_LOG_SEV(my_logger_ca, lt::info) << "- CART# " << this->number << " - " << "Cart print end" ;
        return RC_OK ;
    } else {
        return this->state ;
    }
}

long Cart::persist( )
{
    typedef std::map<uint64_t, long>::iterator barcodes_rows;
    long qty = 0 ;

    BOOST_LOG_SEV(my_logger_ca, lt::info) << "- CART# " << this->number << " - " << "cartFileName: " << cart_file_name ;

    std::ofstream cartFile( cart_file_name.c_str() );

    for(barcodes_rows iterator = barcodes_map.begin(); iterator != barcodes_map.end(); ++iterator)
    {
        const uint64_t row_barcode = iterator->first;
        qty = iterator->second ;
        cartFile << row_barcode << "," << qty << "\n";
    }
    
    cartFile.close() ;
    return RC_OK ;
}

long Cart::sendToPos( uint32_t p_pos_number, string p_scan_in_path, string p_store_id )
{
    if (this->getState() == CART_STATE_READY_FOR_ITEM)
    {
        cart_sendtopos_mutex.lock() ;

        typedef std::map<uint64_t, long>::iterator barcodes_rows;
        std::stringstream temp_string_stream;

        temp_string_stream.str(std::string());
        temp_string_stream.clear();

        uint64_t rowBarcode ;
        long qty = 0 ;
        string scan_in_tmp_file_name = (boost::format("%s/POS%03lu.TMP") % p_scan_in_path % p_pos_number).str();
        string scan_in_file_name = (boost::format("%s/POS%03lu.IN") % p_scan_in_path % p_pos_number).str();
        BOOST_LOG_SEV(my_logger_ca, lt::info) << "- CART# " << this->number << " - " << "Sending to pos with file: " << cart_file_name ;
        auto *facet = new boost::posix_time::time_facet("%H%M%S%Y%m%d");
        std::stringstream date_stream;
        date_stream.imbue(std::locale(date_stream.getloc(), facet));
        date_stream << boost::posix_time::microsec_clock::universal_time();
        std::ofstream scan_in_file( scan_in_tmp_file_name.c_str() );

        temp_string_stream << loy_cards_map.begin()->second ;

        scan_in_file << "03;" << date_stream.str() //tsInit
        << ";" << date_stream.str() //tsEnd
        << ";" << p_pos_number
        << ";" << temp_string_stream.str().substr(1,8) //customerCode
        << ";" << p_store_id //serverId
        << ";" << this->number
        << ";" << "0"
        << ";" << this->items_number
        << ";" << totals_map[0].total_amount
        << ";" << totals_map[0].total_amount
        << ";" << "0" << loy_cards_map.begin()->second
        << endl;

        for(barcodes_rows iterator = barcodes_map.begin(); iterator != barcodes_map.end(); iterator++)
        {
            rowBarcode = static_cast<uint64_t>(iterator->first);
            qty = iterator->second ;
            if (qty > 0)
            {
                for (long repetitions = 0; repetitions < qty; repetitions++ )
                {
                    scan_in_file << "04;" << rowBarcode << ";" << "0.00" << ";" << date_stream.str() << endl;
                }
            }
        }

        scan_in_file.close() ;

        boost::filesystem::rename(scan_in_tmp_file_name.c_str(), scan_in_file_name);
        
        cart_sendtopos_mutex.unlock() ;
        
        return RC_OK ;
    } else {
        return this->state ;
    }
}

string Cart::getAllCartJson( ArchiveMap<Item> p_all_items_map, bool p_with_barcodes )
{
    typedef std::map<unsigned int, uint64_t>::iterator loyCardRows ;
    
    std::stringstream temp_string_stream;

    temp_string_stream.str(std::string());
    temp_string_stream.clear();

    long qty = 0 ;
    bool first_row = true ;

    BOOST_LOG_SEV(my_logger_ca, lt::info) << "- CART# " << this->number << " - Getting full cart in JSON format" ;

    temp_string_stream << "{\"loyCards\":" ;
    for(loyCardRows iterator = loy_cards_map.begin(); iterator != loy_cards_map.end(); iterator++)
    {
        if (!first_row)
        {
            temp_string_stream << "," ;
        }
        const uint64_t row_card_code = iterator->second;
        temp_string_stream << "{\"loyCard\":" << row_card_code << "}" ;
        first_row = false ;
    }
    //itemsMap[&pItem] = { ITEM, qtyItem } ;
    first_row = true ;

    if (p_with_barcodes)
    {
        typedef std::map<uint64_t, long>::iterator barcodes_rows ;
        temp_string_stream << ",\"barcodes\":{" ;
        for(barcodes_rows iterator = barcodes_map.begin(); iterator != barcodes_map.end(); ++iterator)
        {
            const uint64_t row_bar_code = iterator->first;
            qty = iterator->second ;
            if (qty > 0)
            {
                if (!first_row)
                {
                    temp_string_stream << "," ;
                }
                temp_string_stream << "\"barcode\":{\"code\":" << row_bar_code << ",\"qty\":" << qty << "}" << endl;
                first_row = false ;
            }
        }
    } else {
        typedef std::map<const void*, CartRow>::iterator itemRows ;
        temp_string_stream << ",\"items\":{" ;

        for(itemRows iterator = cart_items_map.begin(); iterator != cart_items_map.end(); ++iterator)
        {
            const auto temp_itm = (Item *) iterator->first;
            const uint64_t row_int_code = temp_itm->getCode();
            qty = iterator->second.quantity ;
            if (qty > 0)
            {
                if (!first_row)
                {
                    temp_string_stream << "," ;
                }
                temp_string_stream << "\"item\":{\"intCode\":" << row_int_code << ",\"description\":\"" << p_all_items_map[row_int_code].getDescription() << "\",\"qty\":" << qty << "}" << endl;
                first_row = false ;
            }
        }
    }
    temp_string_stream << "}}" ;

    return temp_string_stream.str() ;
}

std::map <uint64_t, Totals> Cart::getTotals()
{
    return totals_map ;
}

long Cart::close()
{
    //tmpTransactionFile.close() ;
    state = CART_STATE_CLOSED ;
    return RC_OK ;
}

void Cart::setRescan(const bool p_rescan_required) {
    rescan_required = p_rescan_required;
}
