#ifndef __uniimage__hpp__
#define __uniimage__hpp__

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <signal.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cerrno>
#include <clocale>
#include <cmath>
#include <cstring>

#include <vector>
#include <iostream>
#include <fstream>
#include <memory>
#include <thread>
#include <sstream>
#include <unordered_map>
#include <map>
#include <tuple>
#include <getopt.h>
#include <atomic>
#include <future>

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/ossl_typ.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

#include "http.hpp"



namespace noor {

    class Service;
    class CommonResponse;
    class Uniimage;
    class Tls;
    class  RestClient;

    struct response {
        std::uint16_t type;
        std::uint16_t command;
        std::uint16_t messages_id;
    };

            //EMP (Embedded Micro Protocol) parser
        enum EMP_COMMAND_TYPE : std::uint16_t {
           Request = 0,
           Command_OR_Notification = 1,
           Response = 2,
        };
        enum EMP_COMMAND_ID : std::uint16_t {
           RegisterGetVariable     = 104,          /**< Register to get notified immediately and when a path or sub path changes. */
           RegisterVariable        = 105,          /**< Register to get notified when a path or sub path changes. */
           UnregisterVariable      = 106,          /**< Unregister a previously registered notification. */
           NotifyVariable          = 107,          /**< Notify a change in the monitored values. */
           ExecVariable            = 108,          /**< Execute a node. */
           RegisterExec            = 109,          /**< Register to get notified when a path is executed. */
           NotifyExec              = 110,          /**< Notify that a path is executed. */
           GetVariable             = 113,          /**< Recursively get values in a single request. */
           SingleGetVariable       = 114,          /**< Get a single path value. */
           SetVariable             = 115,          /**< Set one or more values. */
           ListVariable            = 116,          /**< List direct children of a branch path. */
           NotifyFd                = 200,          /**< Notify file descriptor passing. */
        };

        struct emp {
            emp() : m_type(0), m_command(0), m_message_id(0), m_response_length(0), m_response("") {}
            ~emp() {}
            std::uint16_t m_type;
            std::uint16_t m_command;
            std::uint16_t m_message_id;
            std::uint32_t m_response_length;
            std::string m_response;
        };

        enum client_connection: std::uint16_t {
            Disconnected = 0,
            Inprogress,
            Connected
        };

        enum cache_element: std::uint32_t {
            CMD_TYPE = 0,
            CMD = 1,
            MESSAGE_ID = 2,
            PREFIX = 3,
            RESPONSE = 4
        };

        enum ServiceType: std::uint32_t {

            Tcp_DeviceMgmtServer_Server_Console_Service = 10, //server will be running on DeviceMgmtServer
            Tcp_DeviceMgmtServer_Client_Console_Service_Async, //client will be running on Gateway
            Tcp_DeviceMgmtServer_Client_Console_Service_Sync, //client will be running on Gateway
            Tcp_Device_MgmtServer_Client_Console_Connected_Service, //client is connected between DeviceMgmtServer and Gateway.
            

            Tcp_DeviceMgmtServer_Web_Server_Service = 50, //Web Server will be running on DeviceMgmtServer.
            Tcp_DeviceMgmtServer_Web_Client_Connected_Service, //WebClient connected to WebServer Running on DeviceMgmt Server.
            
            Tcp_DeviceMgmtServer_Server_Gateway_Service = 60, //Tcp Server will be running on DeviceMgmtServer.
            Tcp_DeviceMgmtServer_Client_Gateway_Service_Async, // Tcp Asyn client is running on Gateway.
            Tcp_DeviceMgmtServer_Client_Gateway_Service_Sync, //Tcp Sync client is running on Gateway.
            Tcp_DeviceMgmtServer_Client_Gateway_Connected_Service, //Tcp Client connected between DeviceMgmtServer and Gateway.
            
            Tls_Tcp_DeviceMgmtServer_Server_Gateway_Service = 80, //Tls over Tcp Server will be running on DeviceMgmtServer.
            Tls_Tcp_DeviceMgmtServer_Client_Gateway_Service_Async, //Tls over Tcp Async client will be running on Gateway. 
            Tls_Tcp_DeviceMgmtServer_Client_Gateway_Service_Sync, //Tls over Tcp sync client will be running on Gateway.
            Tls_Tcp_DeviceMgmtServer_Client_Gateway_Connected_Service, //Tls over Tcp client connected between DeviceMgmtServer and Gateway.

            Tls_Udp_DeviceMgmtServer_Server_Gateway_Service = 100, //Tls over Udp Server will be running on DeviceMgmtServer.
            Tls_Udp_DeviceMgmtServer_Client_Gateway_Service_Async, //Tls over Udp Async client will be running on Gateway. 
            Tls_Udp_DeviceMgmtServer_Client_Gateway_Service_Sync, //Tls over Udp sync client will be running on Gateway.
            Tls_Udp_DeviceMgmtServer_Client_Gateway_Connected_Service, //Tls over Udp client connected between DeviceMgmtServer and Gateway.

            Tls_Tcp_Rest_Client_For_Gateway_Service_Async = 200, //Tls over Tcp Async Rest Client will be running on Gateway.
            Tls_Tcp_Rest_Client_For_Gateway_Service_Sync, //Tls over Tcp sync Rest Client will be running on Gateway.
        };
}


class noor::Uniimage {

    public:
        noor::Service* GetService(noor::ServiceType serviceType, const std::string& serialNumber);
        noor::Service* GetService(noor::ServiceType serviceType);
        noor::Service* GetService(noor::ServiceType serviceType, const std::int32_t& channel);
        void DeleteService(noor::ServiceType serviceType, const std::int32_t& channel);
        void DeleteService(noor::ServiceType serviceType);
        //For Unix socket IP, PORT and isAsync is don't care.
        std::int32_t CreateServiceAndRegisterToEPoll(noor::ServiceType serviceType, const std::string& IP="127.0.0.1", const std::uint16_t& PORT=65344, bool isAsync=false);
        std::int32_t RegisterToEPoll(noor::ServiceType serviceType, std::int32_t channel);
        std::int32_t DeRegisterFromEPoll(std::int32_t fd);
        std::int32_t start(std::int32_t to);
        std::int32_t stop(std::int32_t in);
        std::int32_t init();

        auto& getResponseCache() {
            return(m_cache);
        }

        Uniimage() : m_epollFd(-1), m_evts(), m_services(), m_cache() {}
        ~Uniimage() {
            ::close(m_epollFd);
            m_services.clear();
            m_cache.clear();
            m_evts.clear();
            m_config.clear();
        }

        std::unordered_map<std::string, std::string> get_config() const {return(m_config);}
        void set_config(std::unordered_map<std::string, std::string> cfg) {m_config = cfg;}

    private:
        std::int32_t m_epollFd;
        std::vector<struct epoll_event> m_evts;
        std::multimap<noor::ServiceType, std::unique_ptr<noor::Service>> m_services;
        //The key is serial number of device. and value is json object.
        std::unordered_map<std::string, std::string> m_cache;
        std::unordered_map<std::string, std::string> m_config;
};

/**
 * @brief 
 * 
 */
class noor::Tls {
    public:
        /**
         * @brief Construct a new Tls object
         * 
         */
        Tls(): m_method(nullptr), m_ssl_ctx(nullptr, SSL_CTX_free), m_ssl(nullptr, SSL_free) {
#if 0
            OpenSSL_add_all_algorithms();
            SSL_load_error_strings();
            /* ---------------------------------------------------------- *
             * Disabling SSLv2 will leave v3 and TSLv1 for negotiation    *
             * ---------------------------------------------------------- */
            SSL_CTX_set_options(m_ssl_ctx.get(), SSL_OP_NO_SSLv2);
#endif
        }

        Tls(bool role): m_method(SSLv23_server_method()), 
                        m_ssl_ctx(SSL_CTX_new(m_method), SSL_CTX_free),
                        m_ssl(SSL_new(m_ssl_ctx.get()), SSL_free) {

            OpenSSL_add_all_algorithms();
            SSL_load_error_strings();
            /* ---------------------------------------------------------- *
             * Disabling SSLv2 will leave v3 and TSLv1 for negotiation    *
             * ---------------------------------------------------------- */
            SSL_CTX_set_options(m_ssl_ctx.get(), SSL_OP_NO_SSLv2);
        }

        ~Tls() {
            
        }

        /**
         * @brief 
         * 
         * @param fd 
         * @param cert 
         * @param pkey 
         * @return std::int32_t 
         */
        std::int32_t init(std::int32_t fd) {
            m_method = SSLv23_client_method();

            m_ssl_ctx = std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)>(nullptr, SSL_CTX_free);
            m_ssl_ctx.reset(SSL_CTX_new(m_method));

            m_ssl = std::unique_ptr<SSL, decltype(&SSL_free)>(nullptr, SSL_free);
            m_ssl.reset(SSL_new(m_ssl_ctx.get()));

            OpenSSL_add_all_algorithms();
            SSL_load_error_strings();
            /* ---------------------------------------------------------- *
             * Disabling SSLv2 will leave v3 and TSLv1 for negotiation    *
             * ---------------------------------------------------------- */
            SSL_CTX_set_options(m_ssl_ctx.get(), SSL_OP_NO_SSLv2);

            std::int32_t rc = SSL_set_fd(m_ssl.get(), fd);
            
            return(rc);
        }

        std::int32_t init(const std::string &cert="../cert/cert.pem", const std::string& pkey="../cert/pkey.pem") {
            std::int32_t ret = -1;
            //m_method = SSLv23_server_method();
            m_method = TLSv1_2_server_method();
            m_ssl_ctx = std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)>(nullptr, SSL_CTX_free);
            m_ssl_ctx.reset(SSL_CTX_new(m_method));

            m_ssl = std::unique_ptr<SSL, decltype(&SSL_free)>(nullptr, SSL_free);
            m_ssl.reset(SSL_new(m_ssl_ctx.get()));

            OpenSSL_add_all_algorithms();
            SSL_load_error_strings();
            /* ---------------------------------------------------------- *
             * Disabling SSLv2 will leave v3 and TSLv1 for negotiation    *
             * ---------------------------------------------------------- */
            SSL_CTX_set_options(m_ssl_ctx.get(), SSL_OP_NO_SSLv2);

            //For tls server
            if(cert.length() && pkey.length()) {

                if((ret = SSL_CTX_use_certificate_file(m_ssl_ctx.get(), cert.c_str(), SSL_FILETYPE_PEM)) <= 0) {
                    ERR_print_errors_fp(stderr);
                    return(ret);
                }

                if((ret = SSL_CTX_use_PrivateKey_file(m_ssl_ctx.get(), pkey.c_str(), SSL_FILETYPE_PEM)) <= 0 ) {
                    ERR_print_errors_fp(stderr);
                    return(ret);
                }
            }

            return(ret);
        }
        /**
         * @brief 
         * 
         * @return std::int32_t 
         */
        std::int32_t client() {
            std::int32_t rc = SSL_connect(m_ssl.get());
            return(rc);
        }

        /**
         * @brief 
         * 
         * @return std::int32_t 
         */
        std::int32_t server(std::int32_t fd) {
            std::int32_t rc = -1;
            rc = SSL_set_fd(m_ssl.get(), fd);
            rc = SSL_accept(m_ssl.get());
            std::cout << __TIMESTAMP__ << " line: " << __LINE__ << " SSL_accept return: " << rc << std::endl;
            ERR_print_errors_fp(stderr);
            return(rc);
        }

        /**
         * @brief 
         * 
         * @param out 
         * @param len 
         * @return std::int32_t 
         */
        std::int32_t peek(std::string& out, std::uint32_t len = 2048) {
            int rc = -1;
            std::array<char, 2048> ss;
            ss.fill(0);

            rc = SSL_peek(m_ssl.get(), ss.data(), len);

            if(rc > 0) {
                out.assign(ss.data(), rc);
            }
            return(rc);

        }/*peek*/

        /**
         * @brief 
         * 
         * @param out 
         * @param len 
         * @return std::int32_t 
         */
        std::int32_t read(std::string& out, std::uint32_t len = 2048) {
            std::int32_t rc = -1;
            std::array<char, 2048> in;
            in.fill(0);

            if(len == 2048) {
                rc = SSL_read(m_ssl.get(), in.data(), len);
                if(rc < 0) {
                    return(rc);
                }
                out.assign(in.data(), rc);
                return(rc);
            }

            std::stringstream ss;
            std::uint32_t offset = 0;
            std::string tmp;

            if(len < 2048) {
                do {
                    in.fill(0);
                    rc = SSL_read(m_ssl.get(), in.data(), len - offset);

                    if(rc < 0) {
                        return(rc);
                    }

                    offset += rc;
                    tmp.assign(in.data(), rc);
                    ss << tmp;

                }while(len != offset);
            } else {
                do {
                    in.fill(0);
                    rc = SSL_read(m_ssl.get(), in.data(), in.size());

                    if(rc < 0) {
                        return(rc);
                    }

                    offset += rc;
                    tmp.assign(in.data(), rc);
                    ss << tmp;

                }while(len != offset);
            }
            out.assign(ss.str());
            return(offset);

        }/*read*/

        /**
         * @brief 
         * 
         * @param out 
         * @return std::int32_t 
         */
        std::int32_t write(const std::string& out) {
            std::int32_t rc = -1;
            size_t offset = 0;
            auto len = out.length();

            do {
                rc = SSL_write(m_ssl.get(), out.data() + offset, len - offset);

                if(rc < 0) {
                    break;
                }

                offset += rc;
            } while(len != offset);

            return(offset);

        }/*write*/

        /**
         * @brief 
         * 
         * @return auto& 
         */
        auto& ssl_ctx() {
            return(*(m_ssl_ctx.get()));
        }

        /**
         * @brief 
         * 
         * @return auto& 
         */
        auto& ssl() {
            return(*(m_ssl.get()));
        }

    private:
        const SSL_METHOD *m_method;
        std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> m_ssl_ctx;
        std::unique_ptr<SSL, decltype(&SSL_free)> m_ssl;
};

/**
 * @brief 
 * 
 */
class noor::RestClient {
    public:
        RestClient() : m_cookies(""), m_uri(""), m_deviceName(""), m_pending_request(false), m_promise() {}
        ~RestClient() {}
        std::string getToken(const std::string& in);
        std::string authorizeToken(const std::string& in, const std::string& user);
        std::string registerDatapoints(const std::vector<std::string>& dps);
        std::string buildRequest(const std::string& in, std::vector<std::string> param = {});
        std::string processResponse(const std::string& http_header, const std::string& http_body, std::string &svc);
        std::string getEvents(std::string uri="/api/v1/events");
        std::string uri() const {return(m_uri);}
        void uri(std::string path) { m_uri = path;}
        std::string cookies() const { return(m_cookies);}
        void cookies(std::string token) {m_cookies = token;}
        void deviceName(std::string product) {m_deviceName = product;}
        std::string deviceName() const { return(m_deviceName);}
        std::string status_code() {return m_status_code;}
        void status_code(std::string code) {m_status_code = code;}
        bool pending_request() const {return m_pending_request;}
        void pending_request(bool status) { m_pending_request = status;}
        auto& promise() {return m_promise;}
        void promise(const auto& pr) {m_promise = pr;}

    private:
        std::string m_cookies;
        std::string m_uri;
        std::string m_deviceName;
        std::string m_status_code;
        bool m_pending_request;
        std::promise<void> m_promise;
};

class noor::Service {
    public:

        Service() {
            m_is_register_variable = false; 
            m_handle = -1; 
            m_message_id = 0; 
            m_connected_clients.clear();
        }
        Service(std::unordered_map<std::string, std::string> config) {
            if(config.empty()) {
                std::cout << "line: " << __LINE__ << " config is empty" << std::endl;
            }
            
            m_is_register_variable = false; 
            m_handle = -1; 
            m_message_id = 0; 
            m_connected_clients.clear();
        }

        virtual ~Service() {}
        std::int32_t tcp_client(const std::string& IP, std::uint16_t PORT, std::int32_t& channel, bool isAsync=false);
        std::int32_t udp_client(const std::string& IP, std::uint16_t PORT);
        std::int32_t uds_client(const std::string& PATH="/var/run/treemgr/treemgr.sock");
        std::int32_t tcp_server(const std::string& IP, std::uint16_t PORT, std::int32_t& channel);
        std::int32_t udp_server(const std::string& IP, std::uint16_t PORT);
        std::int32_t web_server(const std::string& IP, std::uint16_t PORT);
        std::int32_t tcp_rx(std::string& data);
        std::int32_t tcp_rx(std::int32_t channel, std::string& data);
        std::int32_t tcp_rx(std::int32_t channel, std::string& data, ServiceType svcType);
        emp uds_rx();
        std::int32_t web_rx(std::string& data);
        std::int32_t web_rx(std::int32_t fd, std::string& data);
        std::int32_t web_tx(std::int32_t channel, const std::string& req);
        std::int32_t udp_rx(std::string& data);
        
        std::int32_t web_tx(const std::string& data);
        std::int32_t udp_tx(const std::string& data);
        std::int32_t uds_tx(const std::string& data);
        std::int32_t tcp_tx(const std::string& data);
        std::int32_t tcp_tx(std::int32_t channel, const std::string& data);
        std::string serialise(noor::EMP_COMMAND_TYPE cmd_type, noor::EMP_COMMAND_ID cmd, const std::string& req);
        std::string packArguments(const std::string& prefix, std::vector<std::string> fields = {}, std::vector<std::string> filter = {});
        std::int32_t registerGetVariable(const std::string& prefix, std::vector<std::string> fields = {}, std::vector<std::string> filter = {});
        std::int32_t getSingleVariable(const std::string& prefix);
        std::int32_t getVariable(const std::string& prefix, std::vector<std::string> fields = {}, std::vector<std::string> filter = {});
        std::string build_web_response(Http& http);
        std::string process_web_request(const std::string& req);
        std::string handleGetMethod(Http& http);
        std::string buildHttpResponse(Http& http, const std::string& rsp_body);
        std::string handleOptionsMethod(Http& http);
        std::string buildHttpRedirectResponse(Http& http, std::string rsp_body = "");
        std::string buildHttpResponseOK(Http& http, std::string body, std::string contentType);
        std::string get_contentType(std::string ext);

        virtual std::string onReceive(std::string in) {
            std::cout << "line: " << __LINE__ << "Must be overriden " << std::endl;
            return(in);
        }

        virtual std::int32_t onClose(std::string in) {
            std::cout << "line: " << __LINE__ << "Must be overriden " << std::endl;
            return(in.length());
        }

        void ip(std::string IP) {
            m_ip = IP;
        }

        std::string ip() const {
            return(m_ip);
        }

        void port(std::uint16_t _p) {
            m_port = _p;
        }

        std::uint16_t port() const {
          return(m_port);
        }

        std::int32_t handle() const {
            return(m_handle);
        }

        void handle(std::int32_t fd) {
            m_handle = fd;
        }

        std::string uds_socket_name() const {
            return(m_uds_socket_name);
        }

        void uds_socket_name(std::string uds_name) {
            m_uds_socket_name = uds_name;
        }
        void connected_client(client_connection st) {
            //m_connected_clients.insert(std::make_pair(handle(), st));
            m_connected_clients[handle()] = st;
        }

        auto& connected_client() {
            return(m_connected_clients);
        }

        auto connected_client(std::int32_t channel) {
            return(m_connected_clients[channel]);
        }

        auto& inet_server() {
            return(m_inet_server);
        }

        auto& inet_peer() {
            return(m_inet_peer);
        }

        auto& un_server() {
            return(m_un_server);
        }
        
        bool is_register_variable() const {
            return(m_is_register_variable);
        }
        void is_register_variable(bool yes) {
            m_is_register_variable = yes;
        }

        std::atomic<std::uint16_t>& message_id() {
            return(m_message_id);
        }

        Tls& tls() {
            return(m_tls);
        }

        RestClient& restC() {
            return(m_restC);
        }

        std::string serialNo() const {
            return(m_serialNumber);
        }

        void serialNumber(std::string srNo) {
            m_serialNumber = srNo;
        }

    private:
        std::atomic<std::uint16_t> m_message_id;
        bool m_is_register_variable;
        std::string m_uds_socket_name;
        std::string m_ip;
        std::uint16_t m_port;
        //file descriptor
        std::int32_t m_handle;
        //INET socket address
        struct sockaddr_in m_inet_server;
        struct sockaddr_in m_inet_peer;
        // UNIX socket address 
        struct sockaddr_un m_un_server;
        std::unordered_map<std::int32_t, client_connection> m_connected_clients;

        
        std::vector<struct epoll_event> m_epoll_evts;
        noor::Tls m_tls;
        noor::RestClient m_restC;
        std::string m_serialNumber;
};

class TcpClient: public noor::Service {
    public:
        #if 0
        TcpClient(auto config, auto svcType): Service() {
            std::string BRIP("192.168.1.1");
            if(svcType == noor::ServiceType::Tcp_Device_Console_Client_Service_Async) {
                tcp_client(config.at("server-ip"), 65344, true);
                std::cout << "line: " << __LINE__ << "handle: " << handle() << " console app client connection is-progress: " << connected_client(handle()) << std::endl;

            } if(svcType == noor::ServiceType::Tcp_Web_Client_Proxy_Service) {
                tcp_client(BRIP, 80, false);
                std::cout << "line: " << __LINE__ << "handle: " << handle() << " console app client connection is-progress: " << connected_client(handle()) << std::endl;

            } else if(svcType == noor::ServiceType::Tls_Tcp_Device_Rest_Client_Service_Sync) {
                tcp_client(BRIP, 443, false);

                if(connected_client(handle()) == noor::client_connection::Connected) {
                    tls().init(handle());
                    tls().client();
                }

                std::cout << "line: " << __LINE__ << " handle: " << handle() << " TLS Client Sync: " << connected_client(handle()) << std::endl;

            }  else {
                tcp_client(config.at("server-ip"), std::stoi(config.at("server-port")), true);
                std::cout << "line: " << __LINE__ << " handle: " << handle() << " async client connection is-progress: " << connected_client(handle()) << std::endl;
            }
        }
        #endif
        TcpClient(const std::string& IP, const std::uint16_t& PORT, std::int32_t& channel, bool isAsync) {
            tcp_client(IP, PORT, channel, isAsync);
        }

        TcpClient(const std::int32_t& fd, const std::string& IP , const std::int32_t& PORT) {
            handle(fd);
            //learn them for future
            ip(IP);
            port(PORT);
            connected_client(noor::client_connection::Connected);
        }

        ~TcpClient() {}
        virtual std::string onReceive(std::string in) override;
        virtual std::int32_t onClose(std::string in) override;
};


class UnixClient: public noor::Service {
    public:
        UnixClient(): Service() {
            uds_client();
        }
        ~UnixClient() {

        }
        virtual std::string onReceive(std::string in) override;
        virtual std::int32_t onClose(std::string in) override;
};


class UdpClient: public noor::Service {
    public:
        UdpClient(auto config): Service() {
            udp_client(config.at("server-ip"), std::stoi(config.at("server-port")));
        }
        ~UdpClient() {

        }
        virtual std::string onReceive(std::string in) override;
        virtual std::int32_t onClose(std::string in) override;
};


class TcpServer: public noor::Service {
    public:
    #if 0
        TcpServer(auto config, auto svcType) : Service() {

            std::string sIP("127.0.0.1");
            auto it = std::find_if(config.begin(), config.end(), [] (const auto& ent) {return(!ent.first.compare("server-ip"));});

            if(it != config().end()) {
                sIP.assign(it->second);
            }

            if(noor::ServiceType::Tcp_Device_Console_Server_Service == svcType) {
                tcp_server(sIP, 65344);    
            } else {
                tcp_server(sIP, std::stoi(config.at("server-port")));
            }
        }
        #endif

        TcpServer(const std::string& IP, const std::uint16_t& PORT, std::int32_t& channel) {
            tcp_server(IP, PORT, channel);
        }
        ~TcpServer() {}
        virtual std::string onReceive(std::string in) override;
        virtual std::int32_t onClose(std::string in) override;
};


class UdpServer: public noor::Service {
    public:
        UdpServer(std::string IP, std::int32_t PORT) : Service() {
            udp_server(IP, PORT);
        }
        ~UdpServer() {}
        virtual std::string onReceive(std::string in) override;
        virtual std::int32_t onClose(std::string in) override;
};


class WebServer: public noor::Service {
    public:
        WebServer(auto config, noor::ServiceType svcType) : Service() {

            std::string sIP("127.0.0.1");
            auto it = std::find_if(config.begin(), config.end(), [] (const auto& ent) {return(!ent.first.compare("server-ip"));});

            if(it != config.end()) {
                sIP.assign(it->second);
            }
            if(svcType == noor::ServiceType::Tcp_DeviceMgmtServer_Web_Server_Service) {
                web_server(sIP, std::stoi(config.at("web-port")));
            }
        }
        ~WebServer() {}

        virtual std::string onReceive(std::string in) override;
        virtual std::int32_t onClose(std::string in) override;
};


class UnixServer: public noor::Service {
    public:
        UnixServer() : Service() {}
        ~UnixServer() {}
        virtual std::string onReceive(std::string in) override;
        virtual std::int32_t onClose(std::string in) override;
};

#endif /* __uniimage__hpp__ */
