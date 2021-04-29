//************************************************************
// this is a simple example that uses the easyMesh library
//
//
//
//************************************************************
#include <painlessMesh.h>
#include <dht11.h>

//Definição de pinos para os sensores
#define   LED             2       
#define   DHT11PIN        25 // pino de leitura DHT11
#define   PLUVPIN         13 // pino de leitura Pluviometro 

#define   BLINK_PERIOD    3000 // milliseconds until cycle repeat
#define   BLINK_DURATION  100  // milliseconds LED is on for

#define   MESH_SSID       "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

// DECLARAÇÃO DE VARIÁVEIS
Scheduler     userScheduler; // to control your personal task
painlessMesh  mesh;
dht11         DHT11;
String stationNumber = "01"; // id da estação

// pluviometro
int pluv_val = 0;            // variável de controle do pluviometro                
int pluv_old_val = 0;        // variável de controle do pluviometro        
int pluv_count = 0;          // contador de variações do pluviometro
float pluv_mm = 0;  

bool calc_delay = false;
SimpleList<uint32_t> nodes;

void sendMessage() ; // Prototype
Task taskSendMessage( TASK_SECOND * 5, TASK_FOREVER, &sendMessage ); // start with a one second interval

// Task to blink the number of nodes
Task blinkNoNodes;
bool onFlag = false;

void setup() {
  Serial.begin(115200);

  pinMode(LED, OUTPUT);
  pinMode(PLUVPIN, INPUT_PULLUP);

  mesh.setDebugMsgTypes(ERROR | DEBUG);  // set before init() so that you can see error messages

  mesh.init(MESH_SSID, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
  mesh.onNodeDelayReceived(&delayReceivedCallback);

  userScheduler.addTask( taskSendMessage );
  taskSendMessage.enable();

  blinkNoNodes.set(BLINK_PERIOD, (mesh.getNodeList().size() + 1) * 2, []() {
      // If on, switch off, else switch on
      if (onFlag)
        onFlag = false;
      else
        onFlag = true;
      blinkNoNodes.delay(BLINK_DURATION);

      if (blinkNoNodes.isLastIteration()) {
        // Finished blinking. Reset task for next run 
        // blink number of nodes (including this node) times
        blinkNoNodes.setIterations((mesh.getNodeList().size() + 1) * 2);
        // Calculate delay based on current mesh time and BLINK_PERIOD
        // This results in blinks between nodes being synced
        blinkNoNodes.enableDelayed(BLINK_PERIOD - 
            (mesh.getNodeTime() % (BLINK_PERIOD*1000))/1000);
      }
  });
  userScheduler.addTask(blinkNoNodes);
  blinkNoNodes.enable();

  randomSeed(analogRead(A0));
}

void loop() {
  mesh.update();
  updatePluviometro();
  digitalWrite(LED, !onFlag);
}

void sendMessage() {
  String msg = stationNumber + ";";
  
  int chk = DHT11.read(DHT11PIN);
  
  String aux_temperatura;
  String aux_umidade;

  aux_temperatura = (String)DHT11.temperature;
  aux_umidade = (String)DHT11.humidity;

  msg += "t|" + aux_temperatura + ";";
  msg += "h|" + aux_umidade;
  msg += "pluv|" + (String)pluv_mm;
  
  mesh.sendBroadcast(msg);

  if (calc_delay) {
    SimpleList<uint32_t>::iterator node = nodes.begin();
    while (node != nodes.end()) {
      mesh.startDelayMeas(*node);
      node++;
    }
    calc_delay = false;
  }

  Serial.printf("Sending message: %s\n", msg.c_str());
  
  //taskSendMessage.setInterval( random(TASK_SECOND * 1, TASK_SECOND * 5));  // between 1 and 5 seconds
}


void receivedCallback(uint32_t from, String & msg) {
  Serial.printf("startHere: Received from %u msg=%s\n", from, msg.c_str());
}

void newConnectionCallback(uint32_t nodeId) {
  // Reset blink task
  onFlag = false;
  blinkNoNodes.setIterations((mesh.getNodeList().size() + 1) * 2);
  blinkNoNodes.enableDelayed(BLINK_PERIOD - (mesh.getNodeTime() % (BLINK_PERIOD*1000))/1000);
 
  Serial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
  Serial.printf("--> startHere: New Connection, %s\n", mesh.subConnectionJson(true).c_str());
}

void changedConnectionCallback() {
  Serial.printf("Changed connections\n");
  // Reset blink task
  onFlag = false;
  blinkNoNodes.setIterations((mesh.getNodeList().size() + 1) * 2);
  blinkNoNodes.enableDelayed(BLINK_PERIOD - (mesh.getNodeTime() % (BLINK_PERIOD*1000))/1000);
 
  nodes = mesh.getNodeList();

  Serial.printf("Num nodes: %d\n", nodes.size());
  Serial.printf("Connection list:");

  SimpleList<uint32_t>::iterator node = nodes.begin();
  while (node != nodes.end()) {
    Serial.printf(" %u", *node);
    node++;
  }
  Serial.println();
  calc_delay = true;
}

void nodeTimeAdjustedCallback(int32_t offset) {
  Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(), offset);
}

void delayReceivedCallback(uint32_t from, int32_t delay) {
  Serial.printf("Delay to node %u is %d us\n", from, delay);
}

void updatePluviometro (){
  // ler o estado do switch pelo pino de entrada:
  pluv_val = digitalRead(PLUVPIN);      //Read the status of the Reed swtich
  
  if ((pluv_val == LOW) && (pluv_old_val == HIGH)){    //Check to see if the status has changed
      delay(10);                                       // Delay put in to deal with any "bouncing" in the switch.
      pluv_count = pluv_count + 1;                     //Add 1 to the count of bucket tips
      pluv_old_val = pluv_val;                         //Make the old value equal to the current value
      Serial.print("Medida de chuva (contagem): ");
      Serial.print(pluv_count);//*0.2794); 
      Serial.println(" pulso");
      Serial.print("Medida de chuva (calculado): ");
      Serial.print(pluv_count*0.25); 
      Serial.println(" mm");
      pluv_mm = pluv_count*0.25;
   } 
       
   else {
        pluv_old_val = pluv_val;                      //If the status hasn't changed then do nothing
   }
 }
