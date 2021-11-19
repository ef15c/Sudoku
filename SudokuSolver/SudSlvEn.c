#include "SudSlvEn.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

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

static int slvSud(PtSudokuTable st, PtSolvedActionFunction saf,
	int* pnbe, void* param, int maxTry)
{
	int i, j, s, cr;
	int nbPos, minNbPos, minLigne, minColonne;

	int* tabTryVect;
	int* tryVect;
	int* minTryVect;

	if (maxTry > 0 && *pnbe > maxTry)
		return -2;

	tabTryVect = malloc(2 * sizeof(int) * st->largeur * st->hauteur);
	if (!tabTryVect)
		return -1;

	tryVect = tabTryVect;
	minTryVect = tabTryVect + (st->largeur * st->hauteur);

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
		free(tabTryVect);
		return cr;
	}

	/* Il reste des symboles à placer dans la table, donc on essaie toutes
	   les combinaisons restantes */

	for (s = 0; s < minNbPos; s++) {
		st->table[minLigne][minColonne] = minTryVect[s];
		(*pnbe)++;
		cr = slvSud(st, saf, pnbe, param, maxTry);
		if (cr) {
			/* Il y a une erreur */
			st->table[minLigne][minColonne] = 0;
			free(minTryVect);
			return cr;
		}
	}
	st->table[minLigne][minColonne] = 0;
	free(tabTryVect);
	return 0;
}

SUDOKU_SOLVER_DLLIMPORT int __stdcall solveSudokuMaxTry(PtSudokuTable st,
	PtSolvedActionFunction saf, void* param, int maxTry)
	/* maxTry : nombre maximum d'essais accordés pour tenter de résoudre un
	   sudoku. Si maxTry vaut 0, il n'y a pas de limitation au nombre d'essais.*/
{
	int nbe = 0;
	int cr;

	cr = slvSud(st, saf, &nbe, param, maxTry);

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
