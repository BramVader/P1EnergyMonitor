// Select either one of the target boards:
#define ESP01
//#define ESPDUINO

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "time.h"

#ifdef ESPDUINO
#include <SoftwareSerial.h>
#define rxPin 13
#define P1SERIAL softSerial
#define LED LED_BUILTIN
#endif

#ifdef ESP01
#define P1SERIAL Serial
#define LED 1
#endif

#define LINESIZE 75

// Replace with your network credentials
const char *ssid = "*****";
const char *password = "*****";

#ifdef ESPDUINO
SoftwareSerial softSerial;
#endif

ESP8266WiFiMulti wiFiMulti;
ESP8266WebServer server(80);

typedef word Int16;
typedef int Int32;
typedef char Int8;

struct MeterData
{
    Int32 timestamp;      // Unix epoch time
    Int32 tariff1In;      // In Wh
    Int32 tariff2In;      // In Wh
    Int32 tariff1Out;     // In Wh
    Int32 tariff2Out;     // In Wh
    Int8 tariffType;      // 1 or 2
    Int16 actualPowerOut; // In 0.1W
    Int16 actualPowerIn;  // In 0.1W
    Int16 voltage;        // In 0.1V
    Int16 current;        // In 0.1A
    Int16 activePowerOut; // In 0.1W
    Int16 activePowerIn;  // In 0.1W
    Int32 gasMeter;       // In liter
};

// NOTE: Absolute meter values are stored as deltas to previous sample
// Use the most current sample to calculate the absolute values of the previous sample by applying its deltas to IntervalBuffer.header,
// Do this repeatedly for the older samples.
struct IntervalData
{
    Int16 tariff1In;      // Δ in Wh
    Int16 tariff2In;      // Δ in Wh
    Int16 tariff1Out;     // Δ in Wh
    Int16 tariff2Out;     // Δ in Wh
    Int8 tariffType;      // 1 or 2
    Int16 actualPowerOut; // In 0.1W
    Int16 actualPowerIn;  // In 0.1W
    Int16 voltage;        // In 0.1V
    Int16 current;        // In 0.1A
    Int16 activePowerOut; // In 0.1W
    Int16 activePowerIn;  // In 0.1W
    Int16 gasMeter;       // Δ in liter
};

struct IntervalBuffer
{
    int interval;         // How many seconds between samples
    int duration;         // The duration of the buffer in seconds
    int samples;          // The number of samples in the buffer (=duration/interval)
    int index;            // The current index in the (rotary) buffer
    int count;            // The number
    MeterData header;     // Actual value
    IntervalData *values; // Historical values
};

struct JsonContext
{
    String stream = String();
    int indent = 0;
    bool first = true;
};

// Convenient time conversion constants
const int S = 1;
const int M = 60 * S;
const int H = 60 * M;
const int D = 24 * H;
const int W = 7 * D;

// Intervals, durations and sizes of ringbuffers
IntervalBuffer buffers[4];

void initBuffers()
{
    buffers[0].interval = 1 * S;
    buffers[0].duration = 15 * M;

    buffers[1].interval = 1 /*15*/ * M;
    buffers[1].duration = 1 * W;

    buffers[2].interval = 1 /*4*/ * H;
    buffers[2].duration = 4 * W;

    buffers[3].interval = 1 * D;
    buffers[3].duration = 1 /*54*/ * W;

    for (int i = 0; i < 1; i++)
    {
        buffers[i].samples = buffers[i].duration / buffers[i].interval;
        buffers[i].values = new IntervalData[buffers[i].samples];
        buffers[i].index = 0;
        buffers[i].count = 0;
    }
}

void updateBuffer(IntervalBuffer &buffer, MeterData &data)
{
    noInterrupts();

    /*

        IntervalData &interval = buffer.values[buffer.index];

        // Set delta values
        interval.tariff1In = (Int16)(data.tariff1In - buffer.header.tariff1In);    // Δ in Wh
        interval.tariff2In = (Int16)(data.tariff2In - buffer.header.tariff2In);    // Δ in Wh
        interval.tariff1Out = (Int16)(data.tariff1Out - buffer.header.tariff1Out); // Δ in Wh
        interval.tariff2Out = (Int16)(data.tariff2Out - buffer.header.tariff2Out); // Δ in Wh
        interval.gasMeter = (Int16)(data.gasMeter - buffer.header.gasMeter);       // Δ in liter

        // Set absolute values
        interval.tariffType = data.tariffType;         // 1 or 2
        interval.actualPowerOut = data.actualPowerOut; // In 0.1W
        interval.actualPowerIn = data.actualPowerIn;   // In 0.1W
        interval.voltage = data.voltage;               // In 0.1V
        interval.current = data.current;               // In 0.1A
        interval.activePowerOut = data.activePowerOut; // In 0.1W
        interval.activePowerIn = data.activePowerIn;   // In 0.1W

    */

    // Set the header values
    buffer.header.tariff1In = data.tariff1In;
    buffer.header.tariff2In = data.tariff2In;
    buffer.header.tariff1Out = data.tariff1Out;
    buffer.header.tariff2Out = data.tariff2Out;
    buffer.header.gasMeter = data.gasMeter;
    buffer.header.tariffType = data.tariffType;
    buffer.header.actualPowerOut = data.actualPowerOut;
    buffer.header.actualPowerIn = data.actualPowerIn;
    buffer.header.voltage = data.voltage;
    buffer.header.current = data.current;
    buffer.header.activePowerOut = data.activePowerOut;
    buffer.header.activePowerIn = data.activePowerIn;

    buffer.index = (buffer.index + 1) % buffer.samples;
    if (buffer.count < buffer.samples)
        buffer.count++;

    interrupts();
}

void getActualValues(MeterData *data)
{
    noInterrupts();
    memcpy(data, &buffers[0].header, sizeof(MeterData));
    interrupts();
}

void process(MeterData &data, bool loggingEnabled)
{
    if (loggingEnabled)
    {
        Serial.write(0x0C); // Clear screen
        Serial.println(" -----------------------------------------");
        Serial.print("timestamp      :");
        Serial.println(data.timestamp);
        Serial.print("tariff1In      :");
        Serial.println(data.tariff1In);
        Serial.print("tariff2In      :");
        Serial.println(data.tariff2In);
        Serial.print("tariff1Out     :");
        Serial.println(data.tariff1Out);
        Serial.print("tariff2Out     :");
        Serial.println(data.tariff2Out);
        Serial.print("tariffType     :");
        Serial.println(data.tariffType);
        Serial.print("actualPowerOut :");
        Serial.println(data.actualPowerOut);
        Serial.print("actualPowerIn  :");
        Serial.println(data.actualPowerIn);
        Serial.print("voltage        :");
        Serial.println(data.voltage);
        Serial.print("current        :");
        Serial.println(data.current);
        Serial.print("activePowerOut :");
        Serial.println(data.activePowerOut);
        Serial.print("activePowerIn  :");
        Serial.println(data.activePowerIn);
        Serial.print("gasMeter       :");
        Serial.println(data.gasMeter);
    }
    updateBuffer(buffers[0], data);
}

// Convert time in "YYMMDDhhmmssX" format to epoch time
// X = 'S': DST active
// X = 'W': DST not active
long timeStrToEpoch(char timestr[])
{
    int v[6];
    for (int i = 0; i < 6; i++)
    {
        v[i] = (timestr[i * 2] - '0') * 10 + timestr[i * 2 + 1] - '0';
    }
    struct tm timeinfo;
    timeinfo.tm_sec = v[5];
    timeinfo.tm_min = v[4];
    timeinfo.tm_hour = v[3];
    timeinfo.tm_mday = v[2];
    timeinfo.tm_mon = v[1] - 1;    // Month is 0-based
    timeinfo.tm_year = v[0] + 100; // 0..99 offset from 1900
    timeinfo.tm_isdst = timestr[12] == 'S';
    return mktime(&timeinfo);
}

int whl = 0;   // Whole part of a number
int frct = 0;  // Fractional part of a number
char date[13]; // A date string YYMMDDhhmmssX (where X="S" (DST active) or "W" (not active)

void parseLine(MeterData &data, char line[])
{
    if (sscanf(line, "0-0:1.0.0(%13c)", (char *)date)) //	Date-time stamp of the P1 message
    {
        data.timestamp = timeStrToEpoch(date);
    }
    else if (sscanf(line, "1-0:1.8.1(%d.%d", &whl, &frct)) //	Meter Reading electricity delivered to client (Tariff 1) in 0,001 kWh
    {
        data.tariff1In = whl * 1000 + frct;
    }
    else if (sscanf(line, "1-0:1.8.2(%d.%d", &whl, &frct)) // Meter Reading electricity delivered to client (Tariff 2) in 0,001 kWh
    {
        data.tariff2In = whl * 1000 + frct;
    }
    else if (sscanf(line, "1-0:2.8.1(%d.%d", &whl, &frct)) // Meter Reading electricity delivered by client (Tariff 1) in 0,001 kWh
    {
        data.tariff1Out = whl * 1000 + frct;
    }
    else if (sscanf(line, "1-0:2.8.2(%d.%d", &whl, &frct)) // Meter Reading electricity delivered by client (Tariff 2) in 0,001 kWh
    {
        data.tariff2Out = whl * 1000 + frct;
    }
    else if (sscanf(line, "0-0:96.14.0(%d", &whl)) // Tariff indicator electricity. The tariff indicator can also be used to switch tariff dependent loads e.g boilers. This is the responsibility of the P1 user
    {
        data.tariffType = whl;
    }
    else if (sscanf(line, "1-0:1.7.0(%d.%d", &whl, &frct)) // Actual electricity power delivered (+P) in 1 Watt resolution
    {
        data.actualPowerIn = whl * 10 + frct;
    }
    else if (sscanf(line, "1-0:2.7.0(%d.%d", &whl, &frct)) // Actual electricity power received (-P) in 1 Watt resolution
    {
        data.actualPowerOut = whl * 10 + frct;
    }
    else if (sscanf(line, "1-0:32.7.0(%d.%d", &whl, &frct)) // Instantaneous voltage L1 in V resolution
    {
        data.voltage = whl * 10 + frct;
    }
    else if (sscanf(line, "1-0:31.7.0(%d", &whl)) // Instantaneous current L1 in A resolution.
    {
        data.current = whl * 10;
    }
    else if (sscanf(line, "1-0:21.7.0(%d.%d", &whl, &frct)) // Instantaneous active power L1 (+P) in W resolution
    {
        data.activePowerIn = whl * 10 + frct;
    }
    else if (sscanf(line, "1-0:22.7.0(%d.%d", &whl, &frct)) // Instantaneous active power L1 (-P) in W resolution
    {
        data.activePowerOut = whl * 10 + frct;
    }
    else if (sscanf(line, "0-1:24.2.1(%13c)(%d.%d", (char *)date, &whl, &frct)) // Last 5-minute value (temperature converted), gas delivered to client in m3, including decimal values and capture time
    {
        data.gasMeter = whl * 1000 + frct;
    }
}

void printJsonName(JsonContext &context, const char *name)
{
    if (!context.first)
        context.stream.concat(",\n");
    for (int i = 0; i < context.indent; i++)
        context.stream.concat("\t");
    context.stream.concat("\"");
    context.stream.concat(name);
    context.stream.concat("\": ");
    context.first = false;
}

void printJsonValue(JsonContext &context, const char *name, int value)
{
    printJsonName(context, name);
    context.stream.concat(value);
}

void printJsonValue(JsonContext &context, const char *name, bool value)
{
    printJsonName(context, name);
    context.stream.concat(value ? "true" : "false");
}

void handleGetRealtimeInfo()
{
    JsonContext context;
    context.stream = String();

    MeterData data;
    getActualValues(&data);

    context.stream.concat("{\n");
    context.indent++;
    printJsonValue(context, "timestamp", data.timestamp);
    printJsonValue(context, "tariff1In", data.tariff1In);
    printJsonValue(context, "tariff2In", data.tariff2In);
    printJsonValue(context, "tariff1Out", data.tariff1Out);
    printJsonValue(context, "tariff2Out", data.tariff2Out);
    printJsonValue(context, "tariffType", (bool)data.tariffType);
    printJsonValue(context, "actualPowerOut", data.actualPowerOut);
    printJsonValue(context, "actualPowerIn", data.actualPowerIn);
    printJsonValue(context, "voltage", data.voltage);
    printJsonValue(context, "current", data.current);
    printJsonValue(context, "activePowerOut", data.activePowerOut);
    printJsonValue(context, "activePowerIn", data.activePowerIn);
    printJsonValue(context, "gasMeter", data.gasMeter);

    context.stream.concat("\n}\n");

    server.send(200, "text/plain", context.stream);
}

// Linebuffer
char line[LINESIZE];
int pos = 0;
MeterData data1;
long t1 = 0, t2;
bool dataHandled = false;
bool loggingEnabled = true;
bool connected = false;

void setup()
{
    initBuffers();

    pinMode(LED, OUTPUT);
    Serial.begin(115200);

    // Setup WiFi
    for (int t = 4; t > 0; t--)
    {
        Serial.printf("[WIFI] WAIT %d...\n", t);
        Serial.flush();
        delay(1000);
    }
    WiFi.mode(WIFI_STA);
    wiFiMulti.addAP("Lindendreef117", "huiswerk");

    // We use the serial port for receiving from P1 port of E-meter

#ifdef ESPDUINO
    pinMode(rxPin, INPUT);
    softSerial.begin(115200, SWSERIAL_8N1, rxPin, -1, true);
    if (!softSerial)
    {
        // If the object did not initialize its configuration is invalid
        Serial.println("[SERIAL] ]Invalid SoftwareSerial pin configuration, check config");
        while (1)
        { // Don't continue with invalid configuration
            delay(1000);
        }
    }
#endif
}

void loop()
{
    // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
    if (wiFiMulti.run() == WL_CONNECTED)
    {
        if (!connected)
        {
            Serial.print("[WIFI] Connected to: ");
            Serial.println(WiFi.SSID());
            Serial.print("[WIFI] IP address: ");
            Serial.println(WiFi.localIP());

            server.on("/", HTTP_GET, handleGetRealtimeInfo);

            if (MDNS.begin("p1mon"))
            { // Start the mDNS responder for p1mon.local
                Serial.println("mDNS responder started");
            }
            else
            {
                Serial.println("Error setting up MDNS responder!");
            }

            server.begin();
            Serial.println("[HTTP] HTTP server started");
            connected = true;
        }
    }
    else
    {
        if (connected)
        {
            Serial.println("[WIFI] Disconnected!");
        }
        connected = false;
    }

    if (connected)
    {
        server.handleClient();
    }

    // Toggle logging
    if (Serial.available())
    {
        char c = (char)Serial.read();
        if (c == 't')
        {
            loggingEnabled = !loggingEnabled;
            Serial.print("Logging enabled: ");
            Serial.println(loggingEnabled);
        }
    }

    t2 = (long)millis();
    long tdiff = t2 - t1;

    if (tdiff > 200 && !dataHandled)
    { // Delay >20ms? Then entire telegram has passed

        digitalWrite(LED, HIGH);
        process(data1, loggingEnabled);
        dataHandled = true;
        digitalWrite(LED, LOW);
    }
    if (P1SERIAL.available())
    {
        t1 = t2;
        dataHandled = false;

        char input = (char)P1SERIAL.read() & 0x7F;
        line[pos++] = input;

        if (input == '\n' || pos >= LINESIZE)
        {
            parseLine(data1, line);
            pos = 0;
        }
    }
}
