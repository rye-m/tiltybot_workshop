#include <Arduino.h>
#include <ArduinoJson.h>
#include <string>

#include "init.h"

// // Includes for the server
#include <HTTPSServer.hpp>
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>
#include <util.hpp>
#include <WebsocketHandler.hpp>

#define RXD2 14
#define TXD2 4
#define INDEX_PAGE "/tilty.html"
#define POSITION_MODE 3

// // Robot setup
#include "XL330.h"
XL330 robot;
const int panServo = 1;
const int tiltServo = 2;
int prevGamma = 2048;
int prevBeta = 2048;
const int broadcast = 254;

using namespace httpsserver;

HTTPSServer *secureServer;

// // NETWORK SETUP
// // Replace with your network credentials
const char *ssid = "deviceFarm";
const char *password = "device@theFarm";

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
    initRobot(Serial2, robot, POSITION_MODE);
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
    int posBeta = int(constrain(atof(b) / .088, 70, 2150));
    int posGamma = int(map(atof(g), -89, 89, 1024, 3072));

    Serial.print(millis());
    Serial.print(",");
    Serial.print(b);
    Serial.print(",");
    Serial.println(g);
    int dBeta = abs(prevBeta - posBeta);
    int dGamma = abs(prevGamma - posGamma);
    if (dBeta > 5) // don't flood the bus
    {
        if (dBeta < 1000) // at extreme deltas don't move
        {
            robot.setJointPosition(tiltServo, posBeta);
            delay(1);
            prevBeta = posBeta;
        }
    }
    if (dGamma > 5) // don't flood the bus
    {
        if (dGamma < 1000) // at extreme deltas don't move
        {
            robot.setJointPosition(panServo, posGamma);
            delay(1);
            prevGamma = posGamma;
        }
    }
}
