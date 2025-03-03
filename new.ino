#include <WiFi.h>
#include <WebSocketsClient.h>
#include <NimBLEDevice.h>

const char* ssid = " #wifi name ";
const char* password = " #wifi pass ";
const char* websocket_server = "10.17.65.5";
const uint16_t websocket_port = 8080;

WebSocketsClient webSocket;
NimBLEServer* server;
NimBLECharacteristic* announcementCharacteristic;
NimBLECharacteristic* flightNumberCharacteristic;
String registeredFlightNumber;  // Flight number entered by the user
bool deviceConnected = false;

class MyServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        deviceConnected = true;
        Serial.println(F("Device connected"));
    }

    void onDisconnect(NimBLEServer* pServer) {
        deviceConnected = false;
        Serial.println(F("Device disconnected"));
        server->getAdvertising()->start();
    }
};

// Callback for handling flight number input from LightBlue
class FlightNumberCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) {
        registeredFlightNumber = pCharacteristic->getValue().c_str();
        Serial.println("Flight number registered: " + registeredFlightNumber);
    }
};

// Function to extract the flight number from the WebSocket message
String extractFlightNumber(String message) {
    int index = message.indexOf("Flight number:");
    if (index != -1) {
        String flightNumber = message.substring(index + 14);  // Extract substring after "Flight number:"
        flightNumber.trim();  // Remove any leading or trailing whitespace
        return flightNumber;
    }
    return "";
}

void onWebSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    if (type == WStype_TEXT) {
        String announcement = String((char*)payload);  // Convert payload to String
        Serial.println("Received announcement: " + announcement);

        // Extract the flight number from the announcement
        String announcementFlightNumber = extractFlightNumber(announcement);
        Serial.println("Extracted flight number: " + announcementFlightNumber);

        // Check if the announcement's flight number matches the registered flight number
        if (announcementFlightNumber == registeredFlightNumber) {
            if (deviceConnected) {
                Serial.println("Device is connected via Bluetooth");
                announcementCharacteristic->setValue((uint8_t*)announcement.c_str(), announcement.length());
                announcementCharacteristic->notify();
                Serial.println("Announcement sent via Bluetooth: " + announcement);
            } else {
                Serial.println("No Bluetooth device connected; cannot send notification.");
            }
        } else {
            Serial.println("Flight number mismatch. Announcement not sent.");
        }
    } else {
        Serial.println("Received non-text WebSocket message.");
    }
}

void connectToWiFi() {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println(F("Connecting to WiFi..."));
    }
    Serial.println(F("Connected to WiFi"));
}

void setup() {
    Serial.begin(115200);

    // Connect to Wi-Fi
    connectToWiFi();

    // Initialize WebSocket
    webSocket.begin(websocket_server, websocket_port, "/");
    webSocket.onEvent(onWebSocketEvent);

    // Initialize Bluetooth
    NimBLEDevice::init("Airport Announcer");
    server = NimBLEDevice::createServer();
    server->setCallbacks(new MyServerCallbacks());
    NimBLEService* announcementService = server->createService(NimBLEUUID((uint16_t)0x181C));
    
    // Announcement characteristic for notifications
    announcementCharacteristic = announcementService->createCharacteristic(
        NimBLEUUID((uint16_t)0x2A3D),
        NIMBLE_PROPERTY::NOTIFY
    );
    
    // Flight number characteristic to accept flight number input
    flightNumberCharacteristic = announcementService->createCharacteristic(
        NimBLEUUID((uint16_t)0x2A90), // UUID for flight number characteristic
        NIMBLE_PROPERTY::WRITE
    );
    flightNumberCharacteristic->setCallbacks(new FlightNumberCallback());
    
    announcementService->start();
    server->getAdvertising()->start();
    Serial.println(F("Waiting for clients to connect..."));
}

void loop() {
    webSocket.loop();
    if (WiFi.status() != WL_CONNECTED) {
        connectToWiFi();  // Reconnect if WiFi connection is lost
    }
}
