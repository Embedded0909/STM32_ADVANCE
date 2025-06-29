#include <FS.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <WebServer.h>

const char *ssid = "Test";
const char *password = "11111111";

WebServer server(80);
File uploadFile;

#define START_BYTE 0xAA
#define ACK 0x79
#define NACK 0x1F
#define BLOCK_SIZE 256

uint16_t simpleCRC(uint8_t *data, int len)
{
    uint16_t crc = 0;
    for (int i = 0; i < len; i++)
    {
        crc += data[i];
    }
    return crc;
}

bool waitForAck(uint32_t timeout = 1000)
{
    uint32_t start = millis();
    while (millis() - start < timeout)
    {
        if (Serial2.available())
        {
            uint8_t resp = Serial2.read();
            return (resp == ACK);
        }
    }
    return false;
}

void handleUpload()
{
    HTTPUpload &upload = server.upload();

    if (upload.status == UPLOAD_FILE_START)
    {
        uploadFile = SPIFFS.open("/firmware.bin", FILE_WRITE);
        if (!uploadFile)
        {
            Serial.println("Cant open file /firmware.bin");
            return;
        }
        Serial.println("Upload...");
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (uploadFile)
            uploadFile.write(upload.buf, upload.currentSize);
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (uploadFile)
        {
            uploadFile.close();
            Serial.println("Upload done.");
        }
    }
}

void sendFirmwareToSTM32()
{
    File file = SPIFFS.open("/firmware.bin", FILE_READ);
    if (!file)
    {
        Serial.println("Cant open /firmware.bin");
        return;
    }

    uint8_t data[BLOCK_SIZE];
    uint8_t packet[BLOCK_SIZE + 5];
    int blockCount = 0;

    while (file.available())
    {
        int len = file.read(data, BLOCK_SIZE);
        uint16_t crc = simpleCRC(data, len);

        packet[0] = START_BYTE;
        packet[1] = (len >> 8) & 0xFF;
        packet[2] = len & 0xFF;
        memcpy(&packet[3], data, len);
        packet[3 + len] = (crc >> 8) & 0xFF;
        packet[4 + len] = crc & 0xFF;

        Serial.printf("packet[0] = %02X packet[1] = %02X packet[2] = %02X crc = %d\n", packet[0], packet[1], packet[2], crc);

        int retry = 3;
        while (retry--)
        {
            Serial2.write(packet, len + 5);
            Serial.printf("Sent block %d, waiting for ACK...\n", blockCount);

            if (waitForAck())
            {
                Serial.printf("Block %d: OK\n", blockCount);
                break;
            }
            else
            {
                Serial.printf("Block %d: NACK or timeout. Retrying...\n", blockCount);
            }
        }

        if (retry < 0)
        {
            Serial.println("Too many failed attempts. Aborting.");
            file.close();
            return;
        }

        blockCount++;
        delay(10);
    }

    packet[0] = START_BYTE;
    packet[1] = 0x00;
    packet[2] = 0x00;
    Serial2.write(packet, 3);

    file.close();
    Serial.println("Finished sending firmware.bin to STM32.");
}

void setup()
{
    Serial.begin(9600);
    Serial2.begin(115200);

    SPIFFS.begin(true);

    WiFi.begin(ssid, password);
    Serial.print("WiFi connecting...");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWiFi connected. IP:");
    Serial.println(WiFi.localIP());

    server.on("/", HTTP_GET, []()
              { server.send(200, "text/html", R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <title>Upload Firmware</title>
      <style>
        body {
          background-color: #f0f0f0;
          font-family: Arial, sans-serif;
          text-align: center;
          padding-top: 50px;
        }
        form {
          background: #fff;
          padding: 20px;
          border-radius: 10px;
          display: inline-block;
          box-shadow: 0 0 10px rgba(0,0,0,0.1);
        }
        input[type="file"] {
          margin-bottom: 15px;
        }
        input[type="submit"], a.button {
          background-color: #4CAF50;
          color: white;
          padding: 10px 20px;
          text-decoration: none;
          border: none;
          border-radius: 5px;
          cursor: pointer;
        }
        a.button {
          display: inline-block;
          margin-top: 15px;
        }
      </style>
    </head>
    <body>
      <form method="POST" action="/upload" enctype="multipart/form-data">
        <h2>OTA - STM32 - ESP32</h2>
        <input type="file" name="file" accept=".bin"><br>
        <input type="submit" value="Upload BIN">
        <br>
        <a href="/send" class="button">Send to STM32</a>
      </form>
    </body>
    </html>
  )rawliteral"); });

    server.on("/upload", HTTP_POST, []()
              { server.send(200, "text/html", R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <title>Upload Firmware</title>
      <style>
        body {
          background-color: #f0f0f0;
          font-family: Arial, sans-serif;
          text-align: center;
          padding-top: 50px;
        }
        form {
          background: #fff;
          padding: 20px;
          border-radius: 10px;
          display: inline-block;
          box-shadow: 0 0 10px rgba(0,0,0,0.1);
        }
        input[type="file"] {
          margin-bottom: 15px;
        }
        input[type="submit"], a.button {
          background-color: #4CAF50;
          color: white;
          padding: 10px 20px;
          text-decoration: none;
          border: none;
          border-radius: 5px;
          cursor: pointer;
        }
        a.button {
          display: inline-block;
          margin-top: 15px;
        }
      </style>
    </head>
    <body>
      <form method="POST" action="/upload" enctype="multipart/form-data">
        <h2>OTA - STM32 - ESP32</h2>
        <a href="/send" class="button">Send to STM32</a>
      </form>
    </body>
    </html>
  )rawliteral"); }, handleUpload);

    server.on("/send", HTTP_GET, []()
              {
    server.send(200, "text/plain", "Sending firmware.bin to STM32...");
    sendFirmwareToSTM32(); });

    server.begin();
}

void loop()
{
    server.handleClient();
}
