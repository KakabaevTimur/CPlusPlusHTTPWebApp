#pragma once

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <unistd.h>

#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>
#include <array>
#include <functional>
#include <algorithm>
#include <ranges>

#include "http/http.hpp"
#include "logger.hpp"

struct Route {
    std::string_view uri;
    HTTPMethod method;
    std::function<Response(Request)> handler;

    constexpr bool match(const Request& req) const {
        return uri == req.uri && method == req.method;
    }
};

// Logging
Logger logger{};
const char *START_MESSAGE = "\x1b[32m[Server started]\x1b[0m\n";
const char *NEW_REQ_MESSAGE = "\x1b[32m[New request]\x1b[0m\n";
const char *SENDING_RES_MESSAGE = "\x1b[32m[Sending response]\x1b[0m\n";
const char *ERROR_MESSAGE = "\x1b[31m[Error]\x1b[0m\n";
const char *ACCEPTED_MESSAGE = "\x1b[32m[Accepted client]\x1b[0m\n";
const char *RECIEVED_MESSAGE = "\x1b[32m[Recieved]\x1b[0m\n";

class Server {
    constexpr static size_t CONNMAX = 100;
    constexpr static size_t CONNMAXPERSOCK = 100;
    constexpr static size_t BUFSIZE = 65535;

    const std::string port;
    std::vector<Route> routes;

    int listenfd;
    size_t cur_client = 0;
    std::array<int, CONNMAX> clients;
    std::vector<char> buf;

    void setup();
    void respond(size_t n);

public:
    template<typename... Routes>
    Server(uint16_t port_, Routes... routes_) : port{std::to_string(port_)}, routes{sizeof...(Routes)}, buf(BUFSIZE, 0) {
        (routes.push_back(std::forward<Routes>(routes_)), ...);
        setup();
    }

    void start();
};

void Server::setup()
{
    std::ranges::fill(clients, -1);

    addrinfo hints;
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    addrinfo *res;
    if (getaddrinfo(nullptr, port.c_str(), &hints, &res) != 0)
    {
        perror("getaddrinfo() error");
        exit(1);
    }

    // socket and bind
    addrinfo* p;
    for (p = res; p != nullptr; p = p->ai_next)
    {
        int option{1};
        listenfd = socket(p->ai_family, p->ai_socktype, 0);
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
        if (listenfd == -1)
            continue;
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0)
            break;
    }
    if (p == nullptr)
    {
        perror("Socket creation or binding error");
        exit(1);
    }

    freeaddrinfo(res);

    if (listen(listenfd, CONNMAXPERSOCK) != 0)
    {
        perror("listen error");
        exit(1);
    }
}

void Server::respond(size_t n)
{
    int clientfd = clients[n];

    if (int rcvd = recv(clientfd, buf.data(), BUFSIZE - 1, 0); rcvd < 0)
        fprintf(stderr, ("recv() error\n"));
    else if (rcvd == 0)
        fprintf(stderr, "Client disconnected upexpectedly.\n");
    else
    {
        logger.log(RECIEVED_MESSAGE, rcvd, " bytes from client ", n);

        // Handling received message
        Request req{buf.data()};

        logger.log(NEW_REQ_MESSAGE, req.toString());

        Response response{};
        for (const auto& route : routes) {
            if (route.match(req)) {
                try {
                    response = route.handler(req);
                }
                catch (std::exception& e) {
                    logger.log(ERROR_MESSAGE, "Request handler thrown an exeption: ", e.what());
                    response = Response{"HTTP/1.1", 500, "Internal Server Error"};
                }
                break;
            }
        }
        auto response_str = response.toString();

        logger.log(SENDING_RES_MESSAGE, response_str);

        write(clientfd, response_str.c_str(), response_str.length());

        shutdown(clientfd, SHUT_RDWR);
        close(clientfd);
    }
    clients[n] = -1;

    std::ranges::fill(buf, '\0');
}

void Server::start()
{
    logger.log(START_MESSAGE, "http://127.0.0.1:", port.c_str());
   
    while (true)
    {
        sockaddr_in clientaddr;
        socklen_t addrlen = sizeof(clientaddr);
        clients[cur_client] = accept(listenfd, (sockaddr *)&clientaddr, &addrlen);

        logger.log(ACCEPTED_MESSAGE, cur_client);

        if (clients[cur_client] == -1)
        {
            perror("accept() error");
            continue;
        }
        
        respond(cur_client);
        while (clients[cur_client] != -1) {
            cur_client = (cur_client + 1) % CONNMAX;
        }
    }
}
