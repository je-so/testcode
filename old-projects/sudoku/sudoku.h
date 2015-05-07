#ifndef SUDOKU_H
#define SUDOKU_H

#include <stdbool.h>
#include <stdint.h>

typedef struct sudoku_t        sudoku_t;
typedef struct sudoku_cells_t  sudoku_cells_t;
typedef struct sudoku_solver_t sudoku_solver_t;

struct sudoku_cells_t {
   int size; // size <= 81
   uint8_t row[81/*0..size-1*/];
   uint8_t col[81/*0..size-1*/];
};

#define sudoku_cells_INIT { 0, { 0 }, { 0 } }

static inline void append_sudokucells(sudoku_cells_t* cells, int row/*0..8*/, int col/*0..8*/)
{
   if (cells->size < 81) {
      /* should never overflow cause there are only 81 possible cells */
      cells->row[cells->size] = row;
      cells->col[cells->size] = col;
      ++ cells->size;
   }
}

static inline void remove_sudokucells(sudoku_cells_t* cells, /*out*/int* row, /*out*/int* col)
{
   if (cells->size > 0) {
      -- cells->size;
      *row = cells->row[cells->size];
      *col = cells->col[cells->size];
   }
}


struct sudoku_t {
   uint16_t grid[9][9]; /* grid of 81 cells organized into a table of 9 rows and 9 columns */
};

/* describes list of all numbers from 1..9 encoded in a single bit field */
#define sudoku_INIT_CELL \
         (1/*1*/+2/*2*/+4/*3*/+8/*4*/+16/*5*/+32/*6*/+64/*7*/+128/*8*/+256/*9*/)

/* Inits every cell in sudoku to a list of all possible numbers from 1..9 */
void init_sudoku(/*out*/sudoku_t* sudoku);

/* Returns true in case cell (row,col) contains a single number */
static inline bool issolved_sudoku(const sudoku_t* sudoku, int row/*0..8*/, int col/*0..8*/)
{
         int cell = sudoku->grid[row][col];
         return 0 == (cell & (cell-1));
}

/* Returns true in case cell (row,col) contains the number nr in its list*/
static inline bool isnr_sudoku(const sudoku_t* sudoku, int row/*0..8*/, int col/*0..8*/, int nr/*1..9*/)
{
         int cell = sudoku->grid[row][col];
         return 0 != (cell & (1 << (nr-1)));
}

/* Returns the number stored in cell (row,col). In case of an unsolved cell the number with the lowest value is returned. */
static inline int getnr_sudoku(const sudoku_t* sudoku, int row/*0..8*/, int col/*0..8*/)
{
         int cell = sudoku->grid[row][col];
         int nr = 1;
         while (cell >>= 1) ++nr;
         return nr;
}

/* Returns the length of list of numbers stored in a cell. If the cell is solved 1 is returned.
 * But use function <issolved_sudoku> to test for a single stored number. */
static inline int countnr_sudoku(const sudoku_t* sudoku, int row/*0..8*/, int col/*0..8*/)
{
         int count = 0;
         for (int cell = sudoku->grid[row][col]; cell; cell = cell & (cell-1)/*clear least sign. bit.*/) {
            ++count;
         }
         return count;
}

/* Prints the whole sudoku ; can be used for unsolved sudokus as well. */
void print_sudoku(const sudoku_t* sudoku);

/* reads sudoku from the STDIN as simple string.
 * Format: a digit (1-9) represents a cell value, a dot or digit 0 represents an empty cell.
 * The xth value (starting from 0) is assigned to cell (x/9, x%9).
 * White space and newline are ignored.
 *
 * Example:
 *
 * 0 0 0 2 0 0 0 0 0
 * 0 9 0 0 0 0 0 4 0
 * 7 3 0 4 0 0 0 0 8
 * 0 0 9 0 0 0 0 0 6
 * 0 0 8 0 0 0 0 3 0
 * 1 0 5 6 0 0 0 0 9
 * 0 0 2 0 3 0 0 6 0
 * 0 6 0 0 7 0 0 0 0
 * 0 0 0 9 4 0 5 0 0
 *
 * or with dots
 *
 * . . . 2 . . . . .
 * . 9 . . . . . 4 .
 * 7 3 . 4 . . . . 8
 * . . 9 . . . . . 6
 * . . 8 . . . . 3 .
 * 1 . 5 6 . . . . 9
 * . . 2 . 3 . . 6 .
 * . 6 . . 7 . . . .
 * . . . 9 4 . 5 . .
 *
 * */
int read_sudoku(/*out*/uint8_t grid[9][9]);

/* prints the whole sudoku as a simple square
 *
 * For example:
 * 5 8 4 2 6 9 3 7 1
 * 2 9 1 3 8 7 6 4 5
 * 7 3 6 4 5 1 2 9 8
 * 3 2 9 8 1 4 7 5 6
 * 6 4 8 7 9 5 1 3 2
 * 1 7 5 6 2 3 4 8 9
 * 4 5 2 1 3 8 9 6 7
 * 9 6 3 5 7 2 8 1 4
 * 8 1 7 9 4 6 5 2 3
 * */
void printstring_sudoku(const sudoku_t* sudoku);



struct sudoku_solver_t {
   sudoku_t       sudoku;
   sudoku_cells_t single;
   int            solved_count;
   int            removed_count;
};

void init_sudokusolver(/*out*/sudoku_solver_t* solver);

/* In case nr[r][c] == 0 the content of the cell (r,c) is not changed.
 * Returns EINVAL if the numbers are conflicting. */
int preset_sudokusolver(sudoku_solver_t* solver, uint8_t nr[9][9]);

/* returns EINVAL in case of invalid number or if cell does not contain nr as possible solution.
 * The nr is removed from all other cells in the same row, column and box and if such a modified
 * cell contains only a single number it is added to list_solved in case list_solved not NULL.
 * If a modiefied cell would contain no more possible numbers the process is stopped cause
 * then the current solution is wrong and EINVAL is returned. */
int solvecell_sudokusolver(sudoku_solver_t* solver, int row/*0..8*/, int col/*0..8*/, int nr/*1..9*/);

/*
 * TODO: describe solve_sudokusolver
 * */
int solve_sudokusolver(sudoku_solver_t* solver);

/*
 * TODO: describe trysingle_sudokusolver
 */
int trysingle_sudokusolver(sudoku_solver_t* solver);

/*
 * TODO: describe tryhiddensingle_sudokusolver
 */
int tryhiddensingle_sudokusolver(sudoku_solver_t* solver);

/*
 * TODO: describe trynakedset_sudokusolver
 */
int trynakedset_sudokusolver(sudoku_solver_t* solver);

/*
 * TODO: describe tryhiddenset_sudokusolver
 */
int tryhiddenset_sudokusolver(sudoku_solver_t* solver);

/*
 * TODO: describe tryboxline_sudokusolver
 */
int tryboxline_sudokusolver(sudoku_solver_t* solver);

/*
 * TODO: describe trypointingline_sudokusolver
 */
int trypointingline_sudokusolver(sudoku_solver_t* solver);

#endif
