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

#define INDEX_PAGE "/drive.html"

// // Robot setup
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
    initRobot(Serial2, robot, DRIVE_MODE);
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
    int xRaw = std::get<0>(result);
    int yRaw = std::get<1>(result);
    int x = map(xRaw, -100, 100, -885, 885);
    int y = map(yRaw, -100, 100, -885, 885);
    int speedM1 = constrain(x + y, -855, 855);
    int speedM2 = constrain(x - y, -855, 855);

    Serial.print(millis());
    Serial.print(",");
    Serial.print(speedM1);
    Serial.print(",");
    Serial.println(speedM2);

    if (abs(speedM1 - prevM1) > 5 || speedM1 == 0)
    {
        robot.setJointSpeed(MOTOR1, speedM1);
        prevM1 = speedM1;
        delay(10);
    }
    if (abs(speedM2 - prevM2) > 5 || speedM2 == 0)
    {
        robot.setJointSpeed(MOTOR2, speedM2);
        delay(10);
        prevM2 = speedM2;
    }
}
