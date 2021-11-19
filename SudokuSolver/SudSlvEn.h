#ifndef _SUDSLVEN_H_
#define _SUDSLVEN_H_

#ifdef CHECK_LEAK
#include "LeakChk.h"
#endif

#if SUDOKUSOLVER_EXPORTS
# define SUDOKU_SOLVER_DLLIMPORT __declspec (dllexport)
#else /* Not BUILDING_DLL */
# define SUDOKU_SOLVER_DLLIMPORT __declspec (dllimport)
#endif /* Not BUILDING_DLL */

#ifdef	__cplusplus
extern "C" {
#endif

	/* D�finition de la structure qui contient un tableau Sudoku */
	typedef struct SUDOKUTABLE {
		int largeur;
		int hauteur;
		int** table;
	} SudokuTable, * PtSudokuTable;

	/* Initialise une nouvelle structure de table Sudoku d'un ordre donn� */
	SUDOKU_SOLVER_DLLIMPORT PtSudokuTable __stdcall newSudokuTable(int largeur, int hauteur);
	/* ---------- */

	/* Lib�re une structure de table Sudoku */
	SUDOKU_SOLVER_DLLIMPORT void __stdcall releaseSudokuTable(PtSudokuTable st);
	/* ---------- */

	/* Introduit un symbole dans la table Sudoku
		 st est un pointeur sur une structure de table sudoku,
		 symbole est un nombre compris entre 0 (pour effacer un symbole d�j� ins�r�)
		 et largeur*hauteur que l'on va placer dans le tableau,
		 ligne d�signe la ligne dans laquelle on va ins�rer le symbole, compris
		 entre 0 et largeur*hauteur-1,
		 colonne d�signe la colonne dans laquelle on va ins�rer le symbole, compris
		 entre 0 et largeur*hauteur-1.

		 La fonction retourne 0 si l'insertion a �t� effectu�e, ou l'une des valeurs
		 suivantes en cas d'erreur :
							 10 : ligne ou colonnes sont hors limites,
							 11 : symbole est hors limites,
							 1  : le symbole existe d�j� sur la ligne,
							 2  : le symbole existe d�j� sur la colonne,
							 3  : le symbole existe d�j� dans la r�gion.
	*/
	SUDOKU_SOLVER_DLLIMPORT int __stdcall insertSudokuSymbol(PtSudokuTable st, int symbole,
		int ligne, int colonne);
	/* ---------- */

	/* Retourne le symbole pr�sent dans une case du tableau *
		 retourne -1 s'il y a une erreur */

	SUDOKU_SOLVER_DLLIMPORT int __stdcall getSudokuSymbol(PtSudokuTable st,
		int ligne, int colonne);


	/* D�finition de la fonction "callback" appel�e par le solveur � chaque solution
	   trouv�e.
	   st pointe sur la structure contenant la solution,
	   nbEssais indique le nombre de symboles qui ont �t� essay�s depuis le d�but de
		 la r�solution,
	   param contient la valeur fournie par le programme appelant lors de l'appel de
		   la fonction solveSudoku
		
		Valeur de retour : la fonction peut retourner 0 si elle souhaite poursuivre
							la recherche de solution ou une valeur diff�rente de 0
							si la recherche doit s'arr�ter l�.
	*/
	typedef int __stdcall SolvedActionFunction(PtSudokuTable st, int nbEssais,
		void* param);
	typedef SolvedActionFunction* PtSolvedActionFunction;

	/* R�soud le sudoku et appelle la fonction fournie par l'utilisateur losque
	   une solution est trouv�e.
		 st est un pointeur sur la structure contenant le Sudoku � r�soudre
		 saf est un pointeur sur la fonction "callback" qui sera appel�e
		 par le solveur � chaque fois qu'une solution est trouv�e
		 param est une donn�e � l'usage exclusif du programme appelant le solveur.
					 Sa valeur sera retourn�e � chaque appel de saf */

	SUDOKU_SOLVER_DLLIMPORT void __stdcall solveSudoku(PtSudokuTable st,
		PtSolvedActionFunction saf, void* param);

	/* R�soud le sudoku et appelle la fonction fournie par l'utilisateur losque
	   une solution est trouv�e.
		 st est un pointeur sur la structure contenant le Sudoku � r�soudre
		 saf est un pointeur sur la fonction "callback" qui sera appel�e
		 par le solveur � chaque fois qu'une solution est trouv�e
		 param est une donn�e � l'usage exclusif du programme appelant le solveur.
					 Sa valeur sera retourn�e � chaque appel de saf
		 maxTry est le nombre maximal d'essais allou�s pour r�soudre le sudoku. Si
		 ce nombre d'essais est d�pass�, la fonction retournera un code d'erreur -2.
		 si maxTry vaut 0, il n'y a pas de limite au nombre d'essais */

	SUDOKU_SOLVER_DLLIMPORT int __stdcall solveSudokuMaxTry(PtSudokuTable st,
		PtSolvedActionFunction saf, void* param, int maxTry);

#ifdef	__cplusplus
}
#endif

#endif /* _SUDSVLEN_H_ */

