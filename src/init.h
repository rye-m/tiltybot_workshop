#include <Arduino.h>
#include "XL330.h"
#include <SSLCert.hpp>
#include <LittleFS.h>
#include <SSLCert.hpp>
#include <HTTPSServer.hpp>
#include <tuple>

#define DIR_PUBLIC "/"
#define AP 1
#define LOCAL 0
#define RXD2 14
#define TXD2 4
#define DRIVE_MODE 16
#define POSITION_MODE 3
#define BROADCAST 254

#define MOTOR1 1
#define MOTOR2 2

using namespace httpsserver;

void initRobot(HardwareSerial &serialPort, XL330 &robot, int mode);

SSLCert *initLittleFS();

void initWiFi(const char *ssid, const char *password, const char *index, int mode);

HTTPSServer *initServer(SSLCert *cert);

std::tuple<int, int> parseData(WebsocketInputStreambuf *inbuf);