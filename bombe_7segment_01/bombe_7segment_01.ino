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
bool uneFois = true;
bool blinkUneFois = true;
bool blinkMinutesOuSecondes = false;

int16_t tempsInitialTmp;

// STATUTS
enum {
  BOMBE_ALLUMEE = 0,
  BOMBE_ACTIVE = 1,
  BOMBE_EXPLOSION = 2,
  BOMBE_EXPLOSEE = 3,
  BOMBE_SAFE = 4
};

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

  /*
  Serial.println(F("NETWORK CONFIG"));
  aConfig.printJsonFile("/config/networkconfig.txt");
  aConfig.readNetworkConfig("/config/networkconfig.txt");
  */

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
  if (!aMcp23017.readPin(BOUTON_4) && !aMcp23017.readPin(BOUTON_1) )
  {
    aFastled->allLedOn(CRGB::Yellow);
    
    Serial.println(F(""));
    Serial.println(F("!!! RESET OBJECT CONFIG !!!"));
    Serial.println(F(""));
    aConfig.writeDefaultObjectConfig("/config/objectconfig.txt");
    aConfig.printJsonFile("/config/objectconfig.txt");

    delay(1000);
  }

  // BUZZER
  buzzer = new M_buzzer(PIN_BUZZER, &globalScheduler);
  buzzer->doubleBeep();

  // initialiser l'aleat
  //randomSeed(analogRead(A0));
  randomSeed(ESP.getCycleCount());

  // HEARTBEAT
  currentMillisHB = millis();
  previousMillisHB = currentMillisHB;
  intervalHB = 5000;

  // REFRESH
  previousMillisRefresh = currentMillisHB;
  currentMillisRefresh = currentMillisHB;
  intervalRefresh = 300;

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
  //yield();
  
  // WEBSOCKET
  //ws.cleanupClients();
  
  // manage task scheduler
  globalScheduler.execute();

  // gerer le statut de la serrure
  switch (aConfig.objectConfig.statutBombe)
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
      
    default:
      // nothing
      break;
  }

  // HEARTBEAT
  currentMillisHB = millis();
  if(currentMillisHB - previousMillisHB > intervalHB)
  {
    previousMillisHB = currentMillisHB;

    // send new value to html
    //sendUptime();
        
    Serial.println("heartbeat");
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
    
    tempsInitialTmp=aConfig.objectConfig.tempsInitial;

    Serial.print(F("BOMBE ALLUMEE"));
    Serial.println();
  }

  // check si le temps a changer
  // BOUTON_HAUT_PIN2 appuyé, on augmente le temps
  if (aMcp23017.checkButton(BOUTON_2))
  {
    if (a7segmentDisplay.getBlinkMinutesOuSecondes())
    {
      tempsInitialTmp+=60;
    }
    else
    {
      tempsInitialTmp+=1;
    }
    
    // on ne depasse pas 99 minutes
    tempsInitialTmp=min<int16_t>(99*60, tempsInitialTmp);
  }

  // BOUTON_3  appuyé, on diminue le temps
  if (aMcp23017.checkButton(BOUTON_3))
  {
    if (a7segmentDisplay.getBlinkMinutesOuSecondes())
    {
      tempsInitialTmp-=60;
    }
    else
    {
      tempsInitialTmp-=1;
    }
    
    // on ne depasse pas 0 minutes
    tempsInitialTmp=max<int16_t>(0, tempsInitialTmp);
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
    aConfig.objectConfig.statutBombe = BOMBE_ACTIVE;
    aConfig.objectConfig.tempsRestant=tempsInitialTmp;
    
    uneFois = true;
    
    previousMillisRefresh = millis();
    a7segmentDisplay.setBlinkAffichage(false);
    intervalRefresh=1000;

    // affecter les fils aleatoires
    affecteFilsAleatoires();
    
    Serial.print("AFFECTATION:  ");
    for (uint8_t i=0;i<aConfig.objectConfig.nbFilActif;i++)
    {
      Serial.print(aConfig.objectConfig.actionFil[i]);
      Serial.print(" ");
    }
    Serial.println(" ");
    
    // ecrire la nouvelle config
    //aConfig.writeObjectConfig("/config/objectconfig.txt");
  }

  currentMillisRefresh = millis();
  if(currentMillisRefresh - previousMillisRefresh > intervalRefresh)
  {
    previousMillisRefresh = currentMillisRefresh;
    a7segmentDisplay.setBlinkAffichage(!a7segmentDisplay.getBlinkAffichage());
  }
  
  // mettre a jour l affichage
  a7segmentDisplay.showTempsRestant(tempsInitialTmp);
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
  }

  if ( aConfig.objectConfig.tempsRestant == -1 )
  {
    // le compte a rebours est terminé !!
    uneFois = true;
    aConfig.objectConfig.statutBombe = BOMBE_EXPLOSION;
  }
  else
  {
    // il reste du temps
    currentMillisRefresh = millis();
    if(currentMillisRefresh - previousMillisRefresh > aConfig.objectConfig.intervalTemps)
    {
      previousMillisRefresh = currentMillisRefresh;

      // on decompte le temps restant
      aConfig.objectConfig.tempsRestant-=1;
   
      // beep toutes les X secondes
      if (aConfig.objectConfig.beepEvery != 0)
      {
        if (aConfig.objectConfig.tempsRestant % aConfig.objectConfig.beepEvery == 0)
        {
          buzzer->shortBeep();
        }
      }
    
      // beep toutes les secondes quand il reste Y secondes ou moins
      if (aConfig.objectConfig.beepUnder != 0)
      {
        if (aConfig.objectConfig.tempsRestant <= aConfig.objectConfig.beepUnder )
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

    Serial.print(F("BOMBE EXPLOSION"));
    Serial.println();

    // on demarre l'animation led
    if (!aFastled->isEnabled() && blinkUneFois)
    {
      blinkUneFois = false;

      // on buzze 5 second
      buzzer->beep(800, 5000, 2);

      // on blink lesleds
      aFastled->startAnimBlink(50, 100, aConfig.objectConfig.couleur2, aConfig.objectConfig.activeLeds);

      a7segmentDisplay.setBlinkAffichage(false);
      intervalRefresh=200;
    }
  }

  // on blink l'afficheur 7 segment
  currentMillisRefresh = millis();
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
    aConfig.objectConfig.statutBombe = BOMBE_EXPLOSEE;
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

// check fil coupe
void checkFilCoupe()
{
  // scan all wires
  for (uint8_t i=0;i<aConfig.objectConfig.nbFilActif;i++)
  {
    // if the i wire was not previously cutted ans is now cut
    if ( (aConfig.objectConfig.actionFil[i] != FIL_COUPE) && (aMcp23017.readPin(i)) )
    { 
      switch (aConfig.objectConfig.actionFil[i]) 
      {
        case FIL_NEUTRE:
          // wire is neutral, nothing to do
          aConfig.objectConfig.actionFil[i]=FIL_COUPE;
          Serial.print("fil neutre: ");
          Serial.println(i);
        break;
        
        case FIL_DELAI:
          // divide ul_Interval by 2
          buzzer->doubleBeep();
          aConfig.objectConfig.intervalTemps/=2;
          aConfig.objectConfig.actionFil[i]=FIL_COUPE;
          Serial.print("fil delai: ");
          Serial.println(i);
        break;
    
        case FIL_SAFE:
          // si la bomme est deja au statut BOMBE_EXPLOSION, on ne change pas pour BOMBE_SAFE
          if (aConfig.objectConfig.statutBombe != BOMBE_EXPLOSION)
          {
            // le bomb est safe
            buzzer->shortBeep();
            aConfig.objectConfig.statutBombe = BOMBE_SAFE;
            uneFois = true;
            Serial.print("fil safe: ");
            Serial.println(i);
          }
          aConfig.objectConfig.actionFil[i]=FIL_COUPE;
        break;
        
        case FIL_EXPLOSION:
          // detonate the bomb
          aConfig.objectConfig.statutBombe=BOMBE_EXPLOSION;
          aConfig.objectConfig.actionFil[i]=FIL_COUPE;
          uneFois = true;
          Serial.print("fil boom: ");
          Serial.println(i);
        break;
        
        default:
          // do nothing      
        break;
      }
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
  }
  
  for (uint8_t i=0;i<aConfig.objectConfig.nbFilActif;i++)
  {
    if (aConfig.objectConfig.actionFil[i]==FIL_ALEATOIRE)
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
    if (aConfig.objectConfig.actionFil[i]==FIL_ALEATOIRE)
    {
      // find the greater index in tabAleatoire
      uint8_t indexToUse = indexMaxValeur(filsAleatoiresCptCopie, tabAleatoire);
      tabAleatoire[indexToUse]=0;
      
      // assign the new wire type with the random index
      aConfig.objectConfig.actionFil[i]=filAssignation[indexToUse];
    }
  }

  // s'il reste des fil aleatoire, on les passe en neutre
  for (uint8_t i=0;i<aConfig.objectConfig.nbFilActif;i++)
  {
    if (aConfig.objectConfig.actionFil[i]==FIL_ALEATOIRE)
    {
      aConfig.objectConfig.actionFil[i]=FIL_NEUTRE;
    }
  }
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
