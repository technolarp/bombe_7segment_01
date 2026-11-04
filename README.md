# bombe_7segment_01
Cet objet simule une bombe avec un afficheur 4 x 7 segments et des fils qui permettent de neutraliser la bombe ou au contraire la faire exploser. Le nombre de fils et leur réactions sont configurables, ainsi que plusieurs autres paramètres tel que le temps initial ou la rapidité du décompte. Le paramétrage se fait via un back office accessible via le wifi. l'explosion est simulée par l'afficheur et le buzzer

**[Exemple](#Exemple)** 
**[Composants](#Composants)**  
**[Branchements](#Branchements)**  
**[Installation du firmware](#Installation-du-firmware)**  
**[Mode d’emploi](#)**   
**[Fils de désamorçage](#)**   
**[Back Office](#BackOffice)**  
**[Paramètres de gameplay](#param%C3%A8tres-de-gameplay)**  
**[Paramètres Réseau](#param%C3%A8tres-r%C3%A9seau)** 

## Exemple
Un exemple du kit technoLARP monté pour une serrure_clavier_01

//TODO

Voici un exemple de casing plus avancé

//TODO

## Composants
Vous aurez besoin pour monter la bombe_7segment_01

|  | |
| :---------------- | :------: |
| Un kit technoLarp | <img src="./images/technolarp_pcb_wemos.jpg" width="200"> |
| Un câble micro-USB | <img src="./images/technolarp_cable_micro-usb.png" width="200"> |
| Un ruban ou d’un anneau de led ws2812b ou neopixel | <img src="./images/technolarp_leds_ws2812b_01.jpg" height="200"> |
| Un afficheur 4 * 7 segments | //TODO |
| 4 boutons poussoirs | //TODO |
| Un buzzer piezo | <img src="./images/technolarp_buzzer.jpg" height="200"> |
| Une batterie 18650 et son support | <img src="./images/technolarp_18650.jpg" width="200"> |

## Branchements
//TODO

<img src="./images/technolarp_pcb_connecteurs.png" width="600">

## Installation du firmware
Pour installer le firmware de l'objet, il faut suivre ce [tutorial](https://github.com/technolarp/technolarp.github.io/wiki/Installation-du-firmware)  

## Mode d’emploi

//TODO

## Fils de désamorçage

//TODO

## BackOffice
Pour se connecter au back office de l'objet, il faut suivre ce [tutorial](https://github.com/technolarp/technolarp.github.io/wiki/Connexion-au-back-office-de-l'objet-via-le-wifi)  


<img width="608" alt="serrure_clavier_01_back-office" src="./images/serrure_clavier_01_backoffice.png" />

## Paramètres de gameplay
Ces paramètres permettent de contrôler le gameplay de l’objet.

| Nom                           | Descriptif                                                                                                                                                                       |
|-------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Object name**               | Le nom de l’objet, composé de 1 à 20 lettres et chiffres                                                                                                                         |
| **Object ID**                 | Un numéro d’identification de l’objet                                                                                                                                            |
| **Group ID**                  | Un numéro d’identification du groupe de l’objet                                                                                                                                  |
| **ActiveLeds**                | Le nombre de segments du panneau. Un segment est un groupe de 1 à 5 leds. Nombre entre 1 et 20.                                                                                  |
| **ledParSegment**             | Le nombre de leds groupées pour chaque segment du panneau. Entre 1 et 5                                                                                                          |
| **ActiveLeds**                | Le nombre total de leds utilisées par la bombe. entre 1 et 25                                                                                                                    |
| **Brightness**                | La luminosité des leds, entre 0 (éteinte) et 255 (pleine intensité)                                                                                                              |
| **Scintillement**             | Le scintillement permet d’activer le scintillement des leds pour un effet visuel plus ou moins rapide. le slider permet de régler la vitesse du scintillement                    |

//TODO

## Paramètres Réseau

Pour avoir le descriptif des paramètres réseau suivez ce [lien](https://github.com/technolarp/technolarp.github.io/wiki/Param%C3%A8tres-R%C3%A9seau)  
