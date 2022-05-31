# LiveObjects Iot Device Synchro

---

##Ce dépôt représente les développements effectués dans le cadre de l'agrégation SII-II Session 2022.

---

L'objectif du programme Syncho est de déterminer la durée de prise en charge 
par la plateforme Live Objects d'une communication avec un équipement LoRa. 
Ce programme est complémentaire du projet 
[SynchroSodaq](https://github.com/laurentgiustignano/SynchroSodaq) qui 
s'occupe d'envoyer un Payload d'un Octet. La synchronisation des deux 
programmes s'effectue par la lecture de l'état bas sur les broches de 
communication définies.

Sur le Raspberry Pi, on devra installer la bibliothèque 
[WiringPi](https://github.com/WiringPi/WiringPi.git)  pour faciliter la prise en charge des entrées/sorties du 
GPIO en C++. 

---
### Procédure pour synchroniser l'émission d'un RaspberryPi et une carte Sodaq Explorer

1. Relier les masses ensemble. Par exemple la broche `6` du GPIO avec la broche `14` de la carte Sodaq.
2. Relier les broches `11`, `13`, `15` du Raspberry Pi à la carte Sodaq ExploReR respectivement aux broches `8`, `11`, `12`. Attention, les broches numérotées du GPIO correspondent à la dénomination WiringPi suivante :  `GPIO0`, `GPIO2`, `GPIO3`
3. Configurer la clé d'API dans `C_LOC_CLIENT_DEV_API_KEY_P1` et `C_LOC_CLIENT_DEV_API_KEY_P2` en deux parties.
4. Effectuer le `Build` du projet `synchro`.
5. Si la carte est prête, lancer l'exécution du fichier `synchro`.

---
### Analyse des résultats

1. Se connecter sur [Live Objects](https://liveobjects.orange-business.com/#/login).
2. Consulter le timestamp de l'émission LoRa et le comparer au timestamp contenu dans le payload mqtt.