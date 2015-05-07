# sudoku
Sudoku Solver in C. The compiler needs to support C99 or later.

## Compilation and Execution with Sample Sudoku

The following command must be executed on the command line (tested on Linux):

```Shell
gcc -std=c99 -osudoku sudoku.c; ./sudoku <<EOF
. . . 2 . . . . .
. 9 . . . . . 4 .
7 3 . 4 . . . . 8
. . 9 . . . . . 6
. . 8 . . . . 3 .
1 . 5 6 . . . . 9
. . 2 . 3 . . 6 .
. 6 . . 7 . . . .
. . . 9 4 . 5 . .
EOF
```
The result is:
```Shell
5 8 4 2 6 9 3 7 1
2 9 1 3 8 7 6 4 5
7 3 6 4 5 1 2 9 8
3 2 9 8 1 4 7 5 6
6 4 8 7 9 5 1 3 2
1 7 5 6 2 3 4 8 9
4 5 2 1 3 8 9 6 7
9 6 3 5 7 2 8 1 4
8 1 7 9 4 6 5 2 3
```

## Reference
The strategies to solve sudokus are from the book
[Sudoku Programming with C](http://zambon.com.au/) by *Giulio Zambon*.

Only strategies from level 0 and 1 were implemented.
The fallback to backtracking ensures that every solveable sudoku could be solved.

## Datastructures

A sudoku is represented as an array of 9 rows with 9 columns.

```C
struct sudoku_t {
   uint16_t grid[9][9];
};
```

The numbers 1..9 are encoded as bit values so that a single cell (type ``uint16_t``) could store more
than one number. Bit value 1 represents number 1, and bit value 256 number 9.
The conversion of a number into its bit value is done with ``(1 << (nr-1))``.

To check if a single cell contains only a single number the following code is used:

```C
static inline bool issolved_sudoku(const sudoku_t* sudoku, int row/*0..8*/, int col/*0..8*/)
{
         int cell = sudoku->grid[row][col];
         return 0 == (cell & (cell-1));
}
```
The expression ``(cell-1)`` clears the least significant bit of cell and sets all trailing zero bits to 1. 
With expression ``cell & (cell-1)`` the switched trailing zero bits are switched back to 0. 
The result is 0 if the least significant bit in cell is the only set bit.

## Backtracking
The function ``solve2_sudokusolver`` calls itself in
case no solution could be calculated with the implemented strategies.

```C
int solve2_sudokusolver(sudoku_solver_t* solver, int depth)
{
   ...
         sudoku_solver_t solver2 = *solver;
         printf("Backtracking: choose (%d, %d) = %d (depth: %d; nrsolved: %d)\n", row, col, nr, depth, nrsolved);
         int err = solvecell_sudokusolver(&solver2, row, col, nr);
         if (!err) err = solve2_sudokusolver(&solver2, depth + 1);
         if (!err) {
            *solver = solver2;
            return 0;
         }
   ...
}
```




