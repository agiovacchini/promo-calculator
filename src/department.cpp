//
//  Department.cpp
//  PromoCalculator
//
//  Created by Andrea Giovacchini on 05/01/14.
//  Copyright (c) 2014 Andrea Giovacchini. All rights reserved.
//

#include "department.h"

#include <iostream>
#include <utility>
#include <sstream>

Department::Department()
{
	this->code = 0;
    this->parent_code = 0;
	this->description = "";
}

Department::Department(const uint64_t p_code, const uint64_t p_parent_code, string p_description)
{
	this->code = p_code;
	this->parent_code = p_parent_code;
    this->description = std::move(p_description);
}

void Department::setCode( uint64_t p_code ) {
    this->code = p_code ;
}

uint64_t Department::getCode() const {
    return this->code ;
}

void Department::setParentCode(const uint64_t p_parent_code ) {
    this->parent_code = p_parent_code ;
}

uint64_t Department::getParentCode() const {
    return this->parent_code ;
}

void Department::setDescription( string p_description ) {
    this->description = std::move(p_description) ;
}

string Department::getDescription() const {
    return this->description ;
}

string Department::toStr() const {
	std::stringstream temp_string_stream;
    temp_string_stream.str(std::string());
    temp_string_stream.clear();
    try {
        temp_string_stream << this->code
        << "," << this->parent_code
		<< ",\"" << this->description << "\"" ;
    } catch (std::exception const& e)
    {
        std::cout << "Dept: " << e.what() << std::endl ;
    }


	return temp_string_stream.str();
}
