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
            std::cout << "line: " << __LINE__ << " epoll_create1 is failed" << std::endl;
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

            case noor::ServiceType::Tcp_DeviceMgmtServer_Client_Gateway_Service_Sync:
            case noor::ServiceType::Tcp_DeviceMgmtServer_Client_Gateway_Service_Async:
            case noor::ServiceType::Tcp_DeviceMgmtServer_Client_Console_Service_Async:
            case noor::ServiceType::Tcp_DeviceMgmtServer_Client_Console_Service_Sync:
            case noor::ServiceType::Tls_Tcp_Rest_Client_For_Gateway_Service_Sync:
            case noor::ServiceType::Tls_Tcp_Rest_Client_For_Gateway_Service_Async:
            case noor::ServiceType::Tls_Tcp_DeviceMgmtServer_Client_Gateway_Service_Async:
            {
                std::int32_t channel = -1;
                auto inst = std::make_unique<TcpClient>(IP, PORT, channel, isAsync);
                m_services.insert(std::make_pair(serviceType, std::move(inst)));
                RegisterToEPoll(serviceType, channel);
            }
            break;

            case noor::ServiceType::Tcp_DeviceMgmtServer_Server_Gateway_Service:
            case noor::ServiceType::Tcp_DeviceMgmtServer_Server_Console_Service:
            case noor::ServiceType::Tcp_DeviceMgmtServer_Web_Server_Service:
            case noor::ServiceType::Tls_Tcp_DeviceMgmtServer_Server_Gateway_Service:
            {
                std::int32_t channel = -1;
                auto inst = std::make_unique<TcpServer>(IP, PORT, channel);
                m_services.insert(std::make_pair(serviceType, std::move(inst)));
                RegisterToEPoll(serviceType, channel);
            }
            break;
            
            default:
            {
                //Default case 
                std::cout << "line: " << __LINE__ << " unknown serviceType: " << serviceType << std::endl;
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

    std::vector<struct epoll_event> activeEvt(m_services.size());
    std::int32_t nReady = -1;

    while(true) {
        
        activeEvt.resize(m_evts.size());
        nReady = ::epoll_wait(m_epollFd, activeEvt.data(), activeEvt.size(), toInMilliSeconds);

        //Upon timeout nReady is ZERO and -1 Upon Failure.
        if(nReady > 0) {
            activeEvt.resize(nReady);
            std::cout << __TIMESTAMP__ << " line: " << __LINE__ << " nReady: " << nReady << " m_services.size(): " << m_services.size() << std::endl;

        } else if(nReady < 0) {
            //Error is returned by epoll_wait
            if(EBADF == errno) {
                std::cout << "line: " << __LINE__ << " epfd is not a valid file descriptor" << std::endl;
            } else if(EFAULT == errno) {
                std::cout << "line: " << __LINE__ << " The memory area pointed to by events is not accessible with write permissions" << std::endl;
            } else if(EINTR == errno) {
                std::cout << "line: " << __LINE__ << " The call was interrupted by a signal handler before" << std::endl;
            } else if(EINVAL == errno) {
                std::cout << "line: " << __LINE__ << " epfd is not an epoll file descriptor, or maxevents is less than or equal to zero" << std::endl;
            }
            std::cout << "line: " << __LINE__ << "nReady: " << nReady << std::endl;
            continue;
        } else if(!nReady) {
            //Timeout happens -- for client role only.
            if(get_config()["role"].compare("server")) {
                if(!get_config()["protocol"].compare(0, 3, "tcp")) {
                    auto svc = GetService(noor::ServiceType::Tcp_DeviceMgmtServer_Client_Gateway_Service_Async);
                    if(svc != nullptr) {
                        auto channel = svc->handle();
                        if(noor::client_connection::Disconnected == svc->connected_client(channel)) {
                            auto IP = svc->ip();
                            auto PORT = svc->port();
                            DeRegisterFromEPoll(channel);
                            DeleteService(noor::ServiceType::Tcp_DeviceMgmtServer_Client_Gateway_Service_Async, channel);
                            CreateServiceAndRegisterToEPoll(noor::ServiceType::Tcp_DeviceMgmtServer_Client_Gateway_Service_Async, IP, PORT, true);
                        } else if(noor::client_connection::Disconnected == svc->connected_client(channel)) {
                            //Get the events from Rest Server.
                            if(!getResponseCache().empty()) {
                                auto len = svc->tcp_tx(channel, getResponseCache().begin()->second);
                                if(len > 0) {
                                    std::cout << "line: " << __LINE__ << " sent to Server over TCP len: " << len << std::endl;
                                    std::cout << "line: " << __LINE__ << " sent to Server over TCP : " << getResponseCache().begin()->second << std::endl;
                                }
                            }
                        }
                    }

                    svc = GetService(noor::ServiceType::Tcp_DeviceMgmtServer_Client_Console_Service_Async);
                    if(svc != nullptr) {
                        auto channel = svc->handle();
                        if(noor::client_connection::Disconnected == svc->connected_client(channel)) {
                            auto IP = svc->ip();
                            auto PORT = svc->port();
                            DeRegisterFromEPoll(channel);
                            DeleteService(noor::ServiceType::Tcp_DeviceMgmtServer_Client_Console_Service_Async, channel);
                            CreateServiceAndRegisterToEPoll(noor::ServiceType::Tcp_DeviceMgmtServer_Client_Console_Service_Async, IP, PORT, true);
                        }
                    }
                } else if(!get_config()["protocol"].compare(0, 3, "tls")) {
                    auto svc = GetService(noor::ServiceType::Tls_Tcp_DeviceMgmtServer_Client_Gateway_Service_Async);
                    if(svc != nullptr) {
                        auto channel = svc->handle();
                        if(noor::client_connection::Disconnected == svc->connected_client(channel)) {
                            auto IP = svc->ip();
                            auto PORT = svc->port();
                            DeRegisterFromEPoll(channel);
                            DeleteService(noor::ServiceType::Tls_Tcp_DeviceMgmtServer_Client_Gateway_Service_Async, channel);
                            CreateServiceAndRegisterToEPoll(noor::ServiceType::Tls_Tcp_DeviceMgmtServer_Client_Gateway_Service_Async, IP, PORT, true);
                        } else if(noor::client_connection::Disconnected == svc->connected_client(channel)) {
                            //Get the events from Rest Server.
                            if(!getResponseCache().empty()) {
                                std::int32_t req_len = getResponseCache().begin()->second.length();
                                auto payload_len = htonl(req_len);
                                std::stringstream data("");
                                data.write (reinterpret_cast <char *>(&payload_len), sizeof(std::int32_t));
                                data << getResponseCache().begin()->second;
                                auto len = svc->tls().write(data.str());
                                if(len > 0) {
                                    std::cout << "line: " << __LINE__ << " sent to Server over TLS len: " << len << std::endl;
                                    std::cout << "line: " << __LINE__ << " sent to Server over TLS : " << getResponseCache().begin()->second << std::endl;
                                }
                            }
                        }
                    }
                } else if(!get_config()["protocol"].compare(0, 4, "dtls")) {

                }
            }
            continue;
        }

        for(auto it = activeEvt.begin(); it != activeEvt.end(); ++it) {
            auto ent = *it;
            std::uint32_t Fd = std::uint32_t((ent.data.u64 >> 32) & 0xFFFFFFFF);
            noor::ServiceType serviceType = noor::ServiceType(ent.data.u64 & 0xFFFFFFFF);

            if(ent.events & EPOLLOUT) {
                //Descriptor is ready for Write
                std::cout << __TIMESTAMP__ << " line: " << __LINE__ << " EPOLLOUT is set for serviceType: " << serviceType << " channel: " << Fd << std::endl;
                std::cout << __TIMESTAMP__ << " line: " << __LINE__ << " events: " << ent.events << " serviceType: " << serviceType << std::endl;
                switch(serviceType) {
                    case noor::ServiceType::Tcp_DeviceMgmtServer_Client_Gateway_Service_Async: //Client of DMS over TCP
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
                                    std::cout << __TIMESTAMP__ << " line: " << __LINE__ << " re-attempting the connection " << std::endl;
                                    auto inst = GetService(serviceType);
                                    if(inst == nullptr) break;
                                    auto IP = inst->ip();
                                    auto PORT = inst->port();
                                    DeRegisterFromEPoll(Fd);
                                    DeleteService(serviceType, Fd);
                                    CreateServiceAndRegisterToEPoll(serviceType, IP, PORT, true);
                                    break;
                                }
                            }
                            std::cout << __TIMESTAMP__ << " line: " << __LINE__ << " client is connected successfully for serviceType: " << serviceType << " channel: " << Fd << std::endl;
                            //There's no error on the socket
                            //struct epoll_event evt;
                            ent.events = EPOLLIN | EPOLLRDHUP;
                            auto svc = GetService(serviceType);
                            if(svc == nullptr) break;

                            svc->connected_client(noor::client_connection::Connected);
                            auto ret = ::epoll_ctl(m_epollFd, EPOLL_CTL_MOD, Fd, &ent);
                            (void)ret;
                            
                            if(!getResponseCache().empty()) {
                                auto len = svc->tcp_tx(Fd, getResponseCache().begin()->second);
                                if(len > 0) {
                                    std::cout << "line: " << __LINE__ << " sent to Server over TCP len: " << len << std::endl;
                                    std::cout << "line: " << __LINE__ << " sent to Server over TCP : " << getResponseCache().begin()->second << std::endl;
                                }
                                break;
                            }
                        } while(0);
                    }
                    break;

                    case noor::ServiceType::Tls_Tcp_DeviceMgmtServer_Client_Gateway_Service_Async: //Client of DMS over TLS
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
                                    std::cout << __TIMESTAMP__ << " line: " << __LINE__ << " re-attempting the connection " << std::endl;
                                    auto inst = GetService(serviceType);
                                    if(inst == nullptr) break;
                                    auto IP = inst->ip();
                                    auto PORT = inst->port();
                                    DeRegisterFromEPoll(Fd);
                                    DeleteService(serviceType, Fd);
                                    CreateServiceAndRegisterToEPoll(serviceType, IP, PORT, true);
                                    break;
                                }
                            }
                            std::cout << "line: " << __LINE__ << " Tls Tcp client is connected to DMS successfully " << std::endl;
                            //There's no error on the socket
                            //struct epoll_event evt;
                            ent.events = EPOLLIN |EPOLLRDHUP;
                            auto ret = ::epoll_ctl(m_epollFd, EPOLL_CTL_MOD, Fd, &ent);
                            (void)ret;
                            auto svc = GetService(serviceType);
                            if(svc == nullptr) break;
                            svc->connected_client(noor::client_connection::Connected);
                            //do a TLS Hand shake
                            svc->tls().init(Fd);
                            svc->tls().client();

                            if(!getResponseCache().empty()) {
                                std::int32_t req_len = getResponseCache().begin()->second.length();
                                auto payload_len = htonl(req_len);
                                std::stringstream data("");
                                data.write (reinterpret_cast <char *>(&payload_len), sizeof(std::int32_t));
                                data << getResponseCache().begin()->second;
                                auto len = svc->tls().write(data.str());

                                if(len > 0) {
                                    std::cout << "line: " << __LINE__ << " sent to Device Mgmt Server over TLS len: " << len << std::endl;
                                    break;
                                }
                            }
                        } while(0);
                    }
                    break;

                    case noor::ServiceType::Tcp_DeviceMgmtServer_Client_Console_Service_Async:
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
                                    std::cout << __TIMESTAMP__ << " line: " << __LINE__ << " Server is not UP yet, re-attempting... " << std::endl;
                                    auto inst = GetService(serviceType);
                                    if(inst == nullptr) break;
                                    auto IP = inst->ip();
                                    auto PORT = inst->port();
                                    DeRegisterFromEPoll(Fd);
                                    DeleteService(serviceType, Fd);
                                    CreateServiceAndRegisterToEPoll(serviceType, IP, PORT, true);
                                    break;
                                }
                            }

                            std::cout << __TIMESTAMP__ << " line: " << __LINE__ << " client is connected successfully for serviceType: " << serviceType << " channel: " << Fd << std::endl;
                            //There's no error on the socket
                            //struct epoll_event evt;
                            ent.events = EPOLLIN | EPOLLRDHUP;
                            auto ret = ::epoll_ctl(m_epollFd, EPOLL_CTL_MOD, Fd, &ent);
                            (void)ret;
                        } while(0);
                    }
                    break;
                    case noor::ServiceType::Tls_Tcp_Rest_Client_For_Gateway_Service_Async: //Client of Rest Server at Device.
                    case noor::ServiceType::Tls_Tcp_Rest_Client_For_Gateway_Service_Sync:
                    {
                        //tcp connection is established - do tls handshake
                        auto svc = GetService(serviceType);
                        if(svc == nullptr) break;

                        svc->tls().init(Fd);
                        svc->tls().client();

                        ent.events = EPOLLIN |EPOLLRDHUP;
                        auto ret = ::epoll_ctl(m_epollFd, EPOLL_CTL_MOD, Fd, &ent);
                        (void)ret;

                        //Get Token for Rest Client
                        json jobj = json::object();
                        jobj["login"] = get_config()["userid"];
                        jobj["password"] = get_config()["password"];

                        auto req = svc->restC().getToken(jobj.dump());
                        std::cout << "line: " << __LINE__ << " request sent: " << std::endl << req << std::endl;
                        auto len = svc->tls().write(req);
                        (void)len;
                    }
                    break;
                    default:
                    {
                        std::cout << "line: " << __LINE__ << " default case for EPOLLOUT" << std::endl;
                    }
                    break;
                }
            } else if(ent.events & EPOLLIN) {
                std::cout << __TIMESTAMP__ << " line: " << __LINE__ << " events: " << ent.events << " serviceType: " << serviceType << std::endl;
                //file descriptor is ready for read.
                switch(serviceType) {
                    case noor::ServiceType::Tcp_DeviceMgmtServer_Web_Server_Service:
                    {
                        //New Web Connection
                        std::int32_t newFd = -1;
                        struct sockaddr_in addr;
                        socklen_t addr_len = sizeof(addr);
                        newFd = ::accept(Fd, (struct sockaddr *)&addr, &addr_len);

                        if(newFd > 0) {
                            std::uint16_t PORT = ntohs(addr.sin_port);
                            std::string IP(inet_ntoa(addr.sin_addr));
                            //making socket non-blocking
                            auto flags = ::fcntl(newFd, F_GETFL);
                            if(::fcntl(newFd, F_SETFL, flags | O_NONBLOCK) < 0) {
                                std::cout << __TIMESTAMP__ << ": line: " << __LINE__ << " making socker non-blocking for fd: " << newFd << " failed" << std::endl;
                            }

                            std::cout<< "line: " << __LINE__ << " new client from WEB IP: " << IP <<" PORT: " << PORT << " FD: " << newFd << std::endl;
                            m_services.insert(std::make_pair(noor::ServiceType::Tcp_DeviceMgmtServer_Web_Client_Connected_Service , std::make_unique<TcpClient>(newFd, IP, PORT)));
                            RegisterToEPoll(noor::ServiceType::Tcp_DeviceMgmtServer_Web_Client_Connected_Service, newFd);
                        }
                    }
                    break;
                    case noor::ServiceType::Tcp_DeviceMgmtServer_Server_Gateway_Service:
                    {
                        std::int32_t newFd = -1;
                        struct sockaddr_in addr;
                        socklen_t addr_len = sizeof(addr);
                        newFd = ::accept(Fd, (struct sockaddr *)&addr, &addr_len);
                        
                        // new connection is accepted successfully.

                        if(newFd > 0) {
                            std::uint16_t PORT = ntohs(addr.sin_port);
                            std::string IP(inet_ntoa(addr.sin_addr));
                            std::cout<< "line: " << __LINE__ << " new client from device IP: " << IP <<" PORT: " << PORT << " FD: " << newFd << std::endl;
                            //making socket non-blocking
                            auto flags = ::fcntl(newFd, F_GETFL);
                            if(::fcntl(newFd, F_SETFL, flags | O_NONBLOCK) < 0) {
                                std::cout << __TIMESTAMP__ << ": line: " << __LINE__ << " making socker non-blocking for fd: " << newFd << " failed" << std::endl;
                            }

                            m_services.insert(std::make_pair(noor::ServiceType::Tcp_DeviceMgmtServer_Client_Gateway_Connected_Service , std::make_unique<TcpClient>(newFd, IP, PORT)));
                            RegisterToEPoll(noor::ServiceType::Tcp_DeviceMgmtServer_Client_Gateway_Connected_Service, newFd);
                        }
                    }
                    break;
                    case noor::ServiceType::Tls_Tcp_DeviceMgmtServer_Server_Gateway_Service:
                    {
                        std::int32_t newFd = -1;
                        struct sockaddr_in addr;
                        socklen_t addr_len = sizeof(addr);
                        newFd = ::accept(Fd, (struct sockaddr *)&addr, &addr_len);
                        std::cout << "line: " << __LINE__ << " value of newFd: " << newFd << std::endl;
                        // new connection is accepted successfully.

                        if(newFd > 0) {
                            std::uint16_t PORT = ntohs(addr.sin_port);
                            std::string IP(inet_ntoa(addr.sin_addr));
                            std::cout<< "line: " << __LINE__ << " new client for TCP server IP: " << IP <<" PORT: " << PORT << " FD: " << newFd << std::endl;

                            //making socket non-blocking
                            auto flags = ::fcntl(newFd, F_GETFL);
                            if(::fcntl(newFd, F_SETFL, flags | O_NONBLOCK) < 0) {
                                std::cout << __TIMESTAMP__ << ": line: " << __LINE__ << " making socker non-blocking for fd: " << newFd << " failed" << std::endl;
                            }

                            m_services.insert(std::make_pair(noor::ServiceType::Tls_Tcp_DeviceMgmtServer_Client_Gateway_Connected_Service ,
                                              std::make_unique<TcpClient>(newFd, IP, PORT)));
                            auto svc = GetService(noor::ServiceType::Tls_Tcp_DeviceMgmtServer_Client_Gateway_Connected_Service, newFd);
                            if(svc == nullptr) break;

                            std::string cert("../cert/cert.pem"), pkey("../cert/pkey.pem");
                            svc->tls().init(cert, pkey);
                            svc->tls().server(newFd);

                            RegisterToEPoll(noor::ServiceType::Tls_Tcp_DeviceMgmtServer_Client_Gateway_Connected_Service, newFd);
                        }
                    }
                    break;
                    case noor::ServiceType::Tcp_DeviceMgmtServer_Server_Console_Service:
                    {
                        std::int32_t newFd = -1;
                        struct sockaddr_in addr;
                        socklen_t addr_len = sizeof(addr);
                        newFd = ::accept(Fd, (struct sockaddr *)&addr, &addr_len);
                        // new connection is accepted successfully.
                        if(newFd > 0) {
                            std::uint16_t PORT = ntohs(addr.sin_port);
                            std::string IP(inet_ntoa(addr.sin_addr));

                            //making socket non-blocking
                            auto flags = ::fcntl(newFd, F_GETFL);
                            if(::fcntl(newFd, F_SETFL, flags | O_NONBLOCK) < 0) {
                                std::cout << __TIMESTAMP__ << ": line: " << __LINE__ << " making socker non-blocking for fd: " << newFd << " failed" << std::endl;
                            }

                            m_services.insert(std::make_pair(noor::ServiceType::Tcp_Device_MgmtServer_Client_Console_Connected_Service, std::make_unique<TcpClient>(newFd, IP, PORT)));
                            RegisterToEPoll(noor::ServiceType::Tcp_Device_MgmtServer_Client_Console_Connected_Service, newFd);
                        }
                    }
                    break;
                    case noor::ServiceType::Tcp_DeviceMgmtServer_Client_Gateway_Connected_Service:
                    {
                        do {
                            //Data is availabe for read. --- tcp_rx()
                            std::string request("");
                            auto svc = GetService(serviceType, Fd);
                            if(svc == nullptr) break;

                            auto result = svc->tcp_rx(Fd, request);
                            std::cout << "line: " << __LINE__ << " result: " << result << " connected client " << std::endl;

                            if(!result) {
                                //TCP Connection is closed.
                                std::cout << "line: " << __LINE__ << " closing the client connection " << std::endl;
                                DeleteService(serviceType, Fd);
                                DeRegisterFromEPoll(Fd);
                                break;
                            }
                            json jobj = json::parse(request);
                            auto srNumber = jobj["serialNumber"].get<std::string>();
                            //learn the serial number now.
                            svc->serialNumber(srNumber);
                            getResponseCache().insert(std::pair(srNumber, request));
                            std::cout << "line: " << __LINE__ << " serialNumber: " << srNumber << " received from device over TCP : " << request << std::endl;

                        }while(0);
                    }
                    break;
                    case noor::ServiceType::Tls_Tcp_DeviceMgmtServer_Client_Gateway_Connected_Service: //The Tls client on device is connectedto DMS  
                    {
                        do {
                            //Data is availabe for read. --- tcp_rx()
                            std::string request("");
                            auto svc = GetService(serviceType, Fd);
                            if(svc == nullptr) break;
                            std::string out;
                            auto len = svc->tls().read(out, 4);
                            std::cout << __TIMESTAMP__ << " line: " << __LINE__ << " len: " << len << std::endl;
                            if(len > 0 && len == 4) {
                                std::uint32_t payload_len = 0;
                                std::istringstream istrstr;
                                istrstr.rdbuf()->pubsetbuf(out.data(), len);
                                istrstr.read(reinterpret_cast<char *>(&payload_len), sizeof(payload_len));
                                std::int32_t offset = 0;
                                payload_len = ntohl(payload_len);
                                /////////
                                out.clear();
                                len = svc->tls().read(out, payload_len);
                                if(len < 0) {
                                    //Read from TLS server is failed.
                                    break;
                                }
                                std::cout << __TIMESTAMP__ << " line: " << __LINE__ << " received: " << out << std::endl;
                                //Feed it over REST interface to Device. 
                                svc = GetService(noor::ServiceType::Tls_Tcp_Rest_Client_For_Gateway_Service_Async);
                                if(svc == nullptr) {
                                    //Rest Client Connection is closed, Establish it now for this Request.
                                    json jobj = json::object();
                                    jobj["login"] = get_config()["userid"];
                                    jobj["password"] = get_config()["password"];

                                    auto req = svc->restC().getToken(jobj.dump());
                                    std::cout << "line: " << __LINE__ << " request sent: " << std::endl << req << std::endl;
                                    auto len = svc->tls().write(req);
                                    (void)len;
                                    auto future = svc->restC().promise().get_future();
                                    //fallthrough
                                }
                                len = svc->tls().write(out);
                                if(len > 0) {
                                    std::cout << __TIMESTAMP__ << " line: " << __LINE__ << " Request feed to device over REST interface " << std::endl;
                                    break;
                                }
                            } else if(!len) {
                                //Client is closed
                                DeleteService(serviceType, Fd);
                                DeRegisterFromEPoll(Fd);
                            }
                        }while(0);
                    }
                    break;
                    case noor::ServiceType::Tcp_DeviceMgmtServer_Web_Client_Connected_Service:
                    {
                        do {
                            //Data is availabe for read. --- web_rx()
                            std::string request("");
                            auto svc = GetService(serviceType, Fd);
                            if(svc == nullptr) break;

                            auto result = svc->web_rx(Fd, request);
                            if(!result) {
                                //connection is closed
                                std::cout << "line: " << __LINE__ << " closing the client connection for serviceType: " << serviceType << std::endl;
                                DeleteService(serviceType, Fd);
                                DeRegisterFromEPoll(Fd);
                                break;
                            }
                            
                            Http http(request);
                            if(!http.uri().compare(0, 19, "/api/v1/device/list")) {
                                std::string body("");
                                json jarray = json::array();
                                for(const auto& ent: getResponseCache()) {
                                    jarray.push_back(ent.second);
                                }
                                
                                if(!jarray.empty()) {
                                    std::stringstream ss;
                                    json jobj = json::object();
                                    jobj["devices"] = jarray;
                                    body = jobj.dump();
                                    //ss << "{\"devices\": " << jarray.dump() << "}";
                                    
                                    auto resp = svc->buildHttpResponse(http, body);
                                    auto ret = svc->web_tx(Fd, resp);
                                    std::cout << "line: " << __LINE__ << " sent to webui length: " << ret << " response: " << body << std::endl;
                                }
                                break;
                            }

                            if(!http.uri().compare(0, 14, "/api/v1/db/set") ||
                               !http.uri().compare(0, 14, "/api/v1/db/get") ||
                               !http.uri().compare(0, 15, "/api/v1/db/exec")) {
                                std::string srNo;
                                auto svc = GetService(noor::ServiceType::Tcp_DeviceMgmtServer_Client_Gateway_Connected_Service, srNo);
                                if(svc == nullptr) break;

                                svc->restC().uri(http.uri());
                                std::string request;
                                auto ret = svc->tcp_tx(svc->handle(), request);
                                std::cout << "line: " << __LINE__ << " sending to device on channel: " << svc->handle() << " ret: " << ret << std::endl;
                                break;
                            }

                            auto rsp = svc->process_web_request(request);
                            auto ret = svc->web_tx(Fd, rsp);
                            std::cout << "line: " << __LINE__ << " sent to webui length: " << ret << std::endl;
                        }while(0);
                    }
                    break;
                    case noor::ServiceType::Tcp_Device_MgmtServer_Client_Console_Connected_Service:
                    {
                        //Data is availabe for read. --- tcp_rx()
                        std::string request("");
                        auto svc = GetService(serviceType, Fd);
                        if(svc == nullptr) break;

                        auto result = svc->tcp_rx(Fd, request);
                        if(!result) {
                            std::cout << "line: " << __LINE__ << " closing connection for serviceType: " << serviceType << std::endl;
                            DeleteService(serviceType, Fd);
                            DeRegisterFromEPoll(Fd);
                        }
                    }
                    break;
                    case noor::ServiceType::Tcp_DeviceMgmtServer_Client_Gateway_Service_Sync: //when role=client
                    {
                        do {
                            //Data is availabe for read. --- tcp_rx()
                            std::string request("");
                            auto svc = GetService(serviceType);
                            if(svc == nullptr) break;

                            auto result = svc->tcp_rx(Fd, request);
                            if(!result) {
                                std::cout << "line: " << __LINE__ << " closing the connection for Service: " << serviceType << std::endl;
                                auto IP = svc->ip();
                                auto PORT = svc->port();
                                DeleteService(serviceType);
                                DeRegisterFromEPoll(Fd);
                                CreateServiceAndRegisterToEPoll(serviceType, IP, PORT,true);
                                break;
                            }
                        }while(0);
                    }
                    break;
                    case noor::ServiceType::Tcp_DeviceMgmtServer_Client_Gateway_Service_Async:
                    {
                        do {
                            //Data is availabe for read. --- tcp_rx()
                            std::string request("");
                            auto svc = GetService(serviceType);
                            if(svc == nullptr) break;

                            auto result = svc->tcp_rx(Fd, request);

                            if(!result) {
                                std::cout << "line: " << __LINE__ << " closing the connection for Service: " << serviceType << std::endl;
                                auto IP = svc->ip();
                                auto PORT = svc->port();
                                DeleteService(serviceType);
                                DeRegisterFromEPoll(Fd);
                                CreateServiceAndRegisterToEPoll(serviceType, IP, PORT,true);
                                break;
                            }
                            std::cout << "line: " << __LINE__ << " serviceType: " << serviceType << " received from DMS: " << request << std::endl;
                            {
                                //Pass on over TLS to Device
                                auto svc = GetService(noor::ServiceType::Tls_Tcp_Rest_Client_For_Gateway_Service_Sync);
                                if(svc == nullptr) break;

                                svc->tls().write(request);
                            }
                            
                            break;
                        }while(0);
                    }
                    break;

                    case noor::ServiceType::Tls_Tcp_Rest_Client_For_Gateway_Service_Sync:
                    {
                        auto svc = GetService(serviceType);
                        if(svc == nullptr) break;

                        std::string out;
                        std::int32_t header_len = -1;
                        std::int32_t payload_len = -1;
                        bool is_conn_closed = false;
                        std::string status;
                        do {
                            auto ret = svc->tls().peek(out);
                            if(ret > 0) {
                                Http http(out);
                                status.assign(http.status_code());
                                auto ct = http.value("Content-Length");
                                header_len = http.get_header(out).length() + 1;

                                if(ct.length() > 0) {
                                    //Content-Length is present
                                    payload_len = std::stoi(ct);
                                    std::cout << "line: " << __LINE__ << " value of content-length: " << std::stoi(ct) << std::endl;
                                }
                            } else if(!ret) {
                                //TLS connection is closed
                                std::cout << __TIMESTAMP__ << " line: " << __LINE__ << " tls connection is closed" << std::endl;
                                is_conn_closed = true;
                                break;
                            }
                        }while(header_len != out.length());

                        do {
                            if(is_conn_closed) {
                                //Peer is closed now 
                                DeleteService(serviceType);
                                DeRegisterFromEPoll(Fd);
                                break;
                            }

                            //Read HTTP Header first.
                            auto ret = svc->tls().read(out, header_len);
                            if(ret < 0) {
                                break;
                            }
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

                            if(!svc->restC().status_code().compare(0, 3, "401") || !svc->restC().status_code().compare(0, 3, "403")) {
                                //missing authentication or invalid authentication
                                //Get Token for Rest Client
                                json jobj = json::object();
                                jobj["login"] = get_config()["userid"];
                                jobj["password"] = get_config()["password"];

                                auto req = svc->restC().getToken(jobj.dump());
                                std::cout << "line: " << __LINE__ << " request sent: " << std::endl << req << std::endl;
                                auto len = svc->tls().write(req);
                                (void)len;
                                break;
                            }
                            std::string result("");
                            auto req = svc->restC().processResponse(out, body, result);
                            if(result.length()) {
                                //push result to remote server
                                json jobj = json::parse(result);
                                
                                if(jobj["serialNumber"] != nullptr) {
                                    auto srNumber = jobj["serialNumber"].get<std::string>();
                                    //The key is the serial number
                                    getResponseCache().insert(std::pair(srNumber, result));
                                    std::cout << "line: " << __LINE__ << " serialNumber: " << srNumber << std::endl;
                                }
                                {
                                    auto svc = GetService(noor::ServiceType::Tcp_DeviceMgmtServer_Client_Gateway_Service_Async);
                                    if(svc == nullptr) break;

                                    auto channel = svc->handle();
                                    if(channel > 0 && noor::client_connection::Connected == svc->connected_client(channel)) {
                                        auto len = svc->tcp_tx(channel, result);
                                        if(len > 0) {
                                            std::cout << "line: " << __LINE__ << " sent to TCP Server len: " << len << " for serviceType: " << noor::ServiceType::Tcp_DeviceMgmtServer_Client_Gateway_Service_Async << " result: " << result << std::endl; 
                                        }
                                    }
                                    break;
                                }
                            } else if(req.length()) {
                                ret = svc->tls().write(req);
                                if(ret < 0) {
                                    std::cout << "line: " << __LINE__ << " ssl write failed for serviceTYpe: " << serviceType << std::endl;
                                    break;
                                }
                                std::cout << "line: " << __LINE__ << " request sent to : " << std::endl << req << std::endl;
                            }

                        }while(0);
                    }
                    break;
                    case noor::ServiceType::Tcp_DeviceMgmtServer_Client_Console_Service_Async:
                    {
                        do {
                            //Data is availabe for read. --- tcp_rx()
                            std::string request("");
                            auto svc = GetService(serviceType);
                            if(svc == nullptr) break;

                            auto result = svc->tcp_rx(Fd, request);

                            if(!result) {
                                std::cout << "line: " << __LINE__ << " closing the connection for Service: " << serviceType << std::endl;
                                auto IP = svc->ip();
                                auto PORT = svc->port();
                                DeleteService(serviceType);
                                DeRegisterFromEPoll(Fd);
                                CreateServiceAndRegisterToEPoll(serviceType, IP, PORT,true);
                                break;
                            }

                            std::cout << "line: " << __LINE__ << " serviceType: " << serviceType << " received from DMS: " << request << std::endl;

                        }while(0);
                    }
                    break;

                    default:
                    {
                        std::cout << "line: " << __LINE__ << " EPOLLIN default case" << std::endl;
                        break;

                    }
                }
                
            } else if(ent.events & EPOLLRDHUP) {
                std::cout << __TIMESTAMP__ << " line: " << __LINE__ << " events: " << ent.events << std::endl;
                //Connection is closed by other end
                switch(serviceType) {
                    //Connected client of DeviceMgmtServer when DeviceMgmtServer ruuning Server for below services.
                    case noor::ServiceType::Tcp_DeviceMgmtServer_Web_Client_Connected_Service:
                    case noor::ServiceType::Tcp_DeviceMgmtServer_Client_Gateway_Connected_Service:
                    case noor::ServiceType::Tcp_Device_MgmtServer_Client_Console_Connected_Service:
                    {
                        std::cout << "line: " << __LINE__ << " closing connection for service: " << serviceType << " Fd: " << Fd << std::endl;
                        DeleteService(serviceType, Fd);
                        DeRegisterFromEPoll(Fd);
                    }
                    break;
                    //The Client of DeviceMgmtServer is closed/disconnected.
                    case noor::ServiceType::Tcp_DeviceMgmtServer_Client_Gateway_Service_Async:
                    case noor::ServiceType::Tcp_DeviceMgmtServer_Client_Console_Service_Async:
                    case noor::ServiceType::Tls_Tcp_DeviceMgmtServer_Client_Gateway_Service_Async:
                    {
                        //start attempting the connection...
                        auto inst = GetService(serviceType, Fd);
                        if(inst == nullptr) break;
                        
                        auto IP = inst->ip();
                        auto PORT = inst->port();
                        std::cout << "line: " << __LINE__ << " closing connection for service: " << serviceType << " Fd: " << Fd << std::endl;
                        DeleteService(serviceType);
                        DeRegisterFromEPoll(Fd);
                        CreateServiceAndRegisterToEPoll(serviceType, IP, PORT, true);
                    }
                    break;
                    case noor::ServiceType::Tls_Tcp_Rest_Client_For_Gateway_Service_Async:
                    case noor::ServiceType::Tls_Tcp_Rest_Client_For_Gateway_Service_Sync:
                    {
                        std::cout << "line: " << __LINE__ << " connection is closed for service: " << serviceType << std::endl;
                        DeleteService(serviceType);
                        DeRegisterFromEPoll(Fd);
                    }
                    break;

                    default:
                    {
                        //Connection is closed.
                        std::cout << "line: " << __LINE__ << " default case: connection is closed for service: " << serviceType << std::endl;
                        DeleteService(serviceType);
                        DeRegisterFromEPoll(Fd);
                    }
                }
            } else if(ent.events & EPOLLERR) {
                std::cout << __TIMESTAMP__ << " line: " << __LINE__ << " events: " << ent.events << std::endl;
                std::cout << __TIMESTAMP__ << " line: " << __LINE__ << " epollerr events: " << ent.events  << " Fd:" << Fd << " serviceType: " << serviceType << std::endl;
            } else if(ent.events & EPOLLONESHOT) {
                std::cout << __TIMESTAMP__ << " line: " << __LINE__ << " events: " << ent.events << std::endl;
                std::cout << __TIMESTAMP__ << " line: " << __LINE__ << " oneshots events: " << ent.events << " Fd:" << Fd << " serviceType: " << serviceType << std::endl;
            } else {
                std::cout << __TIMESTAMP__ << " line: " << __LINE__ << " events: " << ent.events << std::endl;
                std::cout << __TIMESTAMP__ << " line: " << __LINE__ << " unhandled events: " << ent.events << " Fd:" << Fd << " serviceType: " << serviceType << std::endl;
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

    if(it != m_evts.end()) {
        if(::epoll_ctl(m_epollFd, EPOLL_CTL_DEL, fd, nullptr) == -1)
        {
            std::cout << "line: " << __LINE__ << " Failed to delete Fd from epoll instance for fd: " << fd << std::endl;
        }

        ::close(fd);
        m_evts.erase(it);
        std::cout << "line: " << __LINE__ << " closed the Fd: " << fd << " m_evts.size(): " <<m_evts.size()  <<std::endl;
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
std::int32_t noor::Uniimage::RegisterToEPoll(noor::ServiceType serviceType, std::int32_t channel) {
    
    struct epoll_event evt;

    std::cout << "line: " << __LINE__ << " handle: " << channel << " serviceType: " << serviceType  << " added to epoll" << std::endl;
    std::uint64_t dd = std::uint64_t(channel);
    evt.data.u64 = std::uint64_t(dd) << 32 | std::uint64_t(serviceType);

    if((serviceType == noor::ServiceType::Tcp_DeviceMgmtServer_Client_Gateway_Service_Async) ||
       (serviceType == noor::ServiceType::Tcp_DeviceMgmtServer_Client_Console_Service_Async) ||
       (serviceType == noor::ServiceType::Tls_Tcp_Rest_Client_For_Gateway_Service_Sync) ||
       (serviceType == noor::ServiceType::Tls_Tcp_Rest_Client_For_Gateway_Service_Async) ||
       (serviceType == noor::ServiceType::Tls_Tcp_DeviceMgmtServer_Client_Gateway_Service_Async)) {
        evt.events = EPOLLOUT|EPOLLRDHUP;
        std::cout << "line: " << __LINE__ << " value of events: " << evt.events << " serviceType: " << serviceType << std::endl;

    } else {
        evt.events = EPOLLIN | EPOLLRDHUP;
        std::cout << "line: " << __LINE__ << " value of events: " << evt.events << " serviceType: " << serviceType << std::endl;
    }

    if(::epoll_ctl(m_epollFd, EPOLL_CTL_ADD, channel, &evt) == -1)
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
noor::Service* noor::Uniimage::GetService(noor::ServiceType serviceType) {
    auto it = m_services.find(serviceType);
    //return(it->second);
    
    if(it != m_services.end()) {
        return(it->second.get());
    }
    
    return(nullptr);
}

noor::Service* noor::Uniimage::GetService(noor::ServiceType serviceType, const std::string& serialNumber) {
    auto it = m_services.equal_range(serviceType);
    for(auto &ent = it.first; ent != it.second; ++ent) {
        if(serialNumber.length() && !serialNumber.compare(ent->second->serialNo())) {
            return(ent->second.get());
        }
    }
    return(nullptr);
}

noor::Service* noor::Uniimage::GetService(noor::ServiceType serviceType, const std::int32_t& channel) {
    auto it = m_services.equal_range(serviceType);
    for(auto &ent = it.first; ent != it.second; ++ent) {
        if(channel > 0 && channel == ent->second->handle()) {
            return(ent->second.get());
        }
    }
    return(nullptr);
}

void noor::Uniimage::DeleteService(noor::ServiceType serviceType, const std::int32_t& channel) {
    auto it = m_services.equal_range(serviceType);
    if(it.first != m_services.end()) {
        for(auto &ent = it.first; ent != it.second; ++ent) {
            if(channel > 0 && channel == ent->second->handle()) {
                //Erasing element from multimap now.
                m_services.erase(ent);
                std::cout << "line: " << __LINE__ << " m_services.size(): " << m_services.size() << std::endl;
                break;
            }
        }
    }
}

void noor::Uniimage::DeleteService(noor::ServiceType serviceType) {
    auto it = m_services.find(serviceType);
    //Erasing element from multimap now.
    if(it != m_services.end()) {
        it = m_services.erase(it);
    }
    std::cout << "line: " << __LINE__ << " m_services.size(): " << m_services.size() << std::endl;
}

/******************************************************************************
 _ __ ___  ___| |_ ___| (_) ___ _ __ | |_ 
| '__/ _ \/ __| __/ __| | |/ _ \ '_ \| __|
| | |  __/\__ \ || (__| | |  __/ | | | |_ 
|_|  \___||___/\__\___|_|_|\___|_| |_|\__|
                                          
*********************************************************************************/
std::string noor::RestClient::getToken(const std::string& in) {
    std::string host("192.168.1.1:443");
    std::stringstream ss("");
    m_uri.assign("/api/v1/auth/tokens");

    ss << "POST " << m_uri <<" HTTP/1.1\r\n"
        << "Host: " << host << "\r\n"
        << "Content-Type: application/vnd.api+json\r\n"
        << "Connection: keep-alive\r\n"
        << "Accept: application/vnd.api+json\r\n"
        << "Content-Length: " << in.length() << "\r\n"
        << "\r\n"
        << in;

    return(ss.str());
}

/**
 * @brief 
 * 
 * @param in 
 * @param user 
 * @return std::string 
 */
std::string noor::RestClient::authorizeToken(const std::string& in, const std::string& user) {
    std::string host("192.168.1.1:443");
    std::stringstream ss("");
    m_uri.assign("/api/v1/auth/authorization/");
    m_uri += user;

    ss << "GET " << m_uri <<" HTTP/1.1\r\n"
        << "Host: " << host << "\r\n"
        << "Content-Type: application/vnd.api+json\r\n"
        << "Connection: keep-alive\r\n"
        << "Accept: application/vnd.api+json\r\n"
        << "Authorization: Bearer " << m_cookies << "\r\n"
        << "Content-Length: 0" << "\r\n"
        << "\r\n";

    return(ss.str());
}

/**
 * @brief 
 * 
 * @param dps 
 * @return std::string 
 */
std::string noor::RestClient::registerDatapoints(const std::vector<std::string>& dps) {
    std::string host("192.168.1.1:443");
    std::stringstream ss("");
    uri("/api/v1/register/db?fetch=true");

    json jarray = json::array();
    for(const auto& ent: dps) {
        jarray.push_back(ent);
    }
    auto body = jarray.dump();
    json jobj = json::object();
    jobj["last"] = jarray;
    body = jobj.dump();

    //clear the previous contents now.
    ss.str("");
    ss << "POST " << uri() <<" HTTP/1.1\r\n"
        << "Host: " << host << "\r\n"
        << "Content-Type: application/vnd.api+json\r\n"
        << "Connection: keep-alive\r\n"
        << "Accept: application/vnd.api+json\r\n"
        << "Authorization: Bearer " << cookies() << "\r\n"
        << "Content-Length: " << body.length() << "\r\n"
        << "\r\n"
        << body;

    return(ss.str());
}

std::string noor::RestClient::buildRequest(const std::string& in, std::vector<std::string> param) {
    return(std::string());
}

std::string noor::RestClient::getEvents(std::string uri) {
   std::string host("192.168.1.1:443");
    std::stringstream ss("");
    m_uri.assign("/api/v1/events?timeout=10");

    ss << "GET " << m_uri <<" HTTP/1.1\r\n"
        << "Host: " << host << "\r\n"
        << "Content-Type: application/vnd.api+json\r\n"
        << "Connection: keep-alive\r\n"
        << "Accept: application/vnd.api+json\r\n"
        << "Authorization: Bearer " << m_cookies << "\r\n"
        << "Content-Length: 0" << "\r\n"
        << "\r\n";

    return(ss.str());
 
}
/**
 * @brief 
 * 
 * @param http_header 
 * @param http_body 
 * @param result 
 * @return std::string 
 */
std::string noor::RestClient::processResponse(const std::string& http_header, const std::string& http_body, std::string& result) {
    std::cout << "line: " << __LINE__ << " header: " <<std::endl << http_header << " http_body: " << http_body << std::endl;
    
    if(!uri().compare(0, 19, "/api/v1/auth/tokens")) {
        json json_object = json::parse(http_body);
        m_cookies.assign(json_object["data"]["access_token"]);

        if(pending_request()) {
            //release the future now
            promise().set_value();
            return(std::string());
        }
        return(authorizeToken(http_body, "test"));

    } else if(!uri().compare(0, 26, "/api/v1/auth/authorization")) {
        std::cout << "line: " << __LINE__ << " http_body: " << http_body << std::endl;
        json json_object = json::parse(http_body);
        auto attmpts = json_object["data"]["attempts"].get<std::int32_t>();
        //auto attmpts = value;
        std::cout << "line: " << __LINE__ << " attempts: " << attmpts << std::endl;
        if(deviceName().length() && deviceName().compare("RX55")) {
            return(registerDatapoints(
                {
                    {"device"},
                    {"system.os"},
                    //Wan IP Address
                    {"net.interface.common[].ipv4.address"},
                    {"net.interface.cellular[]"},
                    //WiFi Mode
                    {"net.interface.wifi[w1].radio.mode"},
                    {"net.interface.wifi[w2].radio.mode"},
                    {"net.interface.wifi[w3].radio.mode"},
                    {"system.bootcheck.signature"}
                })
            );
        }
        //device name is unknown
        return(registerDatapoints({{"device"}}));


    } else if(!uri().compare(0, 19, "/api/v1/register/db")) {
        std::unordered_map<std::string, std::string> cache;
        
        if(http_body.length()) {
            //Parse the json response
            json jobj = json::parse(http_body);

            if(!deviceName().length() && jobj["data"]["device.product"] != nullptr) {
                deviceName(jobj["data"]["device.product"].get<std::string>());
                if(deviceName().length() && deviceName().compare("RX55")) {
                    return(registerDatapoints(
                        {
                            {"device"},
                            {"system.os"},
                            //Wan IP Address
                            {"net.interface.common[].ipv4.address"},
                            {"net.interface.cellular[]"},
                            //WiFi Mode
                            {"net.interface.wifi[w1].radio.mode"},
                            {"net.interface.wifi[w2].radio.mode"},
                            {"net.interface.wifi[w3].radio.mode"},
                            {"system.bootcheck.signature"}
                        })
                    );
                } else {
                    return(registerDatapoints(
                        {
                            {"device"},
                            {"system.os"},
                            //Wan IP Address
                            {"net.interface.common[].ipv4.address"},
                            {"net.interface.cellular[c1]"},
                            //WiFi Mode
                            {"net.interface.wifi[w1].radio.mode"},
                            {"net.interface.wifi[w2].radio.mode"},
                            {"system.bootcheck.signature"}
                        })
                    );
                }       
            }

            cache.insert(std::pair("model", jobj["data"]["device.product"].get<std::string>()));
            cache.insert(std::pair("serialNumber", jobj["data"]["device.provisioning.serial"].get<std::string>()));
            cache.insert(std::pair("osVersion", jobj["data"]["system.os.version"].get<std::string>()));
            cache.insert(std::pair("osBuildNumber", jobj["data"]["system.os.buildnumber"].get<std::string>()));
            cache.insert(std::pair("firmwareName", jobj["data"]["system.os.name"].get<std::string>()));

            if(jobj["data"]["device.product"] != nullptr && !(jobj["data"]["device.product"].get<std::string>()).compare(0, 4, "XR90")) {
                if(jobj["data"]["net.interface.cellular[c5].service"] != nullptr && 
                    !(jobj["data"]["net.interface.cellular[c5].service"].get<std::string>()).compare(0, 9, "Available")) {
                        cache.insert(std::pair("imei", jobj["data"]["net.interface.cellular[c5].imei"].get<std::string>()));
                        cache.insert(std::pair("signalStrength", std::to_string(jobj["data"]["net.interface.cellular[c5].rssi"].get<std::int32_t>())));
                        cache.insert(std::pair("apn", jobj["data"]["net.interface.cellular[c5].apninuse"].get<std::string>()));
                        cache.insert(std::pair("ipAddress", jobj["data"]["net.interface.common[c5].ipv4.address"].get<std::string>()));
                        cache.insert(std::pair("technology", jobj["data"]["net.interface.cellular[c5].technology.current"].get<std::string>()));
                        cache.insert(std::pair("carrier", jobj["data"]["net.interface.cellular[c5].operator"].get<std::string>()));

                } else if(jobj["data"]["net.interface.cellular[c4].service"] != nullptr && 
                    !(jobj["data"]["net.interface.cellular[c4].service"].get<std::string>()).compare(0, 9, "Available")) {
                        cache.insert(std::pair("imei", jobj["data"]["net.interface.cellular[c4].imei"].get<std::string>()));
                        cache.insert(std::pair("signalStrength", std::to_string(jobj["data"]["net.interface.cellular[c4].rssi"].get<std::int32_t>())));
                        cache.insert(std::pair("apn", jobj["data"]["net.interface.cellular[c4].apninuse"].get<std::string>()));
                        cache.insert(std::pair("ipAddress", jobj["data"]["net.interface.common[c4].ipv4.address"].get<std::string>()));
                        cache.insert(std::pair("technology", jobj["data"]["net.interface.cellular[c4].technology.current"].get<std::string>()));
                        cache.insert(std::pair("carrier", jobj["data"]["net.interface.cellular[c4].operator"].get<std::string>()));
                }

            } else if(jobj["data"]["device.product"] != nullptr && !(jobj["data"]["device.product"].get<std::string>()).compare(0, 4, "XR80")) {
                if(jobj["data"]["net.interface.cellular[c2].service"] != nullptr && 
                    !(jobj["data"]["net.interface.cellular[c2].service"].get<std::string>()).compare(0, 9, "Available")) {
                        cache.insert(std::pair("imei", jobj["data"]["net.interface.cellular[c2].imei"].get<std::string>()));
                        cache.insert(std::pair("signalStrength", std::to_string(jobj["data"]["net.interface.cellular[c2].rssi"].get<std::int32_t>())));
                        cache.insert(std::pair("apn", jobj["data"]["net.interface.cellular[c2].apninuse"].get<std::string>()));
                        cache.insert(std::pair("ipAddress", jobj["data"]["net.interface.common[c2].ipv4.address"].get<std::string>()));
                        cache.insert(std::pair("technology", jobj["data"]["net.interface.cellular[c2].technology.current"].get<std::string>()));
                        cache.insert(std::pair("carrier", jobj["data"]["net.interface.cellular[c2].operator"].get<std::string>()));

                } else if(jobj["data"]["net.interface.cellular[c3].service"] != nullptr && 
                    !(jobj["data"]["net.interface.cellular[c3].service"].get<std::string>()).compare(0, 9, "Available")) {
                        cache.insert(std::pair("imei", jobj["data"]["net.interface.cellular[c3].imei"].get<std::string>()));
                        cache.insert(std::pair("signalStrength", std::to_string(jobj["data"]["net.interface.cellular[c3].rssi"].get<std::int32_t>())));
                        cache.insert(std::pair("apn", jobj["data"]["net.interface.cellular[c3].apninuse"].get<std::string>()));
                        cache.insert(std::pair("ipAddress", jobj["data"]["net.interface.common[c3].ipv4.address"].get<std::string>()));
                        cache.insert(std::pair("technology", jobj["data"]["net.interface.cellular[c3].technology.current"].get<std::string>()));
                        cache.insert(std::pair("carrier", jobj["data"]["net.interface.cellular[c3].operator"].get<std::string>()));
                }

            } else if(jobj["data"]["device.product"] != nullptr && !(jobj["data"]["device.product"].get<std::string>()).compare(0, 4, "RX55")) {
                cache.insert(std::pair("imei", jobj["data"]["net.interface.cellular[c1].imei"].get<std::string>()));
                cache.insert(std::pair("signalStrength", std::to_string(jobj["data"]["net.interface.cellular[c1].rssi"].get<std::int32_t>())));
                cache.insert(std::pair("apn", jobj["data"]["net.interface.cellular[c1].apninuse"].get<std::string>()));
                cache.insert(std::pair("ipAddress", jobj["data"]["net.interface.common[c1].ipv4.address"].get<std::string>()));
                cache.insert(std::pair("technology", jobj["data"]["net.interface.cellular[c1].technology.current"].get<std::string>()));
                cache.insert(std::pair("carrier", jobj["data"]["net.interface.cellular[c1].operator"].get<std::string>()));

            }

            json ind = json::object();
            for(const auto& ent: cache) {
                ind[ent.first] = ent.second;
            }
            std::cout << "line: " << __LINE__ << " value: " << ind.dump() << std::endl;

            result.assign(ind.dump());
        }
    } else if(!uri().compare(0, 14, "/api/v1/events")) {
        
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
    {"timeout",                   required_argument, 0, 'o'},
    {"machine",                   required_argument, 0, 'm'},
    {"config-json",               required_argument, 0, 'c'},
    {"userid",                    required_argument, 0, 'u'},
    {"password",                  required_argument, 0, 'd'}
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
    
    while ((c = getopt_long(argc, argv, "r:i:p:w:t:a:s:e:o:m:u:d:", options.data(), &option_index)) != -1) {
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
                config.emplace(std::make_pair("timeout", optarg));
            }
            break;
            case 'm':
            {
                config.emplace(std::make_pair("machine", optarg));
            }
            break;
            case 'u':
            {
                config.emplace(std::make_pair("userid", optarg));
            }
            break;
            case 'd':
            {
                config.emplace(std::make_pair("password", optarg));
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
                          << "--timeout     <value in ms> " << std::endl
                          << "--machine     <host|> " << std::endl
                          << "--userid      <Rest Client User ID> " << std::endl
                          << "--password    <Rest Client Password> " << std::endl;
                          return(-1);
            }
        }
    }
    
    
    noor::Uniimage inst;
    std::string bridgeIP("192.168.1.1");
    std::uint16_t httpPort = 80;
    std::uint16_t httpsPort = 443;
    std::uint16_t consolePort = 65344;

    auto ret = inst.init();
    inst.set_config(config);

    std::cout << "line: " << __LINE__ << " epoll_fd: " << ret << std::endl;

    if(!config["role"].compare("client")) {
        
        if(!config["protocol"].compare("tcp")) {
            inst.CreateServiceAndRegisterToEPoll(noor::ServiceType::Tcp_DeviceMgmtServer_Client_Gateway_Service_Async,
                                                 config["server-ip"], std::stoi(config["server-port"]),
                                                 true);
        } else if(!config["protocol"].compare("udp")) {

        } else if(!config["protocol"].compare("tls")) {
            inst.CreateServiceAndRegisterToEPoll(noor::ServiceType::Tls_Tcp_DeviceMgmtServer_Client_Gateway_Service_Async, 
                                                 config["server-ip"], std::stoi(config["server-port"]),
                                                 true);
        } else if(!config["protocol"].compare("dtls")) {

        } else {
            //Protocol not supported.
            std::cout << __TIMESTAMP__ << " line: " << __LINE__ << " protocol is not supported " << std::endl;
            exit(0);
        }
        inst.CreateServiceAndRegisterToEPoll(noor::ServiceType::Tcp_DeviceMgmtServer_Client_Console_Service_Async, config["server-ip"], consolePort, true);
        //inst.CreateServiceAndRegisterToEPoll(noor::ServiceType::Tcp_Web_Client_Proxy_Service, bridgeIP, httpPort, false);
        inst.CreateServiceAndRegisterToEPoll(noor::ServiceType::Tls_Tcp_Rest_Client_For_Gateway_Service_Sync, bridgeIP, httpsPort);
        
    } else if(!config["role"].compare("server")) {

        if(!config["server-ip"].length()) {
            config["server-ip"] = "127.0.0.1";
        }
        if(!config["protocol"].compare("tcp")) {
            inst.CreateServiceAndRegisterToEPoll(noor::ServiceType::Tcp_DeviceMgmtServer_Server_Gateway_Service, config["server-ip"], std::stoi(config["server-port"]));
        } else if(!config["protocol"].compare("udp")) {

        } else if(!config["protocol"].compare("tls")) {
            inst.CreateServiceAndRegisterToEPoll(noor::ServiceType::Tls_Tcp_DeviceMgmtServer_Server_Gateway_Service, config["server-ip"], std::stoi(config["server-port"]));
        } else if(!config["protocol"].compare("dtls")) {

        } else {
            //Protocol not supported.
            std::cout << __TIMESTAMP__ << " line: " << __LINE__ << " protocol is not supported " << std::endl;
            exit(0);
        }

        inst.CreateServiceAndRegisterToEPoll(noor::ServiceType::Tcp_DeviceMgmtServer_Server_Console_Service, config["server-ip"], consolePort);
        inst.CreateServiceAndRegisterToEPoll(noor::ServiceType::Tcp_DeviceMgmtServer_Web_Server_Service, config["server-ip"], std::stoi(config["web-port"]));
    }

    //The Unit of timeout is in millisecond.
    auto timeout = 100;
    if(config["timeout"].length()) {
        timeout = std::stoi(config["timeout"]);
    }
    // timeout is in milli seconds
    inst.start(timeout);
}

/**
 * @brief 
 * 
 * @param IP 
 * @param PORT 
 * @param isAsync 
 * @return std::int32_t 
 */
std::int32_t noor::Service::tcp_client(const std::string& IP, std::uint16_t PORT, std::int32_t &fd, bool isAsync) {
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
    
    //let the caller see this file descriptor.
    fd = channel;
    
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
            std::cout << __TIMESTAMP__ << " line: " << __LINE__ << " Async connection in progress IP: " << IP << " PORT: " << PORT << std::endl;
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

    if(Tcp_DeviceMgmtServer_Client_Gateway_Connected_Service == svcType) {
        // Received from Datastore 
        return(tcp_rx(channel, data));

    } else if(Tcp_Device_MgmtServer_Client_Console_Connected_Service == svcType) {

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
        //std::cout << "line: " << __LINE__ << " channel: " << channel << " closed " << std::endl;
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
        return(len);
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

    return(0);
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
       << "Content-Type: application/vnd.api+json" << "\r\n";

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
    if(!http.uri().compare(0, 17, "/api/v1/device/ui")) {
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
        std::int32_t offset = 0;
        auto cl = http.value("Content-Length");
        std::int32_t payload_len = 0;

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
        std::int32_t offset = 0;
        auto cl = http.value("Content-Length");
        std::int32_t payload_len = 0;

        if(!cl.length()) {
            std::cout << "line: " << __LINE__ << " Content-Length is not present" << std::endl;
            data = ss;
            return(data.length());

        } else {
            std::cout << "function: "<< __FUNCTION__ << " line: " << __LINE__ <<" value of Content-Length " << cl << std::endl;
            payload_len = std::stoi(cl);
            if(len == (payload_len + std::int32_t(http.header().length()))) {
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
        std::int32_t offset = 0;
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
std::int32_t noor::Service::tcp_server(const std::string& IP, std::uint16_t PORT, std::int32_t& fd) {
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

    std::int32_t channel = ::socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, IPPROTO_TCP);
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

    //let the caller see this file descriptor.
    fd = channel;
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



#endif /* __uniimage__cc__ */
