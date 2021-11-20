#include "SudSlvEn.h"
#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>

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

HANDLE hNbTreadsSemaphore;
int nbCPU;

typedef struct THREAD_PARAM {
	PtSudokuTable st;
	PtSolvedActionFunction saf;
	int nbe;
	void* uparam;
	int maxTry;
	int cr;
} threadParam, * ptThreadParam;

static unsigned __stdcall slvSudThreadProc(void* param);

int waitForThreadsTermination(int *nbt, HANDLE* threads, ptThreadParam tparam, int* pnbe)
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

static int slvSud(PtSudokuTable st, PtSolvedActionFunction saf,
	int* pnbe, void* param, int maxTry)
{
	int i, j, s, cr;
	int nbPos, minNbPos, minLigne, minColonne;

	int* tabTryVect;
	int* tryVect;
	int* minTryVect;
	int tix;

	HANDLE* threads;
	int nbUsedThreads;

	ptThreadParam tparam;

	if (maxTry > 0 && *pnbe > maxTry)
		return -2;

	tabTryVect = malloc(2 * sizeof(int) * st->largeur * st->hauteur);
	if (!tabTryVect)
		return -1;

	tryVect = tabTryVect;
	minTryVect = tabTryVect + (st->largeur * st->hauteur);

	threads = malloc(nbCPU * sizeof(HANDLE));
	if (!threads) {
		free(tabTryVect);
		return -1;
	}

	tparam = malloc(nbCPU * sizeof(threadParam));
	if (!tparam) {
		free(tabTryVect);
		free(threads);
		return -1;
	}


	/* On recherche les cas pour lesquelles le nombre de possibilités est
	   minimal. S'il y en a plusieurs, on choisit la première */
	minNbPos = st->hauteur * st->largeur + 1;
	for (i = 0; i < st->hauteur * st->largeur; i++)
		for (j = 0; j < st->hauteur * st->largeur; j++)
			if (st->table[i][j] == 0) {
				/* La case est vide */
				nbPos = 0;
				for (s = 1; s <= st->hauteur * st->largeur; s++)
					if (isValid(st, s, i, j) == 0)
						tryVect[nbPos++] = s;
				if (nbPos < minNbPos) {
					minNbPos = nbPos;
					for (s = 0; s < nbPos; s++)
						minTryVect[s] = tryVect[s];
					minLigne = i;
					minColonne = j;
				}
			}
	// Pour débug
	//    printf("minNbPos = %u [%u, %u] : ", minNbPos, minLigne, minColonne);
	//    for (s=0; s<minNbPos; s++)
	//    		printf("%u ", minTryVect[s]);
	//    printf("\n");
	// Fin débug
	if (minNbPos == st->hauteur * st->largeur + 1) {
		/* On a trouvé une nouvelle solution */
		cr = (*saf)(st, *pnbe, param);
		free(tparam);
		free(threads);
		free(tabTryVect);
		return cr;
	}

	/* Il reste des symboles à placer dans la table, donc on essaie toutes
	   les combinaisons restantes */

	cr = 0;
	nbUsedThreads = 0;
	for (s = 0; s < minNbPos; s++) {
		/* On tente d'allouer un nouveau thead, dans la limite des CPU disponibles dans la machine */
		if (WaitForSingleObject(hNbTreadsSemaphore, 0) == WAIT_OBJECT_0) {
			/* On peut allouer un nouveau thread */

			/* Faire une copie du sudoku*/
			PtSudokuTable newSt = cloneSudokuTable(st);
			if (!newSt) {
				free(tparam);
				free(threads);
				free(tabTryVect);
				return -1;
			}

			/* On applique la valeur à tester */
			newSt->table[minLigne][minColonne] = minTryVect[s];

			/* On remplit une structure de données pour passer les arguments au thread */
			tparam[nbUsedThreads].st = newSt;
			tparam[nbUsedThreads].saf = saf;
			tparam[nbUsedThreads].nbe = *pnbe;
			tparam[nbUsedThreads].uparam = param;
			tparam[nbUsedThreads].maxTry = maxTry;

			/* On crée le thread */
			threads[nbUsedThreads] = (HANDLE) _beginthreadex(NULL, 0, slvSudThreadProc, (void *) (tparam + nbUsedThreads), 0, NULL);
			if (!threads[nbUsedThreads]) {
				free(tparam);
				/* Provoquer la fin des éventuels threads lancés */
				for (tix = 0; tix < nbUsedThreads; tix++) {
					if (!TerminateThread(threads[tix], 0)) {
						cr = -1;
					}
				}
				free(tparam);
				free(threads);
				return -1;
			}
			nbUsedThreads++;
		} else {
			// Plus de threads disponibles, on effectue le travail dans ce thread */

			st->table[minLigne][minColonne] = minTryVect[s];
			(*pnbe)++;
			cr = slvSud(st, saf, pnbe, param, maxTry);
			if (cr) {
				/* Il y a une erreur */
				st->table[minLigne][minColonne] = 0;
				free(tabTryVect);
				/* Provoquer la fin des éventuels threads lancés */
				for (tix = 0; tix < nbUsedThreads; tix++) {
					if (!TerminateThread(threads[tix], 0)) {
						cr = -1;
					}
				}
				free(tparam);
				free(threads);
				return cr;
			}

			st->table[minLigne][minColonne] = 0;
			/* Attendre la fin des éventuels threads lancés */
			cr = waitForThreadsTermination(&nbUsedThreads, threads, tparam, pnbe);
		}
	}
	/* attendre la fin des éventuels threads lancés */
	cr = waitForThreadsTermination(&nbUsedThreads, threads, tparam, pnbe);
	free(tparam);
	free(threads);
	free(tabTryVect);
	return cr;
}

static unsigned __stdcall slvSudThreadProc(void* vparam)
{
	threadParam* tparam = vparam;

	/* Appel de la fonction de travail */
	tparam-> cr = slvSud(tparam->st, tparam->saf, &(tparam->nbe), tparam->uparam, tparam->maxTry);

	_endthreadex(0);
	return 0;
}

SUDOKU_SOLVER_DLLIMPORT int __stdcall solveSudokuMaxTry(PtSudokuTable st,
	PtSolvedActionFunction saf, void* param, int maxTry)
	/* maxTry : nombre maximum d'essais accordés pour tenter de résoudre un
	   sudoku. Si maxTry vaut 0, il n'y a pas de limitation au nombre d'essais.*/
{
	int nbe = 0;
	int cr;

	SYSTEM_INFO sysinfo;

	GetSystemInfo(&sysinfo);
// Pour test
//	nbCPU = 0;
	nbCPU = sysinfo.dwNumberOfProcessors-1; /* On retire 1 processeur, car il y a déjà un thread en cours */


//	fprintf(stderr, "%d coeurs\n", nbCPU);

	if (nbCPU > 0) {
		hNbTreadsSemaphore = CreateSemaphore(NULL, nbCPU, nbCPU, NULL);
		if (!hNbTreadsSemaphore) {
			return -1;
		}
	}


	cr = slvSud(st, saf, &nbe, param, maxTry);

	CloseHandle(hNbTreadsSemaphore);
	return cr;
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
