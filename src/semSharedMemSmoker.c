/**
 *  \file semSharedMemSmoker.c (implementation file)
 *
 *  \brief Problem name: Smokers
 *
 *  Synchronization based on semaphores and shared memory.
 *  Implementation with SVIPC.
 *
 *  Definition of the operations carried out by the smokers:
 *     \li waitForIngredients
 *     \li rollingCigarette
 *     \li smoke
 *
 *  \author Nuno Lau - December 2019
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <math.h>

#include "probConst.h"
#include "probDataStruct.h"
#include "logging.h"
#include "sharedDataSync.h"
#include "semaphore.h"
#include "sharedMemory.h"

/** \brief logging file name */
static char nFic[51];

/** \brief shared memory block access identifier */
static int shmid;

/** \brief semaphore set access identifier */
static int semgid;

/** \brief pointer to shared memory region */
static SHARED_DATA *sh;

static bool waitForIngredients (int id);
static void rollingCigarette (int id);
static void smoke (int id);


/**
 *  \brief Main program.
 *
 *  Its role is to generate the life cycle of one of intervening entities in the problem: the smoker.
 */
int main (int argc, char *argv[])
{
    int key;                                         /*access key to shared memory and semaphore set */
    char *tinp;                                                    /* numerical parameters test flag */
    int n;

    /* validation of command line parameters */
    if (argc != 5) { 
        freopen ("error_SM", "a", stderr);
        fprintf (stderr, "Number of parameters is incorrect!\n");
        return EXIT_FAILURE;
    }
    else {
       freopen (argv[4], "w", stderr);
       setbuf(stderr,NULL);
    }

    n = (unsigned int) strtol (argv[1], &tinp, 0);
    if ((*tinp != '\0') || (n >= NUMSMOKERS )) { 
        fprintf (stderr, "Smoker process identification is wrong!\n");
        return EXIT_FAILURE;
    }
    strcpy (nFic, argv[2]);
    key = (unsigned int) strtol (argv[3], &tinp, 0);
    if (*tinp != '\0') { 
        fprintf (stderr, "Error on the access key communication!\n");
        return EXIT_FAILURE;
    }

    /* connection to the semaphore set and the shared memory region and mapping the shared region onto the
       process address space */
    if ((semgid = semConnect (key)) == -1) { 
        perror ("error on connecting to the semaphore set");
        return EXIT_FAILURE;
    }
    if ((shmid = shmemConnect (key)) == -1) { 
        perror ("error on connecting to the shared memory region");
        return EXIT_FAILURE;
    }
    if (shmemAttach (shmid, (void **) &sh) == -1) { 
        perror ("error on mapping the shared region on the process address space");
        return EXIT_FAILURE;
    }

    /* initialize random generator */
    srandom ((unsigned int) getpid ());                                                 


    /* simulation of the life cycle of the smoker */
    while(waitForIngredients(n)) {
        rollingCigarette(n);
        smoke(n);
    }

    /* unmapping the shared region off the process address space */
    if (shmemDettach (sh) == -1) {
        perror ("error on unmapping the shared region off the process address space");
        return EXIT_FAILURE;;
    }

    return EXIT_SUCCESS;
}

/**
 *  \brief normal distribution generator with zero mean and stddev deviation. 
 *
 *  Generates random number according to normal distribution.
 * 
 *  \param stddev controls standard deviation of distribution
 */
static double normalRand(double stddev)
{
   int i;

   double r=0.0;
   for (i=0;i<12;i++) {
       r += random()/(RAND_MAX+1.0);
   }
   r -= 6.0;

   return r*stddev;
}

/**
 *  \brief smoker waits for the 2 ingredients he does not have
 *
 *  smoker updates state and waits for watcher notification to proceed to roll cigarette.
 *  After the notification, smoker should update the inventory of ingredients.
 *  It may also happen that watcher will notify smoker not because ingredients are available 
 *  but because the factory is closing. In this case, state should be updated and  the function 
 *  should return false;  
 *
 *  \param id smoker id, that is related to the ingredient that the smoker holds (see HAVE* constants in probConst.h)
 *
 *  \ret true if ingredients available; false if closing
 */
static bool waitForIngredients (int id)
{
    bool ret = true;
    int smokers_ids[NUMSMOKERS] = {id%3, (id+1)%3};

    if (semDown (semgid, sh->mutex) == -1)  {                                                     /* enter critical region: 1 processo por vez*/
        perror ("error on the up operation for semaphore access (SM)");
        exit (EXIT_FAILURE);
    }

    /* TODO: insert your code here */
    /* Esperando pelos ingredientes*/
    sh->fSt.st.smokerStat[id] = WAITING_2ING;
    saveState(nFic, &sh->fSt);

    if (semUp (semgid, sh->mutex) == -1) {                                                         /* exit critical region */
        perror ("error on the down operation for semaphore access (SM)");
        exit (EXIT_FAILURE);
    }

    /* TODO: ****/
    size_t n_smokers = sizeof(smokers_ids) / sizeof(smokers_ids[0]);
    for (int i = 0 ; i < n_smokers ; i++) {
        if (semDown(semgid, sh->wait2Ings[smokers_ids[i]])) {
            perror ("error on the down operation for semaphore access (SM)");
            exit (EXIT_FAILURE);
        }
    }

    /* TODO: semaforo dos ingredientes OK*/
    if (semDown(semgid, sh->wait2Ings[id]) == -1) {
        perror ("error on the down operation for semaphore access (SM)");
        exit (EXIT_FAILURE);
    }

    if (semDown (semgid, sh->mutex) == -1)  {                                                     /* enter critical region */
        perror ("error on the up operation for semaphore access (SM)");
        exit (EXIT_FAILURE);
    }

    /* TODO: insert your code here */
    if (sh->fSt.closing) {
        sh->fSt.st.smokerStat[id] = CLOSING_S;
        saveState(nFic, &sh->fSt);
        ret = false; // \ret true if ingredients available; false if closing
    }

    if (semUp (semgid, sh->mutex) == -1) {                                                         /* exit critical region */
        perror ("error on the down operation for semaphore access (SM)");
        exit (EXIT_FAILURE);
    }

    return ret;
}

/**
 *  \brief smoker rolls cigarette
 *
 *  The smoker updates state and takes some time to roll the cigarette.
 *  after completing the cigarette, the smoker should notify the agent.
 *
 *  \param id smoker id
 */
static void rollingCigarette (int id)
{
    double rollingTime = 100.0 + normalRand(30.0);

    if (semDown (semgid, sh->mutex) == -1)  {                                                     /* enter critical region */
        perror ("error on the up operation for semaphore access (SM)");
        exit (EXIT_FAILURE);
    }

    /* TODO: insert your code here */
    sh->fSt.st.smokerStat[id] = ROLLING;

    // Usando os ingredientes
    for (int i = 0 ; i < NUMINGREDIENTS ; i++) {
        sh->fSt.ingredients[i] = 0;
    }

    saveState(nFic, &sh->fSt);

    if (semUp (semgid, sh->mutex) == -1) {                                                         /* exit critical region */
        perror ("error on the down operation for semaphore access (SM)");
        exit (EXIT_FAILURE);
    }
    
    /* TODO: insert your code here */
    if (rollingTime > 0) {
        nanosleep(rollingTime);
    }

    if (semUp(semgid, sh->waitCigarette) == -1) {
        perror ("error on the down operation for semaphore access (SM)");
        exit (EXIT_FAILURE);
    }

}

/**
 *  \brief smoker smokes
 *
 *  The smoker updates state and the number of cigarretes already smoked and 
 *  takes some time to smoke the cigarette
 *
 *  \param id smoker id
 */
static void smoke(int id)
{
    if (semDown (semgid, sh->mutex) == -1)  {                                                     /* enter critical region */
        perror ("error on the up operation for semaphore access (SM)");
        exit (EXIT_FAILURE);
    }

    /* TODO: insert your code here */
    sh->fSt.st.smokerStat[id] = SMOKING;
    sh->fSt.nCigarettes[id] += 1;
    saveState(nFic, &sh->fSt);

    if (semUp (semgid, sh->mutex) == -1) {                                                         /* exit critical region */
        perror ("error on the down operation for semaphore access (SM)");
        exit (EXIT_FAILURE);
    }

    /* TODO: insert your code here */

}


