/* Simple Terminal program to test input/output.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn
*/
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>


int main(int argc, const char *argv[])
{
   int fd;
   char key[255];
   char data[255];
   size_t bi; 
   struct termios old_stdin_tconf;
   struct termios saved_options;
   struct termios options;
   struct pollfd fds;
   struct {
      speed_t     speed;
      const char* name;
   } baudrate[] = {
      { B9600, "9600" }, { B19200, "19200" }, { B38400, "38400" },
      { B57600, "57600" }, { B115200, "115200" }, { B230400, "230400" }
   };

   if (argc != 4 || strcmp(argv[3], "8N1")) {
USAGE:
      printf("Usage: %s /dev/ttyXXX <baudrate> 8N1\n", argv[0]);
      exit(1);
   }

   fd = open(argv[1], O_RDWR|O_NOCTTY|O_NONBLOCK|O_CLOEXEC);
   if (fd == -1) {
      perror("open"); 
      exit(1);
   }

   if (tcgetattr(0, &old_stdin_tconf)) {
      perror("tcgetattr"); 
      exit(1);
   }
   
   if (tcgetattr(fd, &saved_options)) {
      perror("tcgetattr"); 
      exit(1);
   }

   for (bi = 0; bi < sizeof(baudrate)/sizeof(baudrate[0]); ++bi) {
      if (strcmp(baudrate[bi].name, argv[2]) == 0) break;
   }

   if (bi == sizeof(baudrate)/sizeof(baudrate[0])) {
      errno = EINVAL;
      perror("Unsupported baudrate");
      printf("Supported baudrates:");
      for (bi = 0; bi < sizeof(baudrate)/sizeof(baudrate[0]); ++bi) {
         printf(" %s", baudrate[bi].name);
      }
      printf("\n");
      exit(1);
   }

   options = saved_options;
   options.c_cflag |= (CLOCAL|CREAD);
   options.c_lflag &= ~(ICANON | ECHO | ECHOE);
   options.c_iflag &= ~(IXON | IXOFF | IXANY); // no software flow control
   options.c_iflag &= ~(ICRNL);
   options.c_oflag &= ~(OPOST|OCRNL);
   cfsetispeed(&options, baudrate[bi].speed);
   cfsetospeed(&options, baudrate[bi].speed);
 
   if (0 == strcmp(argv[3], "8N1")) {
      options.c_cflag |= (CLOCAL|CREAD);
      options.c_cflag &= ~PARENB;
      options.c_cflag &= ~CSTOPB;
      options.c_cflag &= ~CSIZE;
      options.c_cflag |= CS8;
   } else {
      errno = EINVAL;
      perror("Unsupported format");
      goto USAGE;
   }

   if (tcsetattr(fd, TCSANOW, &options)) {
      perror("tcsetattr");
      exit(1);
   }

   // switch stdin/stdout (terminal/keyboard) to single character mode
   {
      struct termios tconf = old_stdin_tconf;
      int fl = fcntl(0, F_GETFL);
      fcntl(0, F_SETFL, (fl != -1 ? fl:0)|O_NONBLOCK);
      tconf.c_iflag &= (unsigned) ~(IXON|ICRNL|INLCR);
      tconf.c_oflag &= (unsigned) ~(OCRNL);
      tconf.c_oflag |= (unsigned)  (ONLCR); // map \n to \r\n on output
      tconf.c_lflag &= (unsigned) ~(ICANON/*char mode*/ | ECHO/*echo off*/ | ISIG);
      tconf.c_cc[VMIN]  = 1;
      tconf.c_cc[VTIME] = 0;
      tcsetattr(0, TCSANOW, &tconf);
   }

   printf("Press <CTRL>-D to end program\n");
   fds.fd = fd;
   fds.events = POLLIN;
   for (;;) {
      if (1 == poll(&fds, 1, 100)) {
         int bytes = read(fd, data, sizeof(data));
         if (bytes == -1) {
            if (errno == EAGAIN) continue;
            perror("read");
            goto DONE;
         }
         write(1, data, bytes);
      }
      int nrkeys = read(0, key, sizeof(key));
      if (nrkeys > 0) {
         if (key[nrkeys-1] == old_stdin_tconf.c_cc[VEOF]) {
            write(fd, key, nrkeys-1);
            break;
         } else {
            write(fd, key, nrkeys);
         }
      }
   }

DONE:
   tcsetattr(0, TCSANOW, &old_stdin_tconf);
   tcsetattr(fd, TCSANOW, &saved_options);
   close(fd);

   return 0;
}
