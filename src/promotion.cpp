//
//  Promotion.cpp
//  PromoCalculator
//
//  Created by Andrea Giovacchini on 10/07/15.
//  Copyright (c) 2015 Andrea Giovacchini. All rights reserved.
//

#include "promotion.h"

Promotion::Promotion()
{
    this->promo_code = 0 ;
    this->item_code = 0 ;
    this->discount = 0 ;
    this->discount_type = 0 ;
    this->description = "" ;
}

Promotion::Promotion(const uint64_t p_promo_code, const uint64_t p_item_code, const int64_t p_discount, const unsigned int p_discount_type, const string &p_description)
{
    this->promo_code = p_promo_code ;
    this->item_code = p_item_code ;
    this->discount = p_discount ;
    this->discount_type = p_discount_type ;
    this->description = p_description ;
}

void Promotion::setCode(const uint64_t p_code ) {
    this->item_code = p_code ;
}

uint64_t Promotion::getCode() const {
    return this->item_code ;
}

void Promotion::setPromoCode( const uint64_t p_promo_code ) {
    this->promo_code = p_promo_code ;
}

uint64_t Promotion::getPromoCode() const {
    return this->promo_code ;
}

void Promotion::setDiscount( const int64_t p_discount ) {
    this->discount = p_discount ;
}

uint64_t Promotion::getDiscount() const {
    return this->discount ;
}

void Promotion::setDiscountType( unsigned int p_discount_type ) {
    this->discount_type = p_discount_type ;
}

unsigned int Promotion::getDiscountType() const {
    return this->discount_type ;
}

void Promotion::setDescription(const string &pDescription ) {
    this->description = pDescription ;
}

string Promotion::getDescription() const {
    return this->description ;
}


string Promotion::toStr() const {
    std::stringstream tempStringStream;
    tempStringStream.str(std::string());
    tempStringStream.clear();
    tempStringStream << this->promo_code
    << "," << this->item_code
    << "," << this->discount
    << "," << this->discount_type
    << ",\"" << this->description << "\""
    ;
    
    return tempStringStream.str();
}
