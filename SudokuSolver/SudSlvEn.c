#include "SudSlvEn.h"
#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <strsafe.h>

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

/* A initialiser au dépat quand nbCPU = -1*/
HANDLE hNbTreadsSemaphore;
HANDLE hProtectThreadsParameters;

typedef enum {
	RUNNING,
	SOLUTION_FOUND,
	TERMINATED,
} workerState;

typedef struct THREAD_PARAM {
	HANDLE thread;
	PtSudokuTable st;
	int nbe;
	int maxTry;
	workerState state;
	HANDLE canContinue;
} threadParam, * ptThreadParam;

static ptThreadParam tparam;

/* A initialiser à chaque appel de solveSudokuMaxTry */
static HANDLE solutionFoundSemaphore;
static volatile bool terminationRequested;

static unsigned int __stdcall slvSudThreadProc(void* param);

#if 0
static int waitForThreadsTermination(int *nbt, HANDLE* threads, ptThreadParam tparam, int* pnbe)
{
	while (*nbt) {
		(*nbt)--;
		if (WaitForSingleObject(threads[*nbt], INFINITE) == WAIT_OBJECT_0) {
			*pnbe += tparam->nbe;

			/* Nettoyage avant le retour */
			releaseSudokuTable(tparam[*nbt].st);

			if (!ReleaseSemaphore(hNbTreadsSemaphore, 1, NULL)) {
				return -1;
			}
			if (tparam[*nbt].cr) {
				return tparam->cr;
			}
		}
	}

	return 0;
}
#endif

void workerSolution(ptThreadParam param)
{
	/* Retourner la solution au thread principal */
	param->state = SOLUTION_FOUND;
	assert(ReleaseSemaphore(solutionFoundSemaphore, 1, NULL));
	printf("realease solutionFoundSemaphore\n");
	assert(WaitForSingleObject(param->canContinue, INFINITE) == WAIT_OBJECT_0);
}

HANDLE createNewWorkerThread(PtSudokuTable nst, int nbe, int maxTry)
{
	if (WaitForSingleObject(hProtectThreadsParameters, INFINITE) == WAIT_OBJECT_0) {
		/* On a obtenu le droit de modifier nbUsedThreads */
		int i;
		ptThreadParam p=NULL;

		for (i = 0; i < nbCPU; i++) {
			p = tparam + i;
			if (!p->thread) {
				/* On remplit une structure de données pour passer les arguments au thread */
				p->st = nst;
				p->nbe = nbe;
				p->maxTry = maxTry;

				/* Ce sémaphore permet de bloquer le processus de travail pendant que le processus 
				   principal traite le résultat */
				p->canContinue = CreateSemaphore(NULL, 0, 1, NULL);
				assert(p->canContinue);

				/* On crée le thread */
				p->thread = (HANDLE)_beginthreadex(NULL, 0, slvSudThreadProc, (void*)p, 0, NULL);
				assert(p->thread);

				break;
			}
		}

		assert(p);
		assert(i < nbCPU);
		assert(ReleaseMutex(hProtectThreadsParameters));

		return p->thread;
	}

	return NULL;
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
	}

	/* Il reste des symboles à placer dans la table, donc on essaie toutes
	   les combinaisons restantes */

	for (s = 0; s < minNbPos; s++) {
		if (terminationRequested) {
			/* L'arrêt a été demandé */
			free(tabTryVect);
			return;
		}

		/* On tente d'allouer un nouveau thead, dans la limite des CPU disponibles dans la machine */
		if (WaitForSingleObject(hNbTreadsSemaphore, 0) == WAIT_OBJECT_0) {
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
			par->nbe++;
			slvSud(par);
			par->st->table[minLigne][minColonne] = 0;
		}
	}

	free(tabTryVect);
}

static void ErrorExit(LPTSTR lpszFunction)
{
	// Retrieve the system error message for the last-error code
	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;
	DWORD dw = GetLastError();
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL
		,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf,
		0
		,
		NULL
	);
	// Display the error message and exit the process
	lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
		(lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof	(TCHAR));
	StringCchPrintf((LPTSTR)lpDisplayBuf,
		LocalSize(lpDisplayBuf) /
		sizeof
		(TCHAR),
		TEXT(
			"%s failed with error %d: %s"
		),
		lpszFunction, dw, lpMsgBuf);
	MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);
	LocalFree(lpMsgBuf);
	LocalFree(lpDisplayBuf);
	ExitProcess(dw);
}

static unsigned int __stdcall slvSudThreadProc(void* vparam)
{
	threadParam* tparam = vparam;

	/* Appel de la fonction de travail */
	slvSud(tparam);

	/* La fonction vient de se terminer */
	/* Nettoyage avant le retour */

	releaseSudokuTable(tparam->st);
	tparam->st = NULL;

	assert (CloseHandle(tparam->canContinue));
	tparam->canContinue = NULL;

	/* Libère le bloc */
	tparam->thread = NULL;

	/* Incrémenter le nombre de CPU disponibles */
	assert (ReleaseSemaphore(hNbTreadsSemaphore, 1, NULL));

	/* Libérer le processus principal */
	if (!ReleaseSemaphore(solutionFoundSemaphore, 1, NULL)) {
		printf("realease solutionFoundSemaphore\n");
		ErrorExit(TEXT("slvSudThreadProc"));
	}

	_endthreadex(0);
	return 0;
}

SUDOKU_SOLVER_DLLIMPORT int __stdcall solveSudokuMaxTry(PtSudokuTable st,
	PtSolvedActionFunction saf, void* param, int maxTry)
	/* maxTry : nombre maximum d'essais accordés pour tenter de résoudre un
	   sudoku. Si maxTry vaut 0, il n'y a pas de limitation au nombre d'essais.*/
{
	int nbe = 0;
	bool theadAlive = TRUE;
	
	SYSTEM_INFO sysinfo;

	if (nbCPU == -1) {
		GetSystemInfo(&sysinfo);

		nbCPU = sysinfo.dwNumberOfProcessors;

		assert(nbCPU > 0);

		/* Ce sémaphore limite le nombre de processus de travail au nombre de CPU existants dans la machine */
		hNbTreadsSemaphore = CreateSemaphore(NULL, nbCPU, nbCPU, NULL);
		assert(hNbTreadsSemaphore);

		hProtectThreadsParameters = CreateMutex(NULL, FALSE, NULL);
		assert(hProtectThreadsParameters);

		tparam = malloc(nbCPU * sizeof(threadParam));
		assert(tparam);

		memset(tparam, 0, nbCPU * sizeof(threadParam));

	}

	/* Ce sémaphore sera utilisés par les processus de travail pour signaler la disponibilité des résultats */
	solutionFoundSemaphore = CreateSemaphore(NULL, 0, nbCPU, NULL);
	assert(solutionFoundSemaphore);
	printf("solutionFoundSemaphore created.\n");

	terminationRequested = FALSE;

	/* On crée le premier thead, qui lancera les autres dans la limite des CPU disponibles dans la machine */
	assert(WaitForSingleObject(hNbTreadsSemaphore, 0) == WAIT_OBJECT_0);

	/* Faire une copie du sudoku*/
	PtSudokuTable newSt = cloneSudokuTable(st);
	assert(newSt);

	createNewWorkerThread(newSt, 0, maxTry);

	/* Traitement des données retournées par les threads */
	while (theadAlive) {
		int i;

		printf("Wait for solutionFoundSemaphore\n");
		assert(WaitForSingleObject(solutionFoundSemaphore, INFINITE) == WAIT_OBJECT_0);

		theadAlive = FALSE;
		for (i = 0; i < nbCPU; i++) {
			ptThreadParam p = tparam + i;

			switch (p->state) {
			case RUNNING:
				theadAlive = TRUE;
				break;
			case SOLUTION_FOUND:
				theadAlive = TRUE;
				/* Une solution a été trouvée */
				p->state = RUNNING;
				/* Renvoyer le résultat */
				if ((*saf)(p->st, p->nbe, param)) {
					/* Arrêt demandé */
					terminationRequested = TRUE;
				}
				/* On permet au processus de travail de continuer sa tâche */
				assert(ReleaseSemaphore(p->canContinue, 1, NULL));
				break;
			case TERMINATED:
				/* On permet au processus de travail de continuer sa tâche */
				assert(ReleaseSemaphore(p->canContinue, 1, NULL));
				break;
			}
		}

		if (i >= nbCPU) {
			/* Tous les threads sont terminés */
			break;
		}
	}

	assert (CloseHandle(solutionFoundSemaphore));
	solutionFoundSemaphore = NULL;
	printf("solutionFoundSemaphore destroyed.\n");

	return terminationRequested;
}

SUDOKU_SOLVER_DLLIMPORT void __stdcall deinitSudoku(void)
{
	if (hNbTreadsSemaphore) {
		assert (CloseHandle(hNbTreadsSemaphore));
		hNbTreadsSemaphore = NULL;
	}

	if (hProtectThreadsParameters) {
		assert (CloseHandle(hProtectThreadsParameters));
		hProtectThreadsParameters = NULL;
	}

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
