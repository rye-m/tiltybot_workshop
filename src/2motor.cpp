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

#define INDEX_PAGE "/2motor.html"

#include "XL330.h"
XL330 robot;
int prevM1 = 0;
int prevM2 = 0;

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
    int posM1 = int(map(x, 0, 360, 0, 4095));
    int posM2 = int(map(y, 0, 360, 0, 4095));

    Serial.print(millis());
    Serial.print(",");
    Serial.print(x);
    Serial.print(",");
    Serial.println(y);
    if (abs(posM1 - prevM1) > 5)
    {
        robot.setJointPosition(MOTOR1, posM1);
        prevM1 = posM1;
        delay(1);
    }
    if (abs(posM2 - prevM2) > 5)
    {
        robot.setJointPosition(MOTOR2, posM2);
        delay(1);
        prevM2 = posM2;
    }
}
