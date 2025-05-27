#include "SudSlvEn.h"
#ifdef _WIN32
#include <windows.h>
#include <process.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#ifdef _WIN32
#include <strsafe.h>
#else
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/sysinfo.h>
#define FALSE 0
#define TRUE 1
#define NULL_THREAD 0
#endif // _WIN32

#ifdef NDEBUG
#undef assert
#define assert(expression) (void)(expression)
#endif // !NDEBUG

//#define DEBOGUAGE

#ifdef DEBOGUAGE
static void printSudoku(PtSudokuTable st)
{
	int i, j;

	printf("%u\n%u\n", st->largeur, st->hauteur);
	for (i = 0; i < st->largeur * st->hauteur; i++) {
		for (j = 0; j < st->largeur * st->hauteur; j++)
			printf("%u ", getSudokuSymbol(st, i, j));
		printf("\n");
	}
}
#endif // DEBOGUAGE

SUDOKU_SOLVER_DLLIMPORT PtSudokuTable __stdcall newSudokuTable(int largeur, int hauteur)
{
	PtSudokuTable st;
	int i, j;

	if (largeur == 0 || hauteur == 0)
		return 0;

	st = malloc(sizeof(SudokuTable));
	if (!st)
		return 0;
	st->largeur = largeur;
	st->hauteur = hauteur;

	/* Allocation de la mémoire de la table de vecteurs */
	st->table = malloc(sizeof(int*) * largeur * hauteur);
	if (!st->table) {
		free(st);
		return 0;
	}

	for (i = 0; i < largeur * hauteur; i++) {
		st->table[i] = malloc(sizeof(int) * largeur * hauteur);
		if (!st->table[i]) {
			for (j = 0; j < i; j++)
				free(st->table[j]);
			free(st->table);
			free(st);
			return 0;
		}
	}

	for (i = 0; i < largeur * hauteur; i++)
		for (j = 0; j < largeur * hauteur; j++)
			st->table[i][j] = 0;
	return st;
}

SUDOKU_SOLVER_DLLIMPORT PtSudokuTable __stdcall cloneSudokuTable(PtSudokuTable st)
{
	/* Faire une copie du sudoku*/
	int i;

	PtSudokuTable newSt = newSudokuTable(st->largeur, st->hauteur);
	if (newSt) {
		for (i = 0; i < st->largeur * st->hauteur; i++) {
			memcpy(newSt->table[i], st->table[i], sizeof(int) * (st->largeur * st->hauteur));
		}
	}

	return newSt;
}

SUDOKU_SOLVER_DLLIMPORT void __stdcall releaseSudokuTable(PtSudokuTable st)
{
	int i;

	if (st) {
		if (st->table) {
			for (i = 0; i < st->hauteur * st->largeur; i++)
				if (st->table[i])
					free(st->table[i]);
			free(st->table);
		}
		free(st);
	}
#ifdef CHECK_LEAK
	lck_listAllocatedBlocks();
#endif
}

static int isValid(PtSudokuTable st, int symbole,
	int ligne, int colonne)
{
	int i, j, plr, pcr;

	/* Le symbole ne doit pas être déjà présent sur la même ligne */
	for (j = 0; j < st->largeur * st->hauteur; j++)
		if (st->table[ligne][j] == symbole)
			return 1;
	/* Le symbole ne doit pas être déjà présent sur la même colonne */
	for (i = 0; i < st->largeur * st->hauteur; i++)
		if (st->table[i][colonne] == symbole)
			return 2;
	/* Le symbole ne doit pas être déjà présent dans la même région */
	plr = (ligne / st->hauteur) * st->hauteur;
	pcr = (colonne / st->largeur) * st->largeur;

	for (i = plr; i < plr + st->hauteur; i++)
		for (j = pcr; j < pcr + st->largeur; j++)
			if (st->table[i][j] == symbole)
				return 3;

	return 0;
}

SUDOKU_SOLVER_DLLIMPORT int __stdcall insertSudokuSymbol(PtSudokuTable st, int symbole,
	int ligne, int colonne)
{
	int cr;

	if (ligne < 0
		|| colonne < 0
		|| ligne >= st->largeur * st->hauteur
		|| colonne >= st->largeur * st->hauteur)
		return 10;

	if (symbole < 0
		|| symbole  > st->largeur * st->hauteur)
		return 11;

	/* Si le symbole est déjà dans la table, on ne fait rien et on sort */
	if (st->table[ligne][colonne] == symbole)
		return 0;

	/* Si le symbole vaut 0, il s'agit d'effacer un symbole de la table */
	if (symbole == 0) {
		st->table[ligne][colonne] = symbole;
		return 0;
	}

	/* Il faut insérer un symbole dans la table, on commence par vérifier si
		   on a le droit d'insérer ce symbole à l'endroit demandé */
	if ((cr = isValid(st, symbole, ligne, colonne)) != 0)
		return cr;

	st->table[ligne][colonne] = symbole;
	return 0;
}

SUDOKU_SOLVER_DLLIMPORT int __stdcall getSudokuSymbol(PtSudokuTable st,
	int ligne, int colonne)
{
	if (ligne < 0
		|| colonne < 0
		|| ligne >= st->largeur * st->hauteur
		|| colonne >= st->largeur * st->hauteur)
		return -1;

	return st->table[ligne][colonne];
}

static int nbCPU = -1;

/* A initialiser au départ quand nbCPU = -1*/
#ifdef _WIN32
HANDLE hNbTreadsSemaphore;
HANDLE hProtectThreadsParameters;
#else
static sem_t hNbTreadsSemaphore;
static pthread_mutex_t hProtectThreadsParameters;
#endif
typedef enum {
	TERMINATED,
	RUNNING,
	SOLUTION_FOUND,
} workerState;

typedef struct THREAD_PARAM {
#ifdef _WIN32
	volatile HANDLE thread;
#else
	pthread_t thread;
#endif // _WIN32
	PtSudokuTable st;
	int nbe;
	int maxTry;
	volatile workerState state;
#ifdef _WIN32
	HANDLE canContinue;
#else
	sem_t canContinue;
#endif // _WIN32
} threadParam, * ptThreadParam;

static ptThreadParam tparam;

/* A initialiser à chaque appel de solveSudokuMaxTry */
#ifdef _WIN32
static HANDLE workerEventSemaphore;
#else
static sem_t workerEventSemaphore;
#endif // _WIN32
static bool terminationRequested;

#ifdef _WIN32
static unsigned int __stdcall slvSudThreadProc(void* param);
#else
static void *slvSudThreadProc(void* param);
#endif


static void workerSolution(ptThreadParam param)
{
	/* Retourner la solution au thread principal */
	param->state = SOLUTION_FOUND;
#ifdef DEBOGUAGE
		printf("workerSolution: realease workerEventSemaphore\n");
#endif
#ifdef _WIN32
	assert(ReleaseSemaphore(workerEventSemaphore, 1, NULL));
#else
	assert(sem_post(&workerEventSemaphore) == 0);
#endif
#ifdef DEBOGUAGE
	printf("workerSolution: wait for canContinue\n");
#endif
#ifdef _WIN32
	assert(WaitForSingleObject(param->canContinue, INFINITE) == WAIT_OBJECT_0);
#else
	assert(sem_wait(&param->canContinue) == 0);
#endif
#ifdef DEBOGUAGE
	printf("workerSolution: passed\n");
#endif
}

#ifdef _WIN32
HANDLE createNewWorkerThread(PtSudokuTable nst, int nbe, int maxTry)
#else
static pthread_t createNewWorkerThread(PtSudokuTable nst, int nbe, int maxTry)
#endif
{
#ifdef _WIN32
	if (WaitForSingleObject(hProtectThreadsParameters, INFINITE) == WAIT_OBJECT_0) {
#else
	if (pthread_mutex_lock(&hProtectThreadsParameters) == 0) {
#endif
		/* On a obtenu le droit de modifier nbUsedThreads */
		int i;
		ptThreadParam p=NULL;

		for (i = 0; i < nbCPU; i++) {
			p = tparam + i;
			if (p->state == TERMINATED) {
				assert(!p->thread);
				/* On remplit une structure de données pour passer les arguments au thread */
				p->st = nst;
				p->nbe = nbe;
				p->maxTry = maxTry;
				p->state = RUNNING;

				/* Ce sémaphore permet de bloquer le processus de travail pendant que le processus
				   principal traite le résultat */
#ifdef _WIN32
				p->canContinue = CreateSemaphore(NULL, 0, 1, NULL);
				assert(p->canContinue);
#else
				assert(sem_init(&p->canContinue, 0, 0) == 0);
#endif // _WIN32
				/* On crée le thread */
#ifdef _WIN32
				p->thread = (HANDLE)_beginthreadex(NULL, 0, slvSudThreadProc, (void*)p, 0, NULL);
#else
				assert(pthread_create(&p->thread, NULL, slvSudThreadProc, (void*)p) == 0);
#endif
				assert(p->thread);

				/* On autorise le démarrage du thread */
#ifdef _WIN32
				assert(ReleaseSemaphore(p->canContinue, 1, NULL));
#else
				assert(sem_post(&p->canContinue) == 0);
#endif
#ifdef DEBOGUAGE
				printf("createNewWorkerThread: new thread created\n");
#endif
				break;
			}
		}

		assert(p);
		assert(i < nbCPU);
#ifdef _WIN32
		assert(ReleaseMutex(hProtectThreadsParameters));
#else
		assert(pthread_mutex_unlock(&hProtectThreadsParameters) == 0);
#endif
		return p->thread;
	}

	assert(FALSE);

#ifdef _WIN32
	return NULL;
#else
	return NULL_THREAD;
#endif
}

static void slvSud(ptThreadParam par)
{
	int i, j, s;
	int nbPos, minNbPos, minLigne, minColonne;

	int* tabTryVect;
	int* tryVect;
	int* minTryVect;


	if (par->maxTry > 0 && par->nbe > par->maxTry) {
		return;
	}

	tabTryVect = malloc(2 * sizeof(int) * par->st->largeur * par->st->hauteur);
	assert(tabTryVect);

	tryVect = tabTryVect;
	minTryVect = tabTryVect + (par->st->largeur * par->st->hauteur);

	/* On recherche les cases pour lesquelles le nombre de possibilités est
	   minimal. S'il y en a plusieurs, on choisit la première */

	minNbPos = par->st->hauteur * par->st->largeur + 1;

	for (i = 0; i < par->st->hauteur * par->st->largeur; i++) {
		for (j = 0; j < par->st->hauteur * par->st->largeur; j++) {
			if (par->st->table[i][j] == 0) {
				/* La case est vide */
				nbPos = 0;
				for (s = 1; s <= par->st->hauteur * par->st->largeur; s++) {
					if (isValid(par->st, s, i, j) == 0)
						tryVect[nbPos++] = s;
				}

				if (nbPos < minNbPos) {
					minNbPos = nbPos;
					for (s = 0; s < nbPos; s++)
						minTryVect[s] = tryVect[s];
					minLigne = i;
					minColonne = j;
				}
			}
		}
	}

	if (minNbPos == par->st->hauteur * par->st->largeur + 1) {
		/* On a trouvé une nouvelle solution */
		workerSolution(par);
		free(tabTryVect);
		return;
	}

	/* Il reste des symboles à placer dans la table, donc on essaie toutes
	   les combinaisons restantes */

	for (s = 0; s < minNbPos; s++) {
		if (terminationRequested) {
			/* L'arrêt a été demandé */
			free(tabTryVect);
			return;
		}

		par->nbe++;

		/* On tente d'allouer un nouveau thead, dans la limite des CPU disponibles dans la machine */
#if _WIN32
		if (WaitForSingleObject(hNbTreadsSemaphore, 0) == WAIT_OBJECT_0) {
#else
		if (sem_trywait(&hNbTreadsSemaphore) == 0) {
#endif
			/* On peut allouer un nouveau thread */

			/* Faire une copie du sudoku*/
			PtSudokuTable newSt = cloneSudokuTable(par->st);
			assert(newSt);

			/* On applique la valeur à tester */
			newSt->table[minLigne][minColonne] = minTryVect[s];

			createNewWorkerThread(newSt, par->nbe, par->maxTry);
		} else {
			// Plus de threads disponibles, on effectue le travail dans ce thread */

			par->st->table[minLigne][minColonne] = minTryVect[s];
			slvSud(par);
			par->st->table[minLigne][minColonne] = 0;
		}
	}

	free(tabTryVect);
}

#ifdef _WIN32
static unsigned int __stdcall slvSudThreadProc(void* vparam)
#else
static void *slvSudThreadProc(void* vparam)
#endif
{
	threadParam* tparam = vparam;

	/* On attend l'autorisation de démarrage pour être sûr que tparam->thread
	   soit bien initiialisé */
#ifdef _WIN32
	assert(WaitForSingleObject(tparam->canContinue, INFINITE) == WAIT_OBJECT_0);
#else
	assert(sem_wait(&tparam->canContinue) == 0);
#endif

	assert(tparam->thread);

	/* Appel de la fonction de travail */
	slvSud(tparam);

	/* La fonction vient de se terminer */
	/* Nettoyage avant le retour */

	tparam->state = TERMINATED;

	releaseSudokuTable(tparam->st);
	tparam->st = NULL;

	/* Libérer le processus principal */
#ifdef DEBOGUAGE
	printf("slvSudThreadProc: realease workerEventSemaphore\n");
#endif
#ifdef _WIN32
	assert(ReleaseSemaphore(workerEventSemaphore, 1, NULL));
#else
	assert(sem_post(&workerEventSemaphore) == 0);
#endif
#ifdef DEBOGUAGE
	printf("slvSudThreadProc: wait for canContinue\n");
#endif
#ifdef _WIN32
	assert(WaitForSingleObject(tparam->canContinue, INFINITE) == WAIT_OBJECT_0);
#else
	assert(sem_wait(&tparam->canContinue) == 0);
#endif
#ifdef DEBOGUAGE
	printf("slvSudThreadProc: passed\n");
#endif

#ifdef _WIN32
	assert(CloseHandle(tparam->canContinue));
	tparam->canContinue = NULL;

	tparam->thread = NULL;
#else
	assert(sem_destroy(&tparam->canContinue) == 0);
	tparam->thread = NULL_THREAD;
#endif

	/* Incrémenter le nombre de CPU disponibles */
#ifdef _WIN32
	assert(ReleaseSemaphore(hNbTreadsSemaphore, 1, NULL));
	_endthreadex(0);
#else
	assert(sem_post(&hNbTreadsSemaphore) == 0);
#endif
	return 0;
}

SUDOKU_SOLVER_DLLIMPORT int __stdcall solveSudokuMaxTry(PtSudokuTable st,
	PtSolvedActionFunction saf, void* param, int maxTry)
	/* maxTry : nombre maximum d'essais accordés pour tenter de résoudre un
	   sudoku. Si maxTry vaut 0, il n'y a pas de limitation au nombre d'essais.*/
{
	bool threadAlive;
	int cr;

#ifdef _WIN32
	SYSTEM_INFO sysinfo;
#else

#endif

	if (nbCPU == -1) {
#ifdef _WIN32
		GetSystemInfo(&sysinfo);

		nbCPU = sysinfo.dwNumberOfProcessors;
#else
        nbCPU = get_nprocs();
#endif

		assert(nbCPU > 0);

		/* Ce sémaphore limite le nombre de processus de travail au nombre de CPU existants dans la machine */
#ifdef _WIN32
		hNbTreadsSemaphore = CreateSemaphore(NULL, nbCPU, nbCPU, NULL);
		assert(hNbTreadsSemaphore);
		hProtectThreadsParameters = CreateMutex(NULL, FALSE, NULL);
		assert(hProtectThreadsParameters);

#else
		assert(sem_init(&hNbTreadsSemaphore, 0, nbCPU) == 0);
		assert(pthread_mutex_init(&hProtectThreadsParameters, NULL) == 0);

#endif

		tparam = malloc(nbCPU * sizeof(threadParam));
		assert(tparam);

		memset(tparam, 0, nbCPU * sizeof(threadParam));

	}

	/* Ce sémaphore sera utilisés par les processus de travail pour signaler la disponibilité des résultats */
#ifdef _WIN32
	workerEventSemaphore = CreateSemaphore(NULL, 0, nbCPU, NULL);
	assert(workerEventSemaphore);
#else
	assert(sem_init(&workerEventSemaphore, 0, 0) == 0);
#endif

#ifdef DEBOGUAGE
	printf("workerEventSemaphore created.\n");
#endif

	terminationRequested = FALSE;

	/* On crée le premier thead, qui lancera les autres dans la limite des CPU disponibles dans la machine */
#ifdef _WIN32
	assert(WaitForSingleObject(hNbTreadsSemaphore, 0) == WAIT_OBJECT_0);
#else
	assert(sem_trywait(&hNbTreadsSemaphore) == 0);
#endif

	/* Faire une copie du sudoku*/
	PtSudokuTable newSt = cloneSudokuTable(st);
	assert(newSt);

	createNewWorkerThread(newSt, 0, maxTry);

	/* Traitement des données retournées par les threads */
	cr = 0;

	do {
		int i;
		bool eventProcessed;

#ifdef DEBOGUAGE
		printf("solveSudokuMaxTry: wait for workerEventSemaphore\n");
#endif
#ifdef _WIN32
		assert(WaitForSingleObject(workerEventSemaphore, INFINITE) == WAIT_OBJECT_0);
#else
		assert(sem_wait(&workerEventSemaphore) == 0);
#endif

#ifdef DEBOGUAGE
		printf("solveSudokuMaxTry: passed\n");
#endif

		eventProcessed = FALSE;
		for (i = 0; !eventProcessed && i < nbCPU; i++) {
			ptThreadParam p = tparam + i;
#ifdef _WIN32
			HANDLE thread = p->thread;
#else
			pthread_t thread = p->thread;
#endif
			if (thread) {
				switch (p->state) {
                case RUNNING:
                    break;
				case SOLUTION_FOUND:
					eventProcessed = TRUE;
					/* Une solution a été trouvée */
					/* Renvoyer le résultat, sauf si l'utilisateur n'en veut pas d'autre */
					if (!terminationRequested && (*saf)(p->st, p->nbe, param)) {
						/* Arrêt demandé */
						terminationRequested = TRUE;
					}
					/* On permet au processus de travail de continuer sa tâche */
					p->state = RUNNING;
#ifdef _WIN32
					assert(ReleaseSemaphore(p->canContinue, 1, NULL));
#else
					assert(sem_post(&p->canContinue) == 0);
#endif

					break;
				case TERMINATED:
					eventProcessed = TRUE;
					/* On permet au processus de travail de continuer sa tâche */
					if (p->maxTry > 0 && p->nbe > p->maxTry) {
						cr = -2;
					}
#ifdef _WIN32
					assert(ReleaseSemaphore(p->canContinue, 1, NULL));
#else
					assert(sem_post(&p->canContinue) == 0);
#endif
					/* Attente de terminaison du thread */
#ifdef _WIN32
					assert(WaitForSingleObject(thread, INFINITE) == WAIT_OBJECT_0);
#else
					assert(pthread_join(thread, NULL) == 0);
#endif

					break;
				}
			}
		}

		threadAlive = FALSE;
		for (i = 0; i < nbCPU; i++) {
			ptThreadParam p = tparam + i;

			if (p->thread) {
				threadAlive = TRUE;
				break;
			}
		}
	} while (threadAlive);

#ifdef _WIN32
	assert(CloseHandle(workerEventSemaphore));
	workerEventSemaphore = NULL;
#else
	assert(sem_destroy(&workerEventSemaphore) == 0);
#endif
#ifdef DEBOGUAGE
	printf("workerEventSemaphore destroyed.\n");
#endif

	return cr?cr:terminationRequested;
}

SUDOKU_SOLVER_DLLIMPORT void __stdcall deinitSudoku(void)
{
#ifdef _WIN32
	if (hNbTreadsSemaphore) {
		assert (CloseHandle(hNbTreadsSemaphore));
		hNbTreadsSemaphore = NULL;
	}

	if (hProtectThreadsParameters) {
		assert (CloseHandle(hProtectThreadsParameters));
		hProtectThreadsParameters = NULL;
	}
#else
	assert(sem_destroy(&hNbTreadsSemaphore) == 0);
	assert(pthread_mutex_destroy(&hProtectThreadsParameters) == 0);
#endif

	if (tparam) {
		free(tparam);
		tparam = NULL;
	}

	nbCPU = -1;
}

SUDOKU_SOLVER_DLLIMPORT void __stdcall solveSudoku(PtSudokuTable st,
	PtSolvedActionFunction saf, void* param)
{
	solveSudokuMaxTry(st, saf, param, 0);
}

#ifdef _WIN32
BOOL APIENTRY DllMain(HMODULE hInst     /* Library instance handle. */,
	DWORD reason        /* Reason this function is being called. */,
	LPVOID reserved     /* Not used. */)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		break;

	case DLL_PROCESS_DETACH:
		break;

	case DLL_THREAD_ATTACH:
		break;

	case DLL_THREAD_DETACH:
		break;
	}

	/* Returns TRUE on success, FALSE on failure */
	return TRUE;
}
#endif
