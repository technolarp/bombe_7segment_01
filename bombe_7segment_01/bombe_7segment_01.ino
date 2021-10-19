 /*
   ----------------------------------------------------------------------------
   TECHNOLARP - https://technolarp.github.io/
   BOMBE 7SEGMENT 01 - https://github.com/technolarp/bombe_7segment_01
   version 1.0 - 09/2021
   ----------------------------------------------------------------------------
*/

/*
   ----------------------------------------------------------------------------
   Pour ce montage, vous avez besoin de 
   1 multiplexer MCP23017 + 4 boutons poussoir
   1 afficheur 4*7segement TM1636
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
   ----------------------------------------------------------------------------
*/

#include <Arduino.h>

// WIFI
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);

// WEBSOCKET
AsyncWebSocket ws("/ws");

// TASK SCHEDULER
#define _TASK_OO_CALLBACKS
#include <TaskScheduler.h>
Scheduler globalScheduler;

// CONFIG
#include "config.h"
M_config aConfig;

// LED RGB
#include <technolarp_fastled.h>
M_fastled* aFastled;

// 7SEGMENT
#include <technolarp_7segment.h>
M_7segment a7segmentDisplay;

// MCP23017
#include "technolarp_mcp23017.h"
M_mcp23017 aMcp23017(0);

// BUZZER
#define PIN_BUZZER D8
#include <technolarp_buzzer.h>
M_buzzer* buzzer;

// DIVERS
int16_t tempsRestant;
bool uneFois = true;
bool blinkUneFois = true;
bool explosionUneFois = true;
bool blinkMinutesOuSecondes = false;

//int16_t tempsInitialTmp;
uint8_t actionFil[FILS_MAX];

// STATUTS
enum {
  BOMBE_ALLUMEE = 0,
  BOMBE_ACTIVE = 1,
  BOMBE_EXPLOSION = 2,
  BOMBE_EXPLOSEE = 3,
  BOMBE_SAFE = 4,
  BOMBE_PAUSE = 5,
  BOMBE_UNPAUSE = 6,
  BOMBE_BLINK = 7
};

uint8_t statutBombe = BOMBE_ALLUMEE;
uint8_t statutBombePrecedent = BOMBE_ALLUMEE;

// HEARTBEAT
unsigned long int previousMillisHB;
unsigned long int currentMillisHB;
unsigned long int intervalHB;

// REFRESH
unsigned long int previousMillisRefresh;
unsigned long int currentMillisRefresh;
unsigned long int intervalRefresh;

// DOUBLE POINT
unsigned long int ul_PreviousMillisDoublePoint;
unsigned long int ul_CurrentMillisDoublePoint;
unsigned long int ul_IntervalDoublePoint;

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
  Serial.println(F("version 1.0 - 09/2021"));
  Serial.println(F("----------------------------------------------------------------------------"));
  
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

  // LED RGB
  aFastled = new M_fastled(&globalScheduler);
  aFastled->setNbLed(aConfig.objectConfig.activeLeds);
  aFastled->setBrightness(aConfig.objectConfig.brightness);

  // animation led de depart  
  aFastled->allLedOff();
  for (int i = 0; i < aConfig.objectConfig.activeLeds * 2; i++)
  {
    aFastled->ledOn(i % aConfig.objectConfig.activeLeds, CRGB::Blue);
    delay(50);
    aFastled->ledOn(i % aConfig.objectConfig.activeLeds, CRGB::Black);
  }
  aFastled->allLedOff();

  // CHECK RESET OBJECT CONFIG  
  if (!aMcp23017.readPin(BOUTON_1) && !aMcp23017.readPin(BOUTON_4) )
  {
    aFastled->allLedOn(CRGB::Yellow);
    
    Serial.println(F(""));
    Serial.println(F("!!! RESET OBJECT CONFIG !!!"));
    Serial.println(F(""));
    aConfig.writeDefaultObjectConfig("/config/objectconfig.txt");
    aConfig.printJsonFile("/config/objectconfig.txt");

    delay(1000);
  }
  aFastled->allLedOff();

  // CHECK RESET OBJECT CONFIG  
  if (!aMcp23017.readPin(BOUTON_2) && !aMcp23017.readPin(BOUTON_3) )
  {
    aFastled->allLedOn(CRGB::Cyan);
    
    Serial.println(F(""));
    Serial.println(F("!!! RESET NETWORK CONFIG !!!"));
    Serial.println(F(""));
    aConfig.writeDefaultObjectConfig("/config/networkconfig.txt");
    aConfig.printJsonFile("/config/networkconfig.txt");

    delay(1000);
  }
  aFastled->allLedOff();

  // BUZZER
  buzzer = new M_buzzer(PIN_BUZZER, &globalScheduler);
  buzzer->doubleBeep();

  // initialiser l'aleat
  randomSeed(ESP.getCycleCount());

  // WIFI
  WiFi.disconnect(true);
  
  
  // AP MODE
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(aConfig.networkConfig.apIP, aConfig.networkConfig.apIP, aConfig.networkConfig.apNetMsk);
  WiFi.softAP(aConfig.networkConfig.apName, aConfig.networkConfig.apPassword);
  
  
  /*
  // CLIENT MODE POUR DEBUG
  const char* ssid = "MYDEBUG";
  const char* password = "pppppp";
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  if (WiFi.waitForConnectResult() != WL_CONNECTED) 
  {
    Serial.printf("WiFi Failed!\n");        
  }
*/
  
  // WEB SERVER
  // Print ESP Local IP Address
  Serial.print(F("localIP: "));
  Serial.println(WiFi.localIP());
  Serial.print(F("softAPIP: "));
  Serial.println(WiFi.softAPIP());

  // Route for root / web page
  server.serveStatic("/", LittleFS, "/www/").setDefaultFile("config.html").setTemplateProcessor(processor);
  server.serveStatic("/config", LittleFS, "/config/");
  server.onNotFound(notFound);

  // WEBSOCKET
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  // Start server
  server.begin();

  // HEARTBEAT
  currentMillisHB = millis();
  previousMillisHB = currentMillisHB;
  intervalHB = 5000;

  // double point
  ul_PreviousMillisDoublePoint = millis();
  ul_CurrentMillisDoublePoint = previousMillisHB;
  ul_IntervalDoublePoint = 500;

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
  // avoid watchdog reset
  yield();
  
  // WEBSOCKET
  ws.cleanupClients();
  
  // manage task scheduler
  globalScheduler.execute();

  // gerer le statut de la serrure
  switch (statutBombe)
  {
    case BOMBE_ALLUMEE:
      // la bombe doit etre activée
      bombeAllumee();
      break;

    case BOMBE_ACTIVE:
      // la bombe est active
      bombeActive();
      break;

    case BOMBE_EXPLOSION:
      // la bombe explose
      bombeExplosion();
      break;

     case BOMBE_EXPLOSEE:
      // la bombe a explosee
      bombeExplosee();
      break;

      case BOMBE_SAFE:
      // la bombe a explosee
      bombeSafe();
      break;

      case BOMBE_PAUSE:
      // la bombe est en pause
      bombePause();
      break;

      case BOMBE_UNPAUSE:
      // la bombe reprend son comtpe a rebours
      bombeUnpause();
      break;

      case BOMBE_BLINK:
      // blink leds
      bombeBlink();
      break;
      
    default:
      // nothing
      break;
  }

  // HEARTBEAT
  currentMillisHB = millis();
  if(currentMillisHB - previousMillisHB > intervalHB)
  {
    previousMillisHB = currentMillisHB;

    // envoyer l'uptime
    sendUptime();
        
    //Serial.println("heartbeat");
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

    // on allume les leds verte
    aFastled->allLedOn(aConfig.objectConfig.couleur1);

    a7segmentDisplay.setDoublePoint(false);
    
    // affecter les fils aleatoires
    affecteFilsAleatoires();
    sendActionFil();
    
    // REFRESH
    currentMillisRefresh = millis();
    previousMillisRefresh = currentMillisRefresh;
    intervalRefresh = 300;

    tempsRestant=aConfig.objectConfig.tempsInitial;

    Serial.print(F("BOMBE ALLUMEE"));
    Serial.println();
  }

  // check si le temps a changer
  // BOUTON_HAUT_PIN2 appuyé, on augmente le temps
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

  // check si switch
  // BOUTON_1 appuyé, on switch secondes et minutes
  if (aMcp23017.checkButton(BOUTON_1))
  {
    a7segmentDisplay.setBlinkMinutesOuSecondes(!a7segmentDisplay.getBlinkMinutesOuSecondes());
  }

  // check si la bombe est activée
  // BOUTON_ACTIVATION_PIN appuyé, on active la bombe
  if (aMcp23017.checkButton(BOUTON_4))
  {
    // la bombe est maintenant active
    statutBombe = BOMBE_ACTIVE;
    sendStatutBombe();
        
    uneFois = true;
    
    // ecrire la nouvelle config
    aConfig.writeObjectConfig("/config/objectconfig.txt");
  }

  currentMillisRefresh = millis();
  if(currentMillisRefresh - previousMillisRefresh > intervalRefresh)
  {
    previousMillisRefresh = currentMillisRefresh;
    a7segmentDisplay.setBlinkAffichage(!a7segmentDisplay.getBlinkAffichage());

    // on allume les leds verte
    aFastled->allLedOn(aConfig.objectConfig.couleur1);
  }
  
  // mettre a jour l affichage
  a7segmentDisplay.showTempsRestant(aConfig.objectConfig.tempsInitial);
}

void bombeActive()
{
  if (uneFois)
  {
    uneFois = false;

    // on allume les leds rouge
    aFastled->allLedOn(aConfig.objectConfig.couleur2);
    
    Serial.print(F("BOMBE ACTIVE"));
    Serial.println();

    tempsRestant=aConfig.objectConfig.tempsInitial;
    previousMillisRefresh = millis();
    a7segmentDisplay.setBlinkAffichage(false);
    
    // REFRESH
    currentMillisRefresh = millis();
    previousMillisRefresh = currentMillisRefresh;
    intervalRefresh=1000;
  }

  if ( tempsRestant == -1 )
  {
    // le compte a rebours est terminé !!
    uneFois = true;
    explosionUneFois = true;
    statutBombe = BOMBE_EXPLOSION;
    sendStatutBombe();
  }
  else
  {
    // il reste du temps
    currentMillisRefresh = millis();
    if(currentMillisRefresh - previousMillisRefresh > aConfig.objectConfig.intervalTemps)
    {
      previousMillisRefresh = currentMillisRefresh;

      // on decompte le temps restant
      tempsRestant-=1;
   
      // beep toutes les X secondes
      if (aConfig.objectConfig.beepEvery != 0)
      {
        if (tempsRestant % aConfig.objectConfig.beepEvery == 0)
        {
          buzzer->shortBeep();
        }
      }
    
      // beep toutes les secondes quand il reste Y secondes ou moins
      if (aConfig.objectConfig.beepUnder != 0)
      {
        if (tempsRestant <= aConfig.objectConfig.beepUnder )
        {
          buzzer->shortBeep();
        }
      }
    }
  
    // blinker le :
    ul_CurrentMillisDoublePoint = millis();
    if(ul_CurrentMillisDoublePoint - ul_PreviousMillisDoublePoint > ul_IntervalDoublePoint)
    {
      ul_PreviousMillisDoublePoint = ul_CurrentMillisDoublePoint;
      a7segmentDisplay.setDoublePoint(!a7segmentDisplay.getDoublePoint());

      // on allume les leds rouge
      aFastled->allLedOn(aConfig.objectConfig.couleur2);
    }

    // mettre a jour l affichage
    a7segmentDisplay.showTempsRestant(max<int16_t>(0,tempsRestant));
    sendTempsRestant();

    // check fils coupes
    checkFilCoupe();
  }
}

void bombeExplosion()
{
  if (uneFois)
  {
    uneFois = false;

    Serial.print(F("BOMBE EXPLOSION"));
    Serial.println();

    // on demarre l'animation led
    if (!aFastled->isEnabled() && explosionUneFois)
    {
      explosionUneFois = false;

      // on buzze 5 second
      buzzer->beep(800, 5000, 2);

      // on blink lesleds
      aFastled->startAnimBlink(50, 100, aConfig.objectConfig.couleur2, aConfig.objectConfig.activeLeds);

      a7segmentDisplay.setBlinkAffichage(false);
    }
  }

  // on blink l'afficheur 7 segment
  currentMillisRefresh = millis();
  previousMillisRefresh = currentMillisRefresh;
  intervalRefresh=200;
    
  if(currentMillisRefresh - previousMillisRefresh > intervalRefresh)
  {
    previousMillisRefresh = currentMillisRefresh;
    a7segmentDisplay.setBlinkAffichage(!a7segmentDisplay.getBlinkAffichage());
    a7segmentDisplay.showExplosion();
  }

  if (!aFastled->isAnimActive())
  {
    Serial.println(F("END TASK EXPLOSION"));
  
    uneFois = true;
  
    // changer le statut
    statutBombe = BOMBE_EXPLOSEE;
    sendStatutBombe();
  }
}

void bombeExplosee()
{
  if (uneFois)
  {
    uneFois = false;
    
    // on eteint les leds
    aFastled->allLedOff();

    // on affiche "- - - -"
    a7segmentDisplay.showExplosee();
  }
}

void bombePause()
{
  // on empeche le temsp de progresser
  previousMillisRefresh = millis();
}

void bombeUnpause()
{
    // rien a faire
}

void bombeSafe()
{
  if (uneFois)
  {
    uneFois = false;
    
    // on eteint les leds
    aFastled->allLedOff();

    // on affiche "- - - -"
    a7segmentDisplay.showSafe();
  }
}

void bombeBlink()
{
  if (!aFastled->isEnabled() && blinkUneFois)
  {
    blinkUneFois = false;
    aFastled->startAnimBlink(15, 200, CRGB::Blue, aConfig.objectConfig.activeLeds);
  }

  if (!aFastled->isAnimActive())
  {
    Serial.println(F("END TASK BLINK"));

    //blinkUneFois = true;
    //uneFois = true;
    aFastled->allLedOff();

    // retour au statut precedent
    statutBombe = statutBombePrecedent;
    sendStatutBombe();
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
          buzzer->doubleBeep();
          aConfig.objectConfig.intervalTemps/=2;
          actionFil[i]=FIL_COUPE;
          Serial.print("fil delai: ");
          Serial.println(i);
        break;
    
        case FIL_SAFE:
          // si la bomme est deja au statut BOMBE_EXPLOSION, on ne change pas pour BOMBE_SAFE
          if (statutBombe != BOMBE_EXPLOSION)
          {
            // le bomb est safe
            buzzer->shortBeep();
            statutBombe = BOMBE_SAFE;            
            uneFois = true;
            Serial.print("fil safe: ");
            Serial.println(i);
          }
          actionFil[i]=FIL_COUPE;
        break;
        
        case FIL_EXPLOSION:
          // detonate the bomb
          statutBombe=BOMBE_EXPLOSION;
          actionFil[i]=FIL_COUPE;
          uneFois = true;
          Serial.print("fil boom: ");
          Serial.println(i);
        break;
        
        default:
          // do nothing      
        break;
      }

      sendStatutBombe();
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
int indexMaxValeur(uint8_t arraySize, uint8_t arrayToSearch[])
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
/*
   ----------------------------------------------------------------------------
   FIN DES FONCTIONS ADDITIONNELLES
   ----------------------------------------------------------------------------
*/

void sendUptime()
{
  uint32_t now = millis() / 1000;
  uint16_t days = now / 86400;
  uint16_t hours = (now%86400) / 3600;
  uint16_t minutes = (now%3600) / 60;
  uint16_t seconds = now % 60;
    
  String toSend = "{\"uptime\":\"";
  toSend+= String(days) + String("d ") + String(hours) + String("h ") + String(minutes) + String("m ") + String(seconds) + String("s");
  toSend+= "\"}";

  ws.textAll(toSend);
  Serial.println(toSend);
}

void sendActionFil()
{
  String toSend = "{\"actionFil\":[";
  for (uint8_t i=0;i<FILS_MAX;i++)
  {
    toSend+= actionFil[i];
    if (i <(FILS_MAX-1))
    {
      toSend+= ",";
    }
  }  
  toSend+= "]}";

  ws.textAll(toSend);
}

void sendTempsRestant()
{
  String toSend = "{\"tempsRestant\":";
  toSend+= tempsRestant;
  toSend+= "}";

  ws.textAll(toSend);
}

void sendStatutBombe()
{
  String toSend = "{\"statutBombe\":";
  toSend+= statutBombe;
  toSend+= "}";

  ws.textAll(toSend);
}

void convertStrToRGB(String source, uint8_t* r, uint8_t* g, uint8_t* b)
{
  uint32_t  number = (uint32_t) strtol( &source[1], NULL, 16);
  
  // Split them up into r, g, b values
  *r = number >> 16;
  *g = number >> 8 & 0xFF;
  *b = number & 0xFF;
}

void checkCharacter(char* toCheck, char* allowed, char replaceChar)
{
  for (int i = 0; i < strlen(toCheck); i++)
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

String processor(const String& var)
{  
  if (var == "MAXLEDS")
  {
    return String(aFastled->getNbMaxLed());
  }

  if (var == "APNAME")
  {
    return String(aConfig.networkConfig.apName);
  }

  if (var == "OBJECTNAME")
  {
    return String(aConfig.objectConfig.objectName);
  }
   
  return String();
}


void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) 
{
   switch (type) 
    {
      case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        // send config value to html
        ws.textAll(aConfig.stringJsonFile("/config/objectconfig.txt"));
        ws.textAll(aConfig.stringJsonFile("/config/networkconfig.txt"));

        // send volatile info
        sendActionFil();
        sendTempsRestant();
        sendUptime();
        sendStatutBombe();
    
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
    char buffer[100];
    data[len] = 0;
    sprintf(buffer,"%s\n", (char*)data);
    Serial.print(buffer);
    
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, buffer);
    if (error)
    {
      Serial.println(F("Failed to deserialize buffer"));
    }
    else
    {
        // write config or not
        bool writeObjectConfig = false;
        bool sendObjectConfig = false;
        bool writeNetworkConfig = false;
        bool sendNetworkConfig = false;
        
        // modif object config
        if (doc.containsKey("new_objectName"))
        {
          strlcpy(  aConfig.objectConfig.objectName,
                    doc["new_objectName"],
                    sizeof(aConfig.objectConfig.objectName));
  
          writeObjectConfig = true;
          sendObjectConfig = true;
        }
  
        if (doc.containsKey("new_objectId")) 
        {
          uint16_t tmpValeur = doc["new_objectId"];
          aConfig.objectConfig.objectId = checkValeur(tmpValeur,1,1000);
  
          writeObjectConfig = true;
          sendObjectConfig = true;
        }
  
        if (doc.containsKey("new_groupId")) 
        {
          uint16_t tmpValeur = doc["new_groupId"];
          aConfig.objectConfig.groupId = checkValeur(tmpValeur,1,1000);
          
          writeObjectConfig = true;
          sendObjectConfig = true;
        }
  
        if (doc.containsKey("new_activeLeds")) 
        {
          aFastled->allLedOff();
          
          uint16_t tmpValeur = doc["new_activeLeds"];
          aConfig.objectConfig.activeLeds = checkValeur(tmpValeur,1,aFastled->getNbMaxLed());
          aFastled->setNbLed(aConfig.objectConfig.activeLeds);
          uneFois=true;
          
          writeObjectConfig = true;
          sendObjectConfig = true;
        }
  
        if (doc.containsKey("new_brightness"))
        {
          uint16_t tmpValeur = doc["new_brightness"];
          aConfig.objectConfig.brightness = checkValeur(tmpValeur,0,255);
          aFastled->setBrightness(aConfig.objectConfig.brightness);
          //aFastled->ledShow();
          uneFois=true;
          
          writeObjectConfig = true;
          sendObjectConfig = true;
        }

        if (doc.containsKey("new_tempsRestant"))
        {
          uint16_t tmpValeur = doc["new_tempsRestant"];
          tempsRestant = checkValeur(tmpValeur,0,5940);
          
          currentMillisRefresh = millis();
          previousMillisRefresh = currentMillisRefresh;
          
          if ( (statutBombe == BOMBE_ACTIVE) || (statutBombe == BOMBE_PAUSE) )
          {
            a7segmentDisplay.showTempsRestant(max<int16_t>(0,tempsRestant));
          }          
          sendTempsRestant();
        }

        if (doc.containsKey("new_intervalTemps"))
        {
          uint16_t tmpValeur = doc["new_intervalTemps"];
          aConfig.objectConfig.intervalTemps = checkValeur(tmpValeur,0,10000);
          
          uneFois=true;
          
          writeObjectConfig = true;
          sendObjectConfig = true;
        }        

        if (doc.containsKey("new_actionFilInit")) 
        {
          JsonArray newActionFil = doc["new_actionFilInit"];
        
          uint8_t changePosition = newActionFil[0];
          uint8_t changeAction = newActionFil[1];
          
          aConfig.objectConfig.actionFilInit[changePosition]=changeAction;
          actionFil[changePosition]=changeAction;
          sendActionFil();
          
          writeObjectConfig = true;
          sendObjectConfig = true;
        }

        if (doc.containsKey("new_statutBombe"))
        {
          uint16_t tmpValeur = doc["new_statutBombe"];
          statutBombe = checkValeur(tmpValeur,0,7);
          
          uneFois=true;

          if (statutBombe == BOMBE_EXPLOSION)
          {
            explosionUneFois=true;
          }

          writeObjectConfig = true;
          sendObjectConfig = true;

          sendStatutBombe();
        }

        if ( doc.containsKey("new_pause") && doc["new_pause"]==1 )
        {
          Serial.println(F("PAUSE"));
          
          if (statutBombe == BOMBE_ACTIVE)
          {
            statutBombePrecedent = statutBombe;
            statutBombe = BOMBE_PAUSE;
          }

          sendStatutBombe();
        }

        if ( doc.containsKey("new_unpause") && doc["new_unpause"]==1 )
        {
          Serial.println(F("UNPAUSE"));
          
          if (statutBombePrecedent == BOMBE_ACTIVE)
          {
            statutBombe = statutBombePrecedent;
            previousMillisRefresh = millis();
          }

          sendStatutBombe();
        }

        if (doc.containsKey("new_couleur1")) 
        {
          String newColorStr = doc["new_couleur1"];
          convertStrToRGB(newColorStr,&aConfig.objectConfig.couleur1.red, &aConfig.objectConfig.couleur1.green, &aConfig.objectConfig.couleur1.blue);
          
          writeObjectConfig = true;
          sendObjectConfig = true;
          uneFois=true;
        }

        if (doc.containsKey("new_couleur2")) 
        {
          String newColorStr = doc["new_couleur2"];
          convertStrToRGB(newColorStr,&aConfig.objectConfig.couleur2.red, &aConfig.objectConfig.couleur2.green, &aConfig.objectConfig.couleur2.blue);
           
          writeObjectConfig = true;
          sendObjectConfig = true;
          uneFois=true;
        }

        if (doc.containsKey("new_beepEvery"))
        {
          uint16_t tmpValeur = doc["new_beepEvery"];
          aConfig.objectConfig.beepEvery = checkValeur(tmpValeur,0,300);
                    
          writeObjectConfig = true;
          sendObjectConfig = true;
        }

        if (doc.containsKey("new_beepUnder"))
        {
          uint16_t tmpValeur = doc["new_beepUnder"];
          aConfig.objectConfig.beepUnder = checkValeur(tmpValeur,0,60);
                    
          writeObjectConfig = true;
          sendObjectConfig = true;
        }

        if (doc.containsKey("new_tempsInitial"))
        {
          uint16_t tmpValeur = doc["new_tempsInitial"];
          aConfig.objectConfig.tempsInitial = checkValeur(tmpValeur,0,5940);
                    
          writeObjectConfig = true;
          sendObjectConfig = true;
        }

        if (doc.containsKey("new_nbFilActif"))
        {
          uint16_t tmpValeur = doc["new_nbFilActif"];
          aConfig.objectConfig.nbFilActif = checkValeur(tmpValeur,0,8);
                    
          sendActionFil();
          
          writeObjectConfig = true;
          sendObjectConfig = true;
        }

        if ( doc.containsKey("new_filAleatoire") && doc["new_filAleatoire"]==1 )
        {
          Serial.println(F("RESET fil Aleatoire"));
          
          affecteFilsAleatoires();
          sendActionFil();
          
          writeObjectConfig = false;
          sendObjectConfig = true;
        }

        if (doc.containsKey("new_nbFilExplosion"))
        {
          uint16_t tmpValeur = doc["new_nbFilExplosion"];
          aConfig.objectConfig.nbFilExplosion = checkValeur(tmpValeur,0,8);
                    
          sendActionFil();
          
          writeObjectConfig = true;
          sendObjectConfig = true;
        }

        if (doc.containsKey("new_nbFilSafe"))
        {
          uint16_t tmpValeur = doc["new_nbFilSafe"];
          aConfig.objectConfig.nbFilSafe = checkValeur(tmpValeur,0,8);
                    
          sendActionFil();
          
          writeObjectConfig = true;
          sendObjectConfig = true;
        }

        if (doc.containsKey("new_nbFilDelai"))
        {
          uint16_t tmpValeur = doc["new_nbFilDelai"];
          aConfig.objectConfig.nbFilDelai = checkValeur(tmpValeur,0,8);
                    
          sendActionFil();
          
          writeObjectConfig = true;
          sendObjectConfig = true;
        }
          
        // modif network config
        if (doc.containsKey("new_apName")) 
        {
          strlcpy(  aConfig.networkConfig.apName,
                    doc["new_apName"],
                    sizeof(aConfig.networkConfig.apName));
  
          // check for unsupported char
          checkCharacter(aConfig.networkConfig.apName, "ABCDEFGHIJKLMNOPQRSTUVWYZ", 'A');
          
          writeNetworkConfig = true;
          sendNetworkConfig = true;
        }
  
        if (doc.containsKey("new_apPassword")) 
        {
          strlcpy(  aConfig.networkConfig.apPassword,
                    doc["new_apPassword"],
                    sizeof(aConfig.networkConfig.apPassword));
  
          writeNetworkConfig = true;
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
  
            writeNetworkConfig = true;
          }
          
          sendNetworkConfig = true;
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
  
            writeNetworkConfig = true;
          }
  
          sendNetworkConfig = true;
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

          sendActionFil();
          sendTempsRestant();
          sendStatutBombe();
          sendObjectConfig = true;
          sendNetworkConfig = true;
        }

        if ( doc.containsKey("new_defaultObjectConfig") && doc["new_defaultObjectConfig"]==1 )
        {
          Serial.println(F("reset to default object config"));
          aConfig.writeDefaultObjectConfig("/config/objectconfig.txt");
          
          sendObjectConfig = true;
          uneFois = true;
        }

        if ( doc.containsKey("new_defaultNetworkConfig") && doc["new_defaultNetworkConfig"]==1 )
        {
          Serial.println(F("reset to default network config"));
          aConfig.writeDefaultNetworkConfig("/config/networkconfig.txt");
          
          sendNetworkConfig = true;
        }

        if ( doc.containsKey("new_blink") && doc["new_blink"]==1 )
        {
          Serial.println(F("BLINK"));
          blinkUneFois = true;
          statutBombePrecedent = statutBombe;
          statutBombe = BOMBE_BLINK;
          sendStatutBombe();
        }
  
        // modif config
        // write object config
        if (writeObjectConfig)
        {
          aConfig.writeObjectConfig("/config/objectconfig.txt");
          //aConfig.printJsonFile("/config/objectconfig.txt");
        }
  
        // resend object config
        if (sendObjectConfig)
        {
          ws.textAll(aConfig.stringJsonFile("/config/objectconfig.txt"));
        }
  
        // write network config
        if (writeNetworkConfig)
        {
          aConfig.writeNetworkConfig("/config/networkconfig.txt");
          //aConfig.printJsonFile("/config/networkconfig.txt");
        }
  
        // resend network config
        if (sendNetworkConfig)
        {
          ws.textAll(aConfig.stringJsonFile("/config/networkconfig.txt"));
        }
    }
 
    // clear json buffer
    doc.clear();
  }
}

void notFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "Not found");
}
