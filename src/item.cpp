//
//  Item.cpp
//  PromoCalculator
//
//  Created by Andrea Giovacchini on 04/01/14.
//  Copyright (c) 2014 Andrea Giovacchini. All rights reserved.
//

#include <sstream>

#include "Item.h"

Item::Item()
{
	this->code = 0 ;
	this->price = 0 ;
	this->description = "" ;
	this->department_code = 0 ;
    this->linked_barcode = 0;
}

Item::Item(const uint64_t p_code, const int32_t p_price, const string &p_description, const uint64_t p_department )
{
    this->code = p_code ;
    this->price = p_price ;
	this->description = p_description ;
	this->department_code = p_department ;
    this->linked_barcode = 0;
}

void Item::setCode( uint64_t p_code ) {
    this->code = p_code ;
}

uint64_t Item::getCode() const {
    return this->code ;
}

void Item::setPrice( int64_t p_price ) {
    this->price = p_price ;
}

int64_t Item::getPrice() const {
    return this->price ;
}

void Item::setDescription(const string &p_description ) {
    this->description = p_description ;
}

string Item::getDescription() const {
    return this->description ;
}

//E' una referenza all'oggetto reparto
void Item::setDepartmentCode(const uint64_t p_department_code) {
	this->department_code = p_department_code;
}

uint64_t Item::getDepartmentCode() const {
    return this->department_code ;
}

void Item::setLinkedBarCode(const uint64_t p_linked_barcode ) {
    this->linked_barcode = p_linked_barcode ;
}

uint64_t Item::getLinkedBarCode() const {
    return this->linked_barcode ;
}

string Item::toStr() const {
	std::stringstream temp_string_stream;
	temp_string_stream.str(std::string());
	temp_string_stream.clear();
	temp_string_stream << this->code
		<< ",\"" << this->description << "\""
        << "," << this->department_code
        << "," << this->price
        << "," << this->linked_barcode
    ;

	return temp_string_stream.str();
}