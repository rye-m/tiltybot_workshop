#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <string>

// // Includes for the server
#include <HTTPSServer.hpp>
#include <SSLCert.hpp>
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>
#include <util.hpp>
#include <WebsocketHandler.hpp>

// #define ledPin 4
#define RXD2 14
#define TXD2 4
#define DIR_PUBLIC "/"
#define INDEX_PAGE "/2motor.html"
// // We need to specify some content-type mapping, so the resources get delivered with the
// // right content type and are displayed correctly in the browser
char contentTypes[][2][32] = {
    {".html", "text/html"},
    {".css", "text/css"},
    {".js", "application/javascript"},
    {".json", "application/json"},
    {".png", "image/png"},
    {".jpg", "image/jpg"},
    {"", ""}};

// // Robot setup
#include "XL330.h"
XL330 robot;
const int motorOne = 1;
const int motorTwo = 2;
const int broadcast = 254;
int prevM1 = 0;
int prevM2 = 0;
void initRobot(HardwareSerial serial)
{
    // Serial.flush();
    robot.begin(serial);
    robot.TorqueOFF(broadcast);
    delay(100);

    robot.setControlMode(broadcast, 3);
    delay(50);

    robot.TorqueON(254); // Turn on the torque to control the servo
    delay(50);

    // Blink LED as testing
    robot.LEDON(motorOne);
    robot.LEDON(motorTwo);
    delay(500);
    robot.LEDOFF(motorOne);
    robot.LEDOFF(motorTwo);
    delay(50);
}

// // The HTTPS Server comes in a separate namespace. For easier use, include it here.
using namespace httpsserver;

// // We just create a reference to the server here. We cannot call the constructor unless
// // we have initialized the LittleFS and read or created the certificate
HTTPSServer *secureServer;

SSLCert *getCertificate();
void handleLittleFS(HTTPRequest *req, HTTPResponse *res);

void initLittleFS()
{
    if (!LittleFS.begin())
    {
        Serial.println("An Error has occurred while mounting LittleFS");
        return;
    }
    else
    {
        Serial.print("Used memory: ");
        Serial.print(LittleFS.usedBytes());
        Serial.print("/");
        Serial.println(LittleFS.totalBytes());
        return;
    }
}

// // NETWORK SETUP
// // Replace with your network credentials
const char *ssid = "deviceFarm";
const char *password = "device@theFarm";

// // Initialize WiFi
void initWiFi()
{
    // WiFi.softAP(ssid);
    // IPAddress IP = WiFi.softAPIP();
    // Serial.print("AP IP address: ");
    // Serial.println(IP);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.println("Connecting to WiFi..");
    }

    // Print ESP32 Local IP Address
    Serial.print("HTTPS://");
    Serial.print(WiFi.localIP());
    Serial.println(INDEX_PAGE);
}

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

    initLittleFS();

    // Now that LittleFS is ready, we can create or load the certificate
    SSLCert *cert = getCertificate();
    if (cert == NULL)
    {
        Serial.println("Could not load certificate. Stop.");
        while (true)
            ;
    }

    // Connect to WiFi
    initWiFi();

    // Create the server with the certificate we loaded before
    secureServer = new HTTPSServer(cert);

    // We register the LittleFS handler as the default node, so every request that does
    // not hit any other node will be redirected to the file system.
    ResourceNode *LittleFSNode = new ResourceNode("", "", &handleLittleFS);
    secureServer->setDefaultNode(LittleFSNode);

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

    robot.begin(Serial2);
    robot.TorqueOFF(broadcast);
    delay(50);
    robot.setControlMode(broadcast, 3);
    delay(50);
    robot.TorqueON(broadcast);
    delay(50);
}

void loop()
{
    // This call will let the server do its work
    secureServer->loop();

    delay(5);
}

/**
 * This function will either read the certificate and private key from LittleFS or
 * create a self-signed certificate and write it to LittleFS for next boot
 */
SSLCert *getCertificate()
{
    // Try to open key and cert file to see if they exist
    File keyFile = LittleFS.open("/key.der");
    File certFile = LittleFS.open("/cert.der");

    // If now, create them
    if (!keyFile || !certFile || keyFile.size() == 0 || certFile.size() == 0)
    {
        Serial.println("No certificate found in LittleFS, generating a new one for you.");
        Serial.println("If you face a Guru Meditation, give the script another try (or two...).");
        Serial.println("This may take up to a minute, so please stand by :)");

        SSLCert *newCert = new SSLCert();
        // The part after the CN= is the domain that this certificate will match, in this
        // case, it's esp32.local.
        // However, as the certificate is self-signed, your browser won't trust the server
        // anyway.
        int res = createSelfSignedCert(*newCert, KEYSIZE_1024, "CN=esp32.local,O=acme,C=DE");
        if (res == 0)
        {
            // We now have a certificate. We store it on the LittleFS to restore it on next boot.

            bool failure = false;
            // Private key
            keyFile = LittleFS.open("/key.der", FILE_WRITE);
            if (!keyFile || !keyFile.write(newCert->getPKData(), newCert->getPKLength()))
            {
                Serial.println("Could not write /key.der");
                failure = true;
            }
            if (keyFile)
                keyFile.close();

            // Certificate
            certFile = LittleFS.open("/cert.der", FILE_WRITE);
            if (!certFile || !certFile.write(newCert->getCertData(), newCert->getCertLength()))
            {
                Serial.println("Could not write /cert.der");
                failure = true;
            }
            if (certFile)
                certFile.close();

            if (failure)
            {
                Serial.println("Certificate could not be stored permanently, generating new certificate on reboot...");
            }

            return newCert;
        }
        else
        {
            // Certificate generation failed. Inform the user.
            Serial.println("An error occured during certificate generation.");
            Serial.print("Error code is 0x");
            Serial.println(res, HEX);
            Serial.println("You may have a look at SSLCert.h to find the reason for this error.");
            return NULL;
        }
    }
    else
    {
        Serial.println("Reading certificate from LittleFS.");

        // The files exist, so we can create a certificate based on them
        size_t keySize = keyFile.size();
        size_t certSize = certFile.size();

        uint8_t *keyBuffer = new uint8_t[keySize];
        if (keyBuffer == NULL)
        {
            Serial.println("Not enough memory to load privat key");
            return NULL;
        }
        uint8_t *certBuffer = new uint8_t[certSize];
        if (certBuffer == NULL)
        {
            delete[] keyBuffer;
            Serial.println("Not enough memory to load certificate");
            return NULL;
        }
        keyFile.read(keyBuffer, keySize);
        certFile.read(certBuffer, certSize);

        // Close the files
        keyFile.close();
        certFile.close();
        Serial.printf("Read %u bytes of certificate and %u bytes of key from LittleFS\n", certSize, keySize);
        return new SSLCert(certBuffer, certSize, keyBuffer, keySize);
    }
}

/**
 * This handler function will try to load the requested resource from LittleFS's /public folder.
 *
 * If the method is not GET, it will throw 405, if the file is not found, it will throw 404.
 */
void handleLittleFS(HTTPRequest *req, HTTPResponse *res)
{

    // We only handle GET here
    if (req->getMethod() == "GET")
    {
        // Redirect / to /index.html
        std::string reqFile = req->getRequestString() == "/" ? INDEX_PAGE : req->getRequestString();

        // Try to open the file
        std::string filename = std::string(DIR_PUBLIC) + reqFile;

        // Check if the file exists
        if (!LittleFS.exists(filename.c_str()))
        {
            // Send "404 Not Found" as response, as the file doesn't seem to exist
            res->setStatusCode(404);
            res->setStatusText("Not found");
            res->println("404 Not Found");
            return;
        }

        File file = LittleFS.open(filename.c_str());

        // Set length
        res->setHeader("Content-Length", httpsserver::intToString(file.size()));

        // Content-Type is guessed using the definition of the contentTypes-table defined above
        int cTypeIdx = 0;
        do
        {
            if (reqFile.rfind(contentTypes[cTypeIdx][0]) != std::string::npos)
            {
                res->setHeader("Content-Type", contentTypes[cTypeIdx][1]);
                break;
            }
            cTypeIdx += 1;
        } while (strlen(contentTypes[cTypeIdx][0]) > 0);

        // Read the file and write it to the response
        uint8_t buffer[256];
        size_t length = 0;
        do
        {
            length = file.read(buffer, 256);
            res->write(buffer, length);
        } while (length > 0);

        file.close();
    }
    else
    {
        // If there's any body, discard it
        req->discardRequestBody();
        // Send "405 Method not allowed" as response
        res->setStatusCode(405);
        res->setStatusText("Method not allowed");
        res->println("405 Method not allowed");
    }
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
    int posM1 = int(map(atof(b), 0, 360, 0, 4095));
    int posM2 = int(map(atof(g), 0, 360, 0, 4095));

    Serial.print(millis());
    Serial.print(",");
    Serial.print(b);
    Serial.print(",");
    Serial.println(g);
    if (abs(posM1 - prevM1) > 5)
    {
        robot.setJointPosition(motorOne, posM1);
        prevM1 = posM1;
        delay(1);
    }
    if (abs(posM2 - prevM2) > 5)
    {
        robot.setJointPosition(motorTwo, posM2);
        delay(1);
        prevM2 = posM2;
    }
}
