#define DEVICE_VERSION "1.2.6"
#define DEVICE_MANUFACTURER "MDtronix Lab"
#define DEVICE_MODEL "WaterTank Controller: v4.1"

#define test_mode false

#if test_mode
#define STATIC_IP false
#else
#define STATIC_IP true
#endif

#include <ArduinoHA.h>
#include <ESPAsyncWiFiManager.h>
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

#if STATIC_IP
IPAddress _ip = IPAddress(192, 168, 1, 106);
IPAddress _gw = IPAddress(192, 168, 1, 1);
IPAddress _sn = IPAddress(255, 255, 255, 0);
#endif

int Distance, Value;
bool Mode, Pump, MQTT;
String Command;
byte mac[WL_MAC_ADDR_LENGTH];
const char *min_ = "min";
const char *max_ = "max";
const char *thres_ = "threshold";
int get_min(0), get_max(0), get_threshold(0);

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
Timer t;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head><title>WaterTank Setting</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
</head><body>
    <form action="/set">
        <br><br>
        %setvalue%
        <br><br>
        <input type="submit" value="Save">
    </form>
</body></html>)rawliteral";

String processor(const String &var)
{
  if (var == "setvalue")
  {
    String sendValue = "";
    sendValue += "Min: <input type=\"text\" name=\"min\" value=" + String(get_min) + "><br><br>Max: <input type=\"text\" name=\"max\" value=" + String(get_max) + "><br><br>Start at: <input type=\"text\" name=\"threshold\" value=" + String(get_threshold) + " > ";
    return sendValue;
  }
  return String();
}

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
  Serial.println(pump.getName());
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
    if (part1.equals("$minDistance"))
    {
      int i = part2.toInt();
      get_min = i;
    }
    if (part1.equals("$maxDistance"))
    {
      int i = part2.toInt();
      get_max = i;
    }
    if (part1.equals("$startAt"))
    {
      int i = part2.toInt();
      get_threshold = i;
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

void setting_code()
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/html", index_html, processor); });

  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              int min;
              int max;
              int threshold;
              String message;
              if (request->hasParam(min_) && request->hasParam(max_) && request->hasParam(thres_))
              {
                min = request->getParam(min_)->value().toInt();
                max = request->getParam(max_)->value().toInt();
                threshold = request->getParam(thres_)->value().toInt();

                Serial.printf("$minDistance:%d\n", min ? min : get_min);
                Serial.printf("$maxDistance:%d\n", max ? max : get_max);
                Serial.printf("$startAt:%d\n", threshold ? threshold : get_threshold);

                message = "min: ";
                message += String(min) + '\n';
                message += "max: ";
                message += String(max) + '\n';
                message += "start at: ";
                message += String(threshold);
              }
              else
              {
                message = "No message sent";
              }
              Serial.println("$readSetting:1");
              request->send(200, "text/plain", message);
            });
  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              Serial.println("$readSetting:1");
              char data[32];
              sprintf(data, "min: %d\nmax: %d\nstart at: %d\n", get_min, get_max, get_threshold);
              request->send(200, "text/plain", String(data));
            });
  server.begin();
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
  AsyncWiFiManager wifiManager(&server, &dns);
  wifiManager.setTimeout(150);
  wifiManager.setConfigPortalTimeout(120);
#if STATIC_IP
  wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn);
#endif
#if test_mode
  String ssid = "MDtronix-WaterTank-test";
  ssid += String(ESP.getChipId()).c_str();
#else
  String ssid = "MDtronix-WaterTank-";
  ssid += String(ESP.getChipId()).c_str();
#endif

  wifiManager.autoConnect(ssid.c_str());
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ssid);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  setting_code();
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