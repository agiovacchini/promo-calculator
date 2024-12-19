//
//  department.h
//  PromoCalculator
//
//  Created by Andrea Giovacchini on 05/01/14.
//  Copyright (c) 2014 Andrea Giovacchini. All rights reserved.
//
#ifndef __PromoCalculator__Department__
#define __PromoCalculator__Department__

#include <string>
using namespace std;

#include <cstdint>
#include "base/archive.h"

class Department final : public Archive {
    
    uint64_t code ;
    uint64_t parent_code ;
    string description ;

public:
	Department();

	Department(uint64_t p_code, uint64_t p_parent_code, string p_description);

    void setCode( uint64_t p_code ) ;
    
    [[nodiscard]] uint64_t getCode() const ;

    void setParentCode( uint64_t p_parent_code ) ;
    
    [[nodiscard]] uint64_t getParentCode() const ;

    void setDescription( string p_description ) ;
    
    [[nodiscard]] string getDescription() const ;

    [[nodiscard]] string toStr() const override ;

} ;



#endif /* defined(__PromoCalculator__Department__) */
