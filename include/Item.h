//
//  Item.h
//  PromoCalculator
//
//  Created by Andrea Giovacchini on 04/01/14.
//  Copyright (c) 2014 Andrea Giovacchini. All rights reserved.
//

#include <string>
using namespace std;

#ifndef __PromoCalculator__Item__
#define __PromoCalculator__Item__

#include "base/archive.h"
#include <cstdint>

class Item : public Archive {
    
    uint64_t code ;
    int64_t price ;
    string description ;
	uint64_t department_code ;
    uint64_t linked_barcode ;

public:
	Item();

	Item(uint64_t p_code, int32_t p_price, const string &p_description, uint64_t p_department);
    
    void setCode( uint64_t p_code ) ;
    
    uint64_t getCode() const ;
    
    void setPrice( int64_t p_price ) ;
    
    int64_t getPrice() const ;
    
    void setDescription(const string &p_description ) ;
    
    string getDescription() const ;

	void setDepartmentCode( uint64_t p_department_code );
        
	uint64_t getDepartmentCode() const ;
        
    void setLinkedBarCode( uint64_t p_linked_barcode ) ;
    
    uint64_t getLinkedBarCode() const ;
    
    string toStr() const ;
    
    //Per la mappa, le funzioni che chiamano devono avere il modificatore const per attestare che non
    //hanno effetti collaterali
    friend bool operator== (const Item& p1, const Item& p2)
    {
        return p1.getCode() == p2.getCode() ;
    }

    friend bool operator< (const Item& p1, const Item& p2)
    {
        return p1.getCode() < p2.getCode() ;
    }

    friend bool operator> (const Item& p1, const Item& p2)
    {
        return p1.getCode() > p2.getCode() ;
    }
    
    //Copy constructor
    Item& operator=( const Item& other ) {
        code = other.code ;
        price = other.price ;
        description = other.description ;
		department_code = other.department_code;
        linked_barcode = other.linked_barcode ;
        return *this;
    }
    
} ;

#endif /* defined(__PromoCalculator__Item__) */
