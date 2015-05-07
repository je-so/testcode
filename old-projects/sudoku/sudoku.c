#include "sudoku.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

typedef struct sudoku_unit_t sudoku_unit_t;

struct sudoku_unit_t {
   const char* type;
   int row[9];
   int col[9];
};

#define ROW_TYPE "row"
#define COL_TYPE "col"
#define BOX_TYPE "box"

struct sudoku_unit_t s_sudoku_unit_row[9] = {
      { ROW_TYPE, { 0, 0, 0, 0, 0, 0, 0, 0, 0 }, { 0, 1, 2, 3, 4, 5, 6, 7, 8 } },
      { ROW_TYPE, { 1, 1, 1, 1, 1, 1, 1, 1, 1 }, { 0, 1, 2, 3, 4, 5, 6, 7, 8 } },
      { ROW_TYPE, { 2, 2, 2, 2, 2, 2, 2, 2, 2 }, { 0, 1, 2, 3, 4, 5, 6, 7, 8 } },
      { ROW_TYPE, { 3, 3, 3, 3, 3, 3, 3, 3, 3 }, { 0, 1, 2, 3, 4, 5, 6, 7, 8 } },
      { ROW_TYPE, { 4, 4, 4, 4, 4, 4, 4, 4, 4 }, { 0, 1, 2, 3, 4, 5, 6, 7, 8 } },
      { ROW_TYPE, { 5, 5, 5, 5, 5, 5, 5, 5, 5 }, { 0, 1, 2, 3, 4, 5, 6, 7, 8 } },
      { ROW_TYPE, { 6, 6, 6, 6, 6, 6, 6, 6, 6 }, { 0, 1, 2, 3, 4, 5, 6, 7, 8 } },
      { ROW_TYPE, { 7, 7, 7, 7, 7, 7, 7, 7, 7 }, { 0, 1, 2, 3, 4, 5, 6, 7, 8 } },
      { ROW_TYPE, { 8, 8, 8, 8, 8, 8, 8, 8, 8 }, { 0, 1, 2, 3, 4, 5, 6, 7, 8 } }
};

struct sudoku_unit_t s_sudoku_unit_col[9] = {
      { COL_TYPE, { 0, 1, 2, 3, 4, 5, 6, 7, 8 }, { 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
      { COL_TYPE, { 0, 1, 2, 3, 4, 5, 6, 7, 8 }, { 1, 1, 1, 1, 1, 1, 1, 1, 1 } },
      { COL_TYPE, { 0, 1, 2, 3, 4, 5, 6, 7, 8 }, { 2, 2, 2, 2, 2, 2, 2, 2, 2 } },
      { COL_TYPE, { 0, 1, 2, 3, 4, 5, 6, 7, 8 }, { 3, 3, 3, 3, 3, 3, 3, 3, 3 } },
      { COL_TYPE, { 0, 1, 2, 3, 4, 5, 6, 7, 8 }, { 4, 4, 4, 4, 4, 4, 4, 4, 4 } },
      { COL_TYPE, { 0, 1, 2, 3, 4, 5, 6, 7, 8 }, { 5, 5, 5, 5, 5, 5, 5, 5, 5 } },
      { COL_TYPE, { 0, 1, 2, 3, 4, 5, 6, 7, 8 }, { 6, 6, 6, 6, 6, 6, 6, 6, 6 } },
      { COL_TYPE, { 0, 1, 2, 3, 4, 5, 6, 7, 8 }, { 7, 7, 7, 7, 7, 7, 7, 7, 7 } },
      { COL_TYPE, { 0, 1, 2, 3, 4, 5, 6, 7, 8 }, { 8, 8, 8, 8, 8, 8, 8, 8, 8 } }
};

struct sudoku_unit_t s_sudoku_unit_box[9] = {
      { BOX_TYPE, { 0, 0, 0, 1, 1, 1, 2, 2, 2 }, { 0, 1, 2, 0, 1, 2, 0, 1, 2 } },
      { BOX_TYPE, { 0, 0, 0, 1, 1, 1, 2, 2, 2 }, { 3, 4, 5, 3, 4, 5, 3, 4, 5 } },
      { BOX_TYPE, { 0, 0, 0, 1, 1, 1, 2, 2, 2 }, { 6, 7, 8, 6, 7, 8, 6, 7, 8 } },
      { BOX_TYPE, { 3, 3, 3, 4, 4, 4, 5, 5, 5 }, { 0, 1, 2, 0, 1, 2, 0, 1, 2 } },
      { BOX_TYPE, { 3, 3, 3, 4, 4, 4, 5, 5, 5 }, { 3, 4, 5, 3, 4, 5, 3, 4, 5 } },
      { BOX_TYPE, { 3, 3, 3, 4, 4, 4, 5, 5, 5 }, { 6, 7, 8, 6, 7, 8, 6, 7, 8 } },
      { BOX_TYPE, { 6, 6, 6, 7, 7, 7, 8, 8, 8 }, { 0, 1, 2, 0, 1, 2, 0, 1, 2 } },
      { BOX_TYPE, { 6, 6, 6, 7, 7, 7, 8, 8, 8 }, { 3, 4, 5, 3, 4, 5, 3, 4, 5 } },
      { BOX_TYPE, { 6, 6, 6, 7, 7, 7, 8, 8, 8 }, { 6, 7, 8, 6, 7, 8, 6, 7, 8 } }
};

struct sudoku_unit_t* s_sudoku_unit[3] = {
      s_sudoku_unit_row,
      s_sudoku_unit_col,
      s_sudoku_unit_box
};

static inline int getboxnr(int row, int col)
{
   return (row - row%3) + col / 3;
}

void init_sudoku(/*out*/sudoku_t* sudoku)
{
   for (int r = 0; r < 9; ++r) {
      for (int c = 0; c < 9; ++c) {
         sudoku->grid[r][c] = sudoku_INIT_CELL;
      }
   }
}

void print_sudoku(const sudoku_t* sudoku)
{
   char divrow[] = { "  +---+---+---+ +---+---+---+ +---+---+---+\n" };
   char divbox[] = { "  +===+===+===+ +===+===+===+ +===+===+===+\n" };
   printf("    0   1   2     3   4   5     6   7   8\n");
   for (int row = 0; row < 9; ++row) {
      if (! (row%3)) {
         printf("%s", divbox);
      } else {
         printf("%s", divrow);
      }
      char lines[3][40] = {
         "   |   |   | |   |   |   | |   |   |   ",
         "   |   |   | |   |   |   | |   |   |   ",
         "   |   |   | |   |   |   | |   |   |   "
      };
      for (int col = 0; col < 9; ++col) {
         for (int nr = 1; nr <= 9; ++nr) {
            if (isnr_sudoku(sudoku, row, col, nr)) {
               if (issolved_sudoku(sudoku, row, col)) {
                  lines[1][4*col+col/3*2+1] = '0' + nr;
                  break;
               } else {
                  lines[(nr-1)/3][4*col+col/3*2+(nr-1)%3] = '0' + nr;
               }
            }
         }
      }
      printf("  |%s|\n", lines[0]);
      printf("%d |%s|\n", row, lines[1]);
      printf("  |%s|\n", lines[2]);
   }
   printf("%s", divbox);
}

void printstring_sudoku(const sudoku_t* sudoku)
{
   for (int row = 0; row < 9; ++row) {
      for (int col = 0; col < 8; ++col) {
         printf("%d ", getnr_sudoku(sudoku, row, col));
      }
      printf("%d\n", getnr_sudoku(sudoku, row, 8));
   }
}

int read_sudoku(/*out*/uint8_t grid[9][9])
{
   for (int row = 0; row < 9; ++row) {
      for (int col = 0; col < 9; ++col) {
         int ch;

         do {
            ch = getchar();
         } while (ch == '\n' || ch == ' ');

         if (ch != '.' && (ch < '0' || ch > '9')) return EINVAL;

         if (ch == '0' || ch == '.') {
            grid[row][col] = 0;
         } else {
            grid[row][col] = ch - '0';
         }
      }
   }

   return 0;
}

void init_sudokusolver(/*out*/sudoku_solver_t* solver)
{
   init_sudoku(&solver->sudoku);
   solver->single  = (sudoku_cells_t) sudoku_cells_INIT;
   solver->solved_count = 0;
   solver->removed_count = 0;
}

int preset_sudokusolver(sudoku_solver_t* solver, uint8_t nr[9][9])
{
   for (int row = 0; row < 9; ++row) {
      for (int col = 0; col < 9; ++col) {
         if (nr[row][col] != 0) {
            printf("preset (%d,%d) = %d\n", row, col, nr[row][col]);
            if (  issolved_sudoku(&solver->sudoku, row, col)
                  && getnr_sudoku(&solver->sudoku, row, col) == nr[row][col]) {
               // if single strategy would already solve this cell remove it
               for (int i = 0; i < solver->single.size; ++i) {
                  if (solver->single.row[i] == row && solver->single.col[i]) {
                     if (i != solver->single.size-1) {
                        solver->single.row[i] = solver->single.row[solver->single.size-1];
                        solver->single.col[i] = solver->single.col[solver->single.size-1];
                     }
                     -- solver->single.size;
                     break;
                  }
               }
            }
            int err = solvecell_sudokusolver(solver, row, col, nr[row][col]);
            if (err) return err;
         }
      }
   }

   return 0;
}

/*
 * Tries to apply every implemente strategy until a cell could be solved or a number removed
 * from a cell.
 *
 * Returns EINVAL if the sudoku is not solvable.
 *
 * */
static int trysolve_sudokusolver(sudoku_solver_t* solver)
{
   int err;

   solver->solved_count = 0;
   solver->removed_count = 0;

   err = trysingle_sudokusolver(solver);
   if (err) return err;
   if (solver->solved_count || solver->removed_count) return 0;

   err = tryhiddensingle_sudokusolver(solver);
   if (err) return err;
   if (solver->solved_count || solver->removed_count) return 0;

   err = trypointingline_sudokusolver(solver);
   if (err) return err;
   if (solver->solved_count || solver->removed_count) return 0;

   err = trynakedset_sudokusolver(solver);
   if (err) return err;
   if (solver->solved_count || solver->removed_count) return 0;

   err = tryhiddenset_sudokusolver(solver);
   if (err) return err;
   if (solver->solved_count || solver->removed_count) return 0;

   err = tryboxline_sudokusolver(solver);
   if (err) return err;
   if (solver->solved_count || solver->removed_count) return 0;

   return 0;
}

/*
 * Calls trysolve_sudokusolver and falls back to backtracking if
 * trysolve_sudokusolver does not find a solution.
 *
 * Returns EINVAL if the sudoku is not solvable.
 *
 * The parameter depth is the level of nested calls used during backtracking.
 *
 * */
static int solve2_sudokusolver(sudoku_solver_t* solver, int depth)
{
   int err;

   int nrsolved = 0;
   for (int row = 0; row < 9; ++row) {
      for (int col = 0; col < 9; ++col) {
         nrsolved += issolved_sudoku(&solver->sudoku, row, col);
      }
   }
   nrsolved -= solver->single.size;

   while (nrsolved < 81) {
      err = trysolve_sudokusolver(solver);
      if (err) return err;
      if (! solver->solved_count && !solver->removed_count) break;
      nrsolved += solver->solved_count;
   }

   if (nrsolved < 81) {
      // choose number
      for (int size = 2; size <= 9; ++size) {
         for (int row = 0; row < 9; ++row) {
            for (int col = 0; col < 9; ++col) {
               if (size == countnr_sudoku(&solver->sudoku, row, col)) {
                  for (int nr = 1; nr <= 9; ++nr) {
                     if (isnr_sudoku(&solver->sudoku, row, col, nr)) {
                        sudoku_solver_t solver2 = *solver;
                        printf("Backtracking: choose (%d, %d) = %d (depth: %d; nrsolved: %d)\n", row, col, nr, depth, nrsolved);
                        int err = solvecell_sudokusolver(&solver2, row, col, nr);
                        if (!err) err = solve2_sudokusolver(&solver2, depth + 1);
                        if (!err) {
                           *solver = solver2;
                           return 0;
                        }
                     }
                  }
                  return EINVAL;
               }
            }
         }
      }
   }

   return 0;
}

int solve_sudokusolver(sudoku_solver_t* solver)
{
   return solve2_sudokusolver(solver, 0);
}

static int tryremovebits_sudokusolver(sudoku_solver_t* solver, int row/*0..8*/, int col/*0..8*/, int bits)
{
   int cell = solver->sudoku.grid[row][col];

   if ((cell & bits)) {
      cell &= ~ bits;
      if (! cell) return EINVAL; // no more possible numbers ==> no solution
      solver->sudoku.grid[row][col] = cell;
      if (issolved_sudoku(&solver->sudoku, row, col)) {
         // remember to apply strategy "single" for this cell
         append_sudokucells(&solver->single, row, col);
      }

      ++ solver->removed_count;
   }

   return 0;
}

int solvecell_sudokusolver(sudoku_solver_t* solver, int row/*0..8*/, int col/*0..8*/, int nr/*1..9*/)
{
   int err;

   if (row < 0 || row > 8 || col < 0 || col > 8 || nr < 1 || nr > 9) {
      printf("solvecell_sudokusolver: wrong parameter (%d, %d) = %d\n", row, col, nr);
      return EINVAL;
   }

   int cell = solver->sudoku.grid[row][col];
   int bit  = 1 << (nr-1);
   if (! (cell & bit)) {
      printf("solvecell_sudokusolver: cell (%d, %d) does not contain nr %d\n", row, col, nr);
      return EINVAL;
   }

   // solve cell, set value to single number (single bit)
   solver->sudoku.grid[row][col] = bit;

   // remove this number in same row, same column, and same box
   sudoku_unit_t* unit[3] = { &s_sudoku_unit_row[row], &s_sudoku_unit_col[col], &s_sudoku_unit_box[getboxnr(row, col)] };

   for (int ui = 0; ui < 3; ++ui) {
      for (int i = 0; i < 9; ++i) {
         const int row2 = unit[ui]->row[i];
         const int col2 = unit[ui]->col[i];
         if (row2 == row && col2 == col) continue;

         err = tryremovebits_sudokusolver(solver, row2, col2, bit);
         if (err) return err;
      }
   }

   ++ solver->solved_count;

   return 0;
}

int trysingle_sudokusolver(sudoku_solver_t* solver)
{
   while (solver->single.size > 0) {
      int row, col;
      remove_sudokucells(&solver->single, &row, &col);
      const int nr = getnr_sudoku(&solver->sudoku, row, col);
      printf("single: (%d,%d) = %d\n", row, col, nr);
      int err = solvecell_sudokusolver(solver, row, col, nr);
      if (err) return err;
   }

   return 0;
}

int tryhiddensingle_sudokusolver(sudoku_solver_t* solver)
{
   for (int ut = 0; ut < 3; ++ut) {
      for (int ui = 0; ui < 9; ++ui) {
         sudoku_unit_t* unit = &s_sudoku_unit[ut][ui];

         // count number occurence
         unsigned nrcount[10] = { 0 };

         for (int i = 0; i < 9; ++i) {
            const int row = unit->row[i];
            const int col = unit->col[i];
            int cell = solver->sudoku.grid[row][col];

            for (int nr = 1; cell; ++nr, cell >>= 1) {
               if ((cell & 1)) {
                  ++ nrcount[nr];
               }
            }
         }

         // if a nr occurs only in a single cell of a unit
         // use this number as solution if this cell was not solved already
         for (int nr = 1; nr <= 9; ++nr) {
            if (nrcount[nr] == 0) return EINVAL;
            if (nrcount[nr] == 1) {
               for (int i = 0; i < 9; ++i) {
                  const int row = unit->row[i];
                  const int col = unit->col[i];

                  if (  !issolved_sudoku(&solver->sudoku, row, col)
                        && isnr_sudoku(&solver->sudoku, row, col, nr)) {
                     printf("hiddensingle: (%d,%d) = %d\n", row, col, nr);
                     int err = solvecell_sudokusolver(solver, row, col, nr);
                     if (err) return err;
                     break;
                  }
               }
            }
         }

      }
   }

   return 0;
}

int trynakedset_sudokusolver(sudoku_solver_t* solver)
{
   int err;

   for (int ut = 0; ut < 3; ++ut) {
      for (int ui = 0; ui < 9; ++ui) {
         sudoku_unit_t* unit = &s_sudoku_unit[ut][ui];

         // count occurence of sets of numbers (pair, triple, quad)
         int nrofsets = 0;
         struct {
            int bits;
            int nrcount;
            int nrofcells;
         } nrsets[9] = { { 0, 0, 0 } };

         for (int i = 0; i < 9; ++i) {
            const int row = unit->row[i];
            const int col = unit->col[i];
            int bits = solver->sudoku.grid[row][col];
            int nrcount = countnr_sudoku(&solver->sudoku, row, col);
            if (2 <= nrcount && nrcount <= 4) {
               int f = 0;
               for (; f < nrofsets; ++f) {
                  // this check allows to use a pair as 3rd/4th member of a triple or quad for example
                  if (bits == (nrsets[f].bits & bits)) {
                     ++ nrsets[f].nrofcells;
                     break;
                  }
               }
               if (f >= nrofsets) {
                  nrsets[nrofsets].bits = bits;
                  nrsets[nrofsets].nrcount = nrcount;
                  nrsets[nrofsets].nrofcells = 1;
                  ++ nrofsets;
               }
            }
         }

         // search for setsize cells which contain exactly setsize numbers
         // ==> clear setsize numbers in all other cells
         for (int setsize = 2; setsize <= 4; ++setsize) {
            for (int f = 0; f < nrofsets; ++f) {
               if (  nrsets[f].nrcount == setsize
                     && nrsets[f].nrofcells == setsize) {
                  int oldcount = solver->removed_count;
                  for (int i = 0; i < 9; ++i) {
                     const int row = unit->row[i];
                     const int col = unit->col[i];
                     if (solver->sudoku.grid[row][col] != (solver->sudoku.grid[row][col] & nrsets[f].bits)) {
                        err = tryremovebits_sudokusolver(solver, row, col, nrsets[f].bits);
                        if (err) return err;
                     }
                  }

                  if (oldcount != solver->removed_count) {
                     printf("naked set: found set ");
                     for (int nr = 1, bits = nrsets[f].bits; bits; ++nr, bits >>= 1) {
                        if ((bits & 1)) {
                           printf("%d", nr);
                           if (bits > 1) printf(",");
                        }
                     }
                     printf(" in %s (%d) \n", unit->type, ui);
                     return 0;
                  }

               }
            }
         }

      }
   }

   return 0;
}

int tryhiddenset_sudokusolver(sudoku_solver_t* solver)
{
   int err;

   for (int ut = 0; ut < 3; ++ut) {
      for (int ui = 0; ui < 9; ++ui) {
         sudoku_unit_t* unit = &s_sudoku_unit[ut][ui];

         // count occurence of every number in every cell
         // the first 4 cell indices are remembered
         struct {
            int nrofcells;
            int cellindex[4/*0..nrofcells*/];
         } state[10/*nr from 1..9*/] = { { 0, { 0, 0, 0, 0 } } };

         for (int i = 0; i < 9; ++i) {
            const int row = unit->row[i];
            const int col = unit->col[i];
            int bits = solver->sudoku.grid[row][col];
            for (int nr = 1; bits; ++nr, bits >>= 1) {
               if ((bits & 1)) {
                  if (state[nr].nrofcells < 4) {
                     state[nr].cellindex[state[nr].nrofcells] = i;
                  }
                  ++ state[nr].nrofcells;
               }
            }
         }

         // search for setsize numbers which are stored in only setsize different cells
         // ==> clear numbers in these setsize cells which are not equal to those setsize numbers
         // ==> the hidden set is transformed to a naked set
         for (int setsize = 2; setsize <= 4; ++setsize) {
            struct {
               int nr;
               int nrofcells;
               int cellindex[4];
            } nrfound[4];
            int nsize = 0;
            int nr = 1;
            while (nsize < setsize) {
               if (nr > 9) {
                  if (nsize == 0) {
                     break;
                  }
                  -- nsize;
                  nr = nrfound[nsize].nr + 1;
               }
               NEXT: ;
               for (; nr <= 9; ++nr) {
                  if (state[nr].nrofcells <= setsize) {
                     // add state[nr].cellindex to nrfound[nsize].cellindex
                     nrfound[nsize].nr = nr;
                     nrfound[nsize].nrofcells = state[nr].nrofcells;
                     for (int c = 0; c < state[nr].nrofcells; ++c) {
                        nrfound[nsize].cellindex[c] = state[nr].cellindex[c];
                     }
                     // add nrfound[nsize-1].cellindex to nrfound[nsize].cellindex
                     if (nsize > 0) {
                        for (int c = 0; c < nrfound[nsize-1].nrofcells; ++c) {
                           int s;
                           for (s = 0; s < nrfound[nsize].nrofcells; ++s) {
                              if (nrfound[nsize].cellindex[s] == nrfound[nsize-1].cellindex[c]) {
                                 break;
                              }
                           }
                           if (s == nrfound[nsize].nrofcells) {
                              if (nrfound[nsize].nrofcells == setsize) {
                                 ++ nr;
                                 goto NEXT;
                              }
                              ++ nrfound[nsize].nrofcells;
                              nrfound[nsize].cellindex[s] = nrfound[nsize-1].cellindex[c];
                           }
                        }
                     }

                     ++ nsize;

                     if (nsize == setsize) {
                        -- nsize;
                        if (nrfound[nsize].nrofcells < setsize) {
                           return EINVAL; // not solveable
                        }
                        int oldcount = solver->removed_count;
                        // found setsize cells with setsize numbers
                        // remove all other numbers than the found from setsize cells
                        int bits = 0;
                        for (int i = 0; i < setsize; ++i) {
                           bits |= (1 << (nrfound[i].nr-1));
                        }
                        for (int i = 0; i < nrfound[nsize].nrofcells; ++i) {
                           const int row = unit->row[nrfound[nsize].cellindex[i]];
                           const int col = unit->col[nrfound[nsize].cellindex[i]];
                           err = tryremovebits_sudokusolver(solver, row, col, (sudoku_INIT_CELL & ~bits));
                           if (err) return err;
                        }

                        if (oldcount != solver->removed_count) {
                           printf("hiddenset: found hidden set ");
                           for (int nr = 1; bits; ++nr, bits >>= 1) {
                              if ((bits & 1)) {
                                 printf("%d", nr);
                                 if (bits > 1) printf(",");
                              }
                           }
                           printf(" in %s (%d) \n", unit->type, ui);
                           return 0;
                        }
                        // else test next nr
                        // (nsize < setsize) is ensured by "--nrsize;" as first statement after "if (nsize == setsize) {"
                     }
                  }
               }
            }
         }

      }
   }

   return 0;
}

int tryboxline_sudokusolver(sudoku_solver_t* solver)
{
   int err;

   for (int ut = 0; ut < 3; ++ut) {
      for (int ui = 0; ui < 9; ++ui) {
         sudoku_unit_t* unit = &s_sudoku_unit[ut][ui];
         if (0 == strcmp(BOX_TYPE, unit->type)) continue;

         // check if certain numbers of a row or column occur only in a box
         struct {
             int count;
             int boxnr;
             int row;
             int col;
         } state[10] = { { 0, 0, 0, 0 } };

         for (int i = 0; i < 9; ++i) {
            const int row = unit->row[i];
            const int col = unit->col[i];
            int bits = solver->sudoku.grid[row][col];
            if (! issolved_sudoku(&solver->sudoku, row, col)) {
               const int boxnr = getboxnr(row, col);
               for (int nr = 1; bits; ++nr, bits >>= 1) {
                  if ((bits & 1)) {
                     if (state[nr].count == 0) {
                        state[nr].boxnr = boxnr;
                        state[nr].row = (0 == strcmp(ROW_TYPE, unit->type) ? row : -1);
                        state[nr].col = (0 == strcmp(COL_TYPE, unit->type) ? col : -1);

                     } else if (boxnr != state[nr].boxnr) {
                        state[nr].boxnr = -1;
                     }
                     ++ state[nr].count;
                  }
               }
            }
         }

         // Remove numbers of a row or column which occur only in a single box
         // from all other cells of this box
         for (int nr = 1; nr <= 9; ++nr) {
            if (state[nr].count > 0 && state[nr].boxnr >= 0) {
               int oldcount = solver->removed_count;

               for (int i = 0; i < 9; ++i) {
                  const int row = s_sudoku_unit_box[state[nr].boxnr].row[i];
                  const int col = s_sudoku_unit_box[state[nr].boxnr].col[i];
                  if (row == state[nr].row || col == state[nr].col) continue;
                  err = tryremovebits_sudokusolver(solver, row, col, 1 << (nr-1));
                  if (err) return err;
               }

               if (oldcount != solver->removed_count) {
                  printf("boxline: removed number %d from box (%d) except from %s (%d)\n", nr, state[nr].boxnr, unit->type, state[nr].row >= 0 ? state[nr].row : state[nr].col);
                  return 0;
               }

            }
         }
      }
   }

   return 0;
}

int trypointingline_sudokusolver(sudoku_solver_t* solver)
{
   int err;

   for (int boxnr = 0; boxnr < 9; ++boxnr) {
      sudoku_unit_t* unit = &s_sudoku_unit_box[boxnr];

      // check if certain numbers of a boy occur only in a row or column
      struct {
          int count;
          int row;
          int col;
      } state[10] = { { 0, 0, 0 } };

      for (int i = 0; i < 9; ++i) {
         const int row = unit->row[i];
         const int col = unit->col[i];
         int bits = solver->sudoku.grid[row][col];
         if (! issolved_sudoku(&solver->sudoku, row, col)) {
            for (int nr = 1; bits; ++nr, bits >>= 1) {
               if ((bits & 1)) {
                  if (state[nr].count == 0) {
                     state[nr].row = row;
                     state[nr].col = col;

                  } else {
                     if (state[nr].row != row) state[nr].row = -1;
                     if (state[nr].col != col) state[nr].col = -1;
                  }
                  ++ state[nr].count;
               }
            }
         }
      }

      // Remove numbers of a box occuring in only a row or a column
      // from the row or column outside of the box
      for (int nr = 1; nr <= 9; ++nr) {
         if (state[nr].count > 0 && (state[nr].row != -1 || state[nr].col != -1)) {
            int oldcount = solver->removed_count;

            if (state[nr].row != -1) {
               unit = &s_sudoku_unit_row[state[nr].row];
            } else {
               unit = &s_sudoku_unit_col[state[nr].col];
            }

            for (int i = 0; i < 9; ++i) {
               const int row = unit->row[i];
               const int col = unit->col[i];
               if (boxnr == getboxnr(row, col)) continue;
               err = tryremovebits_sudokusolver(solver, row, col, 1 << (nr-1));
               if (err) return err;
            }

            if (oldcount != solver->removed_count) {
               printf("pointingline: removed number %d from %s (%d) except from box (%d)\n", nr, unit->type, state[nr].row >= 0 ? state[nr].row : state[nr].col, boxnr);
               return 0;
            }

         }
      }

   }

   return 0;
}

int main(int argc, const char* argv[])
{
   int err;
   uint8_t grid[9][9];

   err = read_sudoku(grid);
   if (err) {
      printf("Can not read sudoku from standard input\n");
      return err;
   }

   sudoku_solver_t solver;
   init_sudokusolver(&solver);

   err = preset_sudokusolver(&solver, grid);
   if (err) return err;
   err = solve_sudokusolver(&solver);
   if (err) return err;

   printf("\nSolution:\n\n");
   printstring_sudoku(&solver.sudoku);

   return 0;
}
