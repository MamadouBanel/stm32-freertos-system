# stm32-freertos-system
Système temps réel embarqué — STM32 + FreeRTOS
Système multitâche temps réel sur STM32 avec trois tâches communicant via des queues FreeRTOS.
Description
Le projet implémente une architecture classique de système embarqué temps réel : une tâche d'acquisition lit le capteur à période fixe, une tâche de traitement calcule une moyenne glissante et détecte les anomalies, une tâche de communication envoie les résultats sur UART.
Les tâches ne partagent pas de mémoire directement — elles communiquent uniquement par queues. L'accès à l'UART est protégé par un mutex. C'est le genre d'architecture qu'on retrouve dans les systèmes embarqués critiques (avionique, spatial).
Matériel

STM32F4 Discovery (ou tout STM32 compatible FreeRTOS)
UART (USART2, 115200 bauds) → PC via ST-Link

Stack logiciel

C (C99)
FreeRTOS v10
STM32CubeIDE / HAL


Architecture des tâches
TâchePrioritéPériodeRôleTacheAcquisitionHAUTE (3)100 msLecture capteur, envoi dans la queueTacheTraitementMOYENNE (2)100 msMoyenne glissante + détection anomalieTacheCommunicationBASSE (1)500 msEnvoi des résultats sur UART
TacheAcquisition ──[xQueueCapteur]──► TacheTraitement ──[xQueueResultat]──► TacheCommunication
                                                                                      │
                                                                                   UART TX
Points techniques clés
Queue (xQueueSend / xQueueReceive) :
xQueueCapteur transporte des structures DonneeCapteur_t entre l'acquisition et le traitement. Pas de variable partagée, pas de condition de course.
Mutex (xSemaphoreTake / xSemaphoreGive) :
xMutexUart garantit qu'une seule tâche accède à l'UART à la fois.
Timing déterministe (vTaskDelayUntil) :
L'acquisition s'exécute exactement toutes les 100 ms, indépendamment du temps d'exécution — indispensable pour un échantillonnage régulier.
Compilation et flash

Ouvrir le projet dans STM32CubeIDE
Build : Project → Build All
Flash : Run → Debug via ST-Link
Moniteur série : 115200 bauds sur le port COM ST-Link

Exemple de sortie UART
[COMM] Moyenne : 23.4 cm | Anomalie : NON | Tick : 1200
[COMM] Moyenne : 18.1 cm | Anomalie : OUI | Tick : 1700
[COMM] Moyenne : 45.7 cm | Anomalie : NON | Tick : 2200
Améliorations possibles

Communication CAN bus
Traçage FreeRTOS avec Percepio Tracealyzer
Modes basse consommation entre acquisitions
Watchdog pour la récupération sur erreur

Auteur
Mamadou Banel — mamadoubanel2@gmail.com
