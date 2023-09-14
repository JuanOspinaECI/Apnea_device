

#include <esp_now.h>
#include <WiFi.h>
#include <WebServer.h>                                // needed to create a simple webserver (make sure tools -> board is set to ESP32, otherwise you will get a "WebServer.h: No such file or directory" error)
#include <WebSocketsServer_Generic.h>                           // needed for instant communication between client and server through Websockets
#include <ArduinoJson.h>                              // needed for JSON encapsulation (send multiple variables with one string)
#include <Arduino_JSON.h>

#define CHANNEL 1

// SSID and password of Wifi connection:
const char* ssid = "Apnea_device";
const char* password = "holahola";

// The String below "webpage" contains the complete HTML code that is sent to the client whenever someone connects to the webserver
String webpage = "<!DOCTYPE html><html><head><title>Page Title</title></head><body style='background-color: #EEEEEE;'><span style='color: #003366;'><h1>Sensors information</h1><p>X: <span id='x_acel'>-</span></p><p>Y: <span id='y_acel'>-</span></p><p>Z: <span id='z_acel'>-</span></p><p>beatsPerMinute: <span id='beatsPerMinute_'>-</span></p><p>beatAvg: <span id='beatAvg_'>-</span></p><p>irValue: <span id='irValue_'>-</span></p><p><button type='button' id='BTN_SEND_BACK'>Send info to ESP32</button></p></span> <h2>Cuestionario STOP BANG</h2> <form action='#' method='post'> <label for='ronquidos'>1. Ronquidos:</label><br> Usted ronca fuerte? (lo suficiente para ser escuchado a traves de puertas cerradas o la persona con la que duerme lo mueve por sus ronquidos.)<br> <input type='radio' id='ronquidos_si' name='ronquidos' value='SI'> <label for='ronquidos_si'>SI</label> <input type='radio' id='ronquidos_no' name='ronquidos' value='NO'> <label for='ronquidos_no'>NO</label><br><br> <label for='cansancio'>2. Cansancio:</label><br> Se siente usted frecuentemente cansado, fatigado o somnoliento durante el día? (Por ejemplo se queda usted dormido mientras conduce o habla con alguien)<br> <input type='radio' id='cansancio_si' name='cansancio' value='SI'> <label for='cansancio_si'>SI</label> <input type='radio' id='cansancio_no' name='cansancio' value='NO'> <label for='cansancio_no'>NO</label><br><br> <label for='observacion'>3. Observacion:</label><br> Alguien lo ha observado dejar de respirar o ahogarse/jadear durante el suenno?<br> <input type='radio' id='observacion_si' name='observacion' value='SI'> <label for='observacion_si'>SI</label> <input type='radio' id='observacion_no' name='observacion' value='NO'> <label for='observacion_no'>NO</label><br><br> <label for='presion_arterial'>4. Presion Arterial:</label><br> Tiene o está siendo tratado por hipertension arterial?<br> <input type='radio' id='presion_arterial_si' name='presion_arterial' value='SI'> <label for='presion_arterial_si'>SI</label> <input type='radio' id='presion_arterial_no' name='presion_arterial' value='NO'> <label for='presion_arterial_no'>NO</label><br><br> <label for='imc'>5. Indice de Masa Corporal mayor a 35 kg/m2:</label><br> Su Indice de Masa Corporal (BMI) es mayor a 35 kg/m2? (Calculado como peso en kilogramos dividido por la altura en metros al cuadrado)<br> <input type='radio' id='imc_si' name='imc' value='SI'> <label for='imc_si'>SI</label> <input type='radio' id='imc_no' name='imc' value='NO'> <label for='imc_no'>NO</label><br><br> <label for='edad'>6. Mayor de 50 annos:</label><br> Tiene usted más de 50 annos?<br> <input type='radio' id='edad_si' name='edad' value='SI'> <label for='edad_si'>SI</label> <input type='radio' id='edad_no' name='edad' value='NO'> <label for='edad_no'>NO</label><br><br> <label for='cuello'>7. Tamaño del cuello (Medido alrededor de la manzana de Adan):</label><br> Es el cuello de su camisa de 16 pulgadas / 40 cm o mas grande?<br> <input type='radio' id='cuello_si' name='cuello' value='SI'> <label for='cuello_si'>SI</label> <input type='radio' id='cuello_no' name='cuello' value='NO'> <label for='cuello_no'>NO</label><br><br> <label for='genero'>8. Genero:</label><br> Hombre<br> <input type='radio' id='genero_hombre' name='genero' value='Hombre'> <label for='genero_hombre'>SI</label><br> <input type='radio' id='genero_mujer' name='genero' value='Mujer'> <label for='genero_mujer'>NO</label><br><br> <input type='submit' value='Enviar'> </form></body><script> var Socket; document.getElementById('BTN_SEND_BACK').addEventListener('click', button_send_back); function init() { Socket = new WebSocket('ws://' + window.location.hostname + ':81/'); Socket.onmessage = function(event) { processCommand(event); }; } function button_send_back() { var msg = {brand: 'Gibson',type: 'Les Paul Studio',year: 2010,color: 'white'};Socket.send(JSON.stringify(msg)); } function processCommand(event) {var obj = JSON.parse(event.data);document.getElementById('x_acel').innerHTML = obj.x;document.getElementById('y_acel').innerHTML = obj.y;document.getElementById('z_acel').innerHTML = obj.z;document.getElementById('beatsPerMinute_').innerHTML = obj.beatsPerMinute;document.getElementById('beatAvg_').innerHTML = obj.beatAvg;document.getElementById('irValue_').innerHTML = obj.irValue; console.log(obj.x_acel);console.log(obj.y_acel);console.log(obj.z_acel); } window.onload = function(event) { init(); }</script></html>";
// We want to periodically send values to the clients, so we need to define an "interval" and remember the last time we sent data to the client (with "previousMillis")
int interval = 1000;                                  // send data to the client every 1000ms -> 1s
unsigned long previousMillis = 0;                     // we use the "millis()" command for time reference and this will output an unsigned long


typedef struct struct_message {
    int id;
    float x;
    float y;
    float z;
    float temp;
    int beatsPerMinute;
    int beatAvg;
    int irValue;
    int readingId;
} struct_message;

//Create a struct_message called myData
struct_message incomingReadings;

JSONVar board;



// Initialization of webserver and websocket
WebServer server(80);                                 // the server uses port 80 (standard port for websites
WebSocketsServer webSocket = WebSocketsServer(81);    // the websocket uses port 81 (standard port for websockets

// Init ESP Now with fallback
void InitESPNow() {
  WiFi.disconnect();
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
  }
  else {
    Serial.println("ESPNow Init Failed");
    // Retry InitESPNow, add a counte and then restart?
    // InitESPNow();
    // or Simply Restart
    ESP.restart();
  }
}

// config AP SSID
void configDeviceAP() {
  const char *SSID = "Apnea_device";
  bool result = WiFi.softAP(SSID, "holahola", CHANNEL, 0);
  if (!result) {
    Serial.println("AP Config failed.");
  } else {
    Serial.println("AP Config Success. Broadcasting with AP: " + String(SSID));
    Serial.print("AP CHANNEL "); Serial.println(WiFi.channel());
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESPNow/Basic/Slave Example");
  //Set device in AP mode to begin with
  WiFi.mode(WIFI_AP);
  // configure device AP mode
  configDeviceAP();
  // This is the mac address of the Slave in AP Mode
  Serial.print("AP MAC: "); Serial.println(WiFi.softAPmacAddress());
  // Init ESPNow with a fallback logic
  InitESPNow();
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info.
  esp_now_register_recv_cb(OnDataRecv);

  //WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  server.on("/", []() {                               // define here wat the webserver needs to do
    server.send(200, "text/html", webpage);           //    -> it needs to send out the HTML string "webpage" to the client
  });
  server.begin();                                     // start server
  
  webSocket.begin();                                  // start websocket
  webSocket.onEvent(webSocketEvent);                  // define a callback function -> what does the ESP32 need to do when an event from the websocket is received? -> run function "webSocketEvent()"
}

// callback when data is recv from Master
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  
  Serial.println(macStr);
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
  
  if(incomingReadings.id == 2){
    board["id"] = incomingReadings.id;
    board["x"] = incomingReadings.x;
    board["y"] = incomingReadings.y;
    board["z"] = incomingReadings.y;
    board["readingId"] = String(incomingReadings.readingId);
    String jsonString = JSON.stringify(board);
  } else if(incomingReadings.id == 3){
    board["id"] = incomingReadings.id;
    board["beatsPerMinute"] = incomingReadings.x;
    board["beatAvg"] = incomingReadings.beatAvg;
    board["irValue"] = incomingReadings.irValue;
    board["readingId"] = String(incomingReadings.readingId);
    String jsonString = JSON.stringify(board);
  }
  else {
    board["id"] = incomingReadings.id;
    board["temp"] = incomingReadings.temp;
    board["readingId"] = String(incomingReadings.readingId);
    String jsonString = JSON.stringify(board);
  }


  //events.send(jsonString.c_str(), "new_readings", millis());
  Serial.printf("Board ID %u: %u bytes\n", incomingReadings.id, len);
  if(incomingReadings.id == 2){
    Serial.printf(" x: %4.2f \n", incomingReadings.x);
    Serial.printf(" y: %4.2f \n", incomingReadings.y);
    Serial.printf(" z: %4.2f \n", incomingReadings.z);
    axes_data(incomingReadings.x,incomingReadings.y,incomingReadings.z);
  }
  else if(incomingReadings.id == 3){
    heart_data(incomingReadings.beatsPerMinute, incomingReadings.beatAvg, incomingReadings.irValue);
    Serial.printf(" BPM: ");
    Serial.println(incomingReadings.beatsPerMinute);
    Serial.printf(" BPM_AVG: ");
    Serial.println(incomingReadings.beatAvg);
    Serial.printf(" IRVALUE: ");
    Serial.println(incomingReadings.z);
  }
  else { Serial.printf(" temp: %4.2f \n", incomingReadings.y);}
  Serial.printf("readingID value: %d \n", incomingReadings.readingId);
  Serial.println();
 // Serial.print("Last Packet Recv from: "); Serial.println(macStr);
  //Serial.print("Last Packet Recv Data: "); Serial.println(*data);
  //Serial.println("");
}

void loop() {
  server.handleClient();                              // Needed for the webserver to handle all clients
  webSocket.loop();                                   // Update function for the webSockets 
  
  unsigned long now = millis();                       // read out the current "time" ("millis()" gives the time in ms since the Arduino started)
  if ((unsigned long)(now - previousMillis) > interval) { // check if "interval" ms has passed since last time the clients were updated
   /* 
    String jsonString = "";                           // create a JSON string for sending data to the client
    StaticJsonDocument<200> doc;                      // create a JSON container
    JsonObject object = doc.to<JsonObject>();         // create a JSON Object
    object["x"] = random(100);                    // write data into the JSON object -> I used "rand1" and "rand2" here, but you can use anything else
    object["y"] = random(100);
    object["z"] = random(100);
    serializeJson(doc, jsonString);                   // convert JSON object to string
    //Serial.println(jsonString);                       // print JSON string to console for debug purposes (you can comment this out)
    webSocket.broadcastTXT(jsonString);               // send JSON string to clients
    
    previousMillis = now;    */                         // reset previousMillis
  }
}

void axes_data (float x_, float y_, float z_){
      String jsonString = "";                           // create a JSON string for sending data to the client
StaticJsonDocument<200> doc;                      // create a JSON container
JsonObject object = doc.to<JsonObject>();

            // create a JSON Object
    object["x"] = x_;                    // write data into the JSON object -> I used "rand1" and "rand2" here, but you can use anything else
    object["y"] = y_;
    object["z"] = z_;
    //Serial.println(object.);
    serializeJson(doc, jsonString);                   // convert JSON object to string
    //Serial.println(jsonString);                       // print JSON string to console for debug purposes (you can comment this out)
    webSocket.broadcastTXT(jsonString);               // send JSON string to clients
}

void heart_data(float beatsPerMinute_, int beatAvg_, long irValue_){
  String jsonString = "";                           // create a JSON string for sending data to the client
StaticJsonDocument<200> doc;                      // create a JSON container
JsonObject object = doc.to<JsonObject>();
    object["beatsPerMinute"] = beatsPerMinute_;                    // write data into the JSON object -> I used "rand1" and "rand2" here, but you can use anything else
    object["beatAvg"] = beatAvg_;
    object["irValue"] = irValue_;
    serializeJson(doc, jsonString);                   // convert JSON object to string
    //Serial.println(jsonString);                       // print JSON string to console for debug purposes (you can comment this out)
    webSocket.broadcastTXT(jsonString);               // send JSON string to clients
}

void webSocketEvent(byte num, WStype_t type, uint8_t * payload, size_t length) {      // the parameters of this callback function are always the same -> num: id of the client who send the event, type: type of message, payload: actual data sent and length: length of payload
  switch (type) {                                     // switch on the type of information sent
    case WStype_DISCONNECTED:                         // if a client is disconnected, then type == WStype_DISCONNECTED
      Serial.println("Client " + String(num) + " disconnected");
      break;
    case WStype_CONNECTED:                            // if a client is connected, then type == WStype_CONNECTED
      Serial.println("Client " + String(num) + " connected");
      // optionally you can add code here what to do when connected
      break;
    case WStype_TEXT:                                 // if a client has sent data, then type == WStype_TEXT
      // try to decipher the JSON string received
      StaticJsonDocument<200> doc;                    // create a JSON container
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }
      else {
        // JSON string was received correctly, so information can be retrieved:
        const char* g_brand = doc["brand"];
        const char* g_type = doc["type"];
        const int g_year = doc["year"];
        const char* g_color = doc["color"];
        Serial.println("Received guitar info from user: " + String(num));
        Serial.println("Brand: " + String(g_brand));
        Serial.println("Type: " + String(g_type));
        Serial.println("Year: " + String(g_year));
        Serial.println("Color: " + String(g_color));
      }
      Serial.println("");
      break;
  }
}
