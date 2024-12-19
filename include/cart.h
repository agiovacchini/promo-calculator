//
//  cart.h
//  PromoCalculator
//
//  Created by Andrea Giovacchini on 04/01/14.
//  Copyright (c) 2014 Andrea Giovacchini. All rights reserved.
//

#ifndef __PromoCalculator__Cart__
#define __PromoCalculator__Cart__

#include "map"

#include "Item.h"
#include "base_types.h"
#include "department.h"
#include "base/archive_map.h"

class Cart {
    std::map <unsigned int, uint64_t> loy_cards_map ;
    std::map <const void*, CartRow> cart_items_map ;
    std::map <uint64_t, Item> items_local_copy_map ;
    
    std::map <uint64_t, long> barcodes_map ;
    std::map <uint64_t, Totals> totals_map ;
    uint32_t number ;
    uint32_t items_number ;
    unsigned int loy_cards_number ;
    uint32_t next_request_id ;
    long state ;
    string base_path = "./" ;
    string cart_file_name ;
    string tmp_transaction_file_name ;
    src::severity_logger_mt< boost::log::trivial::severity_level > my_logger_ca;
    bool dummy_rcs ;
    bool rescan_required ;
    
public:
    Cart( string p_base_path, uint32_t p_number, unsigned int p_action, bool p_dummy_rcs ) ;
    
    uint32_t getNumber() const ;
    void setNumber( uint32_t p_number ) ;
    
    void writeTransactionRow(const string &row ) const;
    bool updateLocalItemMap(Item p_item, const Department &p_dept) ;
    long getItemPrice(const Item* p_item, uint64_t p_barcode, unsigned int p_bcode_type, bool p_price_changes_while_shopping) ;
    long addItemByBarcode( const Item& p_item, uint64_t p_barcode, int64_t p_price, int64_t p_discount ) ;
    long addItemByBarcode( const Item& p_item, uint64_t p_barcode, uint32_t p_qty_item, int64_t p_price, int64_t p_discount ) ;
    long removeItemByBarcode( const Item& p_item, uint64_t p_barcode, int64_t p_price, int64_t p_discount ) ;
    long addLoyCard( uint64_t p_loy_card_number, unsigned int max_loy_cards ) ;
    long removeLoyCard( uint64_t p_loy_card_number ) ;
    long getState() const ;
    void setState( unsigned int p_state ) ;
    uint32_t getRequestId() const;
    uint32_t getNextRequestId() ;
    unsigned int getLoyCardsNumber() const ;
    long voidAllCart() ;
    long printCart() ;
    long persist( ) ;
	long sendToPos(uint32_t p_pos_number, string p_scan_in_path, string p_store_id) ;
    string getAllCartJson( ArchiveMap<Item> p_all_items_map, bool p_with_barcodes ) ;
    long close( ) ;
    void setRescan( bool p_rescan_required) ;
    
    std::map <uint64_t, Totals> getTotals() ;
    
    friend bool operator== (const Cart& p1, const Cart& p2)
    {
        return p1.getNumber() == p2.getNumber() ;
    }
    
    
    //http://www.cplusplus.com/forum/beginner/49924/
    friend std::ostream& operator<<(std::ostream& os, const Cart& s)
	{
		// write out individual members of s with an end of line between each one
		os << s.base_path << s.cart_file_name ;
		return os;
	}
    
	// Extraction operator
	friend std::istream& operator>>(std::istream& is, Cart& s)
	{
		// read in individual members of s
		is >> s.base_path >> s.cart_file_name;
		return is;
	}
    
} ;

#endif /* defined(__PromoCalculator__Cart__) */