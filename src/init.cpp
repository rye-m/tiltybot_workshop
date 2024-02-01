#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "XL330.h"
#include <WiFi.h>
#include <tuple>

#include <HTTPSServer.hpp>
#include <SSLCert.hpp>
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>
#include <util.hpp>
#include <WebsocketHandler.hpp>
#include "init.h"

#define INDEX_PAGE "/tilty.html"
using namespace httpsserver;

const int panServo = 1;
const int tiltServo = 2;
const int broadcast = 254;

void initRobot(HardwareSerial &serialPort, XL330 &robot, int mode)
{
    serialPort.flush();
    robot.begin(serialPort);
    robot.TorqueOFF(broadcast);
    delay(50);
    robot.setControlMode(broadcast, mode);
    delay(50);
    robot.TorqueON(broadcast);
    delay(50);

    // Blink LED as testing
    robot.LEDON(panServo);
    robot.LEDON(tiltServo);
    delay(500);
    robot.LEDOFF(panServo);
    robot.LEDOFF(tiltServo);
    delay(50);
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

SSLCert *initLittleFS()
{
    if (!LittleFS.begin())
    {
        Serial.println("An Error has occurred while mounting LittleFS");
    }
    else
    {
        Serial.print("Used memory: ");
        Serial.print(LittleFS.usedBytes());
        Serial.print("/");
        Serial.println(LittleFS.totalBytes());
    }
    SSLCert *cert = getCertificate();
    if (cert == NULL)
    {
        Serial.println("Could not load certificate. Stop.");
        while (true)
            ;
    }
    return cert;
}

// // Initialize WiFi
void initWiFi(const char *ssid, const char *password, const char *index, int mode = AP)
{
    if (mode == AP)
    {
        WiFi.softAP(ssid);
        IPAddress IP = WiFi.softAPIP();
        Serial.print("AP IP address: ");
        Serial.print("HTTPS://");
        Serial.print(IP);
        Serial.println(index);
    }
    else
    {
        WiFi.begin(ssid, password);
        while (WiFi.status() != WL_CONNECTED)
        {
            delay(1000);
            Serial.println("Connecting to WiFi..");
        }

        // Print ESP32 Local IP Address
        Serial.print("HTTPS://");
        Serial.print(WiFi.localIP());
        Serial.println(index);
    }
}

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

HTTPSServer *initServer(SSLCert *cert)
{
    HTTPSServer *secureServer = new HTTPSServer(cert);
    ResourceNode *LittleFSNode = new ResourceNode("", "", &handleLittleFS);
    secureServer->setDefaultNode(LittleFSNode);

    return secureServer;
}

std::tuple<int, int> parseData(WebsocketInputStreambuf *inbuf)
{
    return std::make_tuple(10, 20);
    // Get the input message
    std::ostringstream ss;
    std::string msg;
    ss << inbuf;
    msg = ss.str();
    const size_t capacity = JSON_OBJECT_SIZE(2) + 30;
    DynamicJsonBuffer jsonBuffer(capacity);

    JsonObject &root = jsonBuffer.parseObject(msg.c_str());
    const char *b = root["b"];
    const char *g = root["g"];
    return std::make_tuple(atof(b), atof(g));
}
