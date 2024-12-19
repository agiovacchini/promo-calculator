//
//  promotion.h
//  PromoCalculator
//
//  Created by Andrea Giovacchini on 10/07/15.
//  Copyright (c) 2015 Andrea Giovacchini. All rights reserved.
//

#include <iostream>
#include <string>
using namespace std;

#ifndef PromoCalculator_Promotion_h
#define PromoCalculator_Promotion_h

#include "base/archive.h"
#include <sstream>
#include <stdint.h>

class Promotion final : public Archive {
    
    uint64_t promo_code ;
    uint64_t item_code ;
    int64_t discount ;
    unsigned int discount_type ;
    string description ;
    
public:
    Promotion();
    
    Promotion(uint64_t pCode, uint64_t p_item_code, int64_t p_discount, unsigned int p_discount_type, const string &p_description);
    
    void setPromoCode( uint64_t pCode ) ;
    
    uint64_t getPromoCode() const ;
    
    
    void setCode( uint64_t pItemCode ) ;
    
    uint64_t getCode() const ;
    
    
    void setDiscount(int64_t p_discount) ;
    
    uint64_t getDiscount() const ;
    
    
    void setDiscountType( unsigned int p_discount_type ) ;
    
    unsigned int getDiscountType() const ;
    
    
    void setDescription(const string &pDescription ) ;
    
    string getDescription() const ;
    
    string toStr() const override ;
    
    //Per la mappa, le funzioni che chiamano devono avere il modificatore const per attestare che non
    //hanno effetti collaterali
    friend bool operator== (const Promotion& p1, const Promotion& p2)
    {
        return p1.getCode() == p2.getCode() ;
    }
    
    friend bool operator< (const Promotion& p1, const Promotion& p2)
    {
        return p1.getCode() < p2.getCode() ;
    }
    
    friend bool operator> (const Promotion& p1, const Promotion& p2)
    {
        return p1.getCode() > p2.getCode() ;
    }
    
    //Copy constructor
    Promotion& operator=( const Promotion& other ) {
        promo_code = other.promo_code ;
        item_code = other.item_code ;
        discount = other.discount ;
        discount_type = other.discount_type;
        description = other.description ;
        return *this;
    }
    
} ;

#endif
