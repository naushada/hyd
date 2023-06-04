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
#include <tuple>
#include <getopt.h>
#include <atomic>

#include "http.hpp"



namespace noor {

    class Service;
    class CommonResponse;
    class Uniimage;

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
            // Sync --- Usages Blocking Socket
            Unix_Data_Store_Client_Service_Sync = 1,
            
            // Async --- Usages Non-Blocking Socket
            Tcp_Device_Console_Client_Service_Async = 20,
            Tcp_Device_Console_Client_Service_Sync,
            Tcp_Device_Console_Server_Service,
            Tcp_Device_Console_Connected_Service,

            Tcp_Web_Server_Service = 40,
            Tcp_Web_Client_Connected_Service,
            Tcp_Web_Client_Proxy_Service,

            Tcp_Device_Server_Service = 60,
            Tcp_Device_Client_Connected_Service,
            Tcp_Device_Client_Service_Async,
            Tcp_Device_Client_Service_Sync
        };
}


class Uniimage {

    public:
        
        std::unique_ptr<noor::Service>& GetService(noor::ServiceType serviceType);
        std::int32_t CreateServiceAndRegisterToEPoll(noor::ServiceType serviceType, const std::string& IP, const std::uint16_t& PORT, bool isAsync);
        std::int32_t RegisterToEPoll(noor::ServiceType serviceType);
        std::int32_t DeRegisterFromEPoll(std::int32_t fd);
        std::int32_t start(std::int32_t to);
        std::inst32_t stop(std::int32_t in);
        std::int32_t init();
        Uniimage() : m_epollFd(), m_evts(), m_services() {}
        ~Uniimage() = default;


    private:
        std::int32_t m_epollFd;
        std::vector<struct epoll_event> m_evts;
        std::unordered_map<noor::ServiceType, std::unique_ptr<noor::Service>> m_services;
};

class noor::CommonResponse {
    public:
        
        ~CommonResponse() = default;

        static noor::CommonResponse& instance() {
            static noor::CommonResponse m_inst;
            return(m_inst);
        }

        auto response(std::int32_t fd) {
            return(m_responses[fd]);
        }

        void response(std::int32_t fd, std::string rsp) {
            m_responses[fd].push_back(rsp);
        }
        auto& response() {
            return(m_responses);
        }

    private:
        CommonResponse() = default;
        std::unordered_map<std::int32_t, std::vector<std::string>> m_responses;
};

class noor::Service {
    public:
        

        Service() {
            m_is_register_variable = false; 
            m_handle = -1; 
            m_message_id = 0; 
            m_response_cache.clear();
            m_connected_clients.clear();
            m_web_connections.clear();
            m_tcp_connections.clear();

        }
        Service(std::unordered_map<std::string, std::string> config) {
            if(config.empty()) {
                std::cout << "line: " << __LINE__ << " config is empty" << std::endl;
            }
            m_config = config;
            m_is_register_variable = false; 
            m_handle = -1; 
            m_message_id = 0; 
            m_response_cache.clear();
            m_connected_clients.clear();
            m_web_connections.clear();
            m_tcp_connections.clear();
        }
        virtual ~Service() {}
        void close();
        std::int32_t tcp_client(const std::string& IP, std::uint16_t PORT, bool isAsync=false);
        std::int32_t tcp_client_async(const std::string& IP, std::uint16_t PORT);
        std::int32_t udp_client(const std::string& IP, std::uint16_t PORT);
        std::int32_t uds_client(const std::string& PATH="/var/run/treemgr/treemgr.sock");
        std::int32_t tcp_server(const std::string& IP, std::uint16_t PORT);
        std::int32_t udp_server(const std::string& IP, std::uint16_t PORT);
        std::int32_t web_server(const std::string& IP, std::uint16_t PORT);
        std::int32_t start_client(std::uint32_t timeout_in_ms, std::vector<std::tuple<std::unique_ptr<Service>, ServiceType>>);
        std::int32_t start_server(std::uint32_t timeout_in_ms, std::vector<std::tuple<std::unique_ptr<Service>, ServiceType>>);
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
            return(std::string());
        }

        virtual std::int32_t onClose(std::string in) {
            std::cout << "line: " << __LINE__ << "Must be overriden " << std::endl;
            return(-1);
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

        auto& response_cache() {
            return(m_response_cache);
        }
        void add_element_to_cache(std::tuple<std::uint16_t, std::uint16_t, std::uint16_t, std::string, std::string> elm) {
            m_response_cache.push_back(elm);
        }

        void update_response_to_cache(std::int32_t id, std::string rsp) {
            auto it = std::find_if(m_response_cache.begin(), m_response_cache.end(), [&](auto& inst) {
                if(std::get<cache_element::MESSAGE_ID>(inst) == id) {
                    return(true);
                }
                return(false);
            });

	        if(it != m_response_cache.end()) {
                std::get<cache_element::RESPONSE>(*it).assign(rsp);
	        }
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

        auto& web_connections() {
            return(m_web_connections);
        }

        auto& tcp_connections() {
            return(m_tcp_connections);
        }

        auto& unix_connections() {
            return(m_unix_connections);
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
        
        std::unordered_map<std::string, std::string> get_config() const {
            return(m_config);
        }

        void set_config(std::unordered_map<std::string, std::string> cfg) {
            m_config = cfg;
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
        //type, command, message_id, prefix and response for a tuple
        std::vector<std::tuple<std::uint16_t, std::uint16_t, std::uint16_t, std::string, std::string>> m_response_cache;
        std::unordered_map<std::int32_t, client_connection> m_connected_clients;
        //key = fd, Value = <fd, IP, PORT, service_type, DestIP, RxBytes, TxBytes, timestamp>
        std::unordered_map<std::int32_t, std::tuple<std::int32_t, std::string, std::int32_t, noor::ServiceType, std::string, std::int32_t, std::int32_t, std::int32_t>> m_web_connections;
        std::unordered_map<std::int32_t, std::tuple<std::int32_t, std::string, std::int32_t, noor::ServiceType, std::string, std::int32_t, std::int32_t, std::int32_t>> m_tcp_connections;
        std::unordered_map<std::int32_t, std::tuple<std::int32_t, std::string, std::int32_t, noor::ServiceType, std::string, std::int32_t, std::int32_t, std::int32_t>> m_unix_connections;
        std::unordered_map<std::string, std::string> m_config;
        std::vector<struct epoll_event> m_epoll_evts;
        

};

class TcpClient: public noor::Service {
    public:
        TcpClient(auto cfg, auto svcType): Service(cfg) {
            if(svcType == noor::ServiceType::Tcp_Device_Console_Client_Service_Async) {
                tcp_client_async(get_config().at("server-ip"), 65344);
                std::cout << "line: " << __LINE__ << "handle: " << handle() << " console app client connection is-progress: " << connected_client(handle()) << std::endl;    
            } if(svcType == noor::ServiceType::Tcp_Web_Client_Proxy_Service) {
                tcp_client("192.168.1.1", 80, false);
                std::cout << "line: " << __LINE__ << "handle: " << handle() << " console app client connection is-progress: " << connected_client(handle()) << std::endl;    
            } else {
                tcp_client_async(get_config().at("server-ip"), std::stoi(get_config().at("server-port")));
                std::cout << "line: " << __LINE__ << "handle: " << handle() << " data store app client connection is-progress: " << connected_client(handle()) << std::endl;
            }
        }

        TcpClient(const std::string& IP, const std::uint16_t& PORT, bool isAsync) {
            tcp_client(IP, PORT, isAsync);
        }

        TcpClient(const std::int32_t& fd, const std::string& IP , const std::int32_t& PORT) {
            handle(fd);
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
        UdpClient(auto config): Service(config) {
            udp_client(get_config().at("server-ip"), std::stoi(get_config().at("server-port")));
        }
        ~UdpClient() {

        }
        virtual std::string onReceive(std::string in) override;
        virtual std::int32_t onClose(std::string in) override;
};


class TcpServer: public noor::Service {
    public:
        TcpServer(auto config, auto svcType) : Service(config) {

            std::string sIP("127.0.0.1");
            auto it = std::find_if(get_config().begin(), get_config().end(), [] (const auto& ent) {return(!ent.first.compare("server-ip"));});

            if(it != get_config().end()) {
                sIP.assign(it->second);
            }

            if(noor::ServiceType::Tcp_Device_Server_Service == svcType) {
                tcp_server(sIP, 65344);    
            } else {
                tcp_server(sIP, std::stoi(get_config().at("server-port")));
            }
        }

        ~TcpServer() {}
        virtual std::string onReceive(std::string in) override;
        virtual std::int32_t onClose(std::string in) override;
};


class UdpServer: public noor::Service {
    public:
        UdpServer(auto config) : Service(config) {
            udp_server(get_config().at("server-ip"), std::stoi(get_config().at("server-port")));
        }
        ~UdpServer() {}
        virtual std::string onReceive(std::string in) override;
        virtual std::int32_t onClose(std::string in) override;
};


class WebServer: public noor::Service {
    public:
        WebServer(auto config, noor::ServiceType svcType) : Service(config) {

            std::string sIP("127.0.0.1");
            auto it = std::find_if(get_config().begin(), get_config().end(), [] (const auto& ent) {return(!ent.first.compare("server-ip"));});

            if(it != get_config().end()) {
                sIP.assign(it->second);
            }
            if(svcType == noor::ServiceType::Tcp_Web_Server_Service) {
                web_server(sIP, std::stoi(get_config().at("web-port")));
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
