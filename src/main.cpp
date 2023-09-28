/*
   ----------------------------------------------------------------------------
   TECHNOLARP - https://technolarp.github.io/
   BOMBE 7SEGMENT 01 - https://github.com/technolarp/bombe_7segment_01
   version 1.0 - 09/2023
   ----------------------------------------------------------------------------
*/

/*
   ----------------------------------------------------------------------------
   Pour ce montage, vous avez besoin de 
   1 multiplexer MCP23017 + 4 boutons poussoir
   1 afficheur 4*7segment TM1636
   1 ou + leds neopixel
   1 buzzer piezo
   ----------------------------------------------------------------------------
*/

/*
   ----------------------------------------------------------------------------
   PINOUT
   D0     NEOPIXEL
   D1     I2C SCL
   D2     I2C SDA
   D3     TM1637 DIO
   D5     TM1637 CLK
   D8     BUZZER

   MCP_A0 BOUTON_1
   MCP_A1 BOUTON_2
   MCP_A2 BOUTON_3
   MCP_A3 BOUTON_4
   ----------------------------------------------------------------------------
*/


#include <Arduino.h>

// WIFI
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);

// WEBSOCKET
AsyncWebSocket ws("/ws");

char bufferWebsocket[300];
bool flagBufferWebsocket = false;

// CONFIG
#include "config.h"
M_config aConfig;

#define BUFFERSENDSIZE 600
char bufferToSend[BUFFERSENDSIZE];

// FASTLED
#include <technolarp_fastled.h>
M_fastled aFastled;

// 7SEGMENT
#include <technolarp_7segment.h>
M_7segment a7segmentDisplay;

// MCP23017
#include <technolarp_mcp23017.h>
M_mcp23017 aMcp23017;
bool statutBouton4 = false;

// BUZZER
#define PIN_BUZZER D8
#include <technolarp_buzzer.h>
M_buzzer buzzer(PIN_BUZZER);

// DIVERS
bool uneFois = true;

uint8_t statutActuel = 8;
uint8_t statutPrecedent = 8;
uint16_t intervalTemps = 1000;

uint8_t actionFil[FILS_MAX];

// STATUTS
enum {
  OBJET_BLINK = 5,
  OBJET_ALLUME = 8,
  OBJET_ACTIF = 9,
  OBJET_EXPLOSION = 10,
  OBJET_EXPLOSE = 11,
  OBJET_SAFE = 12,
  OBJET_PAUSE = 13
};

// HEARTBEAT
uint32_t previousMillisHB;
uint32_t intervalHB;

// DECOMPTE
uint32_t previousMillisCountdown;

// FUNCTION DECLARATIONS
uint8_t indexMaxValeur(uint8_t arraySize, uint8_t arrayToSearch[]);
uint16_t checkValeur(uint16_t valeur, uint16_t minValeur, uint16_t maxValeur);
void checkCharacter(char* toCheck, const char* allowed, char replaceChar);
void bombeAllumee();
void bombeActive();
void bombeExplosion();
void bombeExplosee();
void bombePause();
void bombeSafe();
void bombeBlink();
void checkFilCoupe();
void affecteFilsAleatoires();
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len); 
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len); 
void handleWebsocketBuffer();
void notFound(AsyncWebServerRequest *request);
void sendObjectConfig();
void writeObjectConfig();
void sendNetworkConfig();
void writeNetworkConfig();
void convertStrToRGB(const char * source, uint8_t* r, uint8_t* g, uint8_t* b);
void sendMaxLed();
void sendUptime();
void sendActionFil();
void sendTempsRestant();
void sendIntervalTemps();
void sendStatut();
/*
   ----------------------------------------------------------------------------
   SETUP
   ----------------------------------------------------------------------------
*/
void setup()
{
  Serial.begin(115200);

  // VERSION
  delay(500);
  Serial.println(F(""));
  Serial.println(F(""));
  Serial.println(F("----------------------------------------------------------------------------"));
  Serial.println(F("TECHNOLARP - https://technolarp.github.io/"));
  Serial.println(F("BOMBE 7SEGMENT 01 - https://github.com/technolarp/bombe_7segment_01"));
  Serial.println(F("version 1.0 - 09/2023"));
  Serial.println(F("----------------------------------------------------------------------------"));
  
  // I2C RESET
  aConfig.i2cReset();

  // MCP23017
  aMcp23017.beginMcp23017(0);
  statutBouton4 = aMcp23017.readPin(BOUTON_4);
  
  // CONFIG OBJET
  Serial.println(F(""));
  Serial.println(F(""));
  aConfig.mountFS();
  aConfig.listDir("/");
  aConfig.listDir("/config");
  aConfig.listDir("/www");
  
  Serial.println(F("OBJECT CONFIG"));
  aConfig.printJsonFile("/config/objectconfig.txt");
  aConfig.readObjectConfig("/config/objectconfig.txt");

  Serial.println(F("NETWORK CONFIG"));
  aConfig.printJsonFile("/config/networkconfig.txt");
  aConfig.readNetworkConfig("/config/networkconfig.txt");

  // FASTLED
  aFastled.setNbLed(aConfig.objectConfig.activeLeds);
  aFastled.setBrightness(aConfig.objectConfig.brightness);
  
  // animation led de depart  
  aFastled.animationDepart(50, aFastled.getNbLed()*2, CRGB::Blue);

  // CHECK RESET OBJECT CONFIG  
  if (!aMcp23017.readPin(BOUTON_1) && !aMcp23017.readPin(BOUTON_3) )
  {
    aFastled.allLedOn(CRGB::Yellow, true);
    
    Serial.println(F(""));
    Serial.println(F("!!! RESET OBJECT CONFIG !!!"));
    Serial.println(F(""));
    aConfig.writeDefaultObjectConfig("/config/objectconfig.txt");
    sendObjectConfig();

    delay(1000);
  }
  aFastled.allLedOff();

  // CHECK RESET OBJECT CONFIG  
  if (!aMcp23017.readPin(BOUTON_2) && !aMcp23017.readPin(BOUTON_3) )
  {
    aFastled.allLedOn(CRGB::Cyan, true);
    
    Serial.println(F(""));
    Serial.println(F("!!! RESET NETWORK CONFIG !!!"));
    Serial.println(F(""));
    aConfig.writeDefaultNetworkConfig("/config/networkconfig.txt");
    sendNetworkConfig();

    delay(1000);
  }
  aFastled.allLedOff();  

  // initialiser l'aleat
  randomSeed(ESP.getCycleCount());

  // WIFI
  WiFi.disconnect(true);
  
  // AP MODE
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(aConfig.networkConfig.apIP, aConfig.networkConfig.apIP, aConfig.networkConfig.apNetMsk);
  bool apRC = WiFi.softAP(aConfig.networkConfig.apName, aConfig.networkConfig.apPassword);

  if (apRC)
  {
    Serial.println(F("AP WiFi OK"));
  }
  else
  {
    Serial.println(F("AP WiFi failed"));
  }

  // Print ESP soptAP IP Address
  Serial.print(F("softAPIP: "));
  Serial.println(WiFi.softAPIP());
  
  /*
  // CLIENT MODE POUR DEBUG
  const char* ssid = "SID";
  const char* password = "PASSWORD";
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  if (WiFi.waitForConnectResult() != WL_CONNECTED) 
  {
    Serial.println(F("WiFi Failed!"));
  }
  else
  {
    Serial.println(F("WiFi OK"));
  }
  */

  // Print ESP Local IP Address
  Serial.print(F("localIP: "));
  Serial.println(WiFi.localIP());
  
  
  // WEB SERVER
  // Route for root / web page
  server.serveStatic("/", LittleFS, "/www/").setDefaultFile("config.html");
  server.serveStatic("/config", LittleFS, "/config/");
  server.onNotFound(notFound);

  // WEBSOCKET
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  // Start server
  server.begin();

  // HEARTBEAT
  previousMillisHB = millis();
  intervalHB = 5000;

  // SERIAL
  Serial.println(F(""));
  Serial.println(F(""));
  Serial.println(F("START !!!"));
}
/*
   ----------------------------------------------------------------------------
   FIN DU SETUP
   ----------------------------------------------------------------------------
*/




/*
   ----------------------------------------------------------------------------
   LOOP
   ----------------------------------------------------------------------------
*/
void loop()
{
  // AVOID WATCHDOG
  yield();
  
  // WEBSOCKET
  ws.cleanupClients();

  // FASTLED
  aFastled.updateAnimation();
  
  // 7 SEGMENT
  a7segmentDisplay.updateAnimation();

  // CONTROL BRIGHTNESS
  aFastled.controlBrightness(aConfig.objectConfig.brightness);

  // BUZZER
  buzzer.update();
  
  // gerer le statut de la serrure
  switch (statutActuel)
  {
    case OBJET_ALLUME:
      // la bombe doit etre activée
      bombeAllumee();
      break;

    case OBJET_ACTIF:
      // la bombe est active
      bombeActive();
      break;

    case OBJET_EXPLOSION:
      // la bombe explose
      bombeExplosion();
      break;

    case OBJET_EXPLOSE:
      // la bombe a explosee
      bombeExplosee();
      break;

    case OBJET_SAFE:
      // la bombe a explosee
      bombeSafe();
      break;

    case OBJET_PAUSE:
      // la bombe est en pause
      bombePause();
      break;
      
    case OBJET_BLINK:
      // blink leds
      bombeBlink();
      break;
      
    default:
      // nothing
      break;
  }

  // traiter le buffer du websocket
  if (flagBufferWebsocket)
  {
    flagBufferWebsocket = false;
    handleWebsocketBuffer();
  }

  // HEARTBEAT
  if(millis() - previousMillisHB > intervalHB)
  {
    previousMillisHB = millis();

    // envoyer l'uptime
    sendUptime();
  }
}
/*
   ----------------------------------------------------------------------------
   FIN DU LOOP
   ----------------------------------------------------------------------------
*/





/*
   ----------------------------------------------------------------------------
   FONCTIONS ADDITIONNELLES
   ----------------------------------------------------------------------------
*/
void bombeAllumee()
{
  if (uneFois)
  {
    uneFois = false;
    Serial.println(F("BOMBE ALLUMEE"));
    
    sendStatut();

    buzzer.buzzerOff();

    // on demarre le blink vert/rien
    aFastled.setAnimation(0);
    a7segmentDisplay.setAnimation(0);
    aFastled.animationBlink02Start(600, 2000, aConfig.objectConfig.couleurs[1], CRGB::Black, true);

    a7segmentDisplay.setBlinkDoublePoint(false);
    a7segmentDisplay.setStatutDoublePoint(true);
    a7segmentDisplay.setBlinkAffichage(true);
    a7segmentDisplay.setBlinkMinutesOuSecondes(true);
    
    // affecter les fils aleatoires
    affecteFilsAleatoires();
    sendActionFil();

    aConfig.objectConfig.tempsRestant=aConfig.objectConfig.tempsInitial;
  }

  // check si le temps a changer
  // BOUTON_PIN2 appuyé, on augmente le temps
  if (aMcp23017.checkButton(BOUTON_2))
  {
    if (a7segmentDisplay.getBlinkMinutesOuSecondes())
    {
      aConfig.objectConfig.tempsInitial+=60;
    }
    else
    {
      aConfig.objectConfig.tempsInitial+=1;
    }
    
    // on ne depasse pas 99 minutes
    aConfig.objectConfig.tempsInitial=min<int16_t>(99*60, aConfig.objectConfig.tempsInitial);
  }

  // BOUTON_3  appuyé, on diminue le temps
  if (aMcp23017.checkButton(BOUTON_3))
  {
    if (a7segmentDisplay.getBlinkMinutesOuSecondes())
    {
      aConfig.objectConfig.tempsInitial-=60;
    }
    else
    {
      aConfig.objectConfig.tempsInitial-=1;
    }
    
    // on ne depasse pas 0 minutes
    aConfig.objectConfig.tempsInitial=max<int16_t>(0, aConfig.objectConfig.tempsInitial);
  }

  // check si switch m et s
  // BOUTON_1 appuyé, on switch secondes et minutes
  if (aMcp23017.checkButton(BOUTON_1))
  {
    a7segmentDisplay.setBlinkMinutesOuSecondes(!a7segmentDisplay.getBlinkMinutesOuSecondes());
  }

  // check si la bombe est activée
  // BOUTON_4 inversé, on active la bombe
  if (aMcp23017.readPin(BOUTON_4) != statutBouton4)
  {
    // iniatialiser le temps restant
    aConfig.objectConfig.tempsRestant=aConfig.objectConfig.tempsInitial;

    // on beep 2 fois
    buzzer.doubleBeep();
    
    // la bombe est maintenant active
    statutActuel = OBJET_ACTIF;
    uneFois = true;
    
    writeObjectConfig();
  }
  
  // mettre a jour l affichage
  a7segmentDisplay.showTempsRestant(aConfig.objectConfig.tempsInitial);
}

void bombeActive()
{
  if (uneFois)
  {
    uneFois = false;
    Serial.println(F("BOMBE ACTIVE"));

    sendStatut();

    intervalTemps = aConfig.objectConfig.intervalTemps;
    
    aFastled.setAnimation(0);
    a7segmentDisplay.setAnimation(0);

    a7segmentDisplay.setBlinkAffichage(false);
    a7segmentDisplay.setStatutBlinkAffichage(false);
    
    // on allume les leds rouge
    aFastled.allLedOn(aConfig.objectConfig.couleurs[0], true);

    // on fait clignoter le :
    a7segmentDisplay.setBlinkDoublePoint(true);
    a7segmentDisplay.setStatutDoublePoint(true);

    previousMillisCountdown = millis();
    
    // COUNTDOWN
    previousMillisCountdown = millis();
  }

  if ( aConfig.objectConfig.tempsRestant == -1 )
  {
    // le compte a rebours est terminé !!
    uneFois = true;
    statutActuel = OBJET_EXPLOSION;
  }
  else
  {
    // il reste du temps
    if(millis() - previousMillisCountdown > intervalTemps)
    {
      previousMillisCountdown = millis();

      // on decompte le temps restant
      aConfig.objectConfig.tempsRestant-=1;

      // maj webui
      sendTempsRestant();
   
      // beep toutes les X secondes
      if (aConfig.objectConfig.beepEvery != 0)
      {
        if (aConfig.objectConfig.tempsRestant % aConfig.objectConfig.beepEvery == 0)
        {
          buzzer.shortBeep();
        }
      }
    
      // beep toutes les secondes quand il reste Y secondes ou moins
      if (aConfig.objectConfig.beepUnder != 0)
      {
        if (aConfig.objectConfig.tempsRestant <= aConfig.objectConfig.beepUnder )
        {
          buzzer.shortBeep();
        }
      }
    }

    // mettre a jour l affichage
    a7segmentDisplay.showTempsRestant(max<int16_t>(0,aConfig.objectConfig.tempsRestant));
    
    // check fils coupes
    checkFilCoupe();
  }
}

void bombeExplosion()
{
  if (uneFois)
  {
    uneFois = false;
    Serial.println(F("BOMBE EXPLOSION"));

    sendStatut();
    
    aFastled.setAnimation(0);
    a7segmentDisplay.setAnimation(0);

    uint16_t delayAnim = 5000;

    // start buzzer
    buzzer.explosionBeep(delayAnim);
    
    // start led anim
    aFastled.animationBlink02Start(100, delayAnim, aConfig.objectConfig.couleurs[0], CRGB::Black);
    
    // start 7segment anim
    a7segmentDisplay.setBlinkDoublePoint(false);
    a7segmentDisplay.setStatutDoublePoint(false);
    a7segmentDisplay.animationBoomStart(100, delayAnim);
  }

  // fin de l'animation explosion
  if(!aFastled.isAnimActive())
  {
    uneFois = true;

    statutActuel = OBJET_EXPLOSE;

    writeObjectConfig();
    sendObjectConfig();

    Serial.println(F("END EXPLOSION "));
  }
}

void bombeExplosee()
{
  if (uneFois)
  {
    uneFois = false;
    Serial.println(F("BOMBE EXPLOSEE"));

    sendStatut();

    aFastled.setAnimation(0);
    a7segmentDisplay.setAnimation(0);
    buzzer.buzzerOff();
    
    a7segmentDisplay.setBlinkDoublePoint(false);
    a7segmentDisplay.setStatutDoublePoint(false);
    
    // on eteint les leds
    aFastled.allLedOff();

    // on affiche "- - - -"
    a7segmentDisplay.showExplosee();
  }
}

void bombePause()
{
  if (uneFois)
  {
    uneFois = false;
    Serial.println(F("PAUSE"));
          
    sendStatut();
    
    a7segmentDisplay.setBlinkDoublePoint(false);
    a7segmentDisplay.setStatutDoublePoint(true);
  }
  
  // on empeche le temps de progresser
  previousMillisCountdown = millis();
}

void bombeSafe()
{
  if (uneFois)
  {
    uneFois = false;
    sendStatut();

    aFastled.setAnimation(0);
    a7segmentDisplay.setAnimation(0);
    buzzer.buzzerOff();
    
    a7segmentDisplay.setBlinkDoublePoint(false);
    a7segmentDisplay.setStatutDoublePoint(false);
    
    // on eteint les leds
    aFastled.allLedOff();

    // on affiche "S A F E"
    a7segmentDisplay.showSafe();
  }
}

void bombeBlink()
{
  if (uneFois)
  {
    uneFois = false;
    Serial.println(F("BOMBE BLINK"));

    sendStatut();

    aFastled.animationBlink02Start(100, 3000, CRGB::Blue, CRGB::Black);
  }

  // fin de l'animation blink
  if(!aFastled.isAnimActive()) 
  {
    uneFois = true;

    statutActuel = statutPrecedent;

    writeObjectConfig();
    sendObjectConfig();

    Serial.println(F("END BLINK "));
  }
}

// check fil coupe
void checkFilCoupe()
{
  // scan all wires
  for (uint8_t i=0;i<aConfig.objectConfig.nbFilActif;i++)
  {
    // if the i wire was not previously cutted ans is now cut
    if ( (actionFil[i] != FIL_COUPE) && (aMcp23017.readPin(i)) )
    {
      switch (actionFil[i]) 
      {
        case FIL_NEUTRE:
          // wire is neutral, nothing to do
          actionFil[i]=FIL_COUPE;
          Serial.print("fil neutre: ");
          Serial.println(i);
        break;
        
        case FIL_DELAI:
          // divide ul_Interval by 2
          buzzer.doubleBeep();
          intervalTemps/=2;
          actionFil[i]=FIL_COUPE;
          sendIntervalTemps();
          Serial.print("fil delai: ");
          Serial.println(i);
        break;
    
        case FIL_SAFE:
          // si la bomme est deja au statut OBJET_EXPLOSION, on ne change pas pour OBJET_SAFE
          if (statutActuel != OBJET_EXPLOSION)
          {
            // le bomb est safe
            buzzer.shortBeep();
            statutActuel = OBJET_SAFE;            
            uneFois = true;
            Serial.print("fil safe: ");
            Serial.println(i);
          }
          actionFil[i]=FIL_COUPE;
        break;
        
        case FIL_EXPLOSION:
          // detonate the bomb
          statutActuel=OBJET_EXPLOSION;
          actionFil[i]=FIL_COUPE;
          uneFois = true;
          Serial.print("fil boom: ");
          Serial.println(i);
        break;
        
        default:
          // do nothing      
        break;
      }

      sendStatut();
      sendActionFil();
    }    
  }
}

void affecteFilsAleatoires()
{
  uint8_t filsAleatoiresCpt = 0;
  uint8_t filsAleatoiresCptCopie = 0;
  uint8_t indexfilAssignation = 0;

  uint8_t nbFilExplosionTmp=aConfig.objectConfig.nbFilExplosion;
  uint8_t nbFilSafeTmp=aConfig.objectConfig.nbFilSafe;
  uint8_t nbFilDelaiTmp=aConfig.objectConfig.nbFilDelai;

  uint8_t filAssignation[aConfig.objectConfig.nbFilActif];
  for (uint8_t i=0;i<aConfig.objectConfig.nbFilActif;i++)
  {
    filAssignation[i]=FIL_ALEATOIRE;
    actionFil[i]=aConfig.objectConfig.actionFilInit[i];
  }
  
  for (uint8_t i=0;i<aConfig.objectConfig.nbFilActif;i++)
  {
    if (aConfig.objectConfig.actionFilInit[i]==FIL_ALEATOIRE)
    {
      filsAleatoiresCpt++;
    }
  }

  filsAleatoiresCptCopie=filsAleatoiresCpt;
  
  // prepare FIL_EXPLOSION wires
  while( (filsAleatoiresCpt>0) && (nbFilExplosionTmp>0) )
  {
    filAssignation[indexfilAssignation]=FIL_EXPLOSION;
    indexfilAssignation++;
    nbFilExplosionTmp--;
    filsAleatoiresCpt--;
  }

  // prepare FIL_SAFE wires
  while( (filsAleatoiresCpt>0) && (nbFilSafeTmp>0) )
  {
    filAssignation[indexfilAssignation]=FIL_SAFE;
    indexfilAssignation++;
    nbFilSafeTmp--;
    filsAleatoiresCpt--;
  }

  // prepare FIL_DELAI wires
  while( (filsAleatoiresCpt>0) && (nbFilDelaiTmp>0) )
  {
    filAssignation[indexfilAssignation]=FIL_DELAI;
    indexfilAssignation++;
    nbFilDelaiTmp--;
    filsAleatoiresCpt--;
  }

  // create an array of random number between 10 and 50
  uint8_t tabAleatoire[aConfig.objectConfig.nbFilActif];
  for (uint8_t i=0;i<aConfig.objectConfig.nbFilActif;i++)
  {
    tabAleatoire[i]=random(10,50);
  }
    
  // assign new wire in actionFil[]  
  for (uint8_t i=0;i<aConfig.objectConfig.nbFilActif;i++)
  {
    if (actionFil[i]==FIL_ALEATOIRE)
    {
      // find the greater index in tabAleatoire
      uint8_t indexToUse = indexMaxValeur(filsAleatoiresCptCopie, tabAleatoire);
      tabAleatoire[indexToUse]=0;
      
      // assign the new wire type with the random index
      actionFil[i]=filAssignation[indexToUse];
    }
  }

  // s'il reste des fil aleatoire, on les passe en neutre
  for (uint8_t i=0;i<aConfig.objectConfig.nbFilActif;i++)
  {
    if (actionFil[i]==FIL_ALEATOIRE)
    {
      actionFil[i]=FIL_NEUTRE;
    }
  }

  Serial.print("AFFECTATION:  ");
  for (uint8_t i=0;i<aConfig.objectConfig.nbFilActif;i++)
  {
    Serial.print(actionFil[i]);
    Serial.print(" ");
  }
  Serial.println(" ");
}

// index of max value in an array
uint8_t indexMaxValeur(uint8_t arraySize, uint8_t arrayToSearch[])
{
  uint8_t indexMax=0;
  uint8_t currentMax=0;
  
  for (uint8_t i=0;i<arraySize;i++)
  {
    if (arrayToSearch[i]>=currentMax)
    {
      currentMax = arrayToSearch[i];
      indexMax = i;
    }
  }  
  return(indexMax);
}

void checkCharacter(char* toCheck, const char* allowed, char replaceChar)
{
  for (uint8_t i = 0; i < strlen(toCheck); i++)
  {
    if (!strchr(allowed, toCheck[i]))
    {
      toCheck[i]=replaceChar;
    }
    Serial.print(toCheck[i]);
  }
  Serial.println("");
}

uint16_t checkValeur(uint16_t valeur, uint16_t minValeur, uint16_t maxValeur)
{
  return(min(max(valeur,minValeur), maxValeur));
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) 
{
   switch (type) 
    {
      case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        // send config value to html
        sendObjectConfig();
        sendNetworkConfig();
        
        // send volatile info
        sendUptime();
        sendMaxLed();

        sendActionFil();
        sendTempsRestant();
        sendUptime();
        sendStatut();
    
        break;
        
      case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        break;
        
      case WS_EVT_DATA:
        handleWebSocketMessage(arg, data, len);
        break;
        
      case WS_EVT_PONG:
      case WS_EVT_ERROR:
        break;
  }
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) 
{
  
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) 
  {
    data[len] = 0;
    sprintf(bufferWebsocket,"%s\n", (char*)data);
    Serial.print(len);
    Serial.print(bufferWebsocket);
    flagBufferWebsocket = true;
  }
}

void handleWebsocketBuffer()
{    
    DynamicJsonDocument doc(JSONBUFFERSIZE);
    
    DeserializationError error = deserializeJson(doc, bufferWebsocket);
    if (error)
    {
      Serial.println(F("Failed to deserialize buffer"));
    }
    else
    {
        // write config or not
        bool writeObjectConfigFlag = false;
        bool sendObjectConfigFlag = false;
        bool writeNetworkConfigFlag = false;
        bool sendNetworkConfigFlag = false;
        
        // **********************************************
        // modif object config
        // **********************************************
        if (doc.containsKey("new_objectName"))
        {
          strlcpy(  aConfig.objectConfig.objectName,
                    doc["new_objectName"],
                    SIZE_ARRAY);
  
          writeObjectConfigFlag = true;
          sendObjectConfigFlag = true;
        }
  
        if (doc.containsKey("new_objectId")) 
        {
          uint16_t tmpValeur = doc["new_objectId"];
          aConfig.objectConfig.objectId = checkValeur(tmpValeur,1,1000);
  
          writeObjectConfigFlag = true;
          sendObjectConfigFlag = true;
        }
  
        if (doc.containsKey("new_groupId")) 
        {
          uint16_t tmpValeur = doc["new_groupId"];
          aConfig.objectConfig.groupId = checkValeur(tmpValeur,1,1000);
          
          writeObjectConfigFlag = true;
          sendObjectConfigFlag = true;
        }
  
        if (doc.containsKey("new_activeLeds")) 
        {
          aFastled.allLedOff();
          
          uint16_t tmpValeur = doc["new_activeLeds"];
          aConfig.objectConfig.activeLeds = checkValeur(tmpValeur,1,NB_LEDS_MAX);
          aFastled.setNbLed(aConfig.objectConfig.activeLeds);

          writeObjectConfigFlag = true;
          sendObjectConfigFlag = true;
        }
  
        if (doc.containsKey("new_brightness"))
        {
          uint16_t tmpValeur = doc["new_brightness"];
          aConfig.objectConfig.brightness = checkValeur(tmpValeur,0,255);
          aFastled.setBrightness(aConfig.objectConfig.brightness);
          aFastled.ledShow();
          
          writeObjectConfigFlag = true;
          sendObjectConfigFlag = true;
        }

        if (doc.containsKey("new_intervalScintillement"))
        {
          uint16_t tmpValeur = doc["new_intervalScintillement"];
          aConfig.objectConfig.intervalScintillement = checkValeur(tmpValeur,0,1000);
          aFastled.setIntervalControlBrightness(aConfig.objectConfig.intervalScintillement);
          
          writeObjectConfigFlag = true;
          sendObjectConfigFlag = true;
        }
        
        if (doc.containsKey("new_scintillementOnOff"))
        {
          uint16_t tmpValeur = doc["new_scintillementOnOff"];
          aConfig.objectConfig.scintillementOnOff = checkValeur(tmpValeur,0,1);
          aFastled.setControlBrightness(aConfig.objectConfig.scintillementOnOff);
          
          if (aConfig.objectConfig.scintillementOnOff == 0)
          {
            FastLED.setBrightness(aConfig.objectConfig.brightness);
          }
          
          writeObjectConfigFlag = true;
          sendObjectConfigFlag = true;
        }
        
        if (doc.containsKey("new_tempsRestant"))
        {
          uint16_t tmpValeur = doc["new_tempsRestant"];
          aConfig.objectConfig.tempsRestant = checkValeur(tmpValeur,0,5940);
         
          previousMillisCountdown = millis();
          
          if ( (statutActuel == OBJET_ACTIF) || (statutActuel == OBJET_PAUSE) )
          {
            a7segmentDisplay.showTempsRestant(max<int16_t>(0,aConfig.objectConfig.tempsRestant));
          }          
          sendTempsRestant();
        }

        if (doc.containsKey("new_intervalTemps"))
        {
          uint16_t tmpValeur = doc["new_intervalTemps"];
          aConfig.objectConfig.intervalTemps = checkValeur(tmpValeur,0,10000);
          intervalTemps = aConfig.objectConfig.intervalTemps;
          uneFois=true;
          
          writeObjectConfigFlag = true;
          sendObjectConfigFlag = true;
        }        

        if (doc.containsKey("new_actionFilInit")) 
        {
          JsonArray newActionFil = doc["new_actionFilInit"];
        
          uint8_t changePosition = newActionFil[0];
          uint8_t changeAction = newActionFil[1];
          
          aConfig.objectConfig.actionFilInit[changePosition]=changeAction;
          actionFil[changePosition]=changeAction;
          sendActionFil();
          
          writeObjectConfigFlag = true;
          sendObjectConfigFlag = true;
        }
        
        if (doc.containsKey("new_statutActuel"))
        {
          statutPrecedent=statutActuel;
          
          uint16_t tmpValeur = doc["new_statutActuel"];
          statutActuel=tmpValeur;

          aFastled.setNbLed(aConfig.objectConfig.activeLeds);
      
          uneFois=true;
          
          writeObjectConfigFlag = true;
          sendObjectConfigFlag = true;
        }

        if ( doc.containsKey("new_objetPause") && doc["new_objetPause"]==1 )
        {
          if (statutActuel == OBJET_ACTIF)
          {
            statutPrecedent = statutActuel;
            statutActuel = OBJET_PAUSE;

            uneFois=true;
          }
        }

        if ( doc.containsKey("new_objetUnpause") && doc["new_objetUnpause"]==1 )
        {
          Serial.println(F("UNPAUSE"));
          
          if (statutPrecedent == OBJET_ACTIF)
          {
            statutActuel = statutPrecedent;
            previousMillisCountdown = millis();
            a7segmentDisplay.setBlinkDoublePoint(true);
          }

          sendStatut();
        }
        
        if (doc.containsKey("new_couleurs")) 
        {
          JsonArray newCouleur = doc["new_couleurs"];
  
          uint8_t i = newCouleur[0];
          char newColorStr[8];
          strncpy(newColorStr, newCouleur[1], 8);
            
          uint8_t r;
          uint8_t g;
          uint8_t b;
            
          convertStrToRGB(newColorStr, &r, &g, &b);
          aConfig.objectConfig.couleurs[i].red=r;
          aConfig.objectConfig.couleurs[i].green=g;
          aConfig.objectConfig.couleurs[i].blue=b;
            
          writeObjectConfigFlag = true;
          sendObjectConfigFlag = true;
        }

        if (doc.containsKey("new_beepEvery"))
        {
          uint16_t tmpValeur = doc["new_beepEvery"];
          aConfig.objectConfig.beepEvery = checkValeur(tmpValeur,0,300);
                    
          writeObjectConfigFlag = true;
          sendObjectConfigFlag = true;
        }

        if (doc.containsKey("new_beepUnder"))
        {
          uint16_t tmpValeur = doc["new_beepUnder"];
          aConfig.objectConfig.beepUnder = checkValeur(tmpValeur,0,60);
                    
          writeObjectConfigFlag = true;
          sendObjectConfigFlag = true;
        }

        if (doc.containsKey("new_tempsInitial"))
        {
          uint16_t tmpValeur = doc["new_tempsInitial"];
          aConfig.objectConfig.tempsInitial = checkValeur(tmpValeur,0,5940);
                    
          writeObjectConfigFlag = true;
          sendObjectConfigFlag = true;
        }

        if (doc.containsKey("new_nbFilActif"))
        {
          uint16_t tmpValeur = doc["new_nbFilActif"];
          aConfig.objectConfig.nbFilActif = checkValeur(tmpValeur,0,8);
                    
          sendActionFil();
          
          writeObjectConfigFlag = true;
          sendObjectConfigFlag = true;
        }

        if ( doc.containsKey("new_filAleatoire") && doc["new_filAleatoire"]==1 )
        {
          Serial.println(F("RESET fil Aleatoire"));
          
          affecteFilsAleatoires();
          sendActionFil();
          
          writeObjectConfigFlag = false;
          sendObjectConfigFlag = true;
        }

        if (doc.containsKey("new_nbFilExplosion"))
        {
          uint16_t tmpValeur = doc["new_nbFilExplosion"];
          aConfig.objectConfig.nbFilExplosion = checkValeur(tmpValeur,0,8);
                    
          sendActionFil();
          
          writeObjectConfigFlag = true;
          sendObjectConfigFlag = true;
        }

        if (doc.containsKey("new_nbFilSafe"))
        {
          uint16_t tmpValeur = doc["new_nbFilSafe"];
          aConfig.objectConfig.nbFilSafe = checkValeur(tmpValeur,0,8);
                    
          sendActionFil();
          
          writeObjectConfigFlag = true;
          sendObjectConfigFlag = true;
        }

        if (doc.containsKey("new_nbFilDelai"))
        {
          uint16_t tmpValeur = doc["new_nbFilDelai"];
          aConfig.objectConfig.nbFilDelai = checkValeur(tmpValeur,0,8);
                    
          sendActionFil();
          
          writeObjectConfigFlag = true;
          sendObjectConfigFlag = true;
        }
          
        // **********************************************
        // modif network config
        // **********************************************
        if (doc.containsKey("new_apName")) 
        {
          strlcpy(  aConfig.networkConfig.apName,
                    doc["new_apName"],
                    sizeof(aConfig.networkConfig.apName));
        
          // check for unsupported char
          const char listeCheck[] = "ABCDEFGHIJKLMNOPQRSTUVWYXZ0123456789_-";
          checkCharacter(aConfig.networkConfig.apName, listeCheck, 'A');
          
          writeNetworkConfigFlag = true;
          sendNetworkConfigFlag = true;
        }
        
        if (doc.containsKey("new_apPassword")) 
        {
          strlcpy(  aConfig.networkConfig.apPassword,
                    doc["new_apPassword"],
                    sizeof(aConfig.networkConfig.apPassword));
        
          writeNetworkConfigFlag = true;
          sendNetworkConfigFlag = true;
        }
        
        if (doc.containsKey("new_apIP")) 
        {
          char newIPchar[16] = "";
        
          strlcpy(  newIPchar,
                    doc["new_apIP"],
                    sizeof(newIPchar));
        
          IPAddress newIP;
          if (newIP.fromString(newIPchar))
          {
            Serial.println("valid IP");
            aConfig.networkConfig.apIP = newIP;
        
            writeNetworkConfigFlag = true;
          }
          
          sendNetworkConfigFlag = true;
        }
        
        if (doc.containsKey("new_apNetMsk")) 
        {
          char newNMchar[16] = "";
        
          strlcpy(  newNMchar,
                    doc["new_apNetMsk"],
                    sizeof(newNMchar));
        
          IPAddress newNM;
          if (newNM.fromString(newNMchar)) 
          {
            Serial.println("valid netmask");
            aConfig.networkConfig.apNetMsk = newNM;
        
            writeNetworkConfigFlag = true;
          }
        
          sendNetworkConfigFlag = true;
        }
        
        // actions sur le esp8266
        if ( doc.containsKey("new_restart") && doc["new_restart"]==1 )
        {
          Serial.println(F("RESTART RESTART RESTART"));
          ESP.restart();
        }
        
        if ( doc.containsKey("new_refresh") && doc["new_refresh"]==1 )
        {
          Serial.println(F("REFRESH"));
        
          sendObjectConfigFlag = true;
          sendNetworkConfigFlag = true;
        }
        
        if ( doc.containsKey("new_defaultObjectConfig") && doc["new_defaultObjectConfig"]==1 )
        {
          aConfig.writeDefaultObjectConfig("/config/objectconfig.txt");
          Serial.println(F("reset to default object config"));
        
          aFastled.allLedOff();
          aFastled.setNbLed(aConfig.objectConfig.activeLeds);          
          aFastled.setControlBrightness(aConfig.objectConfig.scintillementOnOff);
          aFastled.setIntervalControlBrightness(aConfig.objectConfig.intervalScintillement);
          
          sendObjectConfigFlag = true;
          uneFois = true;
        }
        
        if ( doc.containsKey("new_defaultNetworkConfig") && doc["new_defaultNetworkConfig"]==1 )
        {
          aConfig.writeDefaultNetworkConfig("/config/networkconfig.txt");
          Serial.println(F("reset to default network config"));          
          
          sendNetworkConfigFlag = true;
        }
        
        // modif config
        // write object config
        if (writeObjectConfigFlag)
        {
          writeObjectConfig();
        
          // update statut
          uneFois = true;
        }
        
        // resend object config
        if (sendObjectConfigFlag)
        {
          sendObjectConfig();
        }
        
        // write network config
        if (writeNetworkConfigFlag)
        {
          writeNetworkConfig();
        }
        
        // resend network config
        if (sendNetworkConfigFlag)
        {
          sendNetworkConfig();
        }
    }
 
    // clear json buffer
    doc.clear();
}

void notFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "Not found");
}

void sendObjectConfig()
{
  aConfig.stringJsonFile("/config/objectconfig.txt", bufferToSend, BUFFERSENDSIZE);
  ws.textAll(bufferToSend);
}

void writeObjectConfig()
{
  aConfig.writeObjectConfig("/config/objectconfig.txt");
}

void sendNetworkConfig()
{
  aConfig.stringJsonFile("/config/networkconfig.txt", bufferToSend, BUFFERSENDSIZE);
  ws.textAll(bufferToSend);
}

void writeNetworkConfig()
{
  aConfig.writeNetworkConfig("/config/networkconfig.txt");
}

void convertStrToRGB(const char * source, uint8_t* r, uint8_t* g, uint8_t* b)
{ 
  uint32_t  number = (uint32_t) strtol( &source[1], NULL, 16);
  
  // Split them up into r, g, b values
  *r = number >> 16;
  *g = number >> 8 & 0xFF;
  *b = number & 0xFF;
}

void sendMaxLed()
{
  char toSend[20];
  snprintf(toSend, 20, "{\"maxLed\":%i}", NB_LEDS_MAX);
  
  ws.textAll(toSend);
}

void sendUptime()
{
  uint32_t now = millis() / 1000;
  uint16_t days = now / 86400;
  uint16_t hours = (now%86400) / 3600;
  uint16_t minutes = (now%3600) / 60;
  uint16_t seconds = now % 60;
    
  char toSend[100];
  snprintf(toSend, 100, "{\"uptime\":\"%id %ih %im %is\"}", days, hours, minutes, seconds);

  ws.textAll(toSend);
}

void sendActionFil()
{
  char toSend[150];
  snprintf(toSend, 100, "{\"actionFil\":[%i,%i,%i,%i,%i,%i,%i,%i]}", 
                          actionFil[0],
                          actionFil[1],
                          actionFil[2],
                          actionFil[3],
                          actionFil[4],
                          actionFil[5],
                          actionFil[6],
                          actionFil[7]
                          );
  ws.textAll(toSend);
}

void sendTempsRestant()
{
  char toSend[50];
  snprintf(toSend, 100, "{\"tempsRestant\":%i}", aConfig.objectConfig.tempsRestant);

  ws.textAll(toSend);
}

void sendIntervalTemps()
{
  char toSend[50];
  snprintf(toSend, 100, "{\"intervalTemps\":%i}", intervalTemps);

  ws.textAll(toSend);
}

void sendStatut()
{
  char toSend[100];
  snprintf(toSend, 100, "{\"statutActuel\":%i}", statutActuel); 

  ws.textAll(toSend);
}
/*
   ----------------------------------------------------------------------------
   FIN DES FONCTIONS ADDITIONNELLES
   ----------------------------------------------------------------------------
*/
