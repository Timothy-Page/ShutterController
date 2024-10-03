  #include <ESP8266WiFi.h>
  #include <ArduinoMqttClient.h>
  #include <ESP_EEPROM.h>

  //Set debugMode to set if controller will send serial messages or not.
  const bool debugMode = false;

  //Set network setting
  const char* ssid = "GalaxyHidden";
  const char* password = "kf3sY3PN";

  //Set mqtt broker settings
  const char* broker = "10.37.20.20";
  int port = 1883;
  const char* topic = "MeyersCres/Shutters/Log";
  const long interval = 1000; //mqtt timeout duration
  const char* Shutter1RequestTopic = "MeyersCres/Shutters/Shutter1";
  const char* Shutter2RequestTopic = "MeyersCres/Shutters/Shutter2";
  const char* Shutter3RequestTopic = "MeyersCres/Shutters/Shutter3";
  const char* Shutter1StatusTopic = "MeyersCres/Shutters/Shutter1/Status";
  const char* Shutter2StatusTopic = "MeyersCres/Shutters/Shutter2/Status";
  const char* Shutter3StatusTopic = "MeyersCres/Shutters/Shutter3/Status";

  const char* RequestTopics[] = {Shutter1RequestTopic, Shutter2RequestTopic, Shutter3RequestTopic};
  const char* StatusTopics[] = {Shutter1StatusTopic, Shutter2StatusTopic, Shutter3StatusTopic};

  WiFiClient wifiClient;
  MqttClient mqttClient(wifiClient);

  unsigned long previousReadMillis = 0;
  int count = 0;
  
  //Board pin map
  //const int D0 = 16;
  //const int D1 = 5;
  //const int D2 = 4;
  //const int D3 = 0;
  //const int D4 = 2;
  //const int D5 = 14;
  //const int D6 = 12;
  //const int D7 = 13;
  //const int D8 = 15;

  //Shutter Data
  const int Relay1Pin = D0;
  const int Relay2Pin = D5;
  const int Relay3Pin = D6;
  const int Relay4Pin = D7;
  const int Relay5Pin = D1;
  const int Relay6Pin = D2;
  const int Relay7Pin = D3;
  const int Relay8Pin = D4;
  const int ShuntPin = A0;

  const int Shutter1A = Relay3Pin;
  const int Shutter1B = Relay6Pin;
  const int Shutter2A = Relay2Pin;
  const int Shutter2B = Relay5Pin;
  const int Shutter3A = Relay1Pin;
  const int Shutter3B = Relay4Pin;
  
  const int PowerA = Relay7Pin;
  const int PowerB = Relay8Pin;

  int Shutter1StateAddress = 0;
  int Shutter2StateAddress = 4;
  int Shutter3StateAddress = 8;

  //Motor Side true=right, false=left
  const bool Shutter1Side = true;
  const bool Shutter2Side = true;
  const bool Shutter3Side = false;

  const int ShutterPinsA[] = {Shutter1A, Shutter2A, Shutter3A};
  const int ShutterPinsB[] = {Shutter1B, Shutter2B, Shutter3B};
  const int ShutterSides[] = {Shutter1Side, Shutter2Side, Shutter3Side};
  const int ShutterAddress[] = {Shutter1StateAddress,Shutter2StateAddress,Shutter3StateAddress};

  const int ShuntMaxDown = 25;
  const int ShuntBoarderDown = 24;
  const int ShuntMaxUp = 45;
  const int ShuntBoarderUp = 40;
  const int MiniTime = 3;

  const int ShutterMaxRuntimeUp = 30000;
  const int ShutterMaxRuntimeDown = 24000;
  const int ShutterRuntimeHalfUp = 5000;

  int PreviousShuntValue;

  
void connectWiFi() {

  WiFi.begin(ssid, password);
  if (debugMode) {
    Serial.print("connecting to ");
    Serial.print(ssid);
    Serial.println(" ...");
  }

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) { // Wait for the Wi-Fi to connect
    delay(1000);
    if (debugMode) {
      Serial.print(++i); Serial.print(' ');
    }
  }

  if (debugMode) {
    Serial.println('\n');
    Serial.println("Conection established!");
    Serial.print("IP address:\t");
    Serial.print(WiFi.localIP());
  }
}

void connectMqttBroker() {
  
  if (debugMode) {
    Serial.print("Attempting to connect to the MQTT broker: ");
    Serial.println(broker);
  }

  if (!mqttClient.connect(broker, port)) {
    if (debugMode) {
      Serial.print("MQTT connection failed! Error code = ");
      Serial.println(mqttClient.connectError());
    }

    while (1);
  }
  if (debugMode) {
    Serial.println("You're connected to the MQTT broker!");
    Serial.println();
  }
}

void mqttClientSubscribe(){

  mqttClient.subscribe(RequestTopics[0]);
  mqttClient.subscribe(RequestTopics[1]);
  mqttClient.subscribe(RequestTopics[2]);
  mqttClient.subscribe(StatusTopics[0]);
  mqttClient.subscribe(StatusTopics[1]);
  mqttClient.subscribe(StatusTopics[2]);

}

void setPinmodes(){
  pinMode(Shutter1A, OUTPUT);
  pinMode(Shutter1B, OUTPUT);
  pinMode(Shutter2A, OUTPUT);
  pinMode(Shutter2B, OUTPUT);
  pinMode(Shutter3A, OUTPUT);
  pinMode(Shutter3B, OUTPUT);
  pinMode(PowerA, OUTPUT);
  pinMode(PowerB, OUTPUT);

  digitalWrite(Shutter1A, LOW);
  digitalWrite(Shutter1B, LOW);
  digitalWrite(Shutter2A, LOW);
  digitalWrite(Shutter2B, LOW);
  digitalWrite(Shutter3A, LOW);
  digitalWrite(Shutter3B, LOW);
  digitalWrite(PowerA, LOW);
  digitalWrite(PowerB, LOW);
  
  RelayDirection(false); // Set direction relays to down
}

void setup() {
  
  Serial.begin(9600);
  EEPROM.begin(16);

  if (debugMode) {
    Serial.begin(9600);
    delay(100);
    Serial.println('\n');
    Serial.println("Controller starting");
  }

  setPinmodes();

  connectWiFi();
  connectMqttBroker(); 
  mqttClientSubscribe();
  
  mqttClient.beginMessage(topic);
  mqttClient.print("Controller Starting");
  mqttClient.endMessage();
  
  SendShutterStatus();
}

void loop() {
  //Check WiFi still connected
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }
  //Check Mqtt broker still connected
  if (!mqttClient.connected()) {
    connectMqttBroker();
    mqttClientSubscribe();
  }


  checkMqttMessages();
  //ReadShunt();
  //generateSampleMqttMessage();

}

void checkMqttMessages(){

  int messageSize = mqttClient.parseMessage();
  if (messageSize) {

    String messageTopic = mqttClient.messageTopic();
    if (debugMode) {
      Serial.print("Received a message with topic '");
      Serial.print(messageTopic);
      Serial.print("', length ");
      Serial.print(messageSize);
      Serial.println(" bytes:");
    }

    // use the Stream interface to print the contents
    String messageString = "";
    while (mqttClient.available()) {
      messageString = messageString + (char)mqttClient.read();
    }
    if (debugMode) {
      Serial.print(messageString);
      Serial.println();
      Serial.println();
    }

    int ShutterCounter = 0;
    int ShutterIndex;
    bool WasRequest = false;

    while (ShutterCounter < 3) {
      if (messageTopic.equals(RequestTopics[ShutterCounter])) {
        ShutterIndex = ShutterCounter;
        WasRequest = true;
        break;
      }
      ShutterCounter += 1;
    }

    bool SetDirection = false;
    if (messageString.equals("0")) {
      SetDirection = false;
    } else if (messageString.equals("1")) {
      SetDirection = true;
    } else {
      if (debugMode) {
        Serial.println("MQTT message was not 0 or 1");
        return;
      }
    }

  if (WasRequest) {
    int Shutter = ShutterIndex + 1;
    MoveShutter(SetDirection, Shutter);
  }

  }
}

void RelayDirection(boolean SetDirection) {
  if (SetDirection == true)
  {
    digitalWrite(PowerA, HIGH);
    delay(50);
    digitalWrite(PowerB, HIGH);
    delay(50);
  }
  else
  {
    digitalWrite(PowerA, LOW);
    delay(50);
    digitalWrite(PowerB, LOW);
    delay(50);
  }
  
  CurrentDirection = SetDirection;
  
}

void ShutterOn(int ShutterIndex){
  
  int PinA = ShutterPinsA[ShutterIndex];
  int PinB = ShutterPinsB[ShutterIndex];
  
  if (CurrentDirection == false)
  {
    digitalWrite(PinB, HIGH);
    delay(50);
    digitalWrite(PinA, HIGH);
    delay(50);
  }
  else
  {
    digitalWrite(PinA, HIGH);
    delay(50);
    digitalWrite(PinB, HIGH);
    delay(50);
  }
  
}

void ShutterOff(int ShutterIndex) {
  
  int PinA = ShutterPinsA[ShutterIndex];
  int PinB = ShutterPinsB[ShutterIndex];
  
  if (CurrentDirection == false)
  {
    digitalWrite(PinA, LOW);
    delay(50);
    digitalWrite(PinB, LOW);
    delay(50);
  }
  else
  {
    digitalWrite(PinB, LOW);
    delay(50);
    digitalWrite(PinA, LOW);
    delay(50);
  }
  
}

void MoveShutter(boolean SetDirection, int Shutter){
  if (debugMode) {
    Serial.print("Set Direction: ");
    Serial.println(SetDirection);
    Serial.print("Shutter: ");
    Serial.println(Shutter);
  }

  int ShutterIndex = Shutter - 1;
  bool ActualDirection;

  int currentShutterDirection;
  EEPROM.get(ShutterAddress[ShutterIndex],currentShutterDirection);
  if (debugMode) {
    Serial.print("Current Direction: ");
    Serial.println(currentShutterDirection);
  }

  if (currentShutterDirection == (int)SetDirection) {
    if (debugMode) {
      Serial.println("Shutter Already in set direction");
    }
    return;
  }

  //Adjust direction for left/right mounted motors
  if (ShutterSides[ShutterIndex]) {
    ActualDirection = SetDirection;
  }
  else {
    ActualDirection = !SetDirection;
  }

  int MaxRuntime = 0;
  if (SetDirection == true)
  {
    MaxRuntime = ShutterMaxRuntimeUp;
    //delay(25000);
  }
  else
  {
    MaxRuntime = ShutterMaxRuntimeDown;
    //delay(23000);
  }
  
  unsigned long startMillis = millis();
  unsigned long currentMillis = millis();
  bool limitFound = false;
  PreviousShuntValue = 60;
  int FinalShuntValue = 0;

  RelayDirection(ActualDirection);

  if (debugMode) {
    Serial.println("Starting Shutter");
  }

  mqttClient.beginMessage(topic);
  mqttClient.print("Shutter Moving");
  mqttClient.endMessage();
  int ShuntMax = 0;
  int ShuntBoarder = 0;

  if (SetDirection) {
    ShuntMax = ShuntMaxUp;
    ShuntBoarder = ShuntBoarderUp;
  } else {
    ShuntMax = ShuntMaxDown;
    ShuntBoarder = ShuntBoarderDown;
  }

  ShutterOn(ShutterIndex);

  delay(5);

  if (debugMode) {
    Serial.print("Limit Found: ");
    Serial.println(limitFound);
    Serial.print("Current RunTime: ");
    Serial.println(currentMillis - startMillis);
  }

  while ((!limitFound) && (currentMillis - startMillis < MaxRuntime)) {
    currentMillis = millis();
    int CurrentShuntValue = ReadShunt();
    FinalShuntValue = CurrentShuntValue;

    if ((CurrentShuntValue > ShuntMax) || ((CurrentShuntValue > PreviousShuntValue) && (CurrentShuntValue > ShuntBoarder))) {
      limitFound = true;
      break;
    }

    PreviousShuntValue = CurrentShuntValue;
  }

  ShutterOff(ShutterIndex);
  RelayDirection(false);

  mqttClient.beginMessage(topic);
  mqttClient.print("Shutter Stopping ");
  mqttClient.print("Final Shunt Value ");
  mqttClient.print(FinalShuntValue);
  mqttClient.endMessage();

  if (debugMode) {
    Serial.println("Stopping Shutter");
  }

  EEPROM.put(ShutterAddress[ShutterIndex], (int)SetDirection);
  bool committed = EEPROM.commit();

  if (!committed){
    mqttClient.beginMessage(topic);
    mqttClient.print("EEPROM Update Failed");
    mqttClient.endMessage();
  } else {
    mqttClient.beginMessage(topic);
    mqttClient.print("EEPROM Updated");
    mqttClient.endMessage();
  }

  mqttClient.beginMessage(StatusTopics[ShutterIndex]);
  mqttClient.print(SetDirection);
  mqttClient.endMessage();

  mqttClient.beginMessage(topic);
  mqttClient.print("Status Updated");
  mqttClient.endMessage();

  SendShutterStatus();

}

int ReadShunt() {
  int ShuntValue = 0;
  unsigned long CurrentMillis = millis();
  unsigned long MillisSinceLastRead = CurrentMillis - previousReadMillis;

  if (MillisSinceLastRead < MiniTime){
    delay(MiniTime-MillisSinceLastRead);
  }
  previousReadMillis = millis();

  ShuntValue = analogRead(ShuntPin);

  if (debugMode) {
    Serial.println(ShuntValue);
  }

  return ShuntValue;
}

void SendShutterStatus(){
  int loopCounter = 0;

  while (loopCounter < 3) {
    int shutterNumber = loopCounter + 1;

    int currentShutterDirection;
    EEPROM.get(ShutterAddress[loopCounter],currentShutterDirection);

    mqttClient.beginMessage(topic);
    mqttClient.print("Shutter" + (String)shutterNumber + ": " + (String)currentShutterDirection + ", ");
    mqttClient.endMessage();

    loopCounter += 1;
  }
}
