//
//  Barcodes.cpp
//  PromoCalculator
//
//  Created by Andrea Giovacchini on 08/01/14.
//  Copyright (c) 2014 Andrea Giovacchini. All rights reserved.
//

#include "barcodes.h"
#include "base_types.h"

void Barcodes::setCode(const uint64_t p_code ) {
    this->code = p_code ;
}

uint64_t Barcodes::getCode() const {
    return this->code ;
}

void Barcodes::setItemCode(const uint64_t p_item_code ) {
    this->item_code = p_item_code ;
}

uint64_t Barcodes::getItemCode() const {
    return this->item_code ;
}

string Barcodes::toStr() const {
    std::stringstream row ;
    
    row << this->code
    << "," << this->item_code ;
    
    return row.str() ;
}