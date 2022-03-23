#include <stdio.h>
#include <stdlib.h>
#include "SudSlvEn.h"

void printSudoku(PtSudokuTable st)
{
	int i, j;

	printf("%u\n%u\n", st->largeur, st->hauteur);
	for (i = 0; i < st->largeur * st->hauteur; i++) {
		for (j = 0; j < st->largeur * st->hauteur; j++)
			printf("%u ", getSudokuSymbol(st, i, j));
		printf("\n");
	}
}

int __stdcall SolutionSudoku(PtSudokuTable st, int nbEssais, void* param)
{
	static int nbS = 0;

	printf("\nSolution %u, %u essais :\n", ++nbS, nbEssais);
	printSudoku(st);
	return 0;
}

int main(int argc, char* argv[])
{
	PtSudokuTable st;
	int cr, symbole;

	printf("argc = %u\n", argc);
	if (argc <= 1) {
		st = newSudokuTable(3, 2);
		if (st) {
			printf("Cr‚ation d'un Sudoku 3x2 r‚ussie !\n");

			cr = insertSudokuSymbol(st, 5, 1, 0);
			printf("insertSudokuSymbol(st, 5, 1, 0) = %u\n", cr);
			cr = insertSudokuSymbol(st, 5, 1, 0);
			printf("insertSudokuSymbol(st, 5, 1, 0) = %u\n", cr);
			cr = insertSudokuSymbol(st, 5, 0, -1);
			printf("insertSudokuSymbol(st, 5, 0, -1) = %u\n", cr);
			cr = insertSudokuSymbol(st, 5, -1, 0);
			printf("insertSudokuSymbol(st, 5, -1, 0) = %u\n", cr);
			cr = insertSudokuSymbol(st, 5, 6, 0);
			printf("insertSudokuSymbol(st, 5, 6, 0) = %u\n", cr);
			cr = insertSudokuSymbol(st, 5, 0, 6);
			printf("insertSudokuSymbol(st, 5, 0, 6) = %u\n", cr);
			cr = insertSudokuSymbol(st, -1, 1, 0);
			printf("insertSudokuSymbol(st, -1, 0, 1) = %u\n", cr);
			cr = insertSudokuSymbol(st, 7, 1, 0);
			printf("insertSudokuSymbol(st, 7, 0, 1) = %u\n", cr);
			cr = insertSudokuSymbol(st, 5, 1, 5);
			printf("insertSudokuSymbol(st, 5, 1, 5) = %u\n", cr);
			cr = insertSudokuSymbol(st, 5, 4, 0);
			printf("insertSudokuSymbol(st, 5, 4, 0) = %u\n", cr);
			cr = insertSudokuSymbol(st, 5, 0, 2);
			printf("insertSudokuSymbol(st, 5, 0, 2) = %u\n", cr);
			cr = insertSudokuSymbol(st, 4, 0, 2);
			printf("insertSudokuSymbol(st, 4, 0, 2) = %u\n", cr);
			cr = insertSudokuSymbol(st, 5, 2, 1);
			printf("insertSudokuSymbol(st, 5, 2, 1) = %u\n", cr);

			symbole = getSudokuSymbol(st, 2, 1);
			printf("getSudokuSymbol(st, 2, 1) = %u\n", symbole);

			symbole = getSudokuSymbol(st, 5, 5);
			printf("getSudokuSymbol(st, 5, 5) = %u\n", symbole);

			symbole = getSudokuSymbol(st, 6, 5);
			printf("getSudokuSymbol(st, 6, 5) = %d\n", symbole);
			releaseSudokuTable(st);
			printf("Sudoku 3x2 d‚truit !\n");
		}
	}
	else {
		FILE* fs;
		int h, l, i, j, s, cr;
		errno_t fcr;

		printf("argv[1]=%s\n", argv[1]);
		fcr = fopen_s(&fs, argv[1], "r");
		if (fcr) {
			printf("Erreur d'ouverture du fichier %s\n", argv[1]);
			return 1;
		}
		fscanf_s(fs, "%u", &l);
		fscanf_s(fs, "%u", &h);
		st = newSudokuTable(l, h);
		if (st) {
			printf("Cr‚ation d'un Sudoku %ux%u r‚ussie !\n",
				st->largeur, st->hauteur);

			for (i = 0; i < st->largeur * st->hauteur; i++)
				for (j = 0; j < st->largeur * st->hauteur; j++) {
					fscanf_s(fs, "%u", &s);
					cr = insertSudokuSymbol(st, s, i, j);
					if (cr) {
						/* Erreur à l'insertion */
						releaseSudokuTable(st);
						return -1;
					}
				}
			fclose(fs);

			for (i = 0; i < 2; i++) {
				printSudoku(st);
				solveSudoku(st, SolutionSudoku, NULL);
			}

			printf("Sudoku %ux%u",
				st->largeur, st->hauteur);
			releaseSudokuTable(st);
			printf(" d‚truit !\n");
		}
	}

	deinitSudoku();

	return 0;
}
