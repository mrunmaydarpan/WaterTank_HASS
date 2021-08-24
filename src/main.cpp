#define DEVICE_VERSION "1.3.0"
#define DEVICE_MANUFACTURER "MDtronix Lab"
#define DEVICE_MODEL "WaterTank Controller: v4.1"

#define test_mode false
#define debug_mode true

#include <ArduinoHA.h>
#include <WiFiManager.h>
#include <Timer.h>

#define BROKER_ADDR IPAddress(192, 168, 1, 100)
#define BROKER_USER "hassio"
#define BROKER_PASS "darkmaster"

#if test_mode
#define DEVICE_NAME "MDtronix WaterTank test"
#define pump_name "WaterTank Pump test"
#define mode_name "WaterTank Mode test"
#define distance_name "WaterTank Distance test"
#define level_name "WaterTank Level test"
#define SensorError_name "WaterTank Sensor test"
#else
#define DEVICE_NAME "MDtronix WaterTank"
#define pump_name "WaterTank Pump"
#define mode_name "WaterTank Mode"
#define distance_name "WaterTank Distance"
#define level_name "WaterTank Level"
#define SensorError_name "WaterTank Sensor"
#endif

int Distance, Value;
bool Mode, Pump, MQTT;
String Command;
byte mac[WL_MAC_ADDR_LENGTH];

WiFiClient client;
HADevice device;
HAMqtt mqtt(client, device);
HASwitch pump("Pump", false);
HASensor value("Level");
HASensor distance("Distance");
HASensor mode("Mode");
HASensor sensor_error("System");

Timer t;

void pump_action(bool state, HASwitch *s)
{
  Serial.printf("$Pump:%d\n", state);
}

void set_device()
{
  WiFi.macAddress(mac);
  device.setUniqueId(mac, sizeof(mac)); // Unique ID must be set!
  device.setName(DEVICE_NAME);
  device.setSoftwareVersion(DEVICE_VERSION);
  device.setManufacturer(DEVICE_MANUFACTURER);
  device.setModel(DEVICE_MODEL);
  device.enableSharedAvailability();
  device.enableLastWill();
  pump.onStateChanged(pump_action); // handle switch state
  pump.setName(pump_name);
  pump.setIcon("mdi:water");
  value.setName(level_name);
  value.setIcon("mdi:waves");
  value.setUnitOfMeasurement("%");
  distance.setName(distance_name);
  distance.setIcon("mdi:ruler");
  distance.setUnitOfMeasurement("cm");
  mode.setName(mode_name);
  mode.setIcon("mdi:nintendo-switch");
  sensor_error.setName(SensorError_name);
#if debug_mode
  Serial.println(pump.getName());
#endif
}

void sendData()
{
  Serial.printf("$wifi:%d\n", MQTT);
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

void on_mqtt_success()
{
  Serial.println("$wifi:1");
  MQTT = true;
}

void on_mqtt_failed()
{
  Serial.println("$wifi:0");
  MQTT = false;
}

void setup()
{
  delay(1000);
  Serial.begin(9600);
  WiFiManager wifiManager;
  wifiManager.setTimeout(150);
#if test_mode
  String ssid = "MDtronix-WaterTank-test-";
  ssid += String(ESP.getChipId()).c_str();
#else
  String ssid = "MDtronix-WaterTank-";
  ssid += String(ESP.getChipId()).c_str();
#endif
  wifiManager.autoConnect(ssid.c_str(), "12345678");
  Serial.println("connected");
  set_device();
  mqtt.begin(BROKER_ADDR, BROKER_USER, BROKER_PASS);
  mqtt.onConnected(on_mqtt_success);
  mqtt.onConnectionFailed(on_mqtt_failed);
  t.every(5000, sendData);
}

void loop()
{
  mqtt.loop();
  getData();
  t.update();
}