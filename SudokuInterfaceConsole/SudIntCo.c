#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <SudSlvEn.h>

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

static int nbS;

static bool print = true;

int __stdcall SolutionSudoku(PtSudokuTable st, int nbEssais, void* param)
{
	if (print) {
		printf("\nSolution %u, %u essais :\n", ++nbS, nbEssais);
		printSudoku(st);
	}

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
#ifdef _WIN32
			printf("Cr‚ation d'un Sudoku 3x2 r‚ussie !\n");
#else
			printf("Création d'un Sudoku 3x2 réussie !\n");
#endif
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
#ifdef _WIN32
			printf("Sudoku 3x2 d‚truit !\n");
#else
			printf("Sudoku 3x2 détruit !\n");
#endif
		}
	}
	else {
		FILE* fs;
		int h, l, i, j, s, cr;
#ifdef _WIN32
		errno_t fcr;
#endif
		if (argc > 2) {
			if (!strncmp(argv[2], "--no-print", 11)) {
				print = false;
			}
		}
		printf("argv[1]=%s\n", argv[1]);
#ifdef _WIN32
		fcr = fopen_s(&fs, argv[1], "r");
#else
		fs = fopen(argv[1], "r");
#endif
#ifdef _WIN32
		if (fcr) {
#else
		if (!fs) {
#endif
			printf("Erreur d'ouverture du fichier %s\n", argv[1]);
			return 1;
		}

#ifdef _WIN32
		fscanf_s(fs, "%u", &l);
		fscanf_s(fs, "%u", &h);
#else
		fscanf(fs, "%u", &l);
		fscanf(fs, "%u", &h);
#endif
		st = newSudokuTable(l, h);
		if (st) {
#ifdef _WIN32
			printf("Cr‚ation d'un Sudoku %ux%u r‚ussie !\n",
#else
			printf("Création d'un Sudoku %ux%u réussie !\n",
#endif
				st->largeur, st->hauteur);

			for (i = 0; i < st->largeur * st->hauteur; i++)
				for (j = 0; j < st->largeur * st->hauteur; j++) {
#ifdef _WIN32
					fscanf_s(fs, "%u", &s);
#else
					fscanf(fs, "%u", &s);
#endif // _WIN32
					cr = insertSudokuSymbol(st, s, i, j);
					if (cr) {
						/* Erreur à l'insertion */
						releaseSudokuTable(st);
						return -1;
					}
				}
			fclose(fs);

			for (i = 0; i < 2; i++) {
				int cr;

				printSudoku(st);
				nbS = 0;
				cr = solveSudokuMaxTry(st, SolutionSudoku, NULL, 1000000);
				printf("cr: %d -------------\n", cr);
			}

			printf("Sudoku %ux%u",
				st->largeur, st->hauteur);
			releaseSudokuTable(st);
#ifdef _WIN32
			printf(" d‚truit !\n");
#else
			printf(" détruit !\n");
#endif // _WIN32
		}
	}

	deinitSudoku();

	return 0;
}
