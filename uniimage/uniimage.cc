#ifndef __uniimage__cc__
#define __uniimage__cc__

/**
 * @file uniimage.cc
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-05-06
 * 
 * @copyright Copyright (c) 2023
 *
 _   _ _ __ (_|_)_ __ ___   __ _  __ _  ___ 
| | | | '_ \| | | '_ ` _ \ / _` |/ _` |/ _ \
| |_| | | | | | | | | | | | (_| | (_| |  __/
 \__,_|_| |_|_|_|_| |_| |_|\__,_|\__, |\___|
                                 |___/     
 */

#include "uniimage.hpp"
#include "http.hpp"

using json = nlohmann::json;

/**
 * @brief 
 * 
 * @return std::int32_t 
 */
std::int32_t noor::Uniimage::init() {
    m_epollFd = ::epoll_create1(EPOLL_CLOEXEC);
    if(m_epollFd < 0) {
        // Creation of epoll instance is failed.
        if(EMFILE == errno) {
            std::cout << "line: " << __LINE__ << " Unable to create epoll instance errno: EMFILE open file descriptor limits for a process is reached" << std::endl;
        } else if(ENFILE == errno) {
            std::cout << "line: " << __LINE__ << " Unable to create epoll instance errno: ENFILE open files limit is reached" << std::endl;

        } else if(ENOMEM == errno) {
            std::cout << "line: " << __LINE__ << " Unable to create epoll instance errno: ENOMEM no memory to create kernel object" << std::endl;
        } else {

        }
    }
    return(m_epollFd);
}

/**
 * @brief 
 * 
 * @param serviceType 
 * @param IP 
 * @param PORT 
 * @param isAsync 
 * @return std::int32_t 
 */
std::int32_t noor::Uniimage::CreateServiceAndRegisterToEPoll(noor::ServiceType serviceType, const std::string& IP, const std::uint16_t& PORT, bool isAsync) {
    do {
        switch(serviceType) {

            case noor::Tcp_Device_Client_Service_Sync:
            case noor::Tcp_Device_Client_Service_Async:
            case noor::Tcp_Device_Console_Client_Service_Async:
            case noor::Tcp_Device_Console_Client_Service_Sync:
            case noor::Tcp_Web_Client_Proxy_Service:
            case noor::Tls_Tcp_Device_Rest_Client_Service_Sync:
            {
                if(!m_services.insert(std::make_pair(serviceType, std::make_unique<TcpClient>(IP, PORT, isAsync))).second) {
                    //Unable to insert the instance into container.
                    std::cout << "line: " << __LINE__ << " element for Key: " << serviceType << " is already present" << std::endl;
                    return(-1);
                }
                RegisterToEPoll(serviceType);
            }
            break;

            case noor::Tcp_Device_Server_Service:
            case noor::Tcp_Device_Console_Server_Service:
            case noor::Tcp_Web_Server_Service:
            {
                if(!m_services.insert(std::make_pair(serviceType, std::make_unique<TcpServer>(IP, PORT))).second) {
                    //Unable to insert the instance into container.
                    std::cout << "line: " << __LINE__ << " element for Key: " << serviceType << " is already present" << std::endl;
                    return(-1);
                }
                RegisterToEPoll(serviceType);
            }
            break;
            case noor::ServiceType::Unix_Data_Store_Client_Service_Sync:
            {
                if(!m_services.insert(std::make_pair(serviceType, std::make_unique<UnixClient>())).second) {
                    //Unable to insert the instance into container.
                    std::cout << "line: " << __LINE__ << " element for Key: " << serviceType << " is already present" << std::endl;
                    return(-1);
                }
                RegisterToEPoll(serviceType);
            }
            break;
            default:
            {
                //Default case 
            }
        }
    }while(0);

    return(0);
}

/**
 * @brief 
 * 
 * @param in 
 * @return std::int32_t 
 */
std::int32_t noor::Uniimage::stop(std::int32_t in) {
    return(0);
}

/**
 * @brief 
 * 
 * @param toInMilliSeconds 
 * @return std::int32_t 
 */
std::int32_t noor::Uniimage::start(std::int32_t toInMilliSeconds) {

    if(m_evts.empty()) {
        std::cout << "line: " << __LINE__ << " event list is empty, please create the events to monitor" << std::endl;
        return(-1);
    }

    if(toInMilliSeconds < 0) {
        std::cout << "line: " << __LINE__ << " timeout value is -ve hence resetting it to zero" << std::endl;
        toInMilliSeconds = -1;
    }

    std::vector<struct epoll_event> activeEvt(m_evts.size());

    while(true) {
        std::int32_t nReady = -1;

        nReady = ::epoll_wait(m_epollFd, activeEvt.data(), activeEvt.size(), toInMilliSeconds);
        //Upon timeout nReady is ZERO and -1 Upon Failure.
        if(nReady >= 0) {
            activeEvt.resize(nReady);
        } else if(nReady < 0) {
            //Error is returned by epoll_wait
            continue;
        }

        for(auto it = activeEvt.begin(); it != activeEvt.end(); ++it) {
            auto ent = *it;
            std::uint32_t Fd = std::uint32_t(ent.data.u64 >> 32 & 0xFFFFFFFF);
            noor::ServiceType serviceType = noor::ServiceType(ent.data.u64 & 0xFFFFFFFF);

            if(ent.events == EPOLLOUT) {
                //Descriptor is ready for Write
                std::cout << "line: " << __LINE__ << " EPOLLOUT is set for serviceType: " << serviceType << std::endl;
                switch(serviceType) {
                    case noor::ServiceType::Tcp_Device_Client_Service_Async:
                    {
                        do {
                            // check that there's no error for socket.
                            std::int32_t optval = -1;
                            socklen_t optlen = sizeof(optval);
                            if(!getsockopt(Fd, SOL_SOCKET, SO_ERROR, &optval, &optlen)) {
                                struct sockaddr_in peer;
                                socklen_t sock_len = sizeof(peer);
                                memset(&peer, 0, sizeof(peer));

                                auto ret = getpeername(Fd, (struct sockaddr *)&peer, &sock_len);
                                if(ret < 0 && errno == ENOTCONN) {
                                    //re-attemp connection now.
                                    std::cout << "line: " << __LINE__ << " re-attempting the connection " << std::endl;
                                    auto& inst = GetService(serviceType);
                                    auto IP = inst->ip();
                                    auto PORT = inst->port();
                                    DeRegisterFromEPoll(Fd);
                                    CreateServiceAndRegisterToEPoll(serviceType, IP, PORT, true);
                                    break;
                                }
                            }
                            std::cout << "line: " << __LINE__ << " client is connected successfully " << std::endl;
                            //There's no error on the socket
                            ent.events = EPOLLIN | EPOLLHUP | EPOLLERR; 
                            auto ret = ::epoll_ctl(m_epollFd, EPOLL_CTL_MOD, Fd, &ent);
                            //Send the cached response from Data Store now.
                            auto &svc = GetService(noor::ServiceType::Unix_Data_Store_Client_Service_Sync);
                            if(!getResponseCache().empty()) {
                                for(const auto& ent: getResponseCache().begin()->second) {
                                    std::cout << "line: " << __LINE__ << " from cache: " << ent << std::endl;
                                }
                                //svc->tcp_tx(Fd, getResponseCache().begin()->second);
                            }
                            
                        } while(0);
                        
                    }
                    break;

                    case noor::ServiceType::Tls_Tcp_Device_Client_Service_Async:
                    {
                        do {
                            // check that there's no error for socket.
                            std::int32_t optval = -1;
                            socklen_t optlen = sizeof(optval);
                            if(!getsockopt(Fd, SOL_SOCKET, SO_ERROR, &optval, &optlen)) {
                                struct sockaddr_in peer;
                                socklen_t sock_len = sizeof(peer);
                                memset(&peer, 0, sizeof(peer));

                                auto ret = getpeername(Fd, (struct sockaddr *)&peer, &sock_len);
                                if(ret < 0 && errno == ENOTCONN) {
                                    //re-attemp connection now.
                                    std::cout << "line: " << __LINE__ << " re-attempting the connection " << std::endl;
                                    auto& inst = GetService(serviceType);
                                    auto IP = inst->ip();
                                    auto PORT = inst->port();
                                    DeRegisterFromEPoll(Fd);
                                    CreateServiceAndRegisterToEPoll(serviceType, IP, PORT, true);
                                    break;
                                }
                            }
                            std::cout << "line: " << __LINE__ << " Tls Tcp client is connected successfully " << std::endl;
                            //There's no error on the socket
                            ent.events = EPOLLIN | EPOLLHUP | EPOLLERR; 
                            auto ret = ::epoll_ctl(m_epollFd, EPOLL_CTL_MOD, Fd, &ent);
                            //Send the cached response from Data Store now.
                            auto &svc = GetService(noor::ServiceType::Unix_Data_Store_Client_Service_Sync);
                            if(!getResponseCache().empty()) {
                                for(const auto& ent: getResponseCache().begin()->second) {
                                    std::cout << "line: " << __LINE__ << " from cache: " << ent << std::endl;
                                }
                                //svc->tls().write();
                            }
                            
                        } while(0);
                        
                    }
                    break;

                    case noor::ServiceType::Tcp_Device_Console_Client_Service_Async:
                    {
                        do {
                            // check that there's no error for socket.
                            std::int32_t optval = -1;
                            socklen_t optlen = sizeof(optval);
                            if(!getsockopt(Fd, SOL_SOCKET, SO_ERROR, &optval, &optlen)) {
                                struct sockaddr_in peer;
                                socklen_t sock_len = sizeof(peer);
                                memset(&peer, 0, sizeof(peer));

                                auto ret = getpeername(Fd, (struct sockaddr *)&peer, &sock_len);
                                if(ret < 0 && errno == ENOTCONN) {
                                    //re-attemp connection now.
                                    auto& inst = GetService(serviceType);
                                    auto IP = inst->ip();
                                    auto PORT = inst->port();
                                    DeRegisterFromEPoll(Fd);
                                    CreateServiceAndRegisterToEPoll(serviceType, IP, PORT, true);
                                    break;
                                }
                            }

                            //There's no error on the socket
                            ent.events = EPOLLIN | EPOLLHUP | EPOLLERR; 
                            auto ret = ::epoll_ctl(m_epollFd, EPOLL_CTL_MOD, Fd, &ent);

                        } while(0);
                    }
                    break;
                    case noor::ServiceType::Tls_Tcp_Device_Rest_Client_Service_Async:
                    case noor::ServiceType::Tls_Tcp_Device_Rest_Client_Service_Sync:
                    {
                        //tcp connection is established - do tls handshake
                        auto& svc = GetService(serviceType);
                        svc->tls().init(Fd);
                        svc->tls().client();

                        ent.events = EPOLLIN | EPOLLHUP | EPOLLERR; 
                        auto ret = ::epoll_ctl(m_epollFd, EPOLL_CTL_MOD, Fd, &ent);
                        //Get Token for Rest Client
                        std::stringstream body("");
                        body << "{" << "\"login\": " << "\"test\"," << "\"password\": " << "\"test123\"}";
                        auto req = svc->restC().getToken(body.str());
                        std::cout << "line: " << __LINE__ << " Rest Request " << std::endl << req << std::endl;
                        auto len = svc->tls().write(req);
                        std::cout << "line: " << __LINE__ << " sent to REST server length: " << len << std::endl;
                    }
                    break;
                    default:
                    {

                    }
                    break;
                }
            } else if(ent.events == EPOLLIN) {
                //file descriptor is ready for read.
                switch(serviceType) {
                    case noor::ServiceType::Tcp_Web_Server_Service:
                    {
                        //New Web Connection
                        std::int32_t newFd = -1;
                        struct sockaddr_in addr;
                        socklen_t addr_len = sizeof(addr);
                        newFd = ::accept(Fd, (struct sockaddr *)&addr, &addr_len);
                        // new connection is accepted successfully.

                        if(newFd > 0) {
                            std::uint16_t PORT = ntohs(addr.sin_port);
                            std::string IP(inet_ntoa(addr.sin_addr));

                            if(!m_services.insert(std::make_pair(noor::ServiceType::Tcp_Web_Client_Connected_Service , std::make_unique<TcpClient>(newFd, IP, PORT))).second) {
                                //Unable to insert the instance into container.
                                std::cout << "line: " << __LINE__ << " element for Key: " << serviceType << " is already present" << std::endl;
                                return(-1);
                            }
                            RegisterToEPoll(noor::ServiceType::Tcp_Web_Client_Connected_Service);
                        }
                    }
                    break;
                    case noor::ServiceType::Tcp_Device_Server_Service:
                    {
                        std::int32_t newFd = -1;
                        struct sockaddr_in addr;
                        socklen_t addr_len = sizeof(addr);
                        newFd = ::accept(Fd, (struct sockaddr *)&addr, &addr_len);
                        // new connection is accepted successfully.

                        if(newFd > 0) {
                            std::uint16_t PORT = ntohs(addr.sin_port);
                            std::string IP(inet_ntoa(addr.sin_addr));

                            if(!m_services.insert(std::make_pair(noor::ServiceType::Tcp_Device_Client_Connected_Service , std::make_unique<TcpClient>(newFd, IP, PORT))).second) {
                                //Unable to insert the instance into container.
                                std::cout << "line: " << __LINE__ << " element for Key: " << serviceType << " is already present" << std::endl;
                                return(-1);
                            }
                            RegisterToEPoll(noor::ServiceType::Tcp_Device_Client_Connected_Service);
                        }
                    }
                    break;
                    case noor::ServiceType::Tcp_Device_Console_Server_Service:
                    {
                        std::int32_t newFd = -1;
                        struct sockaddr_in addr;
                        socklen_t addr_len = sizeof(addr);
                        newFd = ::accept(Fd, (struct sockaddr *)&addr, &addr_len);
                        // new connection is accepted successfully.
                        if(newFd > 0) {
                            std::uint16_t PORT = ntohs(addr.sin_port);
                            std::string IP(inet_ntoa(addr.sin_addr));

                            if(!m_services.insert(std::make_pair(noor::ServiceType::Tcp_Device_Console_Connected_Service, std::make_unique<TcpClient>(newFd, IP, PORT))).second) {
                                //Unable to insert the instance into container.
                                std::cout << "line: " << __LINE__ << " element for Key: " << serviceType << " is already present" << std::endl;
                                return(-1);
                            }
                            RegisterToEPoll(noor::ServiceType::Tcp_Device_Console_Connected_Service);
                        }
                    }
                    break;
                    case noor::ServiceType::Tcp_Device_Client_Connected_Service:
                    {
                        //Data is availabe for read. --- tcp_rx()
                        std::string request("");
                        auto &svc = GetService(serviceType);
                        auto result = svc->tcp_rx(Fd, request);
                        if(!result) {
                            //TCP Connection is closed.
                            DeRegisterFromEPoll(Fd);
                            //Initiate the connection now.
                            CreateServiceAndRegisterToEPoll(serviceType);
                        }
                    }
                    break;
                    case noor::ServiceType::Tcp_Web_Client_Connected_Service:
                    {
                        //Data is availabe for read. --- web_rx()
                        std::string request("");
                        auto &svc = GetService(serviceType);
                        auto result = svc->web_rx(Fd, request);
                        auto rsp = svc->process_web_request(request);
                        auto ret = svc->web_tx(Fd, rsp);
                    }
                    break;
                    case noor::ServiceType::Tcp_Device_Console_Connected_Service:
                    {
                        //Data is availabe for read. --- tcp_rx()
                        std::string request("");
                        auto &svc = GetService(serviceType);
                        auto result = svc->tcp_rx(Fd, request);
                    }
                    break;
                    case noor::ServiceType::Unix_Data_Store_Client_Service_Sync:
                    {
                        //Data is availabe for read. --- uds_rx()
                        auto &svc = GetService(serviceType);
                        auto result = svc->uds_rx();
                        auto json_arr = json::parse(result.m_response);
                        auto json_obj = json_arr.at(0);

                        #if 0
                        std::cout << "line: " << __LINE__ << " object: " << json_obj << std::endl;
                        if(json_obj["device.provisioning.serial"] != nullptr && json_obj["device.provisioning.serial"].get<std::string>().length()) {
                            auto serialNo = json_obj["device.provisioning.serial"];
                            std::cout << "line: " << __LINE__ << " serialnumber: " << serialNo << std::endl;
                        }
                        #endif

                        if(!m_deviceRspCache.size()) {
                            for(auto it = json_obj.begin(); it != json_obj.end(); ++it) {
                                if(!it.key().compare("device.provisioning.serial") && it.value().is_string()) {
                                    std::cout << "line: " << __LINE__ << " serialnumber: " << it.value() << std::endl;
                                    std::vector<std::string> rsp;
                                    rsp.push_back(result.m_response);
                                    m_deviceRspCache[it.value()] = rsp;
                                }
                            }
                        } else {
                            m_deviceRspCache.begin()->second.push_back(result.m_response);
                            std::cout << "line: " << __LINE__ << " number of elements: " << m_deviceRspCache.begin()->second.size() << std::endl;
                            for(const auto&ent: m_deviceRspCache.begin()->second) {
                                std::cout << "line: " << __LINE__ << " m_devideRspCache: " << ent <<std::endl;
                            }
                            if(5 == m_deviceRspCache.begin()->second.size()) {
                                auto &svc = GetService(noor::ServiceType::Tcp_Device_Client_Service_Async);
                                if(noor::client_connection::Connected == svc->connected_client(svc->handle())) {
                                    //Push to Device Management Server
                                }
                            }
                        }
                    }
                    break;
                    case noor::ServiceType::Tcp_Device_Console_Client_Service_Async:
                    {
                        //Data is availabe for read. --- tcp_rx()
                        std::string request("");
                        auto &svc = GetService(serviceType);
                        auto result = svc->tcp_rx(Fd, request);
                    }
                    break;
                    case noor::ServiceType::Tcp_Web_Client_Proxy_Service:
                    {
                        //Data is availabe for read. --- tcp_rx()
                        std::string request("");
                        auto &svc = GetService(serviceType);
                        auto result = svc->web_rx(Fd, request);
                    }
                    break;
                    case noor::ServiceType::Tcp_Device_Client_Service_Async:
                    {
                        //Data is availabe for read. --- tcp_rx()
                        std::string request("");
                        auto &svc = GetService(serviceType);
                        auto result = svc->tcp_rx(Fd, request);
                    }
                    break;

                    case noor::ServiceType::Tls_Tcp_Device_Rest_Client_Service_Sync:
                    {
                        auto &svc = GetService(serviceType);
                        std::string out;
                        std::int32_t header_len = -1;
                        std::int32_t payload_len = -1;
                        
                        do {
                            auto ret = svc->tls().peek(out);
                            if(ret > 0) {
                                Http http(out);
                                auto ct = http.value("Content-Length");
                                header_len = http.get_header(out).length() + 1;

                                if(ct.length() > 0) {
                                    //Content-Length is present
                                    payload_len = std::stoi(ct);
                                    std::cout << "line: " << __LINE__ << " value of content-length: " << std::stoi(ct) << std::endl;
                                }
                            }
                        }while(header_len != out.length());

                        do {
                            //Read HTTP Header first.
                            auto ret = svc->tls().read(out, header_len);
                            if(ret < 0) {
                                break;
                            }
                            std::cout << "line: " << __LINE__ << " tls_read: " << out  <<" length: " << out.length() << std::endl;

                            //HTTP Body
                            if(payload_len < 0) {
                                break;
                            }

                            std::string body;
                            body.clear();
                            ret = svc->tls().read(body, payload_len);

                            //Process Response Now.
                            if(ret < 0) {
                                break;
                            }

                            //auto json_obj = json::parse(body);
                            //std::cout << "line: " << __LINE__ << " json_obj[data][access_token]: " << json_obj["data"]["access_token"] << std::endl;
                            auto req = svc->restC().processResponse(out, body);
                            ret = svc->tls().write(req);
                            if(ret < 0) {
                                break;
                            }

                            std::cout << "line: " << __LINE__ << " request sent to : " << req << std::endl;

                        }while(0);
                    }
                    break;

                    default:
                    {

                    }
                }
                
            } else if(ent.events == EPOLLHUP) {
                //Connection is closed by other end
                switch(serviceType) {
                    case noor::ServiceType::Tcp_Device_Client_Service_Async:
                    case noor::ServiceType::Tcp_Device_Console_Client_Service_Async:
                    case noor::ServiceType::Tcp_Web_Client_Proxy_Service:
                    {
                        //start attempting the connection...
                        auto& inst = m_services[serviceType];
                        auto IP = inst->ip();
                        auto PORT = inst->port();
                        DeRegisterFromEPoll(Fd);
                        CreateServiceAndRegisterToEPoll(serviceType, IP, PORT, true);
                    }
                    break;
                    case noor::ServiceType::Unix_Data_Store_Client_Service_Sync:
                    {
                        DeRegisterFromEPoll(Fd);
                        CreateServiceAndRegisterToEPoll(serviceType);
                    }
                    break;
                    default:
                    {
                        //Connection is closed.
                        DeRegisterFromEPoll(Fd);
                    }
                }
            } else if(ent.events == EPOLLERR) {
                std::cout << "line: " << __LINE__ << " epollerr events: " << ent.events << std::endl;
            } else {
                //std::cout << "line: " << __LINE__ << " unhandled events: " << ent.events << std::endl;
            }
        }
    }
    return(0);
}

/**
 * @brief 
 * 
 * @param fd 
 * @return std::int32_t 
 */
std::int32_t noor::Uniimage::DeRegisterFromEPoll(std::int32_t fd) {
    noor::ServiceType serviceType;
    auto it = std::find_if(m_evts.begin(), m_evts.end(), [&](const auto& ent) ->bool {
        auto evtFd = std::int32_t((ent.data.u64 >> 32) & 0xFFFFFFFF);
        serviceType = noor::ServiceType(ent.data.u64 & 0xFFFFFFFF);
        return(evtFd == fd);
    });

    if(::epoll_ctl(m_epollFd, EPOLL_CTL_DEL, fd, nullptr) == -1)
    {
        std::cout << "line: " << __LINE__ << " Failed to delete Fd from epoll instance for fd: " << fd << std::endl;
    }

    close(fd);
    if(it != m_evts.end()) {
        m_evts.erase(it);
        //Release the ownerofunique_ptr now
        m_services[serviceType].reset(nullptr);
        return(0);
    }

    return(-1);
}

/**
 * @brief 
 * 
 * @param serviceType 
 * @return std::int32_t 
 */
std::int32_t noor::Uniimage::RegisterToEPoll(noor::ServiceType serviceType) {
    auto &inst = GetService(serviceType);
    struct epoll_event evt;

    std::cout << "line: " << __LINE__ << " handle: " << inst->handle() << " serviceType: " << serviceType << std::endl;
    std::uint64_t dd = std::uint64_t(inst->handle());
    evt.data.u64 = std::uint64_t( dd << 32) | std::uint64_t(serviceType);

    if((serviceType == noor::ServiceType::Tcp_Device_Client_Service_Async) ||
       (serviceType == noor::ServiceType::Tcp_Device_Console_Client_Service_Async) ||
       (serviceType == noor::ServiceType::Tls_Tcp_Device_Rest_Client_Service_Async) ||
       (serviceType == noor::ServiceType::Tls_Tcp_Device_Rest_Client_Service_Sync)) {
        evt.events = EPOLLOUT | EPOLLERR | EPOLLHUP;
    } else {
        evt.events = EPOLLIN | EPOLLERR | EPOLLHUP;
    }

    if(::epoll_ctl(m_epollFd, EPOLL_CTL_ADD, inst->handle(), &evt) == -1)
    {
        std::cout << "line: " << __LINE__ << " Failed to add Fd to epoll instance for serviceType: " << serviceType << " Terminating the process"<< std::endl;
        exit(2);
    }

    m_evts.push_back(evt);
    return(0);
}

/**
 * @brief 
 * 
 * @param serviceType 
 * @return std::unique_ptr<noor::Service>& 
 */
std::unique_ptr<noor::Service>& noor::Uniimage::GetService(noor::ServiceType serviceType) {
    return(m_services[serviceType]);
}

std::string noor::RestClient::getToken(const std::string& in) {
    std::string host("192.168.1.1:443");
    std::stringstream ss("");
    uri.assign("/api/v1/auth/tokens");

    ss << "POST " << uri <<" HTTP/1.1\r\n"
        << "Host: " << host << "\r\n"
        << "Content-Type: application/vnd.api+json\r\n"
        << "Connection: keep-alive\r\n"
        << "Accept: application/vnd.api+json\r\n"
        << "Content-Length: " << in.length() << "\r\n"
        << "\r\n"
        << in;

    return(ss.str());
}

std::string noor::RestClient::authorizeToken(const std::string& in, const std::string& user) {
    std::string host("192.168.1.1:443");
    std::stringstream ss("");
    uri.assign("/api/v1/auth/authorization/");
    uri += user;

    ss << "GET " << uri <<" HTTP/1.1\r\n"
        << "Host: " << host << "\r\n"
        << "Content-Type: application/vnd.api+json\r\n"
        << "Connection: keep-alive\r\n"
        << "Accept: application/vnd.api+json\r\n"
        << "Authorization: Bearer " << cookies << "\r\n"
        << "Content-Length: 0" << "\r\n"
        << "\r\n";

    return(ss.str());
}

std::string noor::RestClient::registerDatapoints(const std::vector<std::string>& dps) {
    std::string host("192.168.1.1:443");
    std::stringstream ss("");
    uri.assign("/api/v1/register/db?fetch=true");

    json jarray = json::array();
    for(const auto& ent: dps) {
        jarray.push_back(ent);
    }
    auto body = jarray.dump();

    ss << "{\"last\": " << body << "}";
    //json jobject = json::object({"last", jarray});
    //json jobject{"last", body};
    //body = jobject.dump();
    body = ss.str();
    //clear the previous contents now.
    ss.str("");
    
    std::cout << "line: " << __LINE__ << " json: " << body << std::endl;

    ss << "POST " << uri <<" HTTP/1.1\r\n"
        << "Host: " << host << "\r\n"
        << "Content-Type: application/vnd.api+json\r\n"
        << "Connection: keep-alive\r\n"
        << "Accept: application/vnd.api+json\r\n"
        << "Authorization: Bearer " << cookies << "\r\n"
        << "Content-Length: " << body.length() << "\r\n"
        << "\r\n"
        << body;

    return(ss.str());
}

std::string noor::RestClient::buildRequest(const std::string& in, std::vector<std::string> param) {
    return(std::string());
}

std::string noor::RestClient::processResponse(const std::string& http_header, const std::string& http_body) {
    if(!uri.compare(0, 19, "/api/v1/auth/tokens")) {
        json json_object = json::parse(http_body);
        cookies.assign(json_object["data"]["access_token"]);
        return(authorizeToken(http_body, "test"));

    } else if(!uri.compare(0, 26, "/api/v1/auth/authorization")) {
        std::cout << "line: " << __LINE__ << " http_body: " << http_body << std::endl;
        json json_object = json::parse(http_body);
        auto attmpts = json_object["data"]["attempts"].get<std::int32_t>();
        //auto attmpts = value;
        std::cout << "line: " << __LINE__ << " attempts: " << attmpts << std::endl;
        return(registerDatapoints({{"net.cellular.simdb.common[].operator"}, {"net.cellular.simdb.common[].apn"}}));

    } else if(!uri.compare(0, 19, "/api/v1/register/db")) {
        std::cout << "line: " << __LINE__ << " response for register/db: " << http_body << std::endl;
    } else {
        
    }
    return(std::string());
}


std::vector<struct option> options = {
    {"role",                      required_argument, 0, 'r'},
    {"server-ip",                 required_argument, 0, 'i'},
    {"server-port",               required_argument, 0, 'p'},
    {"web-port",                  required_argument, 0, 'w'},
    {"wan-interface-instance",    required_argument, 0, 'a'},
    {"protocol",                  required_argument, 0, 't'},
    {"self-ip",                   required_argument, 0, 's'},
    {"self-port",                 required_argument, 0, 'e'},
    {"time-out",                  required_argument, 0, 'o'},
    {"machine",                   optional_argument, 0, 'm'},
    {"config-json",               optional_argument, 0, 'c'},
};

/*
 _ __ ___   __ _(_)_ __  
| '_ ` _ \ / _` | | '_ \ 
| | | | | | (_| | | | | |
|_| |_| |_|\__,_|_|_| |_|
*/
/**
 * @brief 
 * 
 * @param argc 
 * @param argv 
 * @return int 
 */
int main(std::int32_t argc, char *argv[]) {
    std::int32_t c;
    std::int32_t option_index = 0;
    std::unordered_map<std::string, std::string> config;
    
    while ((c = getopt_long(argc, argv, "r:i:p:w:t:a:s:e:o:m:", options.data(), &option_index)) != -1) {
        switch(c) {
            case 'r':
            {
                std::string role("");
                role = optarg;
                if(role.compare("client") && (role.compare("server"))) {
                    std::cout << "Invalid value for --role, possible value is client or server "<< std::endl;
                    return(-1);
                }
                config.emplace(std::make_pair("role", optarg));
            }
            break;
            case 'i':
            {
                config.emplace(std::make_pair("server-ip", optarg));
            }
            break;
            case 'p':
            {
                config.emplace(std::make_pair("server-port", optarg));
            }
            break;
            case 'w':
            {
                config.emplace(std::make_pair("web-port", optarg));
            }
            break;
            case 'a':
            {
                config.emplace(std::make_pair("wan-interface-instance", optarg));
            }
            break;
            case 't':
            {
                config.emplace(std::make_pair("protocol", optarg));
            }
            break;
            case 's':
            {
                config.emplace(std::make_pair("self-ip", optarg));
            }
            break;
            case 'e':
            {
                config.emplace(std::make_pair("self-port", optarg));
            }
            break;
            case 'o':
            {
                config.emplace(std::make_pair("time-out", optarg));
            }
            break;
            case 'm':
            {
                config.emplace(std::make_pair("machine", optarg));
            }
            break;

            default:
            {
                std::cout << "--role <client|server> " << std::endl
                          << "--server-ip <ip address of server> " << std::endl
                          << "--server-port <server port number> " << std::endl
                          << "--web-port  <server-web-port for http request> " << std::endl
                          << "--self-ip   <self ip for bind receive request> " << std::endl
                          << "--self-port <self port for bind to receive request> " << std::endl
                          << "--protocol  <tcp|udp|unix/tls> " << std::endl
                          << "--wan-interface-instance <c1|c3|c4|c5|w1|w2|e1|e2|e3> " << std::endl
                          << "--time-out <value in ms> " << std::endl
                          << "--machine <host|> " << std::endl;
                          return(-1);
            }
        }
    }
    
    
    noor::Uniimage inst;
    std::string bridgeIP("192.168.1.1");
    std::uint16_t httpPort = 80;
    std::uint16_t httpsPort = 443;
    std::uint16_t consolePort = 65344;

    inst.init();

    if(!config["role"].compare("client")) {
        
        inst.CreateServiceAndRegisterToEPoll(noor::ServiceType::Tcp_Device_Client_Service_Async, config["server-ip"], std::stoi(config["server-port"]), true);
        inst.CreateServiceAndRegisterToEPoll(noor::ServiceType::Tcp_Device_Console_Client_Service_Async, config["server-ip"], consolePort, true);
        //inst.CreateServiceAndRegisterToEPoll(noor::ServiceType::Tcp_Web_Client_Proxy_Service, bridgeIP, httpPort, false);
        inst.CreateServiceAndRegisterToEPoll(noor::ServiceType::Tls_Tcp_Device_Rest_Client_Service_Sync, bridgeIP, httpsPort, false);
        inst.CreateServiceAndRegisterToEPoll(noor::ServiceType::Unix_Data_Store_Client_Service_Sync);

        auto& ent = inst.GetService(noor::ServiceType::Unix_Data_Store_Client_Service_Sync);

        ent->getVariable("device", {{"machine"}, {"product"}, {"provisioning.serial"}});
        ent->getVariable("net.interface.wifi[]", {{"radio.mode"}, {"mac"},{"ap.ssid"}}, {{"radio.mode__eq\": \"sta"}});
        ent->getVariable("net.interface.common[]", {{"ipv4.address"}, {"ipv4.connectivity"}, {"ipv4.prefixlength"}});
        ent->getVariable("system.os", {{"version"}, {"buildnumber"}, {"name"}});
        ent->getVariable("system.bootcheck.signature");
        #if 0
        std::vector<const std::string> dp_list = {
            {"device.machine"},
            {"device.product"},
            {"device.provisioning.serial"},
            {"system.os.version"},
            {"system.os.buildnumber"},
            {"system.os.name"},
            {"system.bootcheck.signature"},
            {"net.interface.wifi[]"},
            {"net.interface.common[].ipv4.address"},
            {"net.interface.common[].ipv4.connectivity"},
            {"net.interface.common[].ipv4.prefixlength"}};
        ent->getVariable(dp_list);
        #endif
        //GetToken for RestClient and then Authorize the the token

    } else if(!config["role"].compare("server")) {

        if(!config["server-ip"].length()) {
            config["server-ip"] = "127.0.0.1";
        }

        inst.CreateServiceAndRegisterToEPoll(noor::ServiceType::Tcp_Device_Server_Service, config["server-ip"], std::stoi(config["server-port"]));
        inst.CreateServiceAndRegisterToEPoll(noor::ServiceType::Tcp_Device_Console_Server_Service, config["server-ip"], consolePort);
        inst.CreateServiceAndRegisterToEPoll(noor::ServiceType::Tcp_Web_Server_Service, config["server-ip"], std::stoi(config["web-port"]));
    }

    auto timeout = 100;
    if(config["time-out"].length()) {
        timeout = std::stoi(config["time-out"]);
    }
    // timeout is in milli seconds
    inst.start(timeout);

#if 0
    noor::Service unimanage;
    std::vector<std::tuple<std::unique_ptr<noor::Service>, noor::ServiceType>> ent;
    ent.clear();
    

    if(!config["role"].compare("client")) {

        /**
         * @brief machine command line argument is required to do unit testing of client on x86 machine.
         *        if machine = host meaning this is running on x86 machine or any value for aarm64 machine.
         * 
         */
        if(!config["machine"].length()) {
            
            ent.at(0) = {std::make_unique<UnixClient>(), noor::ServiceType::Unix_Data_Store_Client_Service_Sync};
            std::get<0>(ent.at(0))->getVariable("net.interface.wifi[]", {{"radio.mode"}, {"mac"},{"ap.ssid"}}, {{"radio.mode__eq\": \"sta"}});
            std::get<0>(ent.at(0))->getVariable("device", {{"machine"}, {"product"}, {"provisioning.serial"}});
            std::get<0>(ent.at(0))->getVariable("net.interface.common[]", {{"ipv4.address"}, {"ipv4.connectivity"}, {"ipv4.prefixlength"}});
            std::get<0>(ent.at(0))->getVariable("system.os", {{"version"}, {"buildnumber"}, {"name"}});
            //std::cout << "line: " << __LINE__ << " TCP_ASYNC: " << std::get<0>(ent.at(0))->handle() << " : " <<std::get<0>(ent.at(0))->connected_client(std::get<0>(ent.at(0))->handle())<< std::endl;
        }

        if(!config["protocol"].compare("tcp")) {
            ent.at(1) = {std::make_unique<TcpClient>(config, noor::ServiceType::Tcp_Device_Client_Service_Async), noor::ServiceType::Tcp_Device_Client_Service_Async};
            ent.at(2) = {std::make_unique<TcpClient>(config, noor::ServiceType::Tcp_Device_Console_Client_Service_Async), noor::ServiceType::Tcp_Device_Console_Client_Service_Async};
            ent.at(2) = {std::make_unique<TcpClient>(config, noor::ServiceType::Tcp_Web_Client_Proxy_Service), noor::ServiceType::Tcp_Web_Client_Proxy_Service};
        }

        

        auto timeout = 100;
        if(config["time-out"].length()) {
            timeout = std::stoi(config["time-out"]);
        }

        unimanage.start_client(timeout, std::move(ent));
        
    } else if(!config["role"].compare("server")) {
        ///server 
        if(!config["protocol"].compare("tcp")) {
            ent.push_back({std::make_unique<TcpServer>(config, noor::ServiceType::Tcp_Device_Console_Server_Service), noor::ServiceType::Tcp_Device_Console_Server_Service});
            ent.push_back({std::make_unique<TcpServer>(config, noor::ServiceType::Tcp_Device_Server_Service), noor::ServiceType::Tcp_Device_Server_Service});
        }
        
        ent.push_back({std::make_unique<WebServer>(config, noor::ServiceType::Tcp_Web_Server_Service), noor::ServiceType::Tcp_Web_Server_Service});

        auto timeout = 100;
        if(config["time-out"].length()) {
            timeout = std::stoi(config["time-out"]);
        }

        unimanage.start_server(timeout, std::move(ent));
    }
#endif
}

std::int32_t noor::Service::tcp_client_async(const std::string& IP, std::uint16_t PORT) {
    return(tcp_client(IP, PORT, true));
}

/**
 * @brief 
 * 
 * @param IP 
 * @param PORT 
 * @param isAsync 
 * @return std::int32_t 
 */
std::int32_t noor::Service::tcp_client(const std::string& IP, std::uint16_t PORT, bool isAsync) {
    /* Set up the address we're going to bind to. */
    bzero(&m_inet_server, sizeof(m_inet_server));
    m_inet_server.sin_family = AF_INET;
    m_inet_server.sin_port = htons(PORT);
    m_inet_server.sin_addr.s_addr = inet_addr(IP.c_str());
    memset(m_inet_server.sin_zero, 0, sizeof(m_inet_server.sin_zero));
    auto len = sizeof(m_inet_server);
    std::int32_t channel = -1;
    //learn them for future
    ip(IP);
    port(PORT);

    if(isAsync) {
        channel = ::socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, 0);
        if(channel < 0) {
            std::cout << "line: " << __LINE__ <<" Creation of INET socket Failed" << std::endl;
            return(-1);
        }
    } else {
        channel = ::socket(AF_INET, SOCK_STREAM, 0);
        if(channel < 0) {
            std::cout << "line: " << __LINE__ <<" Creation of INET socket Failed" << std::endl;
            return(-1);
        }
    }

    handle(channel);
    connected_client(noor::client_connection::Disconnected);

    /* set the reuse address flag so we don't get errors when restarting */
    auto flag = 1;
    if(::setsockopt(channel, SOL_SOCKET, SO_REUSEADDR, (std::int8_t *)&flag, sizeof(flag)) < 0 ) {
        std::cout << "line: " << __LINE__ << " Error: Could not set reuse address option on INET socket!" << std::endl;
        ::close(handle());
        connected_client().erase(handle());
        handle(-1);
        return(-1);
    }
    
    auto rc = ::connect(channel, (struct sockaddr *)&inet_server(), len);
    if(rc < 0) {
        if(errno == EINPROGRESS) {    
            connected_client(noor::client_connection::Inprogress);
            std::cout << "line: " << __LINE__ << " Async connection in progress IP: " << IP << " PORT: " << PORT << std::endl;
            return(0);

        } else if(errno == ECONNREFUSED) {
            //Server is not strated yet
            std::cout << "line: " << __LINE__ << " Connect is refused errno: "<< std::strerror(errno) << std::endl;
            ::close(handle());
            connected_client().erase(handle());
            handle(-1);
            return(-1);

        } else {
            std::cout << "line: " << __LINE__ << " Connect is failed errno: "<< std::strerror(errno) << std::endl;
            ::close(handle());
            connected_client().erase(handle());
            handle(-1);
            return(-1);
        }
    } else {
        connected_client(noor::client_connection::Connected);
        std::cout << "line: " << __LINE__ << " client is connected IP: " << IP << " PORT: " << PORT << std::endl;
    }

    return(0);
}

/**
 * @brief 
 * 
 * @param IP 
 * @param PORT 
 * @return std::int32_t 
 */
std::int32_t noor::Service::udp_client(const std::string& IP, std::uint16_t PORT) {
    // UDP Client .... 
    bzero(&m_inet_server, sizeof(m_inet_server));
    m_inet_server.sin_family = AF_INET;
    m_inet_server.sin_port = htons(PORT);
    m_inet_server.sin_addr.s_addr = inet_addr(IP.c_str());
    memset(m_inet_server.sin_zero, 0, sizeof(m_inet_server.sin_zero));

    std::int32_t channel = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(channel < 0) {
        std::cout << "line: " << __LINE__ <<" Creation of INET socket Failed" << std::endl;
        return(-1);
    }
    handle(channel);
    
    /* set the reuse address flag so we don't get errors when restarting */
    auto flag = 1;
    if(::setsockopt(channel, SOL_SOCKET, SO_REUSEADDR, (std::int8_t *)&flag, sizeof(flag)) < 0 ) {
        std::cout << "line: " << __LINE__ << " Error: Could not set reuse address option on INET socket!" << std::endl;
        ::close(handle());
        handle(-1);
        return(-1);
    }

    return(0);
}

/**
 * @brief 
 * 
 * @param PATH 
 * @return std::int32_t 
 */
std::int32_t noor::Service::uds_client(const std::string& PATH) {
    std::int32_t channel = -1;
    /* Set up the address we're going to bind to. */
    bzero(&m_un_server, sizeof(m_un_server));
    m_un_server.sun_family = PF_UNIX;
    strncpy(m_un_server.sun_path, PATH.c_str(), sizeof(m_un_server.sun_path) -1);
    std::size_t len = sizeof(struct sockaddr_un);

    channel = ::socket(PF_UNIX, SOCK_STREAM/*|SOCK_NONBLOCK*/, 0);
    if(channel < 0) {
        std::cout << "line: "<<__LINE__ << "Creation of Unix socket Failed" << std::endl;
        return(-1);
    }

    handle(channel);
    connected_client(noor::client_connection::Disconnected);
    /* set the reuse address flag so we don't get errors when restarting */
    auto flag = 1;
    if(::setsockopt(channel, SOL_SOCKET, SO_REUSEADDR, (std::int8_t *)&flag, sizeof(flag)) < 0 ) {
        std::cout << "line: " << __LINE__ << "Error: Could not set reuse address option on unix socket!" << std::endl;
        ::close(handle());
        handle(-1);
        return(-1);
    }

    auto rc = ::connect(channel, reinterpret_cast< struct sockaddr *>(&un_server()), len);
    if(rc == -1) {
        std::cout << __FILE__ <<":"<<__LINE__ <<"Connect is failed errno: "<< std::strerror(errno) << std::endl;
        ::close(handle());
        return(-1);
    }

    connected_client(noor::client_connection::Connected);
    return(0);
}

/**
 * @brief 
 * 
 * @return noor::Service::emp 
 */
noor::emp noor::Service::uds_rx() {
    std::uint16_t command;
    std::uint16_t message_id;
    std::uint32_t payload_size;
    std::uint16_t type;
    std::string response;
    std::array<char, 16> arr; 
    std::uint8_t EMP_HDR_SIZE = 8;
    arr.fill(0);

    auto len = recv(handle(), arr.data(), EMP_HDR_SIZE, 0);
    if(len == EMP_HDR_SIZE) {
        //parse emp header
        std::istringstream istrstr;
        istrstr.rdbuf()->pubsetbuf(arr.data(), len);
        istrstr.read(reinterpret_cast<char *>(&command), sizeof(command));
        command = ntohs(command);
        type = (command >> 14) & 0x3;
        command &= 0xFFF;
        istrstr.read(reinterpret_cast<char *>(&message_id), sizeof(message_id));
        istrstr.read(reinterpret_cast<char *>(&payload_size), sizeof(payload_size));
        message_id = ntohs(message_id);
        payload_size = ntohl(payload_size);

        std::cout << std::endl << "line: " << __LINE__ 
                  << " type: " << type << " command: " 
                  << command << " message_id: " << message_id 
                  << " payload_size: " << payload_size << std::endl;
        std::uint32_t offset = 0;
        std::unique_ptr<char[]> payload = std::make_unique<char[]>(payload_size);

        do {
            len = recv(handle(), (void *)(payload.get() + offset), (size_t)(payload_size - offset), 0);
            if(len < 0) {
                break;
            }
            offset += len;
        } while(offset != payload_size);

        if(offset == payload_size) {
            std::string ss((char *)payload.get(), payload_size);
            std::cout << "Payload: " << ss << std::endl;
            emp res;
            res.m_type = type;
            res.m_command = command;
            res.m_message_id = message_id;
            res.m_response_length = payload_size;
            res.m_response = ss;
            return(res);
        }
    }
    return(emp {});
}

/**
 * @brief 
 * 
 * @param channel 
 * @param data 
 * @param svcType 
 * @return std::int32_t 
 */
std::int32_t noor::Service::tcp_rx(std::int32_t channel, std::string& data, noor::ServiceType svcType) {

    if(Tcp_Device_Client_Connected_Service == svcType) {
        // Received from Datastore 
        return(tcp_rx(channel, data));

    } else if(Tcp_Device_Console_Connected_Service == svcType) {

        // Received the Console output
        std::array<char, 2048> payload;
        payload.fill(0);
        std::size_t len = -1;
        len = recv(channel, (void *)payload.data(), (size_t)payload.size(), 0);
        if(len < 0) {
            std::cout << "line: " << __LINE__ << " recv error for channel: " << channel << std::endl;
            return(0);
        }
        data.assign(std::string(payload.data(), len));
        return(data.length());

    }

    return(0);
}

/**
 * @brief 
 * 
 * @param channel 
 * @param data 
 * @return std::int32_t 
 */
std::int32_t noor::Service::tcp_rx(std::int32_t channel, std::string& data) {
    std::array<char, 8> arr;
    arr.fill(0);
    std::int32_t len = -1;
    //read 4 bytes - the payload length
    len = recv(channel, arr.data(), sizeof(std::int32_t), 0);
    if(!len) {
        std::cout << "line: " << __LINE__ << " channel: " << channel << " closed " << std::endl;
        return(len);

    } else if(len > 0) {
        //std::cout << "line: " << __LINE__ << " len: " << len << std::endl;
        std::uint32_t payload_len; 
        std::istringstream istrstr;
        istrstr.rdbuf()->pubsetbuf(arr.data(), len);
        istrstr.read(reinterpret_cast<char *>(&payload_len), sizeof(payload_len));
        std::int32_t offset = 0;
        payload_len = ntohl(payload_len);
        //std::cout << "line: " << __LINE__ << " tcp payload length: " << payload_len << std::endl;

        std::unique_ptr<char[]> payload = std::make_unique<char[]>(payload_len);
        do {
            len = recv(channel, (void *)(payload.get() + offset), (size_t)(payload_len - offset), 0);
            if(len < 0) {
                offset = len;
                break;
            }
            offset += len;
        } while(offset != payload_len);
                
        if(offset == payload_len) {
            std::string ss((char *)payload.get(), payload_len);
            //std::cout <<"line: "<< __LINE__ << " From TCP Client Received: " << ss << std::endl;
            data = ss;
            return(payload_len);
        }
    }

    return(std::string().length());

}

/**
 * @brief 
 * 
 * @param data 
 * @return std::int32_t 
 */
std::int32_t noor::Service::tcp_rx(std::string& data) {
    std::array<char, 8> arr;
    arr.fill(0);
    std::int32_t len = -1;
    //read 4 bytes - the payload length
    len = recv(handle(), arr.data(), sizeof(std::int32_t), 0);
    if(!len) {
        std::cout << "line: " << __LINE__ << " closed" << std::endl;
        return(std::string().length());
    } else if(len > 0) {
        std::cout << "line: " << __LINE__ << " len: " << len << std::endl;
        std::uint32_t payload_len; 
        std::istringstream istrstr;
        istrstr.rdbuf()->pubsetbuf(arr.data(), len);
        istrstr.read(reinterpret_cast<char *>(&payload_len), sizeof(payload_len));
        std::int32_t offset = 0;
        payload_len = ntohl(payload_len);
        std::cout << "line: " << __LINE__ << " tcp payload length: " << payload_len << std::endl;

        std::unique_ptr<char[]> payload = std::make_unique<char[]>(payload_len);
        do {
            len = recv(handle(), (void *)(payload.get() + offset), (size_t)(payload_len - offset), 0);
            if(len < 0) {
                offset = len;
                break;
            }
            offset += len;
        } while(offset != payload_len);
                
        if(offset == payload_len) {
            std::string ss((char *)payload.get(), payload_len);
            std::cout <<"line: "<< __LINE__ << " From TCP Client Received: " << ss << std::endl;
            data = ss;
            return(payload_len);
        }
    }

    return(std::string().length());
}

std::string noor::Service::get_contentType(std::string ext)
{
    std::string cntType("");
    /* get the extension now for content-type */
    if(!ext.compare("woff")) {
      cntType = "font/woff";
    } else if(!ext.compare("woff2")) {
      cntType = "font/woff2";
    } else if(!ext.compare("ttf")) {
      cntType = "font/ttf";
    } else if(!ext.compare("otf")) {
      cntType = "font/otf";
    } else if(!ext.compare("css")) {
      cntType = "text/css";
    } else if(!ext.compare("js")) {
      cntType = "text/javascript";
    } else if(!ext.compare("eot")) {
      cntType = "application/vnd.ms-fontobject";
    } else if(!ext.compare("html")) {
      cntType = "text/html";
    } else if(!ext.compare("svg")) {
      cntType = "image/svg+xml";
    } else if(!ext.compare("gif")) {
      cntType ="image/gif";
    } else if(!ext.compare("png")) {
      cntType = "image/png";
    } else if(!ext.compare("ico")) {
      cntType = "image/vnd.microsoft.icon";
    } else if(!ext.compare("jpg")) {
      cntType = "image/jpeg";
    } else if(!ext.compare("json")) {
      cntType = "application/json";
    } else {
      cntType = "text/html";
    }
    return(cntType);
}


std::string noor::Service::buildHttpResponseOK(Http& http, std::string body, std::string contentType)
{
    std::stringstream ss("");

    ss << "HTTP/1.1 200 OK\r\n"
       << "Connection: "
       << http.value("Connection")
       << "\r\n"
       << "Host: "
       << http.value("Host")
       << "\r\n"
       << "Access-Control-Allow-Origin: *\r\n";

    if(body.length()) {
        ss << "Content-Length: "
           << body.length()
           << "\r\n"
           << "Content-Type: "
           << contentType
           <<"\r\n"
           << "\r\n"
           << body;

    } else {
        ss << "Content-Length: 0\r\n";
    }
    return(ss.str());
}

std::string noor::Service::buildHttpRedirectResponse(Http& http, std::string rsp_body) {
    std::stringstream ss("");
    if(!rsp_body.length()) {
        rsp_body.assign("<html><title></title><head></head><body><h2>Redirecting to http://10.20.129.111</h2></body></html>");
    }

    ss << "HTTP/1.1 301 FOUND\r\n"
       << "Location: https://"
       << http.value("ipAddress")
       << ":443\r\n"
       << "Host: " << http.value("Host") << "\r\n"
       << "Connection: " << http.value("Connection") << "\r\n"
       << "Content-Type: text/html" << "\r\n"
       << "Content-Length: " << rsp_body.length() << "\r\n";
    
    if(!http.value("Origin").length()) {
        ss << "Access-Control-Allow-Origin: *\r\n";
    } else {
        ss << "Access-Control-Allow-Origin: "
           << http.value("Origin")
           << "\r\n";
    }

    ss << "\r\n"
       << rsp_body;

    return(ss.str());
}

std::string noor::Service::buildHttpResponse(Http& http, const std::string& rsp_body) {
    std::stringstream ss("");
    if(!rsp_body.length()) {
        ss << "HTTP/1.1 200 OK\r\n"
           << "Connection: close" 
           << "Content-Length: 0\r\n";
       return(ss.str());
    }

    ss << "HTTP/1.1 200 OK\r\n"
       << "Host: " << http.value("Host") << "\r\n"
       << "Connection: " << http.value("Connection") << "\r\n"
       << "Content-Type: application/json" << "\r\n";

    if(!http.value("Origin").length()) {
        ss << "Access-Control-Allow-Origin: *\r\n";
    } else {
        ss << "Access-Control-Allow-Origin: "
           << http.value("Origin")
           << "\r\n";
    }

    ss << "Content-Length: " << rsp_body.length() << "\r\n"
       << "\r\n"
       << rsp_body;

    return(ss.str());
}

std::string noor::Service::handleOptionsMethod(Http& http) {
    std::stringstream http_header("");
    http_header << "HTTP/1.1 200 OK\r\n";
    http_header << "Access-Control-Allow-Methods: GET, POST, OPTIONS, PUT, DELETE\r\n";
    http_header << "Access-Control-Allow-Headers: DNT, User-Agent, X-Requested-With, If-Modified-Since, Cache-Control, Content-Type, Range\r\n";
    http_header << "Access-Control-Max-Age: 1728000\r\n";

    if(!http.value("Origin").length()) {
        http_header << "Access-Control-Allow-Origin: *\r\n";
    } else {
        http_header << "Access-Control-Allow-Origin: "
           << http.value("Origin")
           << "\r\n";
    }
    
    http_header << "Content-Type: text/plain; charset=utf-8\r\n";
    http_header << "Content-Length: 0\r\n";
    http_header << "\r\n";

    return(http_header.str());
}
std::string noor::Service::handleGetMethod(Http& http) {

    std::stringstream ss("");
    if(!http.uri().compare(0, 19, "/api/v1/device/list")) {
        //Provide the device's list to Webclient.
        if(!noor::CommonResponse::instance().response().empty()) {
            ss << "[";
            std::for_each(noor::CommonResponse::instance().response().begin(), noor::CommonResponse::instance().response().end(), [&](const auto& ent) {
                ss << "[";
                std::for_each(ent.second.begin(), ent.second.end(), [&](const auto & elm) {
                    ss << elm << ",";
                });
                //get rid of last ',' from above array now.
                ss.seekp(-1, std::ios_base::end);
                ss << "],";
            });
            //get rid of last ','.
            ss.seekp(-1, std::ios_base::end);
            ss << "]";
        } else {
            //Test Data ---
            ss << "[[{\"device.machine\": \"lexus-medium\", \"device.provisioning.serial\":\"A1234\", \"net.interface[w1].ipv4.address\": \"192.168.0.140\"}]]";
        }

        auto rsp = buildHttpResponse(http, ss.str());
        return(rsp);

    } else if(!http.uri().compare(0, 17, "/api/v1/device/ui")) {
        return(buildHttpRedirectResponse(http));

    } else if(!http.uri().compare(0, 21, "/api/v1/shell/command")) {
        //Sheel command to be executed
        http.dump();
        auto serialNumber = http.value("serialNo");
        auto command = http.value("command");
        auto IP = http.value("ipAddress");

        if(!command.length()) {
            return(buildHttpResponse(http, ""));
        }

        //Find the TCP Client Fd for sending the command.
        if(!tcp_connections().empty()) {
            auto it = std::find_if(tcp_connections().begin(), tcp_connections().end(), [&](const auto& ent) -> bool {
                return(IP.length() && IP == std::get<1>(ent.second));
            });
            if(it != tcp_connections().end()) {
                auto connFd = it->first;
                //auto ret = tcp_tx(connFd, command);
            }
        }

    } else if((!http.uri().compare(0, 7, "/webui/"))) {
        /* build the file name now */
        std::string fileName("");
        std::string ext("");

        std::size_t found = http.uri().find_last_of(".");
        if(found != std::string::npos) {
          ext = http.uri().substr((found + 1), (http.uri().length() - found));
          fileName = http.uri().substr(6, (http.uri().length() - 6));
          std::string newFile = "../webgui/swi/" + fileName;
          /* Open the index.html file and send it to web browser. */
          std::ifstream ifs(newFile.c_str());
          std::stringstream ss("");

          if(ifs.is_open()) {
              std::string cntType("");
              cntType = get_contentType(ext); 

              ss << ifs.rdbuf();
              ifs.close();
              return(buildHttpResponseOK(http, ss.str(), cntType));
          } {
            std::cout << "line: " << __LINE__ << " couldn't open the file: " << newFile << std::endl; 
          }
        } else {
            std::cout <<"line: " << __LINE__ << " processing index.html file " << std::endl;
            std::string newFile = "../webgui/swi/index.html";
            /* Open the index.html file and send it to web browser. */
            std::ifstream ifs(newFile.c_str(), std::ios::binary);
            std::stringstream ss("");
            std::string cntType("");

            if(ifs.is_open()) {
                cntType = "text/html";
                ss << ifs.rdbuf();
                ifs.close();
                return(buildHttpResponseOK(http, ss.str(), cntType));
            } else {
                std::cout << "line: " << __LINE__ << " couldn't open the file: " << newFile << std::endl;
            }
        }
    } else if(!http.uri().compare(0, 1, "/")) {
        std::cout <<"line: " << __LINE__ << " processing index.html file " << std::endl;
        std::string newFile = "../webgui/swi/index.html";
        /* Open the index.html file and send it to web browser. */
        std::ifstream ifs(newFile.c_str(), std::ios::binary);
        std::stringstream ss("");
        std::string cntType("");

        if(ifs.is_open()) {
            cntType = "text/html";
            ss << ifs.rdbuf();
            ifs.close();

            return(buildHttpResponseOK(http, ss.str(), cntType));
        } else {
            std::cout << "line: " << __LINE__ << " couldn't open the file: " << newFile << std::endl;
        }
    }

    return(std::string());
}

std::string noor::Service::process_web_request(const std::string& req) {
    Http http(req);
    if(!http.method().compare("GET")) {
        //handleGetRequest()
        auto rsp_body = handleGetMethod(http);
        return(rsp_body);
        /*
        if(rsp_body.length()) {
            auto rsp = buildHttpResponse(http, rsp_body);
            return(rsp);
        }*/
    }
    else if(!http.method().compare("POST")) {
        //handlePostMethod()
    }
    else if(!http.method().compare("PUT")) {
        //handlePutMethod()
    }
    else if(!http.method().compare("OPTIONS")) {
        return(handleOptionsMethod(http));
    }
    else if(!http.method().compare("DELETE")) {
        //handleDeleteMethod()
    }
    else {
        //Error
    }
    return(std::string());
}

/**
 * @brief 
 * 
 * @param http 
 * @return std::string 
 */
std::string noor::Service::build_web_response(Http& http) {
    //Build HTTP Response
    std::cout << "URI: " << http.uri() << " method: " << http.method() << std::endl;
    std::stringstream ss("");
    std::string payload("<html><title></title><head></head><body><h2>Redirecting to http://10.20.129.111</h2></body></html>");
    ss << "HTTP/1.1 302 Found\r\n"
       //<< "Location: https://192.168.1.1:443\r\n"
       << "Location: http://10.20.129.111\r\n"
       << "Content-length: " << payload.length() << "\r\n"
       << "Connection: close\r\n"
       //<< "Cookie: unity_token=IC3wWl66tT3XrqO88iLBSxCYbuxhPvGz; unity_login=admin; last_connection={\"success_last\":\"Sat Apr  8 03:47:22 2023\",\"success_from\":\"192.168.1.100\",\"failures\":0}" 
       << "Cookie: " << http.value("Cookies")
       << "\r\n\r\n"
       << payload;

    std::cout << "The Web Response is " << ss.str() << std::endl;
    return(ss.str());
}

/**
 * @brief 
 * 
 * @param channel 
 * @param data 
 * @return std::int32_t 
 */
std::int32_t noor::Service::web_rx(std::int32_t channel, std::string& data) {
    std::cout << "line: " << __LINE__ << " " << __PRETTY_FUNCTION__ << " handle:" << channel <<std::endl;
    std::array<char, 2048> arr;
    arr.fill(0);
    std::int32_t len = -1;
    len = recv(channel, arr.data(), arr.size(), 0);
    if(!len) {
        std::cout << "function: "<<__FUNCTION__ << " line: " << __LINE__ << " channel: " << channel << " be closed" << std::endl;
        return(len);

    } else if(len > 0) {
        std::string ss(arr.data(), len);
        std::cout << "HTTP:" << std::endl << ss << std::endl;
        Http http(ss);
        std::cout << "line: " << __LINE__ << " URI: "   << http.uri()    << std::endl;
        std::cout << "line: " << __LINE__ << " Header " << http.header() << std::endl;
        std::cout << "line: " << __LINE__ << " Body "   << http.body()   << std::endl;
        std::uint32_t offset = 0;
        auto cl = http.value("Content-Length");
        size_t payload_len = 0;

        if(!cl.length()) {
            std::cout << "line: " << __LINE__ << " Content-Length is not present" << std::endl;
            data = ss;
            return(data.length());

        } else {
            std::cout << "function: "<< __FUNCTION__ << " line: " << __LINE__ <<" value of Content-Length " << cl << std::endl;
            payload_len = std::stoi(cl);
            if(len == (payload_len + http.header().length())) {
                //We have received the full HTTP packet
                data = ss;
                return(data.length());

            } else {
                //compute the effective length
                payload_len = (std::stoi(cl) + http.header().length() - len);
                std::unique_ptr<char[]> payload = std::make_unique<char[]>(payload_len);
                std::int32_t tmp_len = 0;
                do {
                    tmp_len = recv(channel, (void *)(payload.get() + offset), (size_t)(payload_len - offset), 0);
                    if(tmp_len < 0) {
                        offset = len;
                        break;
                    }
                    offset += tmp_len;
                    
                } while(offset != payload_len);

                if(offset == payload_len) {
                    std::string header(arr.data(), len);
                    std::string ss((char *)payload.get(), payload_len);
                    std::string request = header + ss;
                    std::cout << "function: "<<__FUNCTION__ <<" line: " <<__LINE__ << " From Web Client Received: " << request << std::endl;
                    data = ss;
                    return(data.length());
                }
            }
        }
    }
    return(0);
}

/**
 * @brief 
 * 
 * @param data 
 * @return std::int32_t 
 */
std::int32_t noor::Service::web_rx(std::string& data) {
    std::cout << "line: " << __LINE__ << " " << __PRETTY_FUNCTION__ << " handle:" << handle() <<std::endl;
    std::array<char, 2048> arr;
    arr.fill(0);
    std::int32_t len = -1;
    len = recv(handle(), arr.data(), arr.size(), 0);
    if(!len) {
        std::cout << "function: "<<__FUNCTION__ << " line: " << __LINE__ << " closed" << std::endl;
    } else if(len > 0) {
        std::string ss(arr.data(), len);
        std::cout << "HTTP: " << std::endl << ss << std::endl;
        Http http(ss);
        std::cout << "line: " << __LINE__ << " URI: "   << http.uri()    << std::endl;
        std::cout << "line: " << __LINE__ << " Header " << http.header() << std::endl;
        std::cout << "line: " << __LINE__ << " Body "   << http.body()   << std::endl;
        std::uint32_t offset = 0;
        auto cl = http.value("Content-Length");
        size_t payload_len = 0;

        if(!cl.length()) {
            std::cout << "line: " << __LINE__ << " Content-Length is not present" << std::endl;
            data = ss;
            return(data.length());

        } else {
            std::cout << "function: "<< __FUNCTION__ << " line: " << __LINE__ <<" value of Content-Length " << cl << std::endl;
            payload_len = std::stoi(cl);
            if(len == (payload_len + http.header().length())) {
                //We have received the full HTTP packet
                data = ss;
                return(data.length());

            } else {
                //compute the effective length
                payload_len = (std::stoi(cl) + http.header().length() - len);
                std::unique_ptr<char[]> payload = std::make_unique<char[]>(payload_len);
                std::int32_t tmp_len = 0;
                do {
                    tmp_len = recv(handle(), (void *)(payload.get() + offset), (size_t)(payload_len - offset), 0);
                    if(tmp_len < 0) {
                        offset = len;
                        break;
                    }
                    offset += tmp_len;
                    
                } while(offset != payload_len);

                if(offset == payload_len) {
                    std::string header(arr.data(), len);
                    std::string ss((char *)payload.get(), payload_len);
                    std::string request = header + ss;
                    std::cout << "function: "<<__FUNCTION__ <<" line: " <<__LINE__ << " From Web Client Received: " << request << std::endl;
                    data = ss;
                    return(data.length());
                }
            }
        }
    }
    return(std::string().length());  
}

/**
 * @brief 
 * 
 * @param data 
 * @return std::int32_t 
 */
std::int32_t noor::Service::udp_rx(std::string& data) {
    std::array<char, 8> arr;
    arr.fill(0);
    std::int32_t len = -1;
    struct sockaddr_in peer;
    socklen_t peer_addr_len = sizeof(peer);

    len = recvfrom(handle(), arr.data(), sizeof(std::int32_t), MSG_PEEK, (struct sockaddr *)&peer, &peer_addr_len);
    if(!len) {
        std::cout << "line: " << __LINE__ << " closed" << std::endl;
        return(std::string().length());

    } else if(len > 0) {
        std::int32_t payload_len = 0; 
        std::istringstream istrstr;
        istrstr.rdbuf()->pubsetbuf(arr.data(), len);
        std::cout << "\nline: " << __LINE__ << " to be received bytes: " << len <<std::endl;
        istrstr.read(reinterpret_cast<char *>(&payload_len), sizeof(payload_len));
        std::uint32_t offset = 0;
        payload_len = ntohl(payload_len) + 4; //+4 for 4bytes of length prepended to payload
        std::cout << "line: " << __LINE__ << " udp payload length: " << payload_len << std::endl;

        std::unique_ptr<char[]> payload = std::make_unique<char[]>(payload_len);

        do {
            len = recvfrom(handle(), (void *)(payload.get() + offset), (size_t)(payload_len - offset), MSG_WAITALL, (struct sockaddr *)&peer, &peer_addr_len);
            if(len < 0) {
                offset = len;
                break;
            }
            offset += len;
        } while(offset != payload_len);
                
        if(offset> 0 && offset == payload_len) {
            std::string ss((char *)payload.get() + 4, payload_len-4);
            //std::cout << "line: "<< __LINE__ << " From UDP Client Received: " << ss << std::endl;
            data = ss;
            return(ss.length());
        }
    }
    return(std::string().length());
}

/**
 * @brief 
 * 
 * @param IP 
 * @param PORT 
 * @return std::int32_t 
 */
std::int32_t noor::Service::tcp_server(const std::string& IP, std::uint16_t PORT) {
   /* Set up the address we're going to bind to. */
    bzero(&m_inet_server, sizeof(m_inet_server));
    m_inet_server.sin_family = AF_INET;
    m_inet_server.sin_port = htons(PORT);
    if(!IP.compare("127.0.0.1")) {
        m_inet_server.sin_addr.s_addr = INADDR_ANY;    
    } else {
        m_inet_server.sin_addr.s_addr = inet_addr(IP.c_str());
    }
    memset(m_inet_server.sin_zero, 0, sizeof(m_inet_server.sin_zero));
    auto len = sizeof(m_inet_server);

    std::int32_t channel = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(channel < 0) {
        std::cout << "line: " << __LINE__ << " Creation of INET socket Failed" << std::endl;
        return(-1);
    }

    handle(channel);
    /* set the reuse address flag so we don't get errors when restarting */
    auto flag = 1;
    if(::setsockopt(channel, SOL_SOCKET, SO_REUSEADDR, (std::int8_t *)&flag, sizeof(flag)) < 0 ) {
        std::cout << "line: " << __LINE__ << "Error: Could not set reuse address option on INET socket!" << std::endl;
        ::close(handle());
        handle(-1);
        return(-1);
    }
    auto ret = ::bind(channel, (struct sockaddr *)&m_inet_server, sizeof(m_inet_server));
    if(ret < 0) {
        std::cout <<"line: " << __LINE__ << " bind to IP: " << IP << " PORT: " << PORT << " Failed" <<std::endl;
        ::close(handle());
        handle(-1);
	    return(-1);
    }

    if(listen(channel, 10) < 0) {
        std::cout << "line: " << __LINE__ << " listen to channel: " << channel << " Failed" <<std::endl;
        ::close(handle());
        handle(-1);
	    return(-1);
    }

    return(0); 
}

/**
 * @brief 
 * 
 * @param IP 
 * @param PORT 
 * @return std::int32_t 
 */
std::int32_t noor::Service::udp_server(const std::string& IP, std::uint16_t PORT) {
    // UDP Server .... 
    /* Set up the address we're going to bind to. */
    bzero(&m_inet_server, sizeof(m_inet_server));
    m_inet_server.sin_family = AF_INET;
    m_inet_server.sin_port = htons(PORT);
    m_inet_server.sin_addr.s_addr = inet_addr(IP.c_str());
    memset(m_inet_server.sin_zero, 0, sizeof(m_inet_server.sin_zero));
    auto len = sizeof(m_inet_server);

    std::int32_t channel = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(channel < 0) {
        std::cout << "line: " << __LINE__ <<" Creation of INET socket Failed" << std::endl;
        return(-1);
    }

    handle(channel);
    
    /* set the reuse address flag so we don't get errors when restarting */
    auto flag = 1;
    if(::setsockopt(channel, SOL_SOCKET, SO_REUSEADDR, (std::int8_t *)&flag, sizeof(flag)) < 0 ) {
        std::cout << "line: " << __LINE__ << " Error: Could not set reuse address option on INET socket!" << std::endl;
        ::close(handle());
        handle(-1);
        return(-1);
    }

    auto ret = ::bind(channel, (struct sockaddr *)&inet_server(), len);
    if(ret < 0) {
        std::cout << "line: "<< __LINE__ << " bind to UDP protocol failed" << std::endl;
        ::close(handle());
        handle(-1);
        return(-1);
    }
    return(0);
}

/**
 * @brief 
 * 
 * @param IP 
 * @param PORT 
 * @return std::int32_t 
 */
std::int32_t noor::Service::web_server(const std::string& IP, std::uint16_t PORT) {
    /* Set up the address we're going to bind to. */
    bzero(&m_inet_server, sizeof(m_inet_server));
    m_inet_server.sin_family = AF_INET;
    m_inet_server.sin_port = htons(PORT);

    if(!IP.compare("127.0.0.1")) {
        m_inet_server.sin_addr.s_addr = INADDR_ANY;    
    } else {
        m_inet_server.sin_addr.s_addr = inet_addr(IP.c_str());
    }

    memset(m_inet_server.sin_zero, 0, sizeof(m_inet_server.sin_zero));
    auto len = sizeof(m_inet_server);

    std::int32_t channel = ::socket(AF_INET, SOCK_STREAM, 0);
    if(channel < 0) {
        std::cout << "Creation of INET socket Failed" << std::endl;
        return(-1);
    }

    handle(channel);
    /* set the reuse address flag so we don't get errors when restarting */
    auto flag = 1;
    if(::setsockopt(channel, SOL_SOCKET, SO_REUSEADDR, (std::int8_t *)&flag, sizeof(flag)) < 0 ) {
        std::cout << "Error: Could not set reuse address option on INET socket!" << std::endl;
        ::close(handle());
        handle(-1);
        return(-1);
    }
    auto ret = ::bind(channel, (struct sockaddr *)&inet_server(), len);
    if(ret < 0) {
        std::cout << "line: "<< __LINE__ << " bind to IP: " << IP << " PORT: " << PORT << " Failed" <<std::endl;
        ::close(handle());
        handle(-1);
	return(-1);
    }

    if(listen(channel, 10) < 0) {
        std::cout << "line: " << __LINE__ << " listen to channel: " << channel << " Failed" <<std::endl;
        ::close(handle());
        handle(-1);
	return(-1);
    }

    return(0); 
}

/**
 * @brief 
 * 
 * @param channel 
 * @param req 
 * @return std::int32_t 
 */
std::int32_t noor::Service::web_tx(std::int32_t channel, const std::string& req) {
    std::int32_t offset = 0;
    std::int32_t req_len = req.length();
    std::int32_t len = -1;

    do {
        len = send(channel, req.data() + offset, req_len - offset, 0);
        if(len < 0) {
            offset = len;
            break;
        } 
        offset += len;
    } while(offset != req_len);

    return(offset);
}

/**
 * @brief 
 * 
 * @param req 
 * @return std::int32_t 
 */
std::int32_t noor::Service::web_tx(const std::string& req) {
    std::int32_t offset = 0;
    std::int32_t req_len = req.length();
    std::int32_t len = -1;

    do {
        len = send(handle(), req.data() + offset, req_len - offset, 0);
        if(len < 0) {
            offset = len;
            break;
        } 
        offset += len;
    } while(offset != req_len);

    return(offset);
}

/**
 * @brief 
 * 
 * @param req 
 * @return std::int32_t 
 */
std::int32_t noor::Service::udp_tx(const std::string& req) {
    std::int32_t offset = 0;
    std::int32_t payload_len = req.length();
    std::int32_t len = -1;
    auto total_len = htonl(payload_len);
    std::stringstream data("");
    data.write(reinterpret_cast <char *>(&total_len), sizeof(std::int32_t));
    data << req;
    payload_len = data.str().length();

    do {
        len = sendto(handle(), data.str().data() + offset, payload_len - offset, 0, (struct sockaddr *)&m_inet_server, sizeof(m_inet_server));
        if(len < 0) {
            offset = len;
            break;
        }
        offset += len;
    } while(offset != payload_len);

    if(offset > 0 && offset == payload_len) {
        std::cout <<"line: " << __LINE__ << " Request sent to UDP Server successfully length: "<< offset << std::endl;
    }
    return(offset);
}

/**
 * @brief 
 * 
 * @param req 
 * @return std::int32_t 
 */
std::int32_t noor::Service::uds_tx(const std::string& req) {
    std::int32_t offset = 0;
    std::int32_t req_len = req.length();
    std::int32_t len = -1;

    do {
        len = send(handle(), req.data() + offset, req_len - offset, 0);
        if(len < 0) {
            offset = len;
            break;
        } 
        offset += len;
    } while(offset != req_len);

    if(offset == req_len) {
        for(std::int32_t idx = 0; idx < 8; ++idx) {
            printf("%X ", req.c_str()[idx]);
        }
        std::string ss(reinterpret_cast<const char *>(&req.c_str()[8]));
        std::cout << "Query pushed to DS ==> " << ss << std::endl;
    }
    return(offset);
}

/**
 * @brief 
 * 
 * @param req 
 * @return std::int32_t 
 */
std::int32_t noor::Service::tcp_tx(const std::string& req) {
    std::int32_t offset = 0;
    std::int32_t req_len = req.length();
    std::int32_t len = -1;
    auto payload_len = htonl(req_len);
    std::stringstream data("");
    data.write (reinterpret_cast <char *>(&payload_len), sizeof(std::int32_t));
    data << req;
    req_len = data.str().length();
    do {
        len = send(handle(), data.str().data() + offset, req_len - offset, 0);
        if(len < 0) {
            offset = len;
            break;
        }
        offset += len;
    } while(offset != req_len);

    if(offset == req_len) {
        std::cout <<"line: "<< __LINE__ << " Request sent to TCP Server successfully: req_len:" << req_len << std::endl;
    }
    return(offset);
}

std::int32_t noor::Service::tcp_tx(std::int32_t channel, const std::string& req) {
    std::int32_t offset = 0;
    std::int32_t req_len = req.length();
    std::int32_t len = -1;
    auto payload_len = htonl(req_len);
    std::stringstream data("");
    data.write (reinterpret_cast <char *>(&payload_len), sizeof(std::int32_t));
    data << req;

    req_len = data.str().length();
    do {
        len = send(channel, data.str().data() + offset, req_len - offset, 0);
        if(len < 0) {
            offset = len;
            break;
        }
        offset += len;
    } while(offset != req_len);

    if(offset == req_len) {
        std::cout <<"line: "<< __LINE__ << " Request sent to TCP Client successfully: req_len:" << req_len << std::endl;
    }
    return(offset);
}

/**
 * @brief 
 * 
 * @param timeout_in_ms 
 * @param intf_list 
 * @return std::int32_t 
 */
std::int32_t noor::Service::start_client(std::uint32_t timeout_in_ms, std::vector<std::tuple<std::unique_ptr<noor::Service>, noor::ServiceType>> services) {
    int conns  = -1;
    fd_set fdList;
    fd_set fdWrite;

    // These are pipe FD
    std::int32_t rdFd[2] = {-1, -1};
    std::int32_t wrFd[2] = {-1, -1};

    while (1) {

        struct timeval to;
        to.tv_sec = timeout_in_ms / 1000;
        to.tv_usec = timeout_in_ms % 1000;

        FD_ZERO(&fdList);
        FD_ZERO(&fdWrite);
        
        for(auto& [inst, type]: services) {
            auto channel = inst->handle();

            if(channel > 0 && noor::ServiceType::Unix_Data_Store_Client_Service_Sync == type) {
                FD_SET(channel, &fdList);

            } else if(channel > 0 && noor::ServiceType::Tcp_Web_Client_Proxy_Service == type) {
                FD_SET(channel, &fdList);

            } else if(channel > 0 && noor::ServiceType::Tcp_Device_Client_Service_Async == type) {
                if(inst->connected_client(channel) == noor::client_connection::Connected) {
                    //std::cout << "line: " << __LINE__ << " function: " << __FUNCTION__ << " handle: " << channel << "connected " << std::endl;
                    FD_SET(channel, &fdList);
                } else if(inst->connected_client(inst->handle()) == noor::client_connection::Inprogress) {
                    //std::cout << "line: " << __LINE__ << " function: " << __FUNCTION__ << " handle: " << channel << "fdWrite " << std::endl;
                    FD_SET(channel, &fdWrite);
                }

            } else if(channel > 0 && noor::ServiceType::Tcp_Device_Console_Client_Service_Async == type) {
                if(inst->connected_client(channel) == noor::client_connection::Connected) {
                    //std::cout << "line: " << __LINE__ << " function: " << __FUNCTION__ << " handle: " << channel << "connected " << std::endl;
                    FD_SET(channel, &fdList);
                } else if(inst->connected_client(inst->handle()) == noor::client_connection::Inprogress) {
                    //std::cout << "line: " << __LINE__ << " function: " << __FUNCTION__ << " handle: " << channel << "fdWrite " << std::endl;
                    FD_SET(channel, &fdWrite);
                }

            }
        }

        //This is used to pass shell command to child process.
        if(rdFd[0] > 0) {
            FD_SET(rdFd[0], &fdList);
        }

        conns = ::select(FD_SETSIZE, (fd_set *)&fdList, (fd_set *)&fdWrite, (fd_set *)NULL, (struct timeval *)&to);
        
        if(conns > 0) {

            if(rdFd[0] > 0 && FD_ISSET(rdFd[0], &fdList)) {
                //Shell command response has come pass on to web for display.
                auto it = std::find_if(services.begin(), services.end(),[&](const auto& ent) -> bool {
                    auto &[inst, type] = ent;
                    return(type == noor::ServiceType::Tcp_Device_Console_Client_Service_Async);
                });

                if(it != services.end()) {
                    auto &inst = std::get<0>(*it);
                    auto channel = inst->handle();
                    std::string rsp("");
                    std::int32_t len = -1;
                    len = recv(rdFd[0], rsp.data(), 2048, 0);
                    if(len > 0) {
                        rsp.resize(len);
                        auto ret = tcp_tx(channel, rsp);
                        std::cout << "line: " << __LINE__ << " Shell command response length: " << ret << " rsp: " << rsp << std::endl;
                    }
                }
            }

            for(auto& [inst, type]: services) {
                auto channel = inst->handle();

                // Received on Unix Socket
                if(channel > 0 && type == noor::ServiceType::Unix_Data_Store_Client_Service_Sync && FD_ISSET(channel, &fdList)) {
                    //Received response from Data store
                    std::string request("");
                    std::cout << "From DS line: " << __LINE__<<" Response received " << std::endl;
                    auto req = inst->uds_rx();
                    if(!req.m_response.length()) {
                        ::close(channel);
                        inst->connected_client().erase(channel);
                        inst->handle(-1);
                        std::cout <<"line: " << __LINE__ << " Data store is down" << std::endl;
                        exit(0);
                    } else {
                        std::cout << "line: " << __LINE__ << " Caching the response" << std::endl;
                        //Cache the response and will be sent later when TCP connection is established or upon timed out
                        inst->update_response_to_cache(req.m_message_id, req.m_response);
                    }
                }

                if(channel > 0 && type == noor::ServiceType::Tcp_Web_Client_Proxy_Service && FD_ISSET(channel, &fdList)) {
                    // send to tcp server (tcp_tx)
                    //send to DS APP Consumer 
                    std::string rsp("");
                    auto ret = recv(channel, rsp.data(), 2048, 0);
                    if(ret > 0) {
                        rsp.resize(ret);
                        std::cout << "line: " << __LINE__ << " rsp: " << rsp << std::endl; 
                        
                    }
                }

                //The TCP client might be connected
                if(channel > 0 && type == noor::ServiceType::Tcp_Device_Client_Service_Async && FD_ISSET(channel, &fdWrite)) {
                    //TCP connection established successfully.
                    //Push changes if any now
                    //When the connection establishment (for non-blocking socket) encounters an error, the descriptor becomes both readable and writable (p. 530 of TCPv2).
                    socklen_t optlen;
                    std::int32_t optval = -1;
                    optlen = sizeof (optval);
                    if(!getsockopt(channel, SOL_SOCKET, SO_ERROR, &optval, &optlen)) {
                        struct sockaddr_in peer;
                        socklen_t sock_len = sizeof(peer);
                        memset(&peer, 0, sizeof(peer));
                        auto ret = getpeername(channel, (struct sockaddr *)&peer, &sock_len);
                        if(ret < 0 && errno == ENOTCONN) {
                            ::close(channel);
                            inst->connected_client().erase(channel);
                            inst->handle(-1);
                        } else if(!ret) {
                            //TCP Client is connected 
                            inst->connected_client(noor::client_connection::Connected);
                            FD_CLR(channel, &fdWrite);
                            FD_ZERO(&fdWrite);
                            std::cout << "line: " << __LINE__ << " async data store Connected to server handle: " << inst->handle() << std::endl;

                            auto it = std::find_if(services.begin(), services.end(), [&](const auto& ent) {
                                return(noor::ServiceType::Unix_Data_Store_Client_Service_Sync == std::get<1>(ent));
                            });

                            if(it!= services.end() && !std::get<0>(*it)->response_cache().empty()) {
                                for(const auto& ent: std::get<0>(*it)->response_cache()) {
                                    std::string payload = std::get<cache_element::RESPONSE>(ent);

                                    //don't push to TCP server If response is awaited.
                                    if(payload.compare("default")) {
                                        std::uint32_t payload_len = payload.length();
                                        payload_len = htonl(payload_len);
                                        std::stringstream data("");
                                        data.write (reinterpret_cast <char *>(&payload_len), sizeof(payload_len));
                                        data << payload;
                                        auto ret = inst->tcp_tx(payload);
                                        std::cout << "line: " << __LINE__ << " sent to TCP Server data-length:"<< ret << std::endl;
                                    }
                                }
                            }
                        }
                    }
                }
                if(channel > 0 && type == noor::ServiceType::Tcp_Device_Client_Service_Async && FD_ISSET(channel, &fdList)) {
                    //From TCP Server
                    std::string request("");
                    auto req = inst->tcp_rx(request);
                    std::cout << "line: "<< __LINE__ << " Response received from TCP Server length:" << req << std::endl;
                    if(!req && inst->connected_client(channel) == noor::client_connection::Connected) {
                        ::close(channel);
                        inst->connected_client().erase(channel);
                        inst->handle(-1);
                    } else {
                        //Got from TCP server 
                        std::cout <<"line: " << __LINE__ << "Received from TCP server length: " << req << " command: " << request << std::endl;
                        //send to http server on device 
                        auto it = std::find_if(services.begin(), services.end(), [&](const auto &ent) {
                            return(noor::ServiceType::Tcp_Web_Client_Proxy_Service == std::get<1>(ent));
                        });

                        if(it != services.end()) {
                            auto &inst = std::get<0>(*it);
                            auto channel = inst->handle();
                            std::cout << "line: " << __LINE__ << " received fro web-proxy " << request << std::endl;
                            std::int32_t offset = 0;
                            do {
                            auto ret = send(channel, request.data() + offset , request.length() - offset, 0);
                            offset += ret;
                            } while(offset != request.length());
                        }
                    }
                }

                //The TCP client might be connected
                if(channel > 0 && type == noor::ServiceType::Tcp_Device_Console_Client_Service_Async && FD_ISSET(channel, &fdWrite)) {
                    //TCP connection established successfully.
                    //Push changes if any now
                    //When the connection establishment (for non-blocking socket) encounters an error, the descriptor becomes both readable and writable (p. 530 of TCPv2).
                    socklen_t optlen;
                    std::int32_t optval = -1;
                    optlen = sizeof (optval);
                    if(!getsockopt(channel, SOL_SOCKET, SO_ERROR, &optval, &optlen)) {
                        struct sockaddr_in peer;
                        socklen_t sock_len = sizeof(peer);
                        memset(&peer, 0, sizeof(peer));
                        auto ret = getpeername(channel, (struct sockaddr *)&peer, &sock_len);
                        if(ret < 0 && errno == ENOTCONN) {
                            ::close(channel);
                            inst->connected_client().erase(channel);
                            inst->handle(-1);
                        } else if(!ret) {
                            //TCP Client is connected 
                            inst->connected_client(noor::client_connection::Connected);
                            FD_CLR(channel, &fdWrite);
                            FD_ZERO(&fdWrite);
                            std::cout << "line: " << __LINE__ << " Device Console App Connected to server handle: " << inst->handle() << std::endl;
                            // create command processing process using fork.
                            #if 0
                            pipe(rdFd);
                            pipe(wrFd);
                            
                            auto pid = fork();
                            if(pid > 0) {
                                //Parent process
                                //::close(rdFd[1]);
                                //::close(wrFd[0]);
                                //exit hte child process now
                                
                            } else if(!pid) {
                                //Child process
                                ::close(rdFd[0]);
                                ::close(wrFd[1]);
                                ::dup2(rdFd[1], fileno(stdout));
                                ::dup2(wrFd[0], fileno(stdin));

                                //block the parent process now.
                                char* args[] = {"/bin/sh", NULL};
                                ::execlp(args[0], args[0], args[1]);
                            } else {
                                //error
                            }

                            ::close(rdFd[1]);
                            ::close(wrFd[0]);
                            // use rdFd[0] -- read/recv from Child process
                            // use wrFd[1] -- write/send to Child process
                            //dup2(rdFd[0], channel);
                            #endif
                        }
                    }
                }
                if(channel > 0 && type == noor::ServiceType::Tcp_Device_Console_Client_Service_Async && FD_ISSET(channel, &fdList)) {
                    //From TCP Server
                    std::string request("");
                    auto req = inst->tcp_rx(request);
                    std::cout << "line: "<< __LINE__ << " Response received from TCP Server length:" << req << std::endl;
                    if(!req && inst->connected_client(channel) == noor::client_connection::Connected) {
                        ::close(channel);
                        inst->connected_client().erase(channel);
                        inst->handle(-1);
                    } else {
                        //Got from TCP server 
                        std::cout <<"line: " << __LINE__ << " Received from TCP server for shell command length: " << req << std::endl;
                        if(wrFd[1] > 0) {
                            auto len = send(wrFd[1], request.data(), req, 0);
                            std::cout <<"line: " << __LINE__ << " sent to Shell Process length: " << len << " command: " << request << std::endl;
                        }
                    }
                }
            }
        }
        else if(!conns) {
            //time out happens
            auto it = std::find_if(services.begin(), services.end(), [&](auto& ent) {
                auto type = std::get<1>(ent);
                return(type == noor::ServiceType::Tcp_Device_Client_Service_Async);
            });
            if((it != services.end()) && (std::get<0>(*it)->handle() < 0) && (!std::get<0>(*it)->get_config().at("protocol").compare("tcp"))) {
                std::get<0>(*it)->tcp_client_async(std::get<0>(*it)->get_config().at("server-ip"), std::stoi(std::get<0>(*it)->get_config().at("server-port")));
            }

            it = std::find_if(services.begin(), services.end(), [&](auto& ent) {
                auto type = std::get<1>(ent);
                return(type == noor::ServiceType::Tcp_Device_Console_Client_Service_Async);
            });

            if((it != services.end()) && (std::get<0>(*it)->handle() < 0) && (!std::get<0>(*it)->get_config().at("protocol").compare("tcp"))) {
                memset(rdFd, -1, sizeof(rdFd));
                memset(wrFd, -1, sizeof(wrFd));
                std::get<0>(*it)->tcp_client_async(std::get<0>(*it)->get_config().at("server-ip"), 65344);
            }

        }
    } /* End of while loop */
}

/**
 * @brief 
 * 
 * @param timeout_in_ms 
 * @param intf_list 
 * @return * std::int32_t 
 */
std::int32_t noor::Service::start_server(std::uint32_t timeout_in_ms, 
                                              std::vector<std::tuple<std::unique_ptr<noor::Service>, noor::ServiceType>> services) {
    int conns   = -1;
    fd_set readFd;

    while (1) {

        /* A timeout for 100ms*/ 
        struct timeval to;
        to.tv_sec = timeout_in_ms /1000;
        to.tv_usec = timeout_in_ms % 1000;
        FD_ZERO(&readFd);

        for(const auto& [inst, type]: services) {

            // For handling request from Web client
            if(noor::ServiceType::Tcp_Web_Server_Service == type && inst->handle() > 0) {
                FD_SET(inst->handle(), &readFd);
            }
            
            // For Receiving Data from Data store
            if(noor::ServiceType::Tcp_Device_Server_Service == type && inst->handle() > 0) {
                FD_SET(inst->handle(), &readFd);
            }

            // For receiving console command output
            if(noor::ServiceType::Tcp_Device_Console_Server_Service == type && inst->handle() > 0) {
                FD_SET(inst->handle(), &readFd);
            }

            if(!inst->web_connections().empty()) {
                for(const auto& [key, value]: inst->web_connections()) {
                    auto channel = std::get<0>(value);
                    if(channel > 0) { 
                        FD_SET(channel, &readFd);
                    }
                }
            }

            if(!inst->tcp_connections().empty()) {
                for(const auto& [key, value]: inst->tcp_connections()) {
                    auto channel = std::get<0>(value);
                    if(channel > 0) { 
                        FD_SET(channel, &readFd);
                    }
                }
            }
        }

        conns = ::select(FD_SETSIZE, (fd_set *)&readFd, (fd_set *)NULL, (fd_set *)NULL, (struct timeval *)&to);

        if(conns > 0) {
            for(const auto& [inst, type]: services) {

                if(type == noor::ServiceType::Tcp_Device_Server_Service && inst->handle() > 0 && FD_ISSET(inst->handle(), &readFd)) {
                    // accept a new connection 
                    struct sockaddr_in peer;
                    socklen_t peer_len = sizeof(peer);
                    auto newFd = ::accept(inst->handle(), (struct sockaddr *)&peer, &peer_len);
                    if(newFd > 0) {
                        std::string IP(inet_ntoa(peer.sin_addr));
                        inst->tcp_connections().insert(std::make_pair(newFd, std::make_tuple(newFd, IP, ntohs(peer.sin_port), Tcp_Device_Client_Connected_Service, "", 0, 0, 0)));
                        std::cout << "line: " << __LINE__ << " datastore channel: " << newFd << " IP: " << IP <<" port:" << ntohs(peer.sin_port) << std::endl;
                    }
                }

                if(type == noor::ServiceType::Tcp_Device_Console_Server_Service && inst->handle() > 0 && FD_ISSET(inst->handle(), &readFd)) {
                    // accept a new connection 
                    struct sockaddr_in peer;
                    socklen_t peer_len = sizeof(peer);
                    auto newFd = ::accept(inst->handle(), (struct sockaddr *)&peer, &peer_len);
                    if(newFd > 0) {
                        std::string IP(inet_ntoa(peer.sin_addr));
                        inst->tcp_connections().insert(std::make_pair(newFd, std::make_tuple(newFd, IP, ntohs(peer.sin_port), Tcp_Device_Console_Connected_Service, "", 0, 0, 0)));
                        std::cout << "line: " << __LINE__ << " console channel: " << newFd << " IP: " << IP <<" port:" << ntohs(peer.sin_port) << std::endl;
                    }
                }

                if(type == noor::ServiceType::Tcp_Web_Server_Service && inst->handle() > 0 && FD_ISSET(inst->handle(), &readFd)) {
                    // accept a new connection 
                    struct sockaddr_in peer;
                    socklen_t peer_len = sizeof(peer);
                    auto newFd = ::accept(inst->handle(), (struct sockaddr *)&peer, &peer_len);
                    if(newFd > 0) {
                        std::string IP(inet_ntoa(peer.sin_addr));
                        inst->web_connections().insert(std::make_pair(newFd, std::make_tuple(newFd, IP, ntohs(peer.sin_port), Tcp_Web_Client_Connected_Service, "", 0, 0, 0)));
                        std::cout << "line: " << __LINE__ << " web channel: " << newFd << " IP: " << IP <<" port:" << ntohs(peer.sin_port) << std::endl;
                    }
                }
                
                if(!inst->tcp_connections().empty()) {
                    for(auto &[key, value]: inst->tcp_connections()) {
                        auto channel = std::get<0>(value);
                        auto svc_type = std::get<3>(value);

                        if(channel > 0 && FD_ISSET(channel, &readFd)) {
                            //From TCP Client
                            std::string request("");
                            std::cout << "line: "<< __LINE__ << " Response received from TCP client: " << std::endl;
                            auto req = inst->tcp_rx(channel, request, svc_type);
                            if(!req) {
                                //client is closed now
                                std::cout << "line: " << __LINE__ << " req.length: " << request.length() << " service_type: " << svc_type << std::endl; 
                                ::close(channel);
                                std::get<0>(value) = -1;
                                FD_CLR(channel, &readFd);
                                //inst->tcp_connections().erase(channel);
                            } else {
                                std::cout << "line: " << __LINE__ << " Data TCP Server Received: " << request << std::endl;
                                if(noor::ServiceType::Tcp_Device_Client_Connected_Service == svc_type) {
                                   noor::CommonResponse::instance().response(channel, request);

                                } else if(noor::ServiceType::Tcp_Device_Console_Connected_Service == svc_type) {
                                    // This response to be sent to web client
                                    auto it = std::find_if(services.begin(), services.end(), [&](auto& ent) -> bool {
                                        return(std::get<1>(ent) == Tcp_Web_Server_Service );
                                    });

                                    if(it != services.end()) {
                                        auto& webInst = *std::get<0>(*it);
                                        auto iter = std::find_if(webInst.web_connections().begin(), webInst.web_connections().end(), [&](const auto& ent) -> bool {
                                            return(std::get<4>(ent.second) == std::get<1>(value));
                                        });

                                        if(iter != webInst.web_connections().end()) {
                                            std::cout << "line: " << __LINE__ << " sending to web-client" << std::endl;
                                            //send to web-client
                                            auto rlen = web_tx(iter->first, request);
                                            std::cout << "line: " << __LINE__ << " rlen: " << rlen << std::endl;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                
                std::erase_if(inst->tcp_connections(), [](auto& ent) {
                    return(std::get<0>(ent.second) == -1);
                });

                if(!inst->web_connections().empty()) {
                    for(auto &[key, value]: inst->web_connections()) {
                        auto channel = std::get<0>(value);
                        if(channel > 0 && FD_ISSET(channel, &readFd)) {
                            //From Web Client 
                            std::string request("");
                            auto req = inst->web_rx(channel, request);
                            if(!req) {
                                std::cout << "line: " << __LINE__ << " req.length: " << request.length() << " channel: " << channel << " closing now "<<std::endl; 
                                //client is closed now 
                                ::close(channel);
                                std::get<0>(value) = -1;
                                FD_CLR(channel, &readFd);
                                //auto it = inst->web_connections().erase(channel);
                            } else {
                                std::cout << "line: " << __LINE__ << " Request from Web client channel: " << channel <<" Received: " << request << std::endl;
                                Http http(request);
                                //auto rsp = build_web_response(http);
                                auto rsp = process_web_request(request);
                                if(rsp.length()) {

                                    if(http.value("command").length()) {
                                        // This is the Console command to be Executed pass o this over TCP to Device for a given IP.
                                        std::string IP  = http.value("ipAddress");
                                        std::int32_t tcp_channel = -1;

                                        auto iter = std::find_if(services.begin(), services.end(), [&](auto& ent) {
                                            return(std::get<1>(ent) == Tcp_Device_Console_Server_Service);
                                        });
                                        
                                        if(iter != services.end()) {
                                             auto& tcpInst = *std::get<0>(*iter);
                                             auto it = std::find_if(tcpInst.tcp_connections().begin(), tcpInst.tcp_connections().end(), [&](auto& elm) -> bool {
                                                if(std::get<3>(elm.second) == Tcp_Device_Console_Connected_Service && !(std::get<1>(elm.second).compare(IP))) {
                                                    // learn the IP of web-client
                                                    std::get<4>(elm.second) = std::get<1>(value);
                                                    tcp_channel = std::get<0>(elm.second);
                                                    std::cout << "line: " << __LINE__ << " tcp_channel: " << tcp_channel << " IP: " << IP << " std::get<1>(value) " << std::get<1>(value) << std::endl;
                                                    return(true);
                                                }
                                                return(false);
                                             });

                                             if(it != tcpInst.tcp_connections().end()) {
                                                 std::string command = http.value("command");
                                                 std::int32_t ret = tcp_tx(tcp_channel, command);
                                                 std::cout << "line: " << __LINE__ << " the command: " << command << " sent to shell handler " << std::endl;
                                             }
                                            
                                        }
                                    }

                                    auto ret = web_tx(channel, rsp);
                                }
                            }
                        }
                    }
                }
                // get rid of all entries whose fd is -1 
                std::erase_if(inst->web_connections(), [](auto& ent) {
                    return(std::get<0>(ent.second) == -1);
                });
            }

        } /*conns > 0*/
    } /* End of while loop */
}

/**
 * @brief 
 * 
 * @param cmd_type 
 * @param cmd 
 * @param req 
 * @return std::string 
 */
std::string noor::Service::serialise(noor::EMP_COMMAND_TYPE cmd_type, noor::EMP_COMMAND_ID cmd, const std::string& req) {
    cmd = (noor::EMP_COMMAND_ID)(((cmd_type & 0x3 ) << 12) | (cmd & 0xFFF));

    std::uint32_t payload_len = req.length();
    std::cout << "Payload length: " << payload_len << " REQUEST: " << req << std::endl;
    cmd = (noor::EMP_COMMAND_ID)htons(cmd);
    ++m_message_id;
    auto message_id = htons(m_message_id);
    payload_len = htonl(payload_len);
    std::stringstream data("");
    
    data.write (reinterpret_cast <char *>(&cmd), sizeof(cmd));
    data.write (reinterpret_cast <char *>(&message_id), sizeof(message_id));
    data.write (reinterpret_cast <char *>(&payload_len), sizeof(payload_len));
    data << req;
    return(data.str());
}

/**
 * @brief 
 * 
 * @param prefix 
 * @param fields 
 * @param filter 
 * @return std::string 
 */
std::string noor::Service::packArguments(const std::string& prefix, std::vector<std::string> fields, std::vector<std::string> filter) {
    std::stringstream rsp("");
    std::string result("");

    if(prefix.empty()) {
        //This can't be empty
        return(std::string());
    } else {
	if(true == is_register_variable()) {
	    // First argument will be callback , hence blank
            rsp << "[\"\", \"" <<  prefix << "\"";
	} else {
            rsp << "[\"" <<  prefix << "\"";
	}
        result += rsp.str();
        rsp.str("");
    }
    if(!fields.empty()) {
        if(1 == fields.size()) {
            rsp << ",[\"" << fields.at(0) << "\"]";
            result += rsp.str();
	    rsp.str("");
        } else {
            rsp << ",[";
            for(const auto& elm: fields) {
                rsp << "\"" << elm << "\",";
            }
            result += rsp.str().substr(0, rsp.str().length() - 1);
            result += "]";
            rsp.str("");
        }
    }
    //filters ... field_name__eq
    if(!filter.empty()) {
        if(1 == filter.size()) {
            rsp << ",{\"" << filter.at(0) << "\"}";
            result += rsp.str();
            rsp.str("");
        } else {
            rsp << ",{";
            for(const auto& elm: filter) {
                rsp << "\"" << elm << "\",";
            }
            result += rsp.str().substr(0, rsp.str().length() - 1);
            result += "}";
            rsp.str("");
        }
    }
    result +="]";
    return(result);
}

/**
 * @brief 
 * 
 * @param prefix 
 * @param fields 
 * @param filter 
 * @return std::int32_t 
 */
std::int32_t noor::Service::registerGetVariable(const std::string& prefix, std::vector<std::string> fields, std::vector<std::string> filter) {
    noor::EMP_COMMAND_TYPE cmd_type = noor::EMP_COMMAND_TYPE::Request;
    noor::EMP_COMMAND_ID cmd = noor::EMP_COMMAND_ID::RegisterGetVariable;
    is_register_variable(true); 
    std::string rsp = packArguments(prefix, fields, filter);
    std::string data = serialise(cmd_type, cmd, rsp);
    std::int32_t ret = uds_tx(data);
    std::string response("");
    add_element_to_cache({cmd_type, cmd, message_id(), prefix, response}); 
    is_register_variable(false);
    return(ret);

}

/**
 * @brief 
 * 
 * @param prefix 
 * @return std::int32_t 
 */
std::int32_t noor::Service::getSingleVariable(const std::string& prefix) {
    noor::EMP_COMMAND_TYPE cmd_type = noor::EMP_COMMAND_TYPE::Request;
    noor::EMP_COMMAND_ID cmd = noor::EMP_COMMAND_ID::SingleGetVariable;
    
    std::string rsp = packArguments(prefix);
    std::string data = serialise(cmd_type, cmd, rsp);
    std::int32_t ret = uds_tx(data); 
    std::string response("");
    add_element_to_cache({cmd_type, cmd, message_id(), prefix, response}); 
    
    return(ret);
}

/**
 * @brief 
 * 
 * @param prefix 
 * @param fields 
 * @param filter 
 * @return std::int32_t 
 */
std::int32_t noor::Service::getVariable(const std::string& prefix, std::vector<std::string> fields, std::vector<std::string> filter) {
    noor::EMP_COMMAND_TYPE cmd_type = noor::EMP_COMMAND_TYPE::Request;
    noor::EMP_COMMAND_ID cmd = noor::EMP_COMMAND_ID::GetVariable;

    std::string rsp = packArguments(prefix, fields, filter);
    std::string data = serialise(cmd_type, cmd, rsp);
    std::int32_t ret = uds_tx(data);
    std::string response("");
    add_element_to_cache({cmd_type, cmd, message_id(), prefix, response});
    return(ret);
}

/**
 * @brief 
 * 
 * @param in 
 * @return std::int32_t 
 */
std::string TcpClient::onReceive(std::string in) {
    std::cout << "line: " << __LINE__ << " " << __PRETTY_FUNCTION__ << std::endl;
    return(std::string());
}

std::int32_t TcpClient::onClose(std::string in) {
    std::cout << "line: " << __LINE__ << " " << __PRETTY_FUNCTION__ << std::endl;
    return(0);
}

std::string UdpClient::onReceive(std::string in) {
    std::cout << "line: " << __LINE__ << " " << __PRETTY_FUNCTION__ << std::endl;
    return(std::string());
}

std::int32_t UdpClient::onClose(std::string in) {
    std::cout << "line: " << __LINE__ << " " << __PRETTY_FUNCTION__ << std::endl;
    return(0);
}

std::string UnixClient::onReceive(std::string in) {
    std::cout << "line: " << __LINE__ << " " << __PRETTY_FUNCTION__ << std::endl;
    return(std::string());
}

std::int32_t UnixClient::onClose(std::string in) {
    std::cout << "line: " << __LINE__ << " " << __PRETTY_FUNCTION__ << std::endl;
    return(0);
}

std::string TcpServer::onReceive(std::string in) {
    std::cout << "line: " << __LINE__ << " " << __PRETTY_FUNCTION__ << std::endl;
    return(std::string());
}

std::int32_t TcpServer::onClose(std::string in) {
    std::cout << "line: " << __LINE__ << " " << __PRETTY_FUNCTION__ << std::endl;
    return(0);
}

std::string UdpServer::onReceive(std::string in) {
    std::cout << "line: " << __LINE__ << " " << __PRETTY_FUNCTION__ << std::endl;
    return(std::string());
}

std::int32_t UdpServer::onClose(std::string in) {
    std::cout << "line: " << __LINE__ << " " << __PRETTY_FUNCTION__ << std::endl;
    return(0);
}

std::string WebServer::onReceive(std::string in) {
    std::cout << "line: " << __LINE__ << " " << __PRETTY_FUNCTION__ << std::endl;
    return(std::string());
}

std::int32_t WebServer::onClose(std::string in) {
    std::cout << "line: " << __LINE__ << " " << __PRETTY_FUNCTION__ << std::endl;
    return(0);
}

std::string UnixServer::onReceive(std::string in) {
    std::cout << "line: " << __LINE__ << " " << __PRETTY_FUNCTION__ << std::endl;
    return(std::string());
}

std::int32_t UnixServer::onClose(std::string in) {
    std::cout << "line: " << __LINE__ << " " << __PRETTY_FUNCTION__ << std::endl;
    return(0);
}



std::uint32_t from_json_element_to_string(const std::string json_obj, const std::string key, std::string& str_out)
{

#if 0
  bsoncxx::document::value doc_val = bsoncxx::from_json(json_obj.c_str());
  bsoncxx::document::view doc = doc_val.view();

  auto it = doc.find(key);
  if(it == doc.end()) {
    std::cout << "line: " << __LINE__ << "key: " << key;
    str_out.clear();
    return(1);    
  }

  bsoncxx::document::element elm_value = *it;
  if(elm_value && bsoncxx::type::k_utf8 == elm_value.type()) {
      std::string elm(elm_value.get_utf8().value.data(), elm_value.get_utf8().value.length());
      str_out.assign(elm);
  } else {
    str_out.clear();
  }
#endif  
  return(0);
}

std::uint32_t from_json_array_to_map(const std::string json_obj, std::unordered_map<std::string, std::string>& out)
{
#if 0    
    bsoncxx::document::value doc_val = bsoncxx::from_json(json_obj.c_str());
    bsoncxx::document::view doc = doc_val.view();
    auto key="element";
    
    auto it = doc.find(key);
    if(it == doc.end()) {
        std::cout << "line: " << __LINE__ << " key: " << key << std::endl;
        return(1);    
    }

    bsoncxx::document::element elm_value = *it;

    if(elm_value && bsoncxx::type::k_array == elm_value.type()) {
        bsoncxx::array::view to(elm_value.get_array().value);
        std::cout << "line: " << __LINE__ << " document type is array" << std::endl;
        for(bsoncxx::array::element elm : to) {
          if(bsoncxx::type::k_document == elm.type()) {
            std::cout << "line: " << __LINE__ << " document type is document" << std::endl;
            auto doc = elm.get_document().value;
            auto it = doc.find("key");
            if(it != doc.end()) {
                std::cout << "line: " << __LINE__ << " value: " << doc["key"].get_utf8().value.data() << std::endl;
            }
          }
        }
    }
#endif  
    return(0);
}


#endif /* __uniimage__cc__ */
