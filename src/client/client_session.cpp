#include <client_session.h>
#include <logging.h>
/* For Visual Studio Compilers */
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "AdvApi32.lib")
/*----------------------------*/

using namespace rchat;

void client_session::start_session() {
    initialise_wsa();
    find_server_connect();
    kick_threads(); 
}
 

void client_session::initialise_wsa() {
    line(); 
    log(message_type::START, "WSA is starting up...");
    // Initialise winsock2 
    _result = WSAStartup(MAKEWORD(2,2), &_wsa_data);
    if(_result != 0) { 
        log(message_type::ERR, "Startup faield with error", _result);
    }

    line(); 

    ZeroMemory(&_hints, sizeof(_hints));
    _hints.ai_family = AF_UNSPEC;
    _hints.ai_socktype = SOCK_STREAM;
    _hints.ai_protocol = IPPROTO_TCP;
}

void client_session::find_server_connect(){
    log(message_type::START, "Getting server information...");

    // Translation from host name to an address getting all sockets that could be used for connection
    _result = getaddrinfo(_ip, _port, &_hints, &_addr_results);
    if(_result != 0) {
        log(message_type::ERR,"Getaddrinfo failed with error", _result);
        WSACleanup();
        return;
    }

    // Check which server socket is valid and available
    for(_ptr_to_addr = _addr_results; _ptr_to_addr != NULL;  _ptr_to_addr = _ptr_to_addr->ai_next) {
        _server  = socket(_ptr_to_addr->ai_family, _ptr_to_addr->ai_socktype, _ptr_to_addr->ai_protocol);
        if(_server  == INVALID_SOCKET) {
            log(message_type::ERR,"Socket failed with error", WSAGetLastError());
            WSACleanup();
            return; 
        }

        // Attempt connection to the socket
        _result = connect(_server , _ptr_to_addr->ai_addr, (int)_ptr_to_addr->ai_addrlen);
        if(_result == SOCKET_ERROR) {
            closesocket(_server );
            _server  = INVALID_SOCKET; 
            continue;
        }
        printf("Connected!");
        break;
    }
    
    freeaddrinfo(_addr_results);

    if(_server  == INVALID_SOCKET) {
        log(message_type::ERR,"Unable to connect to server!");
        WSACleanup();
        return; 
    }

    line(); 
}

void client_session::kick_threads() {

    if(_result == SOCKET_ERROR) {
        log(message_type::ERR,"Send failed with error", WSAGetLastError());
        closesocket(_server);
        WSACleanup();
        return; 
    }

    _receive_ref = std::thread(&client_session::send_handler, this);
    _send_ref = std::thread(&client_session::receive_handler, this);

    _receive_ref.join();
    _send_ref.join();
}

void client_session::send_handler() {
    int result; 
    while(true) {
        // handle input 
        char sendbuf[rchat::global_network_variables::buflen];
        log(message_type::SESSION,"Send message: ");
        scanf("%s", sendbuf);
        result = send(_server , sendbuf, (int)strlen(sendbuf) + 1, 0);
        if(result == SOCKET_ERROR) {
            log(message_type::ERR,"Send failed with error", WSAGetLastError());
            end_session();
            return;
        }
        if(rchat::global_network_variables::exit.compare(sendbuf) == 0) {
            end_session(); 
            return;
        }
    }
}

void client_session::receive_handler() {
    char recvbuf[rchat::global_network_variables::buflen];
    int result;
    while(true) {
        result = recv(_server , recvbuf, rchat::global_network_variables::buflen, 0);
        if(_end_session) return; 
        if(result > 0) { 
             log(message_type::SESSION,"Server Response", recvbuf); 
        }   
        else if(result < 0){  
           log(message_type::ERR,"Recv failed with error", WSAGetLastError()); 
        }
    }   
}


void client_session::end_session() {
    _end_session = true;
    _result = shutdown(_server , SD_SEND);
    if(_result == SOCKET_ERROR) {
       log(message_type::ERR,"Shutdown failed with error", WSAGetLastError());
        closesocket(_server);
        _server  = INVALID_SOCKET;
        return;
    } 
    log(message_type::SESSION,"Closing session");
    closesocket(_server);
    WSACleanup();
}



