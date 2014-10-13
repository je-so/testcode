#define _GNU_SOURCE
#include <assert.h>
#include <shadow.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

// Compile with:
// gcc -ocheckpass checkpass.c -lcrypt; 
// Run with:
// sudo chgrp shadow ./checkpass; sudo chmod g+s ./checkpass; ./checkpass

int main()
{
   struct termios tattr;
   struct termios tattr2;
   char user[20] = {0};
   char pass[20] = {0};

   write(STDOUT_FILENO, "\nUser: ", 7);
   scanf("%19s", user);
   struct spwd* spwd = getspnam(user);
   if (!spwd || strlen(spwd->sp_pwdp) < 13) {
      printf("can not read /etc/shadow for user %s\n", user);
      exit(1);
   }
   assert(0 == tcgetattr(STDIN_FILENO, &tattr));
   tattr2 = tattr;
   tattr.c_lflag &= ~ECHO;
   assert(0 == tcsetattr(STDIN_FILENO, TCSANOW, &tattr));
   write(STDOUT_FILENO, "Password: ", 10);
   scanf("%19s", pass);
   assert(0 == tcsetattr(STDIN_FILENO, TCSANOW, &tattr2));
   
   if (strcmp(spwd->sp_pwdp, crypt(pass, spwd->sp_pwdp))) {
      printf("Invalid password for user %s\n", user);
   } else {
      printf("\nValid password for user %s\n", user);
   }
   memset(pass, 0, sizeof(pass));

   return 0;
}
