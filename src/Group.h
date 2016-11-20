#ifndef GROUP_UWS_H
#define GROUP_UWS_H

#include "WebSocket.h"
#include "Extensions.h"
#include "HTTPSocket.h"
#include <functional>
#include <stack>
#include <map>

namespace uWS {

struct Hub;

struct UpgradeInfo {
    char *path, *subprotocol;
    size_t pathLength, subprotocolLength;
    uWS::HTTPParser httpParser;

    std::map<std::string, std::string> getRequestHeaders() {
        char* curCursor = httpParser.cursor;
        httpParser.cursor = httpParser.headerBegin;
        std::map<std::string, std::string> headers;

        for (httpParser++; httpParser.key.second; httpParser++) {
            std::string key;
            key.resize(httpParser.key.second);

            size_t i = 0;
            for (; i < httpParser.key.second; i++) {
                key[i] = tolower(httpParser.key.first[i]);
            }
            key[i] = 0;

            headers[key] = std::string(httpParser.value.first, httpParser.value.second);
        }

        // restore cursor back to its original position
        httpParser.cursor = curCursor;

        return headers;
    }
};

template <bool isServer>
struct WIN32_EXPORT Group : protected uS::NodeData {
    friend struct Hub;
    std::function<void(WebSocket<isServer>, UpgradeInfo)> connectionHandler;
    std::function<void(WebSocket<isServer>, char *message, size_t length, OpCode opCode)> messageHandler;
    std::function<void(WebSocket<isServer>, int code, char *message, size_t length)> disconnectionHandler;
    std::function<void(WebSocket<isServer>, char *, size_t)> pingHandler;
    std::function<void(WebSocket<isServer>, char *, size_t)> pongHandler;

    using errorType = typename std::conditional<isServer, int, void *>::type;
    std::function<void(errorType)> errorHandler;

    Hub *hub;
    int extensionOptions;
    uv_timer_t *timer = nullptr;
    std::string userPingMessage;

    // todo: cannot be named user, collides with parent!
    void *userData = nullptr;
    void setUserData(void *user);
    void *getUserData();
    void startAutoPing(int intervalMs, std::string userMessage = "");
    static void timerCallback(uv_timer_t *timer);

    uv_poll_t *webSocketHead = nullptr, *httpSocketHead = nullptr;
    void addWebSocket(uv_poll_t *webSocket);
    void removeWebSocket(uv_poll_t *webSocket);

    std::stack<uv_poll_t *> iterators;

protected:
    Group(int extensionOptions, Hub *hub, uS::NodeData *nodeData);
    void stopListening();

public:
    void onConnection(std::function<void(WebSocket<isServer>, UpgradeInfo ui)> handler);
    void onMessage(std::function<void(WebSocket<isServer>, char *, size_t, OpCode)> handler);
    void onDisconnection(std::function<void(WebSocket<isServer>, int code, char *message, size_t length)> handler);
    void onPing(std::function<void(WebSocket<isServer>, char *, size_t)> handler);
    void onPong(std::function<void(WebSocket<isServer>, char *, size_t)> handler);
    void onError(std::function<void(errorType)> handler);

    void broadcast(const char *message, size_t length, OpCode opCode);
    void terminate();
    void close(int code = 1000, char *message = nullptr, size_t length = 0);
    using NodeData::addAsync;

    // todo: handle nested forEachs with removeWebSocket
    template <class F>
    void forEach(const F &cb) {
        uv_poll_t *iterator = webSocketHead;
        iterators.push(iterator);
        while (iterator) {
            uv_poll_t *lastIterator = iterator;
            cb(WebSocket<isServer>(iterator));
            iterator = iterators.top();
            if (lastIterator == iterator) {
                iterator = ((uS::SocketData *) iterator->data)->next;
                iterators.top() = iterator;
            }
        }
        iterators.pop();
    }
};

}

#endif // GROUP_UWS_H
