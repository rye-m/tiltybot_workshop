#include <Arduino.h>
#include <ArduinoJson.h>
#include <string>

#include "init.h"
#include "network.h"

// // Includes for the server
#include <HTTPSServer.hpp>
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>
#include <util.hpp>
#include <WebsocketHandler.hpp>

#define RXD2 14
#define TXD2 4
#define INDEX_PAGE "/drive.html"
#define DRIVE_MODE 16

#include "XL330.h"
XL330 robot;
const int motorOne = 1;
const int motorTwo = 2;
const int broadcast = 254;
int prevM1 = 0;
int prevM2 = 0;

using namespace httpsserver;

HTTPSServer *secureServer;

// Websockets setup
const int MAX_CLIENTS = 4;
// As websockets are more complex, they need a custom class that is derived from WebsocketHandler
class ChatHandler : public WebsocketHandler
{
public:
    static WebsocketHandler *create();

    // This method is called when a message arrives
    void onMessage(WebsocketInputStreambuf *input);

    // Handler function on connection close
    void onClose();
};

// Simple array to store the active clients:
ChatHandler *activeClients[MAX_CLIENTS];

void initWs()
{
    for (int i = 0; i < MAX_CLIENTS; i++)
        activeClients[i] = nullptr;
}

void setup()
{
    // For logging
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);

    SSLCert *cert = initLittleFS();

    // Connect to WiFi
    initWiFi(ssid, password, INDEX_PAGE);

    // Create the server with the certificate we loaded before
    secureServer = initServer(cert);

    // create ws node
    WebsocketNode *chatNode = new WebsocketNode("/ws", &ChatHandler::create);

    // Adding the node to the server works in the same way as for all other nodes
    secureServer->registerNode(chatNode);

    Serial.println("Starting server...");
    secureServer->start();
    if (secureServer->isRunning())
    {
        Serial.println("Server ready.");
    }
    initRobot(Serial2, robot, DRIVE_MODE);
}

void loop()
{
    // This call will let the server do its work
    secureServer->loop();

    delay(5);
}

WebsocketHandler *ChatHandler::create()
{
    Serial.println("Creating new chat client!");
    ChatHandler *handler = new ChatHandler();
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (activeClients[i] == nullptr)
        {
            activeClients[i] = handler;
            break;
        }
    }
    return handler;
}

// When the websocket is closing, we remove the client from the array
void ChatHandler::onClose()
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (activeClients[i] == this)
        {
            ChatHandler *client = activeClients[i];
            activeClients[i] = nullptr;
            delete client;
            break;
        }
    }
}

void ChatHandler::onMessage(WebsocketInputStreambuf *inbuf)
{
    // Get the input message
    std::ostringstream ss;
    std::string msg;
    ss << inbuf;
    msg = ss.str();
    // Serial.println(msg.c_str());
    const size_t capacity = JSON_OBJECT_SIZE(2) + 30;
    DynamicJsonBuffer jsonBuffer(capacity);

    JsonObject &root = jsonBuffer.parseObject(msg.c_str());
    const char *b = root["b"];
    const char *g = root["g"];
    // int speedM1 = int(map(atof(b), -100, 100, -885, 885));
    // int speedM2 = int(map(atof(g), -100, 100, -885, 885));
    int x = map(atof(b), -100, 100, -885, 885);
    int y = map(atof(g), -100, 100, -885, 885);
    int speedM1 = constrain(x + y, -855, 855);
    int speedM2 = constrain(x - y, -855, 855);

    Serial.print(millis());
    Serial.print(",");
    Serial.print(speedM1);
    Serial.print(",");
    Serial.println(speedM2);

    if (abs(speedM1 - prevM1) > 5 || speedM1 == 0)
    {
        robot.setJointSpeed(motorOne, speedM1);
        prevM1 = speedM1;
        delay(10);
    }
    if (abs(speedM2 - prevM2) > 5 || speedM2 == 0)
    {
        robot.setJointSpeed(motorTwo, speedM2);
        delay(10);
        prevM2 = speedM2;
    }
}
