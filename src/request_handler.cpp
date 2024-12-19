//
// request_handler.cpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2008 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "base_types.h"


#include "request_handler.hpp"
#include <sstream>
#include <string>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include "mime_types.hpp"
#include "reply.hpp"
#include "request.hpp"


namespace http {
    namespace server3 {
        
        request_handler::request_handler(const std::string& doc_root, BaseSystem* pBaseSystem)
        : doc_root_(doc_root), baseSystem(pBaseSystem)
        {
			        }
        
        void request_handler::handle_request(const request& req, reply& rep)
        {
            // Decode url to path.
            std::string request_path;
            if (!url_decode(req.uri, request_path))
            {
                rep = reply::stock_reply(reply::bad_request);
                return;
            }
            
            // Request path must be absolute and not contain "..".
            if (request_path.empty() || request_path[0] != '/'
                || request_path.find("..") != std::string::npos)
            {
                rep = reply::stock_reply(reply::bad_request);
                return;
            }
            
            // If path ends in slash (i.e. is a directory) then add "index.html".
            if (request_path[request_path.size() - 1] == '/')
            {
                request_path += "index.html";
            }
            
            // Determine the file extension.
            std::size_t last_slash_pos = request_path.find_last_of("/");
            std::size_t last_dot_pos = request_path.find_last_of(".");
            std::string extension;
            if (last_dot_pos != std::string::npos && last_dot_pos > last_slash_pos)
            {
                extension = request_path.substr(last_dot_pos + 1);
            }
            
            std::string servlet = "" ;
            std::string servletFunction = "" ;
            std::string servletFunctionAction = "" ;
            std::string servletFunctionPar = "" ;
            keys_and_values<std::string::iterator> paramsParser;    // create instance of parser
            std::map<std::string, std::string> urlParamsMap;        // map to receive results

            int actionBS = WEBI_ACTION_NOT_RECOGNIZED ;
            
            boost::regex exrpWithParams( "(/(.*))(/(.*))(/(.*))([?]+)((.*))" );
            boost::regex exrpNoParams( "(/(.*))(/(.*))(/(.*))" );
            
            boost::match_results<std::string::const_iterator> what;
            if( regex_search( request_path, what, exrpWithParams ) ) {
                servlet = std::string( what[2].first, what[2].second );
                servletFunction = std::string( what[4].first, what[4].second );
                servletFunctionAction = std::string( what[6].first, what[6].second );
                servletFunctionPar = std::string( what[8].first, what[8].second );
                std::string::iterator begin = servletFunctionPar.begin();
                std::string::iterator end = servletFunctionPar.end();
                bool result = qi::parse(begin, end, paramsParser, urlParamsMap);   // returns true if successful

                if (servletFunctionAction.compare("initSession")==0)
                {
                    actionBS = WEBI_SESSION_INIT ;
                }
                
                if (servletFunctionAction.compare("endSession")==0)
                {
                    actionBS = WEBI_SESSION_END ;
                }
                
                if (servletFunctionAction.compare("voidTransaction")==0)
                {
                    actionBS = WEBI_SESSION_VOID ;
                }
                
                if (servletFunctionAction.compare("addCustomer")==0)
                {
                    actionBS = WEBI_CUSTOMER_ADD ;
                }
                
                if (servletFunctionAction.compare("voidCustomer")==0)
                {
                    actionBS = WEBI_CUSTOMER_VOID ;
                }
                
                if (servletFunctionAction.compare("addItem")==0)
                {
                    actionBS = WEBI_ITEM_ADD ;
                }
               
                if (servletFunctionAction.compare("voidItem")==0)
                {
                    actionBS = WEBI_ITEM_VOID ;
                }

                if (servletFunctionAction.compare("getItemInfo")==0)
                {
                    actionBS = WEBI_ITEM_INFO ;
                }
                
                if (servletFunctionAction.compare("getTotals")==0)
                {
                    actionBS = WEBI_GET_TOTALS ;
                }
                
                if (servletFunctionAction.compare("getAllCart")==0)
                {
                    actionBS = WEBI_GET_ALL_CART ;
                }
                
                if (servletFunctionAction.compare("getAllCartWithBarcodes")==0)
                {
                    actionBS = WEBI_GET_ALL_CART_WITH_BARCODES ;
                }
                
                if (servletFunctionAction.compare("manageRescan")==0)
                {
                    actionBS = WEBI_MANAGE_RESCAN ;
                }
            } else if ( regex_search( request_path, what, exrpNoParams ) ) {
                servlet = std::string( what[2].first, what[2].second );
                servletFunction = std::string( what[4].first, what[4].second );
                servletFunctionAction = std::string( what[6].first, what[6].second );
                
                
                if (servletFunctionAction.compare("getStoreInfo")==0)
                {
                    actionBS = WEBI_GET_STORE_INFO ;
                }
 
                if (servletFunctionAction.compare("getCartsList")==0)
                {
                    actionBS = WEBI_GET_CARTS_LIST ;
				}
            }

            rep.content.append(baseSystem->salesActionsFromWebInterface(actionBS, urlParamsMap));
            
            // Fill out the reply to be sent to the client.
            rep.status = reply::ok;
                        
            rep.headers.resize(2);
            rep.headers[0].name = "Content-Length";
            rep.headers[0].value = boost::lexical_cast<std::string>(rep.content.size());
            rep.headers[1].name = "Content-Type";
            rep.headers[1].value = mime_types::extension_to_type(extension);
        }

        bool request_handler::url_decode(const std::string& in, std::string& out)
        {
            out.clear();
            out.reserve(in.size());
            for (std::size_t i = 0; i < in.size(); ++i)
            {
                if (in[i] == '%')
                {
                    if (i + 3 <= in.size())
                    {
                        int value = 0;
                        std::istringstream is(in.substr(i + 1, 2));
                        if (is >> std::hex >> value)
                        {
                            out += static_cast<char>(value);
                            i += 2;
                        }
                        else
                        {
                            return false;
                        }
                    }
                    else
                    {
                        return false;
                    }
                }
                else if (in[i] == '+')
                {
                    out += ' ';
                }
                else
                {
                    out += in[i];
                }
            }
            return true;
        }
        
    } // namespace server3
} // namespace http