//
//  BaseTypes.cpp
//  PromoCalculator
//
//  Created by Andrea Giovacchini on 03/03/14.
//  Copyright (c) 2014 Andrea Giovacchini. All rights reserved.
//

#include "base_types.h"

bool fileMove(std::string pFileOri, std::string pFileDest)
{
    boost::filesystem::path src(pFileOri);
    boost::filesystem::path dest(pFileDest);
    
    try {
        boost::filesystem::rename(src, dest);
    }
    catch (std::exception const& e)
    {
        return false;
    }
    return boost::filesystem::exists(dest);
}

bool fileDelete(std::string p_file_name)
{
    boost::filesystem::path fileToDelete(p_file_name);
    
    try {
        if (boost::filesystem::exists(fileToDelete))
        {
            boost::filesystem::remove(fileToDelete);
        }
    }
    catch (std::exception const& e)
    {
        return false;
    }
    return !boost::filesystem::exists(fileToDelete);
}
