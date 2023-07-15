/*
 * СЕРВЕР
 * СВЯЗЬ ПО СЕТИ: меш-сеть (painlessMesh)
 * ФУНКЦИОНАЛ: 1. Принимает данные от датчиков и выводит их
 *             2. Смотрит зоны громкого звука, группирует подряд идущие
 */

#include "painlessMesh.h"
#include <Arduino_JSON.h>
//#include "VolAnalyzer.h"

#include <PubSubClient.h>
#include <WiFiClient.h>

#define   MESH_PREFIX     "EspMicrophoneMesh"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

#define   STATION_SSID     "OpenWRT"
#define   STATION_PASSWORD "1357924680"

#define HOSTNAME "MQTT_Bridge"

const int NODES_COUNT = 4;
unsigned int net_state[NODES_COUNT][2];

unsigned long last_sent = 0;

//Scheduler     userScheduler;
IPAddress getlocalIP();

IPAddress myIP(0,0,0,0);
IPAddress mqttBroker(192, 168, 0, 107);

// Prototypes
//void receivedCallback( const uint32_t &from, const String &msg );
void mqttCallback(char* topic, byte* payload, unsigned int length);

painlessMesh  mesh;
WiFiClient wifiClient;
PubSubClient mqttClient(mqttBroker, 1883, mqttCallback, wifiClient);
//
//void newConnectionCallback(uint32_t nodeId) {
//  Serial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
//}
//
//void changedConnectionCallback() {
//  Serial.printf("Changed connections\n");
//}
//
//void nodeTimeAdjustedCallback(int32_t offset) {
//  Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(),offset);
//}

void setup() {
  Serial.begin(115200);

  mesh.setDebugMsgTypes( ERROR | STARTUP | CONNECTION );  // set before init() so that you can see startup messages

  // Channel set to 6. Make sure to use the same channel for your mesh and for you other
  // network (STATION_SSID)
  mesh.init( MESH_PREFIX, MESH_PASSWORD, MESH_PORT, WIFI_AP_STA, 1 );
  mesh.onReceive(&receivedCallback);

  mesh.stationManual(STATION_SSID, STATION_PASSWORD);
  mesh.setHostname(HOSTNAME);

  // Bridge node, should (in most cases) be a root node. See [the wiki](https://gitlab.com/painlessMesh/painlessMesh/wikis/Possible-challenges-in-mesh-formation) for some background
  mesh.setRoot(true);
  // This node and all other nodes should ideally know the mesh contains a root, so call this on all nodes
  mesh.setContainsRoot(true);
  
//  Serial.begin(115200);
//
//  mesh.setDebugMsgTypes( ERROR | STARTUP | CONNECTION );  // set before init() so that you can see startup messages
//
//  mesh.init( MESH_PREFIX, MESH_PASSWORD, MESH_PORT, WIFI_AP_STA, 1 );
//  mesh.onReceive(&receivedCallback);
//  
//  mesh.stationManual(STATION_SSID, STATION_PASSWORD);
//  mesh.setHostname(HOSTNAME);
  
//  mesh.onNewConnection(&newConnectionCallback);
//  mesh.onChangedConnections(&changedConnectionCallback);
//  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

  // Bridge node, should (in most cases) be a root node. See [the wiki](https://gitlab.com/painlessMesh/painlessMesh/wikis/Possible-challenges-in-mesh-formation) for some background
//  mesh.setRoot(true);
  // This node and all other nodes should ideally know the mesh contains a root, so call this on all nodes
//  mesh.setContainsRoot(true);
}

void loop() {
  mesh.update();
  mqttClient.loop();

  if(myIP != getlocalIP()){
    myIP = getlocalIP();
    Serial.println("My IP is " + myIP.toString());

    if (mqttClient.connect("painlessMeshClient")) {
      mqttClient.publish("painlessMesh/from/gateway","Ready!");
      mqttClient.subscribe("painlessMesh/to/#");
    } 
  }
  
  String msg = "There is a loud noise in areas of sensors: ";
  bool changed = false;
  boolean is_area = false;
  for (int i=0; i<NODES_COUNT; i++){
    uint32_t time_passed = 0;
    uint32_t my_time = mesh.getNodeTime();
    //min = 60 000 000 microsec
    if (my_time<net_state[i][1]){    time_passed = my_time+(71*60000000-net_state[i][1]);    }
    else {    time_passed = my_time-net_state[i][1];    }
    if (net_state[i][0]>0 && time_passed>=30000000){
      net_state[i][0] = 0;
      String topic = "painlessMesh/node" + String(i+1) + "/status";
      mqttClient.publish(topic.c_str(), "0");
//      String topic2 = "painlessMesh/node" + String(i+1) + "/time";
//      char cstr_my_time[16];
//      itoa(my_time, cstr_my_time, 10);
//      mqttClient.publish(topic.c_str(), cstr_my_time);
    }
    if (net_state[i][0]>0){
      if (!is_area){
        msg += "(";
        is_area = true;
        changed = true;
      }
      msg += " " + String(i+1);
    }
    else {
      if (is_area){
        msg += " )";
        is_area = false;
      }
    }
    if (is_area && i==NODES_COUNT-1){
      msg += " )";
      is_area = false;
    }
  }
  if (changed && (millis()-last_sent>=100)){
    last_sent = millis();
    Serial.println(msg);
  }
}

void receivedCallback( const uint32_t from, const String &msg ) {
  Serial.printf("Received from %u msg=%s\n", from, msg.c_str());
  JSONVar myObject = JSON.parse(msg.c_str());
  int node = myObject["node"];
  int sound = myObject["sound"];
  unsigned long node_time = myObject["time"];
  Serial.print("Node: ");
  Serial.print(node);
  Serial.print("; Sound amplitude: ");
  Serial.print(sound);
  Serial.print("; Time: ");
  Serial.println(node_time);

  net_state[node-1][0] = sound; net_state[node-1][1] = node_time;

  char cstr_sound[16];
  itoa(sound, cstr_sound, 10);
  char cstr_time[16];
  itoa(node_time, cstr_time, 10);

  String topic = "painlessMesh/node" + String(node) + "/status";
  mqttClient.publish(topic.c_str(), cstr_sound);
//  String topic2 = "painlessMesh/node" + String(node) + "/time";
//  mqttClient.publish(topic.c_str(), cstr_time);
}

void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
  char* cleanPayload = (char*)malloc(length+1);
  payload[length] = '\0';
  memcpy(cleanPayload, payload, length+1);
  String msg = String(cleanPayload);
  free(cleanPayload);

  String targetStr = String(topic).substring(16);

  if(targetStr == "gateway")
  {
    if(msg == "getNodes")
    {
      auto nodes = mesh.getNodeList(true);
      String str;
      for (auto &&id : nodes)
        str += String(id) + String(" ");
      mqttClient.publish("painlessMesh/from/gateway", str.c_str());
    }
  }
  else if(targetStr == "broadcast") 
  {
    mesh.sendBroadcast(msg);
  }
  else
  {
    uint32_t target = strtoul(targetStr.c_str(), NULL, 10);
    if(mesh.isConnected(target))
    {
      mesh.sendSingle(target, msg);
    }
    else
    {
      mqttClient.publish("painlessMesh/from/gateway", "Client not connected!");
    }
  }
}

IPAddress getlocalIP() {
  return IPAddress(mesh.getStationIP());
}
