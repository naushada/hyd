#ifndef __http_hpp__
#define __http_hpp__

#include <iostream>
#include <unordered_map>
#include <sstream>
#include <string>
#include <algorithm>
#include "json.hpp"

namespace noor {
    class Http;
}

class noor::Http {
    public:
        Http() {
            m_uri.clear();
            m_params.clear();
        }

        Http(const std::string& in) {
            m_uri.clear();
            m_params.clear();
            m_header.clear();
            m_body.clear();

            m_header = get_header(in);

            do {
                if(m_header.length()) {
                    
                    if(!m_header.compare(0, 8, "HTTP/1.1")) {
                        //this is a response.
                        auto res = m_header.find(' ', 9);
                        if(res != std::string::npos) {
                            m_status_code = m_header.substr(9, res);
                            std::cout << __TIMESTAMP__ << " line: " << __LINE__ << " status_code: " << m_status_code << std::endl;
                        }

                        parse_header(in);
                        m_body = get_body(in);
                        break;
                    }
                    parse_uri(m_header);
                    parse_header(in);
                }

                m_body = get_body(in);
            }while(0);
        }

        ~Http() {
            m_params.clear();
        }

        std::string method() {
            return(m_method);
        }

        void method(std::string _method) {
            m_method = _method;
        }

        std::string uri() const {
            return(m_uri);
        }

        void uri(std::string _uri) {
            m_uri = _uri;
        }

        void add_element(std::string key, std::string value) {
            m_params.insert(std::pair(key, value));
        }

        void dump() {
            std::for_each(m_params.begin(), m_params.end(), [&](const auto& ent) {
                std::cout << "key: " << ent.first << " value: " << ent.second << std::endl;
            });
        }

        std::string value(const std::string& key) {
            auto it = m_params.find(key);
            if(it != m_params.end()) {
                return(it->second);
            }
            return std::string();
        }

        std::string body() {
            return m_body;
        }

        void body(std::string in) {
            m_body = in;
        }

        std::string header() {
            return m_header;
        }

        void header(std::string in) {
            m_header = in;
        }

        std::string status_code() const {return m_status_code;}
        void status_code(std::string code) {m_status_code = code;}

        void format_value(const std::string& param);
        void parse_uri(const std::string& in);
        void parse_header(const std::string& in);
        std::string get_header(const std::string& in);
        std::string get_body(const std::string& in);

    private:
        std::unordered_map<std::string, std::string> m_params;
        std::string m_uri;
        std::string m_header;
        std::string m_body;
        std::string m_method;
        std::string m_status_code;
};

#endif /* __http_hpp__*/
