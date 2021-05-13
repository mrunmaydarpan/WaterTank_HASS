#define DEVICE_VERSION "1.2.3"
#define DEVICE_NAME "MDtronix WaterTank"
#define DEVICE_MANUFACTURER "MDtronix Lab"
#define DEVICE_MODEL "WaterTank Controller: v4.1"

#include <ArduinoHA.h>
#include <ESPAsyncWiFiManager.h>

#define BROKER_ADDR IPAddress(192, 168, 1, 100)
#define BROKER_USER "hassio"
#define BROKER_PASS "darkmaster"

AsyncWebServer server(80);
DNSServer dns;
WiFiClient client;
HADevice device;
HAMqtt mqtt(client, device);
HASwitch pump("Pump", false);
HASensor value("Level");
HASensor distance("Distance");
HASensor mode("Mode");
HASensor sensor_error("System");

int Distance;
int Value;
bool Mode;
bool Pump;
String Command;

void pump_action(bool state, HASwitch *s)
{
  Serial.printf("$Pump:%d\n", state);
}
void set_state()
{
  value.setValue(Value);
  distance.setValue(Distance);
  mode.setValue(Mode ? "Auto" : "Manual");
}

void set_device()
{
  byte mac[WL_MAC_ADDR_LENGTH];
  WiFi.macAddress(mac);
  device.setUniqueId(mac, sizeof(mac)); // Unique ID must be set!
  device.setName(DEVICE_NAME);
  device.setSoftwareVersion(DEVICE_VERSION);
  device.setManufacturer(DEVICE_MANUFACTURER);
  device.setModel(DEVICE_MODEL);
  device.enableSharedAvailability();
  device.enableLastWill();
  pump.onStateChanged(pump_action); // handle switch state
  pump.setName("Pump");
  pump.setIcon("mdi:water");
  value.setName("Water Level");
  value.setIcon("mdi:waves");
  value.setUnitOfMeasurement("%");
  distance.setName("Distance");
  distance.setIcon("mdi:ruler");
  distance.setUnitOfMeasurement("cm");
  mode.setName("Mode");
  mode.setIcon("mdi:nintendo-switch");
  sensor_error.setName("WaterTank Sensor");
}

void parseCommand(String com)
{
  if (com.startsWith("$"))
  {
    String part1 = com.substring(0, com.indexOf(":"));
    String part2 = com.substring(com.indexOf(":") + 1);

    if (part1.equals("$Pump"))
    {
      int i = part2.toInt();
      pump.setState(i);
      Pump = i;
    }
    if (part1.equals("$Level"))
    {
      int i = part2.toInt();
      value.setValue(i);
      Value = i;
    }
    if (part1.equals("$Mode"))
    {
      int i = part2.toInt();
      mode.setValue(i ? "Auto" : "Manual");
      Mode = i;
    }
    if (part1.equals("$Distance"))
    {
      int i = part2.toInt();
      distance.setValue(i);
      Distance = i;
    }
    if (part1.equals("$System"))
    {
      int8_t i = part2.toInt();
      sensor_error.setValue(i ? "Error" : "Ok");
    }
  }
}

void getData()
{
  if (Serial.available())
  {
    char data = Serial.read();
    if (data == '\n')
    {
      parseCommand(Command);
      Command = "";
    }
    else
    {
      Command += data;
    }
  }
}

void setup()
{
  Serial.begin(9600);
  AsyncWiFiManager wifiManager(&server, &dns);
  wifiManager.setTimeout(150);
  wifiManager.setConfigPortalTimeout(120);
  wifiManager.autoConnect("WaterTank");
  Serial.println();
  Serial.println("Connected to the network");
  set_device();
  mqtt.begin(BROKER_ADDR, BROKER_USER, BROKER_PASS);
}

void loop()
{
  mqtt.loop();
  getData();
}