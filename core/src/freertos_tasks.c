/**
 * freertos_tasks.c
 * Système temps réel STM32 — trois tâches FreeRTOS
 *
 * Les tâches communiquent via des queues, sans mémoire partagée directe.
 * L'UART est protégé par un mutex pour éviter les accès concurrents.
 */
 
#include "freertos_tasks.h"
#include "uart_driver.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include <stdio.h>
#include <string.h>
 
/* ── Structures de données ─────────────────────────────────────────────── */
 
typedef struct {
    float    valeur_brute;   /* distance en cm lue par le capteur  */
    uint32_t tick;           /* tick FreeRTOS au moment de la lecture */
} DonneeCapteur_t;
 
typedef struct {
    float    moyenne;        /* moyenne glissante sur les N dernières mesures */
    uint8_t  anomalie;       /* 1 si anomalie détectée, 0 sinon              */
    uint32_t tick;
} ResultatTraitement_t;
 
/* ── Handles des queues et du mutex ────────────────────────────────────── */
 
static QueueHandle_t     xQueueCapteur  = NULL;
static QueueHandle_t     xQueueResultat = NULL;
static SemaphoreHandle_t xMutexUart     = NULL;
 
/* ── Fonction interne ──────────────────────────────────────────────────── */
 
/**
 * Simule la lecture d'un capteur.
 * À remplacer par un vrai appel HAL_I2C ou HAL_ADC sur le matériel réel.
 */
static float lire_capteur(void)
{
    static uint32_t graine = 12345UL;
    graine = graine * 1103515245UL + 12345UL;
    return 20.0f + (float)(graine & 0x3FF) * 0.1f;  /* entre 20 et 122 cm */
}
 
/* ── Implémentation des tâches ─────────────────────────────────────────── */
 
/**
 * TacheAcquisition — priorité HAUTE (3)
 *
 * Lit le capteur toutes les 100 ms de façon déterministe (vTaskDelayUntil).
 * Envoie les données dans xQueueCapteur.
 */
void TacheAcquisition(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xDernierReveil = xTaskGetTickCount();
    const TickType_t xPeriode = pdMS_TO_TICKS(100);
 
    DonneeCapteur_t donnee;
 
    for (;;)
    {
        donnee.valeur_brute = lire_capteur();
        donnee.tick         = xTaskGetTickCount();
 
        /* Envoi non bloquant : on perd la mesure si la queue est pleine */
        xQueueSend(xQueueCapteur, &donnee, 0);
 
        vTaskDelayUntil(&xDernierReveil, xPeriode);
    }
}
 
/**
 * TacheTraitement — priorité MOYENNE (2)
 *
 * Consomme la queue capteur, calcule une moyenne glissante,
 * détecte les anomalies, et envoie le résultat dans xQueueResultat.
 */
void TacheTraitement(void *pvParameters)
{
    (void)pvParameters;
 
    #define TAILLE_FENETRE     5
    #define SEUIL_BAS          25.0f
    #define SEUIL_HAUT         100.0f
 
    static float   fenetre[TAILLE_FENETRE] = {0};
    static uint8_t index   = 0;
    static uint8_t remplie = 0;
 
    DonneeCapteur_t    entree;
    ResultatTraitement_t sortie;
 
    for (;;)
    {
        /* Attente bloquante : la tâche dort jusqu'à l'arrivée d'une mesure */
        if (xQueueReceive(xQueueCapteur, &entree, portMAX_DELAY) == pdTRUE)
        {
            /* Mise à jour de la fenêtre glissante */
            fenetre[index] = entree.valeur_brute;
            index = (index + 1) % TAILLE_FENETRE;
            if (!remplie && index == 0) remplie = 1;
 
            /* Calcul de la moyenne */
            uint8_t n = remplie ? TAILLE_FENETRE : index;
            float somme = 0.0f;
            for (uint8_t i = 0; i < n; i++) somme += fenetre[i];
            sortie.moyenne = somme / (float)n;
 
            /* Détection d'anomalie : moyenne hors des seuils */
            sortie.anomalie = (sortie.moyenne < SEUIL_BAS ||
                               sortie.moyenne > SEUIL_HAUT) ? 1U : 0U;
            sortie.tick = entree.tick;
 
            xQueueSend(xQueueResultat, &sortie, 0);
        }
    }
}
 
/**
 * TacheCommunication — priorité BASSE (1)
 *
 * Vide la queue des résultats toutes les 500 ms et envoie sur UART.
 * Le mutex garantit l'accès exclusif à la liaison série.
 */
void TacheCommunication(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xDernierReveil = xTaskGetTickCount();
    const TickType_t xPeriode = pdMS_TO_TICKS(500);
 
    ResultatTraitement_t resultat;
    char tampon[80];
 
    for (;;)
    {
        vTaskDelayUntil(&xDernierReveil, xPeriode);
 
        while (xQueueReceive(xQueueResultat, &resultat, 0) == pdTRUE)
        {
            snprintf(tampon, sizeof(tampon),
                     "[COMM] Moyenne : %.1f cm | Anomalie : %s | Tick : %lu\r\n",
                     resultat.moyenne,
                     resultat.anomalie ? "OUI" : "NON",
                     (unsigned long)resultat.tick);
 
            uart_envoyer_mutex(tampon, xMutexUart);
        }
    }
}
 
/* ── Initialisation — appelée depuis main.c ────────────────────────────── */
 
void freertos_tasks_init(void)
{
    xQueueCapteur  = xQueueCreate(10, sizeof(DonneeCapteur_t));
    xQueueResultat = xQueueCreate(10, sizeof(ResultatTraitement_t));
    xMutexUart     = xSemaphoreCreateMutex();
 
    configASSERT(xQueueCapteur  != NULL);
    configASSERT(xQueueResultat != NULL);
    configASSERT(xMutexUart     != NULL);
 
    xTaskCreate(TacheAcquisition,    "Acq",  256, NULL, 3, NULL);
    xTaskCreate(TacheTraitement,     "Trait",256, NULL, 2, NULL);
    xTaskCreate(TacheCommunication,  "Comm", 256, NULL, 1, NULL);
}
