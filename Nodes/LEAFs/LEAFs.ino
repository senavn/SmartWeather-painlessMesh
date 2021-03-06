//************************************************************
// Desenvolvido por: 
// Vinícius Sena
// Jefferson Vieira
// Lucas Barreto
// Marcus Vinícius
//
// Faculdade de Tecnologia Termomecanica - São Bernardo do Campo - SP
//************************************************************
#include <painlessMesh.h>
#include <dht11.h>
#include <Adafruit_BMP280.h>

//Definição de pinos para os sensores
#define   LED             2       
#define   DHT11PIN        25 // pino de leitura DHT11
#define   PLUVPIN         13 // pino de leitura Pluviometro 
#define   ANEMPIN         12 // pino de leitura Anemometro
#define   BMP_SDA         21 // pino Sda do BMP
#define   BMP_SCL         22 // pino SCL do BMP
#define   LDRPIN          34 // pino de leitura LDR
#define   GND_H           35 // pino de leitura da umidade do solo

#define   BLINK_PERIOD    3000 // milliseconds until cycle repeat
#define   BLINK_DURATION  100  // milliseconds LED is on for

#define   MESH_SSID       "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

// DECLARAÇÃO DE VARIÁVEIS
Scheduler         userScheduler; // to control your personal task
painlessMesh      mesh;
dht11             DHT11;
Adafruit_BMP280   bmp280;

String stationNumber = "007"; // id da estação

// pluviometro
int pluv_val = 0;            // variável de controle do pluviometro                
int pluv_old_val = 0;        // variável de controle do pluviometro        
int pluv_count = 0;          // contador de variações do pluviometro
float pluv_mm = 0;  

// anemometro
volatile byte anem_counter;   // magnet counter for sensor - int counter = 0;
float anem_RPM = 0;       // Revolutions per minute
float anem_speedwind = 0;// Wind speed (km/s)
float anem_vm=0;
float anem_vmd=0;
float anem_vmax=0;
unsigned long anem_startTime = 0 ;        // long startTime = 0

// Constantes
const float pi = 3.14159265;  // Numero pi
int period = 3000;      // Tempo de medida(miliseconds) 3000 menor que o intervalos de leituras 
int radius = 147;      // Aqui ajusta o raio do anemometro em milimetros  **************
int const_umidade_solo = 4095;

bool calc_delay = false;
SimpleList<uint32_t> nodes;

void sendMessage() ; // Prototype
Task taskSendMessage(TASK_SECOND * 5, TASK_FOREVER, &sendMessage); // start with a one second interval

void windvelocity() ; // Prototype
Task taskUpdateWindSpeed (TASK_MILLISECOND * 3100, TASK_FOREVER, &windvelocity);

// Task to blink the number of nodes
Task blinkNoNodes;
bool onFlag = false;

void setup() {
  Serial.begin(115200);
  pinMode(GND_H, INPUT);
  pinMode(LED, OUTPUT);
  pinMode(PLUVPIN, INPUT_PULLUP);
  pinMode(LDRPIN, INPUT); //DEFINE O PINO COMO ENTRADA
  delay(10);
  pinMode(ANEMPIN, INPUT_PULLUP);
  attachInterrupt(ANEMPIN, addcount, RISING);
  boolean statusBMP = bmp280.begin(0x76);

  mesh.setDebugMsgTypes(ERROR | DEBUG);  // set before init() so that you can see error messages

  mesh.init(MESH_SSID, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
  mesh.onNodeDelayReceived(&delayReceivedCallback);

  userScheduler.addTask(taskSendMessage);
  userScheduler.addTask(taskUpdateWindSpeed);
  taskSendMessage.enable();
  taskUpdateWindSpeed.enable();
  

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
  String aux_pressao;
  String aux_luminosidade;
  String aux_gnd_h;
  
  aux_pressao = (String)(bmp280.readPressure() / 100);
  aux_temperatura = (String)DHT11.temperature;
  aux_umidade = (String)DHT11.humidity;
  aux_luminosidade = (String)analogRead(LDRPIN);
  aux_gnd_h = (String)(100-((analogRead(GND_H)*100)/const_umidade_solo));

  msg += "temperature|" + aux_temperatura + ";";
  msg += "humidity|" + aux_umidade + ";";
  msg += "rain_mm|" + (String)pluv_mm + ";";
  msg += "wind_speed|" + (String)anem_speedwind + ";";
  msg += "pressure|" + aux_pressao + ";";
  msg += "luminosity|" + aux_luminosidade + ";";
  msg += "ground_humidity|" + aux_gnd_h;  
  
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

//-----------------------------------------------------------------------------------------------------------------
// Pluviometro
void updatePluviometro (){
  // ler o estado do switch pelo pino de entrada:
  pluv_val = digitalRead(PLUVPIN);      //Read the status of the Reed swtich
  
  if ((pluv_val == LOW) && (pluv_old_val == HIGH)){    //Check to see if the status has changed
      delay(10);                                       // Delay put in to deal with any "bouncing" in the switch.
      pluv_count = pluv_count + 1;                     //Add 1 to the count of bucket tips
      pluv_old_val = pluv_val;                         //Make the old value equal to the current value
      //Serial.print("Medida de chuva (contagem): ");
      //Serial.print(pluv_count);//*0.2794); 
      //Serial.println(" pulso");
      pluv_mm = pluv_count*0.25;
   } 
       
   else {
        pluv_old_val = pluv_val;                      //If the status hasn't changed then do nothing
   }
 }

//-----------------------------------------------------------------------------------------------------------------
// Anemometro
// Measure wind speed
void windvelocity(){
    if(millis() - anem_startTime >= period) {

        detachInterrupt(ANEMPIN); // Desabilita interrupcao
        //Serial.print("Pulsos :");
        //Serial.print(counter);

        anem_RPM=((anem_counter)*60)/(period/1000);  // Calculate revolutions per minute (RPM) 60
        //Serial.print(" - RPM :");
        //Serial.print(RPM);
        anem_speedwind = 0;
        anem_counter = 0;           // Zera cont pulsos
        unsigned long millis();       
        anem_startTime = millis();

        attachInterrupt(ANEMPIN, addcount, RISING); // Habilita interrupcao  
    }

    anem_speedwind = ((30.35 * anem_RPM) / 1000) * 3.6 ;  // Calculate wind speed on km/h
    //Serial.print(" - Veloc : ");
    //Serial.println(speedwind);
    anem_vm=anem_vm+anem_speedwind;
    if(anem_speedwind > anem_vmax ){
        anem_vmax = anem_speedwind;
    }
}

void addcount(){
  anem_counter++;
} 
