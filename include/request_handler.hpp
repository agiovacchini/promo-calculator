//
// request_handler.hpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2008 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef HTTP_SERVER3_REQUEST_HANDLER_HPP
#define HTTP_SERVER3_REQUEST_HANDLER_HPP

#include <string>
#include <boost/noncopyable.hpp>
#include "base_system.h"

namespace http {
    namespace server3 {
        
        struct reply;
        struct request;
        
        /// The common handler for all incoming requests.
        class request_handler
        : private boost::noncopyable
        {
        public:
            /// Construct with a directory containing files to be served.
            explicit request_handler(const std::string& doc_root, BaseSystem* pBaseSystem);
            
            /// Handle a request and produce a reply.
            void handle_request(const request& req, reply& rep);
            
        private:
            /// The directory containing the files to be served.
            std::string doc_root_;
            BaseSystem* baseSystem;
            /// Perform URL-decoding on a string. Returns false if the encoding was
            /// invalid.
            static bool url_decode(const std::string& in, std::string& out);
        };
        
    } // namespace server3
} // namespace http

#endif // HTTP_SERVER3_REQUEST_HANDLER_HPP