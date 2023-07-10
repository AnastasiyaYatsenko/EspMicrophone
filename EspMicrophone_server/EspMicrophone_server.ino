/*
 * СЕРВЕР
 * СВЯЗЬ ПО СЕТИ: меш-сеть (painlessMesh)
 * ФУНКЦИОНАЛ: 1. Принимает данные от датчиков и выводит их
 *             2. Смотрит зоны громкого звука, группирует подряд идущие
 */

#include "painlessMesh.h"
#include <Arduino_JSON.h>
#include "VolAnalyzer.h"

#define   MESH_PREFIX     "EspMicrophoneMesh"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

const int NODES_COUNT = 4;
unsigned int net_state[NODES_COUNT][2];

unsigned long last_sent = 0;

Scheduler     userScheduler;
painlessMesh  mesh;

void receivedCallback( uint32_t from, String &msg ) {
  Serial.printf("Received from %u msg=%s\n", from, msg.c_str());
  JSONVar myObject = JSON.parse(msg.c_str());
  int node = myObject["node"];
  double sound = myObject["sound"];
  double node_time = myObject["time"];
  Serial.print("Node: ");
  Serial.print(node);
  Serial.print("; Sound amplitude: ");
  Serial.print(sound);
  Serial.print("; Time: ");
  Serial.println(node_time);

  net_state[node-1][0] = sound; net_state[node-1][0] = node_time;
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.printf("Changed connections\n");
}

void nodeTimeAdjustedCallback(int32_t offset) {
  Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(),offset);
}

void setup() {
  Serial.begin(115200);

  mesh.setDebugMsgTypes( ERROR | STARTUP );  // set before init() so that you can see startup messages

  mesh.init( MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT );
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
}

void loop() {
  mesh.update();
  String msg = "There is a loud noise in areas of sensors: ";
  bool changed = false;
  boolean is_area = false;
  for (int i=0; i<NODES_COUNT; i++){
    uint32_t time_passed = 0;
    uint32_t my_time = mesh.getNodeTime();
    //min = 60 000 000 microsec
    if (my_time<net_state[i][1]){    time_passed = my_time+(71*60000000-net_state[i][1]);    }
    else {    time_passed = my_time-net_state[i][1];    }
    if (time_passed>=60000000){
      net_state[i][0] = 0;
    }
    if (net_state[i][1]>0){
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
