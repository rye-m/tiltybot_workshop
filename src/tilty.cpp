#include <Arduino.h>
#include <tuple>

#include "init.h"
#include "network.h"

// // Includes for the server
#include <HTTPSServer.hpp>
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>
#include <util.hpp>
#include <WebsocketHandler.hpp>

#define INDEX_PAGE "/tilty.html"

// // Robot setup
#include "XL330.h"
XL330 robot;
int prevGamma = 2048;
int prevBeta = 2048;

using namespace httpsserver;

HTTPSServer *secureServer;

// class for robot Controler
class ControlHandler : public WebsocketHandler
{
public:
    static WebsocketHandler *create();
    // This method is called when a message arrives
    void onMessage(WebsocketInputStreambuf *input);
};

void setup()
{
    // For logging
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);

    SSLCert *cert = initLittleFS();

    // Connect to WiFi
    initWiFi(ssid, password, INDEX_PAGE, AP);

    // Create the server with the certificate we loaded before
    secureServer = initServer(cert);

    // create ws node
    WebsocketNode *controlNode = new WebsocketNode("/ws", &ControlHandler::create);

    // Adding the node to the server works in the same way as for all other nodes
    secureServer->registerNode(controlNode);

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

WebsocketHandler *ControlHandler::create()
{
    Serial.println("Creating new control client!");
    ControlHandler *handler = new ControlHandler();
    return handler;
}

void ControlHandler::onMessage(WebsocketInputStreambuf *inbuf)
{
    std::tuple<int, int> result = parseData(inbuf);
    int x = std::get<0>(result);
    int y = std::get<1>(result);
    int posBeta = int(constrain(x / .088, 70, 2150));
    int posGamma = int(map(y, -89, 89, 1024, 3072));

    Serial.print(millis());
    Serial.print(",");
    Serial.print(x);
    Serial.print(",");
    Serial.println(y);
    int dBeta = abs(prevBeta - posBeta);
    int dGamma = abs(prevGamma - posGamma);
    if (dBeta > 5) // don't flood the bus
    {
        if (dBeta < 1000) // at extreme deltas don't move
        {
            robot.setJointPosition(MOTOR2, posBeta);
            delay(1);
            prevBeta = posBeta;
        }
    }
    if (dGamma > 5) // don't flood the bus
    {
        if (dGamma < 1000) // at extreme deltas don't move
        {
            robot.setJointPosition(MOTOR1, posGamma);
            delay(1);
            prevGamma = posGamma;
        }
    }
}
