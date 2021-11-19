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

	/* Définition de la structure qui contient un tableau Sudoku */
	typedef struct SUDOKUTABLE {
		int largeur;
		int hauteur;
		int** table;
	} SudokuTable, * PtSudokuTable;

	/* Initialise une nouvelle structure de table Sudoku d'un ordre donné */
	SUDOKU_SOLVER_DLLIMPORT PtSudokuTable __stdcall newSudokuTable(int largeur, int hauteur);
	/* ---------- */

	/* Libère une structure de table Sudoku */
	SUDOKU_SOLVER_DLLIMPORT void __stdcall releaseSudokuTable(PtSudokuTable st);
	/* ---------- */

	/* Introduit un symbole dans la table Sudoku
		 st est un pointeur sur une structure de table sudoku,
		 symbole est un nombre compris entre 0 (pour effacer un symbole déjà inséré)
		 et largeur*hauteur que l'on va placer dans le tableau,
		 ligne désigne la ligne dans laquelle on va insérer le symbole, compris
		 entre 0 et largeur*hauteur-1,
		 colonne désigne la colonne dans laquelle on va insérer le symbole, compris
		 entre 0 et largeur*hauteur-1.

		 La fonction retourne 0 si l'insertion a été effectuée, ou l'une des valeurs
		 suivantes en cas d'erreur :
							 10 : ligne ou colonnes sont hors limites,
							 11 : symbole est hors limites,
							 1  : le symbole existe déjà sur la ligne,
							 2  : le symbole existe déjà sur la colonne,
							 3  : le symbole existe déjà dans la région.
	*/
	SUDOKU_SOLVER_DLLIMPORT int __stdcall insertSudokuSymbol(PtSudokuTable st, int symbole,
		int ligne, int colonne);
	/* ---------- */

	/* Retourne le symbole présent dans une case du tableau *
		 retourne -1 s'il y a une erreur */

	SUDOKU_SOLVER_DLLIMPORT int __stdcall getSudokuSymbol(PtSudokuTable st,
		int ligne, int colonne);


	/* Définition de la fonction "callback" appelée par le solveur à chaque solution
	   trouvée.
	   st pointe sur la structure contenant la solution,
	   nbEssais indique le nombre de symboles qui ont été essayés depuis le début de
		 la résolution,
	   param contient la valeur fournie par le programme appelant lors de l'appel de
		   la fonction solveSudoku
		
		Valeur de retour : la fonction peut retourner 0 si elle souhaite poursuivre
							la recherche de solution ou une valeur différente de 0
							si la recherche doit s'arrêter là.
	*/
	typedef int __stdcall SolvedActionFunction(PtSudokuTable st, int nbEssais,
		void* param);
	typedef SolvedActionFunction* PtSolvedActionFunction;

	/* Résoud le sudoku et appelle la fonction fournie par l'utilisateur losque
	   une solution est trouvée.
		 st est un pointeur sur la structure contenant le Sudoku à résoudre
		 saf est un pointeur sur la fonction "callback" qui sera appelée
		 par le solveur à chaque fois qu'une solution est trouvée
		 param est une donnée à l'usage exclusif du programme appelant le solveur.
					 Sa valeur sera retournée à chaque appel de saf */

	SUDOKU_SOLVER_DLLIMPORT void __stdcall solveSudoku(PtSudokuTable st,
		PtSolvedActionFunction saf, void* param);

	/* Résoud le sudoku et appelle la fonction fournie par l'utilisateur losque
	   une solution est trouvée.
		 st est un pointeur sur la structure contenant le Sudoku à résoudre
		 saf est un pointeur sur la fonction "callback" qui sera appelée
		 par le solveur à chaque fois qu'une solution est trouvée
		 param est une donnée à l'usage exclusif du programme appelant le solveur.
					 Sa valeur sera retournée à chaque appel de saf
		 maxTry est le nombre maximal d'essais alloués pour résoudre le sudoku. Si
		 ce nombre d'essais est dépassé, la fonction retournera un code d'erreur -2.
		 si maxTry vaut 0, il n'y a pas de limite au nombre d'essais */

	SUDOKU_SOLVER_DLLIMPORT int __stdcall solveSudokuMaxTry(PtSudokuTable st,
		PtSolvedActionFunction saf, void* param, int maxTry);

#ifdef	__cplusplus
}
#endif

#endif /* _SUDSVLEN_H_ */

