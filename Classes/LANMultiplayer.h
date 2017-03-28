//
//  LANMultiplayer.hpp
//  lantest
//
//  Created by vamsi krishna on 07/03/17.
//
//

#ifndef LANMultiplayer_hpp
#define LANMultiplayer_hpp

#include <stdio.h>
#include <thread>
#include <vector>
#include "enet/enet.h"

#define CONNECTION_WAIT_MS 5000
#define LISTEN_PORT 3500
#define MAX_SERVERS  10
#define MAX_CLIENTS 10
#define TIMEOUT_SECONDS 2
#define SERVER_LIST_FETCHED "slf"

typedef struct
{
    char hostname[1024];
    enet_uint16 port;
} ServerInfo;

/* you get callbacks on these functions. 
 Do not delete these objects.
 */
class LANEventDelegate {
public:
    virtual void onMessage(ENetPacket *){};
    virtual void onConnect(ENetEvent *){}; // Copy the data from packet->data if you need it for later use.
    virtual void onDisconnect(ENetEvent *){};
};

class LANServer {
    static LANServer *m_instance;
    // The chat server host
    ENetHost *m_pListenHost;
    // The socket for listening and responding to client scans
    ENetSocket m_pListen;
    std::thread m_thread;
    void listenForScan();
    void listenToClients();
    void clientProcessing();
    bool m_bStop;
    LANServer();
public:
    static LANServer *getInstance();
    static LANEventDelegate *_listener;
    bool startServer();
    void stopServer();
    void send_string(char *s);
};

class LANClient {
    LANClient();
    static LANClient *m_instance;
    ServerInfo server_infos[MAX_SERVERS];
    ENetAddress addrs[MAX_SERVERS];
    ENetPeer *m_pPeer;
    ENetHost *m_pHost;
    std::thread m_thread;
    void lookForServers();
    bool m_bStopLookUp;
    bool m_bStopProcessing;
    void message_processing();
    
public:
    std::vector<std::string> server_list;    
    static LANClient * getInstance();
    static LANEventDelegate *_listener;
    
    void startScan();
    bool joinServer(int index);
    void send_string(char *s);
    
    void stop_client();
    void stopLookup();
};

#endif /* LANMultiplayer_hpp */
