/*
 * Идея #1 считать громкий звук с микрофона, хлопок например, или проезжающая машина или мотоцикл, взрыв и сообщить всем остальным в сети что это произошло
 * СВЯЗЬ ПО СЕТИ: меш-сеть (painlessMesh)
 * ФУНКЦИОНАЛ: 1. Засечь громкий звук и сигналить соседям 
 *             2. Хранить n последних (не подозрительных) замеров для формирования среднего значения. 
 *             Это нужно для корректного реагирования: на центральной улице и в переулке будут разные показатели "нормального" шума, так же будут различаться в дневное и ночное время.
 *             Таким образом, не будет завышенного ожидания фонового шума для безлюдных мест, а так же не будет заниженных - для людных
 *             3. Засекать, сколько по длительности громикй звук
 */

#include "painlessMesh.h"
#include <Arduino_JSON.h>
#include "VolAnalyzer.h"

#define   MESH_PREFIX     "EspMicrophoneMesh"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

//Number for this node
int nodeNumber = 4;
unsigned long last_sent = 0;
unsigned long loud_start = -1;
unsigned long last_background = 0;

const int NOISE_COUNT = 20;
int background_noise[NOISE_COUNT];

int soundVolume;
VolAnalyzer analyzer(A2);

Scheduler userScheduler; // to control your personal task
painlessMesh  mesh;

// User stub
void sendMessage() ; // Prototype so PlatformIO doesn't complain

Task taskSendMessage( TASK_SECOND * 1 , TASK_FOREVER, &sendMessage );

void write_noise(int vol){
  for (int i=NOISE_COUNT-1; i>0; i--){
    background_noise[i]=background_noise[i-1];
  }
  background_noise[0]=vol;
}

int noise_vol(){
  int sum = 0;
  for (int i=0; i<NOISE_COUNT; i++){
    sum += background_noise[i];
  }
  return sum/NOISE_COUNT;
}

float get_delta(int vol){
  int noise = noise_vol();
  float delta = ((vol-noise)/100.0f)*100;
  return delta;
}

String getReadings () {
  JSONVar jsonReadings;
  jsonReadings["node"] = nodeNumber;
  jsonReadings["sound"] = soundVolume;
  jsonReadings["time"] = mesh.getNodeTime();
  String readings = JSON.stringify(jsonReadings);
  return readings;
}

void sendMessage() {
//  int t1 = millis();
//  if (analyzer.tick()) {
//    Serial.print("VolAnalyzer: ");
//    Serial.println(analyzer.getVol());
//  }
//  Serial.print("Millis: ");
//  Serial.println(millis()-t1);
//  soundVolume = measureSoundAmplitude();
//  if (soundVolume>100){
//    String msg = getReadings();
//    msg += mesh.getNodeId();
//    mesh.sendBroadcast( msg );
//  }
//  String msg = getReadings();
//  msg += mesh.getNodeId();
//  mesh.sendBroadcast( msg );
//  taskSendMessage.setInterval(TASK_SECOND * 1);
}

void receivedCallback( uint32_t from, String &msg ) {
  Serial.printf("Received from %u msg=%s\n", from, msg.c_str());
  JSONVar myObject = JSON.parse(msg.c_str());
  int node = myObject["node"];
  double sound = myObject["sound"];
  double node_time = myObject["time"];
  Serial.print("Node: ");
  Serial.println(node);
  Serial.print("Sound amplitude: ");
  Serial.println(sound);
  Serial.print("Time: ");
  Serial.println(node_time);
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

//mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
  mesh.setDebugMsgTypes( ERROR | STARTUP );  // set before init() so that you can see startup messages

  mesh.init( MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT );
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

  for (int i=0; i<NOISE_COUNT; i++){
    background_noise[i] = 0;
  }

//  userScheduler.addTask( taskSendMessage );
//  taskSendMessage.enable();
}

void loop() {
  mesh.update();
  if (analyzer.tick()) {
    soundVolume = analyzer.getVol();
  }
  float delta = get_delta(soundVolume);
  
  if ((delta>=50.0f)||((delta>=20.0f)&&(soundVolume>70))){
    if (loud_start==-1){    loud_start = millis();    }
    if ((millis()-loud_start>=300)&&(millis()-last_sent>=100)){
      String msg = getReadings();
      msg += mesh.getNodeId();
      mesh.sendBroadcast( msg );
      last_sent=millis();
    }
  }
  else {
    if (loud_start!=-1){    loud_start = -1;    }
    if (millis()-last_background>500){
      write_noise(soundVolume);
      last_background = millis();
    }
  }
}
