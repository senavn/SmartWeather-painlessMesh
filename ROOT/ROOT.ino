//************************************************************
// Desenvolvido por: 
// Vinícius Sena
// Jefferson Vieira
// Lucas Barreto
// Marcus Vinícius
//
// Faculdade de Tecnologia Termomecanica - São Bernardo do Campo - SP
//************************************************************
#include <Arduino.h>
#include <painlessMesh.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <String.h>

#define   MESH_PREFIX     "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

#define   STATION_SSID     "Swami"
#define   STATION_PASSWORD "papaitop"

#define HOSTNAME "MQTT_Bridge"

// Prototypes
void receivedCallback( const uint32_t &from, const String &msg );
void mqttCallback(char* topic, byte* payload, unsigned int length);

IPAddress getlocalIP();

IPAddress myIP(0,0,0,0);

const char* mqtt_server = "18.219.131.199"; //mqtt server

painlessMesh  mesh;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

void setup() {
  Serial.begin(115200);
  mqttClient.setServer(mqtt_server, 1883);//connecting to mqtt server
  mqttClient.setCallback(mqttCallback);

  mesh.setDebugMsgTypes( ERROR | STARTUP | CONNECTION );  // set before init() so that you can see startup messages

  // Channel set to 6. Make sure to use the same channel for your mesh and for you other
  // network (STATION_SSID)
  mesh.init( MESH_PREFIX, MESH_PASSWORD, MESH_PORT, WIFI_AP_STA, 6 );
  mesh.onReceive(&receivedCallback);

  mesh.stationManual(STATION_SSID, STATION_PASSWORD);
  mesh.setHostname(HOSTNAME);

  // Bridge node, should (in most cases) be a root node. See [the wiki](https://gitlab.com/painlessMesh/painlessMesh/wikis/Possible-challenges-in-mesh-formation) for some background
  mesh.setRoot(true);
  // This node and all other nodes should ideally know the mesh contains a root, so call this on all nodes
  mesh.setContainsRoot(true);
}

void loop() {
  mesh.update();
  mqttClient.loop();

  if(myIP != getlocalIP()){
    myIP = getlocalIP();
    Serial.println("My IP is " + myIP.toString()); 

    if (mqttClient.connect("helix")) {
      Serial.println("########## mqttClient Connected ############");
      //mqttClient.publish("/iot/Mesh/attrs/","Ready!");
      //mqttClient.subscribe("/iot/Mesh/attrs/#");
    }
  }
}

void receivedCallback( const uint32_t &from, const String &msg ) {

  Serial.println(" ");
  Serial.println("##############");
  Serial.printf("bridge: Received from %u msg=%s\n", from, msg.c_str());
    
  String stationNumber = splitStringByIndex(msg, ';', 0);
  Serial.printf("stationNumber %s \n", stationNumber.c_str());

  //Quantidade de parâmetros a serem enviados
  int count = countAttributes(msg, ';');  

  String topic = "/iot/station";
  topic += stationNumber;
  topic += "/attrs/";

  String mensageToSend = "";

  //Realiza a leitura de parametros a serem enviados de forma dinâmica, formato recebido - "tag|valor;tag2|valor2"
  for(int i=1; i < count; i++){
    mensageToSend  = splitStringByIndex(msg, ';', i);

    String debug = "mensageToSend " + String(i) + " mqtt " + mensageToSend.c_str();
    Serial.println(debug);

    //Realiza a publicação no servidor MQTT de forma dinâmica.
    mqttClient.publish(topic.c_str(), mensageToSend.c_str());
  }
  
}

void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
  
}

IPAddress getlocalIP() {
  return IPAddress(mesh.getStationIP());
}

//Método que devolve cada posição de um vetor resultante de Split
String splitStringByIndex(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }

  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

//Método que devolve a quantidade de separadores na mensagem recebida
int countAttributes(String data, char separator)
{
  int found = 0;
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
    }
  }

  return found;
}
