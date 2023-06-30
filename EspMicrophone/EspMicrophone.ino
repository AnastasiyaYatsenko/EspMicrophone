/*
 * Идея #1 считать громкий звук с микрофона, хлопок например, или проезжающая машина или мотоцикл, взрыв и сообщить всем остальным в сети что это произошло
 * СВЯЗЬ ПО СЕТИ: меш-сеть (painlessMesh)
 * ФУНКЦИОНАЛ: 1. Засечь громкий звук и сигналить соседям 
 *             2. Хранить n последних (не подозрительных) замеров для формирования среднего значения. 
 *             Это нужно для корректного реагирования: на центральной улице и в переулке будут разные показатели "нормального" шума, так же будут различаться в дневное и ночное время.
 *             Таким образом, не будет завышенного ожидания фонового шума для безлюдных мест, а так же не будет заниженных - для людных
 *             
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

int soundVolume;
VolAnalyzer analyzer(A2);

Scheduler userScheduler; // to control your personal task
painlessMesh  mesh;

// User stub
void sendMessage() ; // Prototype so PlatformIO doesn't complain

Task taskSendMessage( TASK_SECOND * 1 , TASK_FOREVER, &sendMessage );

unsigned int measureSoundAmplitude() {
  const int sampleWindow = 50;                     // Время измерения в мс
  unsigned int sample;
  // Cохраняем текущие значение millis в startMillis
  unsigned long startMillis= millis();         
  // Создаем переменною peakToPeak, где храним разницу между минимальным и максимальным сигналом
  unsigned int peakToPeak = 0;                
 
  // signalMax максимальным значением
  unsigned int signalMax = 0; 
  // signalMin минимальным значением                 
  unsigned int signalMin = 1024;              
 
  // Пока в startMillis содержащиеся больше заданного sampleWindow, выполняется код в цикле while
  while (millis() - startMillis < sampleWindow) 
  {
    // Сохраняем значение переменной sample считанное с аналогового входе 0
    sample = analogRead(2);    
    // Если значение sample меньше 1024, то есть максимальное значение, читаемое на аналоговом порту                
    if (sample < 1024)                         
    {
      // Если значение sample больше максимального значения, найденного в signalMax
      if (sample > signalMax)                
      {
        // Обновление значения signalMax, содержащимся в sample
        signalMax = sample; 
      }
      //  В противном случае, если значение sample меньше, чем signalMin
      else if (sample < signalMin) 
      {
        // Обновление значения signalMin, содержащимся в sample
        signalMin = sample;  
      }
    }
  }
  //  В переменной peakToPeak будет хранится разницу между максимальным значением и минимальным значением.
  peakToPeak = signalMax - signalMin; 
  return peakToPeak;
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
  soundVolume = measureSoundAmplitude();
  if (soundVolume>100){
    String msg = getReadings();
    msg += mesh.getNodeId();
    mesh.sendBroadcast( msg );
  }
//  String msg = getReadings();
//  msg += mesh.getNodeId();
//  mesh.sendBroadcast( msg );
  taskSendMessage.setInterval(TASK_SECOND * 1);
}

// Needed for painless library
//void receivedCallback( uint32_t from, String &msg ) {
//  Serial.printf("startHere: Received from %u msg=%s\n", from, msg.c_str());
//}

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

//  userScheduler.addTask( taskSendMessage );
//  taskSendMessage.enable();
}

void loop() {
  // it will run the user scheduler as well
  mesh.update();
//  int vol;
  if (analyzer.tick()) {
    soundVolume = analyzer.getVol();
//    Serial.print("VolAnalyzer: ");
//    Serial.println(analyzer.getVol());
  }
  if ((soundVolume>70)&&(millis()-last_sent>=100)){
    String msg = getReadings();
    msg += mesh.getNodeId();
    mesh.sendBroadcast( msg );
    last_sent=millis();
  }
}
