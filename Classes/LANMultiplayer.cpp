//
//  LANMultiplayer.cpp
//  lantest
//
//  Created by vamsi krishna on 07/03/17.
//
//

#include "LANMultiplayer.h"
#include "test_generated.h"
#include "cocos2d.h"
#include <string.h>
USING_NS_CC;

LANServer *LANServer::m_instance = nullptr;
LANEventDelegate *LANServer::_listener = nullptr;

LANServer* LANServer::getInstance()
{
    if(m_instance == nullptr){
        m_instance = new LANServer();
    }
    
    return m_instance;
}

LANServer::LANServer()
{
    m_pListenHost = nullptr;
    m_bStop = true;
}

void LANServer::send_string(char *s)
{
    ENetPacket *packet = enet_packet_create(s, strlen(s) + 1, ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(m_pListenHost, 0, packet);
}

void LANServer::listenForScan()
{
    // Check for data to recv
    ENetSocketSet set;
    ENET_SOCKETSET_EMPTY(set);
    ENET_SOCKETSET_ADD(set, m_pListen);
    if (enet_socketset_select(m_pListen, &set, NULL, 0) <= 0)
    {
        return;
    }
    
    ENetAddress recvaddr;
    char buf;
    ENetBuffer recvbuf;
    recvbuf.data = &buf;
    recvbuf.dataLength = 1;
    const int recvlen = enet_socket_receive(m_pListen, &recvaddr, &recvbuf, 1);
    if (recvlen <= 0)
    {
        return;
    }
    char addrbuf[256];
    enet_address_get_host_ip(&recvaddr, addrbuf, sizeof addrbuf);
    printf("Listen port: received (%d) from %s:%d\n",
           *(char *)recvbuf.data, addrbuf, recvaddr.port);
    
    // Reply to scanner client with our info
    ServerInfo sinfo;
    if (enet_address_get_host(&m_pListenHost->address, sinfo.hostname, sizeof sinfo.hostname) != 0)
    {
        fprintf(stderr, "Failed to get hostname\n");
        return;
    }
    sinfo.port = m_pListenHost->address.port;
//    recvbuf.data = &sinfo;
//    recvbuf.dataLength = sizeof sinfo;
    
    //create flat buffer for server info.
    flatbuffers::FlatBufferBuilder builder(1024);
//    sprintf(sinfo.hostname, "vamsis-MacBook-Pro.local");
    
    auto tmp = LANMultiplayer::CreateServerInformation(builder, builder.CreateString(sinfo.hostname), sinfo.port);
    builder.Finish(tmp);
    recvbuf.data = builder.GetBufferPointer();
    recvbuf.dataLength = builder.GetSize();
    
    CCLOG("Sending data to scanning client %s %d", sinfo.hostname, sinfo.port);
    if (enet_socket_send(m_pListen, &recvaddr, &recvbuf, 1) != (int)recvbuf.dataLength)
    {
        fprintf(stderr, "Failed to reply to scanner\n");
    }
}

void LANServer::clientProcessing()
{
    // Loop and process events
    int check;
    do
    {
        // Check our listening socket for scanning clients
        listenForScan();
        
        ENetEvent event;
        check = enet_host_service(m_pListenHost, &event, 0);
        if (check > 0)
        {
            // Whenever a client connects or disconnects, broadcast a message
            // Whenever a client says something, broadcast it including
            // which client it was from
            char buf[256];
            switch (event.type)
            {
                case ENET_EVENT_TYPE_CONNECT:
                    sprintf(buf, "New client connected: id %d", event.peer->incomingPeerID);
                    send_string(buf);
                    CCLOG("%s\n", buf);
                    if(_listener){
                        _listener->onConnect(&event);
                    }
                    break;
                case ENET_EVENT_TYPE_RECEIVE:
                    sprintf(buf, "Client %d says: %s", event.peer->incomingPeerID, event.packet->data);
                    send_string(buf);
                    CCLOG("%s\n", buf);
                    if(_listener){
                        _listener->onMessage(event.packet);
                    }
                    //clean up the packet.
                    enet_packet_destroy(event.packet);
                    break;
                case ENET_EVENT_TYPE_DISCONNECT:
                    sprintf(buf, "Client %d disconnected", event.peer->incomingPeerID);
                    send_string(buf);
                    CCLOG("%s\n", buf);
                    if(_listener){
                        _listener->onDisconnect(&event);
                    }
                    
                    break;
                default:
                    break;
            }
        }
        else if (check < 0)
        {
            fprintf(stderr, "Error servicing host\n");
        }
    } while (!m_bStop && check >= 0);
    
    
    //when server is stopped, we shutdown all the sockets
    printf("Server closing\n");
    if (enet_socket_shutdown(m_pListen, ENET_SOCKET_SHUTDOWN_READ_WRITE) != 0)
    {
        fprintf(stderr, "Failed to shutdown listen socket\n");
    }
    enet_socket_destroy(m_pListen);
    enet_host_destroy(m_pListenHost);
    enet_deinitialize();
}

void LANServer::listenToClients()
{
    m_thread = std::thread(&LANServer::clientProcessing, this);
}

void LANServer::stopServer()
{
    m_bStop = true;
}

bool LANServer::startServer()
{
    if(m_bStop == false)
    {
        return false;
    }
    
    // Start server
    if (enet_initialize() != 0)
    {
        CCLOG("An error occurred while initializing ENet\n");
        return false;
    }
    
    // Start listening socket
    m_pListen = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if (m_pListen == ENET_SOCKET_NULL)
    {
        CCLOG("Failed to create socket\n");
        return false;
    }
    if (enet_socket_set_option(m_pListen, ENET_SOCKOPT_REUSEADDR, 1) != 0)
    {
        CCLOG("Failed to enable reuse address\n");
        return false;
    }
    ENetAddress listenaddr;
    listenaddr.host = ENET_HOST_ANY;
    listenaddr.port = LISTEN_PORT;
    if (enet_socket_bind(m_pListen, &listenaddr) != 0)
    {
        CCLOG("Failed to bind listen socket\n");
        return false;
    }
    if (enet_socket_get_address(m_pListen, &listenaddr) != 0)
    {
        CCLOG("Cannot get listen socket address\n");
        return false;
    }
    CCLOG("Listening for scans on port %d\n", listenaddr.port);
    
    ENetAddress addr;
    addr.host = ENET_HOST_ANY;
    addr.port = ENET_PORT_ANY;
    m_pListenHost = enet_host_create(&addr, MAX_CLIENTS, 2, 0, 0);
    if (m_pListenHost == NULL)
    {
        CCLOG("Failed to open ENet host\n");
        return false;
    }
    
    char buf[256];
    enet_address_get_host_ip(&m_pListenHost->address, buf, sizeof buf);
    CCLOG("starting server on %s:%u", buf, m_pListenHost->address.port);
    
    m_bStop = false;
    
    listenToClients();
    
    return true;
}

LANClient *LANClient::m_instance = nullptr;
LANEventDelegate *LANClient::_listener = nullptr;

LANClient::LANClient()
{
    m_pHost = nullptr;
    m_pPeer = nullptr;
}

LANClient *LANClient::getInstance()
{
    if(m_instance == nullptr){
        m_instance = new LANClient();
        
        if(enet_initialize() != 0){
            log("Could not initilize enet");
            delete m_instance;
            m_instance = nullptr;
        }
    }
    
    return m_instance;
}

void LANClient::send_string(char *s)
{
    ENetPacket *packet = enet_packet_create(s, strlen(s) + 1, ENET_PACKET_FLAG_RELIABLE);
    if (enet_peer_send(m_pPeer, 0, packet) < 0)
    {
        fprintf(stderr, "Error when sending packet\n");
    }
}

void LANClient::stop_client()
{
    printf("Client closing\n");
    m_bStopProcessing = true;
    
    if(m_thread.joinable()){
        m_thread.join();
    }
    
    enet_peer_disconnect_now(m_pPeer, 0);
    enet_host_destroy(m_pHost);
    enet_deinitialize();
}

void LANClient::message_processing()
{
    int check;
    do {
        // Check for new messages from the server; if there are any
        // just print them out
        ENetEvent event;
        check = enet_host_service(m_pHost, &event, 0);
        if (check > 0)
        {
            switch (event.type)
            {
                case ENET_EVENT_TYPE_RECEIVE:
                    CCLOG("%s\n", event.packet->data);
                    
                    if(_listener){
                        _listener->onMessage(event.packet);
                    }
                    enet_packet_destroy(event.packet);
                    break;
                case ENET_EVENT_TYPE_DISCONNECT:
                    CCLOG("Lost connection with server\n");
                    check = -1;
                    if(_listener){
                        _listener->onDisconnect(&event);
                    }
                    break;
                default:
                    break;
            }
        }
        else if (check < 0)
        {
            fprintf(stderr, "Error servicing host\n");
        }
    }while (check >= 0 && !m_bStopProcessing);
}

bool LANClient::joinServer(int server_index)
{
    CCLOG("+LANClient::joinServer");
//    ENetPeer **peer = &m_pPeer;
    ENetHost *host = enet_host_create(NULL, 1, 2, 0, 0);
    if (host == NULL)
    {
        CCLOG("Failed to open ENet host\n");
        return false;
    }
    char buf[256];
    enet_address_get_host_ip(&addrs[server_index], buf, sizeof buf);
    m_pPeer = enet_host_connect(host, &addrs[server_index], 2, 0);
    if (m_pPeer == NULL)
    {
        fprintf(stderr, "Failed to connect to server on %s:%d\n", buf, addrs[server_index].port);
        return false;
    }
    // Wait for connection to succeed
    ENetEvent event;
    if (enet_host_service(host, &event, CONNECTION_WAIT_MS) > 0 &&
        event.type == ENET_EVENT_TYPE_CONNECT)
    {
        CCLOG("ENet client connected to %s:%d\n", buf, addrs[server_index].port);
    }
    else
    {
        CCLOG("connection failed\n");
        return false;
    }
    
//    return host;
    m_pHost = host;
    m_bStopProcessing = false;
    m_thread  = std::thread(&LANClient::message_processing, this);
    
    CCLOG("-LANClient::joinServer");
    return true;
}

void LANClient::stopLookup()
{
    m_bStopLookUp = true;
    if(m_thread.joinable())
        m_thread.join();
}

void LANClient::startScan()
{
    //prevent multiple calls on start scan.
    if(m_bStopLookUp == false){
        return;
    }
    
    m_bStopLookUp = false;
    m_thread = std::thread(&LANClient::lookForServers, this);
}

void LANClient::lookForServers()
{
    CCLOG("+LANClient::lookForServers");
    server_list.clear();
    
    ENetSocket scanner = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if (scanner == ENET_SOCKET_NULL)
    {
        fprintf(stderr, "Failed to create socket\n");
        return ;
    }
    if (enet_socket_set_option(scanner, ENET_SOCKOPT_BROADCAST, 1) != 0)
    {
        fprintf(stderr, "Failed to enable broadcast socket\n");
        return ;
    }
    ENetAddress scanaddr;
    scanaddr.host = ENET_HOST_BROADCAST;
    scanaddr.port = LISTEN_PORT;
    // Send a dummy payload
    char data = 42;
    ENetBuffer sendbuf;
    sendbuf.data = &data;
    sendbuf.dataLength = 1;
    if (enet_socket_send(scanner, &scanaddr, &sendbuf, 1) != (int)sendbuf.dataLength)
    {
        CCLOG("Failed to scan for LAN servers\n");
        return ;
    }
    // Wait for replies, which will give us server addresses to choose from
    int sinfo_index = 0;
    uint8_t buff_flatbuffer[1024];
    
    for (int i = 0; !m_bStopLookUp && sinfo_index < MAX_SERVERS; i++)
    {
        CCLOG("Scanning for server...\n");
        ENetSocketSet set;
        ENET_SOCKETSET_EMPTY(set);
        ENET_SOCKETSET_ADD(set, scanner);
        while (enet_socketset_select(scanner, &set, NULL, 0) > 0)
        {
            ENetBuffer recvbuf;
            recvbuf.data = buff_flatbuffer; //&server_infos[sinfo_index];
            recvbuf.dataLength = sizeof buff_flatbuffer; //sizeof server_infos[0];
            
            const int recvlen = enet_socket_receive(scanner, &addrs[sinfo_index], &recvbuf, 1);
            if (recvlen > 0)
            {
//                if (recvlen != sizeof(ServerInfo))
//                {
//                    fprintf(stderr, "Unexpected reply from scan\n");
//                    return ;
//                }
                auto verifier = flatbuffers::Verifier((uint8_t*)recvbuf.data, recvbuf.dataLength);
                bool ok = LANMultiplayer::VerifyServerInformationBuffer(verifier);
                if(!ok){
                    fprintf(stderr, "Unexpected reply from scan\n");
                    return ;
                }
                auto tmp = LANMultiplayer::GetServerInformation(recvbuf.data);
                strcpy(server_infos[sinfo_index].hostname, tmp->hostname()->c_str());
                server_infos[sinfo_index].port = tmp->port();
                
                // The server itself runs on a different port,
                // so take it from the message
                addrs[sinfo_index].port = server_infos[sinfo_index].port;
                char buf[256];
                enet_address_get_host_ip(&addrs[sinfo_index], buf, sizeof buf);
                CCLOG("Found server '%s' at %s:%d\n", server_infos[sinfo_index].hostname, buf, addrs[sinfo_index].port);
                server_list.push_back(server_infos[sinfo_index].hostname);
                sinfo_index++;
            }
        }
        sleep(1);
    }
    
    auto ret = enet_socket_shutdown(scanner, ENET_SOCKET_SHUTDOWN_READ_WRITE);
    if ( ret != 0)
    {
        CCLOG("%s", strerror(errno));
        fprintf(stderr, "Failed to shutdown listen socket\n");
//        return ;
    }
    enet_socket_destroy(scanner);
//    return sinfo_index;
    
    CCLOG("Finished Scanning.");
    NotificationCenter::getInstance()->postNotification(SERVER_LIST_FETCHED);
    
    
    CCLOG("-LANClient::lookForServers");
}
