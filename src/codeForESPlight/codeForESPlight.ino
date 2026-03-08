#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <espnow.h>

// -------- PIN DEFINITIONS --------
/*#define OUT1 D1
#define OUT2 D0 
#define PWM1 D6
#define PWM2 D7
#define LED D4
#define BTN1 D2
#define BTN2 D3
#define RESTORE_PIN D5   // long press restore*/
#define OUT1  5
#define OUT2 16 
#define PWM1 12
#define PWM2 13
#define LED 2
#define BTN1 4
#define BTN2 0
#define RESTORE_PIN 14   // long press restore

// ----------------------------------
int brightNess =20;
ESP8266WebServer server(80);

char ssid[32];
char pass[32];


// Store LED states
bool led1State = false;
bool led2State = false;

int led1Brightness = 0;
int led2Brightness = 0;
bool apMode = false;

unsigned long restoreStart = 0;
bool restoreTriggered = false;

String webpage = R"====(
  <!DOCTYPE html>
  <html>

  <head>
  <meta name="viewport" content="width=device-width, initial-scale=1">

  <style>

  body{
    font-family: Arial;
    text-align:center;
    background:#111;
    color:white;
  }

  h2{
    margin-top:20px;
  }

  .card{
    display:inline-block;
    background:#222;
    padding:20px;
    margin:15px;
    border-radius:15px;
    width:250px;
  }

  .slider{
    width:100%;
  }

  button{
    padding:10px 25px;
    border:none;
    border-radius:25px;
    font-size:16px;
    color:white;
    cursor:pointer;
    transition:0.3s;
  }

  .off{
    background:#555;
  }

  .on{
    background:#00c853;
    box-shadow:0 0 15px #00ff88;
  }

  </style>

  </head>

  <body>

  <h2>LED Lighting Control Panel</h2>

  <div class="card">

  <h3>Living Room LED</h3>

  <button id="btn1" class="off" onclick="toggleLED(1)">OFF</button>

  <br><br>

  <input id="slider1" type="range" min="0" max="255" value="0"
  class="slider"
  oninput="setBrightness(1,this.value)">

  <p>Brightness: <span id="b1">0</span></p>

  </div>

  <div class="card">

  <h3>Bedroom LED</h3>

  <button id="btn2" class="off" onclick="toggleLED(2)">OFF</button>

  <br><br>

  <input id="slider2" type="range" min="0" max="255" value="0"
  class="slider"
  oninput="setBrightness(2,this.value)">

  <p>Brightness: <span id="b2">0</span></p>

  </div>

  <script>

  let state1=false;
  let state2=false;

  function updateButton(id,state){

    let btn=document.getElementById("btn"+id);

    if(state){
      btn.innerText="ON";
      btn.classList.remove("off");
      btn.classList.add("on");
    }else{
      btn.innerText="OFF";
      btn.classList.remove("on");
      btn.classList.add("off");
    }

  }

  function toggleLED(id){

    if(id==1){

      state1=!state1;

      fetch("/toggle1?state="+state1);

      updateButton(1,state1);

    }
    else{

      state2=!state2;

      fetch("/toggle2?state="+state2);

      updateButton(2,state2);

    }

  }

  function setBrightness(id,val){

    document.getElementById("b"+id).innerText=val;

    fetch("/brightness"+id+"?value="+val);

  }

  window.onload = function(){

    fetch("/status")
    .then(res=>res.json())
    .then(data=>{

      state1=data.led1State;
      state2=data.led2State;

      updateButton(1,state1);
      updateButton(2,state2);

      document.getElementById("slider1").value=data.led1Brightness;
      document.getElementById("slider2").value=data.led2Brightness;

      document.getElementById("b1").innerText=data.led1Brightness;
      document.getElementById("b2").innerText=data.led2Brightness;

    });

  }

  </script>

  </body>
  </html>
  )====";

enum LedMode {
  LED_RESTORE,
  LED_CONNECTING,
  LED_NORMAL,
  LED_COMMAND_BLINK
};

LedMode currentMode = LED_CONNECTING;

unsigned long previousMillis = 0;
unsigned long blinkInterval = 0;
bool ledState = LOW;

unsigned long commandBlinkStart = 0;
bool commandBlinkActive = false;
//espnow data structure
typedef struct struct_message {
    int data[6];
} struct_message;
struct_message myData;

// handling ESP now communication and their data input
void OnDataRecv(uint8_t * mac_addr, uint8_t *incomingData, uint8_t len) {
  char macStr[18];
  Serial.print("Packet received from: ");
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.println(macStr);
  memcpy(&myData, incomingData, sizeof(myData));
  // Update the structures with the new incoming data
  //Here I am setting data for id 0 to 5 means, if incoming bit is high then make the changes with respect to that
  for(int i=0;i<6;i++){
    Serial.println(myData.data[i]);
    if(myData.data[i]==1){
      switch(i){
        case 0 : led1Brightness = led1Brightness - 5;
                  Serial.print("Brightness : ");
                  Serial.println(led1Brightness);
                  setLed1(led1State?1:0,led1Brightness);
                  break;
        case 1 :  led1State = !led1State;
                  setLed1(led1State?1:0,led1Brightness);
                  break;
        case 2 : led1Brightness = led1Brightness + 5;
                  Serial.print("Brightness : ");
                  Serial.println(led1Brightness);
                  setLed1(led1State?1:0,led1Brightness);
                  break;
        case 3 : led2Brightness = led2Brightness - 5;
                  Serial.print("Brightness : ");
                  Serial.println(led2Brightness);
                  setLed2(led2State?1:0,led2Brightness);
                  break;
        case 4 :  led2State = !led2State;
                  setLed2(led2State?1:0,led2Brightness);
                  break;
        case 5 : led2Brightness = led2Brightness + 5;
                  Serial.print("Brightness : ");
                  Serial.println(led2Brightness);
                  setLed2(led2State?1:0,led2Brightness);
                  break;
        defualt : Serial.println("Invalid button");
                  break;
      }
    }
  }
  Serial.println();
}


// ================= EEPROM =================
void saveCredentials(String s, String p) {
  EEPROM.begin(96);
  s.toCharArray(ssid, 32);
  p.toCharArray(pass, 32);

  for (int i = 0; i < 32; i++) EEPROM.write(i, ssid[i]);
  for (int i = 0; i < 32; i++) EEPROM.write(i + 32, pass[i]);

  EEPROM.commit();
  EEPROM.end();
}

void clearCredentials() {
  EEPROM.begin(96);
  for (int i = 0; i < 96; i++) EEPROM.write(i, 0);
  EEPROM.commit();
  EEPROM.end();
}

void loadCredentials() {
  EEPROM.begin(96);
  for (int i = 0; i < 32; i++) ssid[i] = EEPROM.read(i);
  for (int i = 0; i < 32; i++) pass[i] = EEPROM.read(i + 32);
  EEPROM.end();
}

// ================= WEB PAGES =================
void handleSetupPage() {
  String html = R"====(
  <!DOCTYPE html>
  <html>

  <head>
  <meta name="viewport" content="width=device-width, initial-scale=1">

  <style>

  body{
    margin:0;
    font-family:Arial, Helvetica, sans-serif;
    background:linear-gradient(180deg,#dfe9f3,#f7f9fb);
    display:flex;
    justify-content:center;
    align-items:center;
    height:100vh;
  }

  .container{
    width:380px;
    text-align:center;
  }

  h2{
    margin-bottom:5px;
  }

  .subtitle{
    color:#666;
    margin-bottom:25px;
  }

  .card{
    background:white;
    padding:25px;
    border-radius:12px;
    box-shadow:0 8px 20px rgba(0,0,0,0.1);
    margin-bottom:20px;
  }

  .info{
    font-size:15px;
    color:#333;
  }

  .label{
    text-align:left;
    font-size:14px;
    margin-top:15px;
    color:#444;
  }

  input{
    width:100%;
    padding:10px;
    margin-top:6px;
    border-radius:8px;
    border:1px solid #ccc;
    font-size:14px;
  }

  .password-box{
    position:relative;
  }

  .eye{
    position:absolute;
    right:10px;
    top:10px;
    cursor:pointer;
    font-size:16px;
  }

  button{
    width:100%;
    margin-top:20px;
    padding:12px;
    border:none;
    border-radius:8px;
    background:#3a7ca5;
    color:white;
    font-size:15px;
    cursor:pointer;
    transition:0.3s;
  }

  button:hover{
    background:#2f688c;
  }

  </style>

  </head>

  <body>

  <div class="container">

  <h2>Device Setup</h2>
  <div class="subtitle">Welcome to Your New Smart Device</div>

  <div class="card">
  <div class="info">
  <b>Device Information</b><br><br>
  MAC Address: %%MAC%%
  </div>
  </div>

  <div class="card">

  <b>Connect to Wi-Fi</b>

  <form action="/save">

  <div class="label">Enter Wi-Fi Network Name (SSID)</div>
  <input name="s" required>

  <div class="label">Enter Wi-Fi Password</div>

  <div class="password-box">
  <input id="pass" name="p" type="password">
  <span class="eye" onclick="togglePass()">👁</span>
  </div>

  <button type="submit">Connect and Reboot</button>

  </form>

  </div>

  </div>

  <script>

  function togglePass(){
    var p=document.getElementById("pass");
    if(p.type==="password"){
      p.type="text";
    }else{
      p.type="password";
    }
  }

  </script>

  </body>
  </html>
  )====";

  html.replace("%%MAC%%", WiFi.macAddress());

  server.send(200, "text/html", html);
}

void handleSave() {
  saveCredentials(server.arg("s"), server.arg("p"));
  server.send(200, "text/html", "Saved. Rebooting...");
  delay(2000);
  ESP.restart();
}
void handleRoot(){
  server.send(200,"text/html",webpage);
}

void handleToggle1(){

  String state = server.arg("state");

  led1State = (state == "true");

  //digitalWrite(led1, led1State ? HIGH : LOW);
  setLed1(led1State ? 1 : 0,led1Brightness);

  server.send(200,"text/plain","OK");

}

void handleToggle2(){

  String state = server.arg("state");

  led2State = (state == "true");

  //digitalWrite(led2, led2State ? HIGH : LOW);
  setLed2(led2State ? 1 : 0,led2Brightness);

  server.send(200,"text/plain","OK");

}

void handleBrightness1(){

  led1Brightness = server.arg("value").toInt();

  //analogWrite(led1, led1Brightness);
  setLed1(led1State ? 1 : 0,led1Brightness);

  server.send(200,"text/plain","OK");

}

void handleBrightness2(){

  led2Brightness = server.arg("value").toInt();

  //analogWrite(led2, led2Brightness);
  setLed2(led2State ? 1 : 0,led2Brightness);

  server.send(200,"text/plain","OK");

}

void handleStatus(){

  String json="{";

  json += "\"led1State\":" + String(led1State ? "true":"false") + ",";
  json += "\"led2State\":" + String(led2State ? "true":"false") + ",";
  json += "\"led1Brightness\":" + String(led1Brightness) + ",";
  json += "\"led2Brightness\":" + String(led2Brightness);

  json += "}";

  server.send(200,"application/json",json);

}


void setLed1(int stateLED, int brightness){
  digitalWrite(OUT1, stateLED);
  analogWrite(PWM1, brightness);
  Serial.print("Setting LED1 state:  ");
  Serial.println(stateLED);
  Serial.print("Setting LED1 brightness:  ");
  Serial.println(brightness);
  
}
void setLed2(int stateLED, int brightness){
  digitalWrite(OUT2, stateLED);
  analogWrite(PWM2, brightness);
  Serial.print("Setting LED2 state:  ");
  Serial.println(stateLED);
  Serial.print("Setting LED2 brightness:  ");
  Serial.println(brightness);

}
void updateStatusLED() {

  unsigned long now = millis();

  switch (currentMode) {

    //  Fast blink (restore mode)
    case LED_RESTORE:
      blinkInterval = 100;   // fast blink
      if (now - previousMillis >= blinkInterval) {
        previousMillis = now;
        ledState = !ledState;
        digitalWrite(LED, ledState);
      }
      break;

    // Slow 1Hz blink (connecting)
    case LED_CONNECTING:
      blinkInterval = 500;   // 500ms ON, 500ms OFF
      if (now - previousMillis >= blinkInterval) {
        previousMillis = now;
        ledState = !ledState;
        digitalWrite(LED, ledState);
      }
      break;

    //  Solid ON
    case LED_NORMAL:
      digitalWrite(LED, LOW);
      break;

    //  One short blink for command feedback
    case LED_COMMAND_BLINK:
      if (!commandBlinkActive) {
        commandBlinkActive = true;
        commandBlinkStart = now;
        digitalWrite(LED, LOW);
      }

      if (now - commandBlinkStart > 100) {
        digitalWrite(LED, HIGH);
        currentMode = LED_NORMAL;
        commandBlinkActive = false;
      }
      break;
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  Serial.println("=== SETUP RUNNING ===");

  pinMode(OUT1, OUTPUT);
  pinMode(OUT2, OUTPUT);
  pinMode(PWM1, OUTPUT);
  pinMode(PWM2, OUTPUT);
  pinMode(LED, OUTPUT);

  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);
  pinMode(RESTORE_PIN, INPUT_PULLUP);

  loadCredentials();
  digitalWrite(LED, LOW);

  // If no credentials → AP mode
    String mac = WiFi.macAddress();
    mac.replace(":","_");
    String hostname = "Esp_Setup"+mac;
  if (ssid[0] == 0) {
    apMode = true;
    WiFi.softAP(hostname);
    Serial.println("AP Mode Started");
    //Serial.println(mac);
    currentMode = LED_RESTORE;
    Serial.println(WiFi.softAPIP());

    server.on("/", handleSetupPage);
    server.on("/save", handleSave);
  } else {
    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);  // More stable
    WiFi.begin(ssid, pass);
    Serial.print("Connecting");
    currentMode = LED_CONNECTING;
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 20) {
      delay(500);
      updateStatusLED();
      Serial.print(".");

      timeout++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected");
      currentMode = LED_NORMAL;
      Serial.println(WiFi.localIP());

      server.on("/",handleRoot);
      server.on("/toggle1",handleToggle1);
      server.on("/toggle2",handleToggle2);
      server.on("/brightness1",handleBrightness1);
      server.on("/brightness2",handleBrightness2);
      server.on("/status",handleStatus);
    } else {
      Serial.println("\nWiFi Failed → Starting AP Mode");
      /*WiFi.softAP(hostname);
      currentMode = LED_RESTORE;
      Serial.println(WiFi.softAPIP());
      server.on("/", handleSetupPage);
      server.on("/save", handleSave);*/
      restoreTriggered = true;
      currentMode = LED_RESTORE;
      clearCredentials();
      delay(1000);
      ESP.restart();
    }
  }
  server.begin();
  /*WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Init ESP-NOW*/
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(OnDataRecv);
}

// ================= LOOP =================
void loop() {

  server.handleClient();
  updateStatusLED();

  // -------- LOCAL BUTTON TOGGLE TEST --------
  if (digitalRead(BTN1) == LOW) {
    
      led1State = !led1State;
      Serial.print("Setting led1State with button:  ");
      Serial.println(led1State);
        if (led1State) {
        setLed1(1,led1Brightness);
        }
        else {
        setLed1(0,led1Brightness);
        }
      delay(300);
  }
  if (digitalRead(BTN2) == LOW) {
    
      led2State = !led2State;
      Serial.print("Setting led2State with button:  ");
      Serial.println(led2State);
        if (led2State) {
        setLed2(1,led2Brightness);
        }
        else {
        setLed2(0,led2Brightness);
        }
      delay(300);
  }


  // -------- 10 SECOND RESTORE --------
  if (digitalRead(RESTORE_PIN) == LOW) {

    if (restoreStart == 0)
      restoreStart = millis();

    if ((millis() - restoreStart > 10000) && !restoreTriggered) {
      Serial.println("Restore triggered!");
      restoreTriggered = true;
      currentMode = LED_RESTORE;
      clearCredentials();
      delay(1000);
      ESP.restart();
    }

  } else {
    restoreStart = 0;
    restoreTriggered = false;
  }
}
