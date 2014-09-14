/* title: Terminal impl

   Implements <Terminal>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/io/terminal/terminal.h
    Header file <Terminal>.

   file: C-kern/platform/Linux/io/terminal.c
    Implementation file <Terminal impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/terminal/terminal.h"
#include "C-kern/api/err.h"
#include "C-kern/api/test/errortimer.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/test/resourceusage.h"
#include "C-kern/api/io/accessmode.h"
#include "C-kern/api/io/filesystem/file.h"
#include "C-kern/api/time/systimer.h"
#include "C-kern/api/time/timevalue.h"
#endif



// section: terminal_t

// group: environment-variables

// TEXTDB:SELECT('/* variable: ENVIRON_'id\n' * Name of environment variable – 'description'. */'\n'#define ENVIRON_'id' "'envname'"')FROM("C-kern/resource/config/environvars")WHERE(id=='TERM')
/* variable: ENVIRON_TERM
 * Name of environment variable – used to determine the terminal type. */
#define ENVIRON_TERM "TERM"
// TEXTDB:END

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_terminal_errtimer
 * Simulates an error in different functions. */
static test_errortimer_t   s_terminal_errtimer = test_errortimer_FREE;
#endif

// group: helper

/* function: readconfig
 * Lese Konfiguration von Terminal verbunden mit Filedescriptor fd nach tconf.
 *
 * Return:
 * 0      - OK, tconf ist gültig.
 * ENOTTY - fd ist nicht mit einem (Pseudo-)Terminal verbunden.
 *
 * Hintergrundwissen Kommandozeile:
 *
 * Die Konfiguratin des Terminals kann mittels des Kommandos "stty -a"
 * (siehe auch: man 1 stty) auf der Kommandozeile angezeigt werden.
 *
 * Mittels "stty intr ^C" setzt man etwa die Controltaste "Control-C" zum Erzeugen
 * des Interrupts SIGINT, der an den im Vordergrund laufenden Prozess gesandt wird
 * ("Control-C" ist die Defaultbelegung).
 *
 * Der Parameter "^C" kann entweder als '^' und dann 'C' eingegeben werden oder
 * als numerischer Wert (dezimal 3, oktal 03 oder hexadezimal 0x3). Die dritte
 * Möglichkeit ist mittels Control-V und Control-C. Der Controlcode ^V sorgt dafür,
 * daß die nachfolgende Taste als Wert genommen wird und nicht in ihrer besonderen
 * Control-Funktion interpretiert wird. Falls Control-C nicht speziell interpretiert
 * wird, kann auch nur Control-C (ohne ^V davor) eingegeben werden.
 *
 * */
static inline int readconfig(/*out*/struct termios * tconf, sys_iochannel_t fd)
{
   int err;

   err = tcgetattr(fd, tconf);
   if (err) {
      err = errno;
      goto ONERR;
   }

   return 0;
ONERR:
   return err;
}

static inline int writeconfig(struct termios * tconf, sys_iochannel_t fd)
{
   int err;

   do {
      err = tcsetattr(fd, TCSAFLUSH, tconf);
      if (err) {
         err = errno;
         if (err != EINTR) goto ONERR;
      }
   } while (err);

   return 0;
ONERR:
   return err;
}

static inline int readwinsize(/*out*/struct winsize * size, sys_iochannel_t fd)
{
   int err;

   if (ioctl(fd, TIOCGWINSZ, size)) {
      err = errno;
      goto ONERR;
   }

   return 0;
ONERR:
   return err;
}

/* function: configstore
 * Calls <readconfig> and stores values into <terminal_t.ctrl_lnext> ... <terminal_t.oldconf_onlcr>. */
static inline int configstore(/*out*/terminal_t * terml, sys_iochannel_t fd)
{
   int err;
   struct termios tconf;

   err = readconfig(&tconf, fd);
   SETONERROR_testerrortimer(&s_terminal_errtimer, &err);
   if (err) goto ONERR;

   terml->ctrl_lnext     = tconf.c_cc[VLNEXT];
   terml->ctrl_susp      = tconf.c_cc[VSUSP];
   terml->oldconf_vmin   = tconf.c_cc[VMIN];
   terml->oldconf_vtime  = tconf.c_cc[VTIME];
   terml->oldconf_echo   = (0 != (tconf.c_lflag & ECHO));
   terml->oldconf_icanon = (0 != (tconf.c_lflag & ICANON));
   terml->oldconf_icrnl  = (0 != (tconf.c_iflag & ICRNL));
   terml->oldconf_isig   = (0 != (tconf.c_lflag & ISIG));
   terml->oldconf_ixon   = (0 != (tconf.c_iflag & IXON));
   terml->oldconf_onlcr  = (0 != (tconf.c_oflag & ONLCR));

   return 0;
ONERR:
   return err;
}

// group: lifetime

int init_terminal(/*out*/terminal_t * terml)
{
   int err;
   file_t input = sys_iochannel_STDIN;
   file_t output = sys_iochannel_STDOUT;
   bool doclose = false;

   if (  ! iscontrolling_terminal(input)
         || ! iscontrolling_terminal(output)) {
      ONERROR_testerrortimer(&s_terminal_errtimer, &err, ONERR);
      err = init_file(&input, "/dev/tty", accessmode_RDWR, 0);
      if (err) goto ONERR;
      output = input;
      doclose = true;
   }

   // inits all terml->oldconf_<name> values
   err = configstore(terml, input);
   if (err) goto ONERR;

   terml->input  = input;
   terml->output = output;
   terml->doclose = doclose;

   return 0;
ONERR:
   if (doclose) {
      free_file(&input);
   }
   TRACEEXIT_ERRLOG(err);
   return err;
}

int free_terminal(terminal_t * terml)
{
   int err;

   if (terml->doclose) {
      terml->doclose = false;

      bool issame = (terml->input == terml->output);
      err = free_file(&terml->input);
      SETONERROR_testerrortimer(&s_terminal_errtimer, &err);
      if (issame) terml->output = sys_iochannel_FREE;

      int err2 = free_file(&terml->output);
      SETONERROR_testerrortimer(&s_terminal_errtimer, &err2);
      if (err2) err = err2;

      if (err) goto ONERR;

   } else {
      terml->input  = sys_iochannel_FREE;
      terml->output = sys_iochannel_FREE;
   }

   return 0;
ONERR:
   return err;
}

// group: query

bool hascontrolling_terminal(void)
{
   if (! iscontrolling_terminal(sys_iochannel_STDIN)) {
      int fd = open("/dev/tty", O_RDWR|O_CLOEXEC);
      if (fd == -1) return false;

      close(fd);
   }

   return true;
}

bool is_terminal(sys_iochannel_t fd)
{
   return isatty(fd);
}

bool iscontrolling_terminal(sys_iochannel_t fd)
{
   return getsid(0) == tcgetsid(fd);
}

bool issizechange_terminal()
{
   sigset_t signset;
   siginfo_t info;
   sigemptyset(&signset);
   sigaddset(&signset, SIGWINCH);

   return (SIGWINCH == sigtimedwait(&signset, &info, & (const struct timespec) { 0, 0 }));
}

bool isutf8_terminal(terminal_t * terml)
{
   int err;
   struct termios tconf;

   err = readconfig(&tconf, terml->input);
   SETONERROR_testerrortimer(&s_terminal_errtimer, &err);
   if (err) goto ONERR;

   return (tconf.c_iflag & IUTF8);
ONERR:
   TRACEEXIT_ERRLOG(err);
   return false;
}

int pathname_terminal(const terminal_t * terml, uint16_t len, uint8_t name[len])
{
   int err;

   err = ttyname_r(terml->input, (char *)name, len);
   if (err) {
      err = errno;
      goto ONERR;
   }

   return 0;
ONERR:
   if (err == ERANGE) {
      err = ENOBUFS;
   } else {
      TRACEEXIT_ERRLOG(err);
   }
   return err;
}

int waitsizechange_terminal()
{
   sigset_t signset;
   siginfo_t info;
   sigemptyset(&signset);
   sigaddset(&signset, SIGWINCH);
   sigaddset(&signset, SIGINT);

   return (SIGWINCH == sigwaitinfo(&signset, &info)) ? 0 : EINTR;
}

int type_terminal(uint16_t len, /*out*/uint8_t type[len])
{
   const char * envterm = getenv(ENVIRON_TERM);

   if (!envterm) return ENODATA;

   size_t envlen = strlen(envterm);
   if (envlen >= len) return ENOBUFS;

   memcpy(type, envterm, envlen+1u);

   return 0;
}

// group: read

size_t tryread_terminal(terminal_t * terml, size_t len, /*out*/uint8_t keys[len])
{
   ssize_t nrbytes = read(terml->input, keys, len);
   return (nrbytes < 0) ? 0 : (size_t) nrbytes;
}

int readsize_terminal(terminal_t * terml, uint16_t * rowsize, uint16_t * colsize)
{
   int err;
   struct winsize size;

   err = readwinsize(&size, terml->input);
   if (err) goto ONERR;

   *colsize = size.ws_col;
   *rowsize = size.ws_row;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: update

int removecontrolling_terminal(void)
{
   int err;

   if (iscontrolling_terminal(sys_iochannel_STDIN)) {
      err = ioctl(sys_iochannel_STDIN, TIOCNOTTY);

   } else {
      int fd = open("/dev/tty", O_RDWR|O_CLOEXEC);
      if (fd == -1) {
         err = errno;
         goto ONERR;
      }
      err = ioctl(fd, TIOCNOTTY);
      close(fd);
   }

   if (err) {
      err = errno;
      goto ONERR;
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: config line discipline

int configstore_terminal(terminal_t * terml)
{
   int err;

   err = configstore(terml, terml->input);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int configrestore_terminal(terminal_t * terml)
{
   int err;
   struct termios tconf;

   err = readconfig(&tconf, terml->input);
   if (err) goto ONERR;

   tconf.c_cc[VMIN]  = terml->oldconf_vmin;
   tconf.c_cc[VTIME] = terml->oldconf_vtime;
   if (terml->oldconf_icrnl) {
      tconf.c_iflag |= ICRNL;
   }
   if (terml->oldconf_ixon) {
      tconf.c_iflag |= IXON;
   }
   if (terml->oldconf_onlcr) {
      tconf.c_oflag |= ONLCR;
   }
   if (terml->oldconf_icanon) {
      tconf.c_lflag |= ICANON;
   }
   if (terml->oldconf_echo) {
      tconf.c_lflag |= ECHO;
   }
   if (terml->oldconf_isig) {
      tconf.c_lflag |= ISIG;
   }

   err = writeconfig(&tconf, terml->input);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int configrawedit_terminal(terminal_t * terml)
{
   int err;
   struct termios tconf;

   err = readconfig(&tconf, terml->input);
   if (err) goto ONERR;

   // set raw mode, receive characters immediately, and receive them unaltered
   // turn off signal generating with CTRL-C, CTRL-\, CTRL-Z

   tconf.c_iflag &= (unsigned) ~(ICRNL|IXON);
   tconf.c_oflag &= (unsigned) ~ONLCR;
   tconf.c_lflag &= (unsigned) ~(ICANON/*char mode*/ | ECHO/*echo off*/ | ISIG/*no signals*/);
   tconf.c_cc[VMIN]  = 0;
   tconf.c_cc[VTIME] = 1;

   err = writeconfig(&tconf, terml->input);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}




// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_helper(void)
{
   struct termios tconf;
   struct termios tconf2;
   struct termios oldconf;
   struct winsize size;
   struct winsize size2 = { 0 };
   terminal_t     terml;
   terminal_t     terml2;
   file_t         file = file_FREE;
   bool           isoldconf = false;

   // prepare
   TEST(0 == readconfig(&oldconf, sys_iochannel_STDIN));
   isoldconf = true;
   memset(&tconf, 0, sizeof(tconf));
   memset(&tconf2, 0, sizeof(tconf2));
   memset(&terml, 0, sizeof(terml));
   memset(&terml2, 0, sizeof(terml2));
   TEST(0 == memcmp(&tconf, &tconf2, sizeof(tconf)));
   TEST(0 == initcreate_file(&file, "./xxx", 0));

   // TEST readconfig
   TEST(0 == readconfig(&tconf, sys_iochannel_STDIN));
   TEST(0 != memcmp(&tconf, &tconf2, sizeof(tconf)));
   memset(&tconf, 0, sizeof(tconf));

   // TEST readconfig: ENOTTY
   TEST(ENOTTY == readconfig(&tconf, io_file(file)));
   TEST(0 == memcmp(&tconf, &tconf2, sizeof(tconf)));

   // TEST readconfig: EBADF
   TEST(EBADF  == readconfig(&tconf, sys_iochannel_FREE));
   TEST(0 == memcmp(&tconf, &tconf2, sizeof(tconf)));

   // TEST writeconfig
   TEST(0 == readconfig(&tconf, sys_iochannel_STDIN));
   TEST(0 == writeconfig(&tconf, sys_iochannel_STDIN));

   // TEST readwinsize
   TEST(0 == readwinsize(&size, sys_iochannel_STDIN));
   TEST(0 <  size.ws_col);
   TEST(0 <  size.ws_row);

   // TEST readwinsize: ENOTTY
   memset(&size, 0, sizeof(size));
   TEST(ENOTTY == readwinsize(&size, io_file(file)));
   TEST(0 == memcmp(&size, &size2, sizeof(size)));

   // TEST readwinsize: EBADF
   TEST(EBADF == readwinsize(&size, sys_iochannel_FREE));
   TEST(0 == memcmp(&size, &size2, sizeof(size)));

   // TEST configstore
   TEST(0 == readconfig(&tconf, sys_iochannel_STDIN));
   for (int i = 0; i < 10; ++i) {
      for (int state = 0; state < 2; ++state) {
         memcpy(&tconf2, &tconf, sizeof(tconf2));

         // change config
         switch (i) {
         case 0: tconf2.c_cc[VMIN]  = state ? 10 : 0; break;
         case 1: tconf2.c_cc[VTIME] = state ? 10 : 0; break;
         case 2: tconf2.c_lflag    &= (unsigned) ~ECHO; if (state) tconf2.c_lflag |= ECHO; break;
         case 3: tconf2.c_lflag    &= (unsigned) ~ICANON; if (state) tconf2.c_lflag |= ICANON; break;
         case 4: tconf2.c_iflag    &= (unsigned) ~ICRNL; if (state) tconf2.c_iflag |= ICRNL; break;
         case 5: tconf2.c_lflag    &= (unsigned) ~ISIG; if (state) tconf2.c_lflag |= ISIG; break;
         case 6: tconf2.c_iflag    &= (unsigned) ~IXON; if (state) tconf2.c_iflag |= IXON; break;
         case 7: tconf2.c_oflag    &= (unsigned) ~ONLCR; if (state) tconf2.c_oflag |= ONLCR; break;
         case 8: tconf2.c_cc[VLNEXT] = state ? 10 : 0; break;
         case 9: tconf2.c_cc[VSUSP]  = state ? 10 : 0; break;
         default: TEST(0); break;
         }
         TEST(0 == writeconfig(&tconf2, sys_iochannel_STDIN));

         // test
         TEST(0 == configstore(&terml, sys_iochannel_STDIN));
         // compare result
         TEST(0 != memcmp(&terml, &terml2, sizeof(terml)));
         TEST(terml.ctrl_lnext     == tconf2.c_cc[VLNEXT]);
         TEST(terml.ctrl_susp      == tconf2.c_cc[VSUSP]);
         TEST(terml.oldconf_vmin   == tconf2.c_cc[VMIN]);
         TEST(terml.oldconf_vtime  == tconf2.c_cc[VTIME]);
         TEST(terml.oldconf_echo   == (0 != (tconf2.c_lflag & ECHO)));
         TEST(terml.oldconf_icanon == (0 != (tconf2.c_lflag & ICANON)));
         TEST(terml.oldconf_icrnl  == (0 != (tconf2.c_iflag & ICRNL)));
         TEST(terml.oldconf_isig   == (0 != (tconf2.c_lflag & ISIG)));
         TEST(terml.oldconf_ixon   == (0 != (tconf2.c_iflag & IXON)));
         TEST(terml.oldconf_onlcr  == (0 != (tconf2.c_oflag & ONLCR)));
      }
   }
   TEST(0 == writeconfig(&tconf, sys_iochannel_STDIN));

   // TEST configstore: ENOTTY
   memset(&terml, 0, sizeof(terml));
   TEST(ENOTTY == configstore(&terml, io_file(file)));
   TEST(0 == memcmp(&terml, &terml2, sizeof(terml)));

   // TEST configstore: EBADF
   TEST(EBADF == configstore(&terml, sys_iochannel_FREE));
   TEST(0 == memcmp(&terml, &terml2, sizeof(terml)));

   // unprepare
   TEST(0 == free_file(&file));
   TEST(0 == remove_file("./xxx", 0));
   TEST(0 == writeconfig(&oldconf, sys_iochannel_STDIN));

   return 0;
ONERR:
   if (isoldconf) {
      writeconfig(&oldconf, sys_iochannel_STDIN);
   }
   free_file(&file);
   remove_file("./xxx", 0);
   return EINVAL;
}

static int test_initfree(void)
{
   terminal_t     terml = terminal_FREE;
   struct termios tconf;
   int            stdfd;
   file_t         oldstd = file_FREE;

   // prepare
   TEST(0 == readconfig(&tconf, sys_iochannel_STDIN));

   // TEST terminal_FREE
   TEST(isfree_file(terml.input));
   TEST(isfree_file(terml.output));
   TEST(0 == terml.oldconf_vmin);
   TEST(0 == terml.oldconf_vtime);
   TEST(0 == terml.oldconf_echo);
   TEST(0 == terml.oldconf_icanon);
   TEST(0 == terml.oldconf_icrnl);
   TEST(0 == terml.oldconf_isig);
   TEST(0 == terml.oldconf_onlcr);
   TEST(0 == terml.doclose);

   // TEST init_terminal: use sys_iochannel_STDIN/sys_iochannel_STDOUT
   TEST(0 == init_terminal(&terml));
   TEST(sys_iochannel_STDIN  == terml.input);
   TEST(sys_iochannel_STDOUT == terml.output);
   TEST(terml.ctrl_lnext     == tconf.c_cc[VLNEXT]);
   TEST(terml.ctrl_susp      == tconf.c_cc[VSUSP]);
   TEST(terml.oldconf_vmin   == tconf.c_cc[VMIN]);
   TEST(terml.oldconf_vtime  == tconf.c_cc[VTIME]);
   TEST(terml.oldconf_echo   == (0 != (tconf.c_lflag & ECHO)));
   TEST(terml.oldconf_icanon == (0 != (tconf.c_lflag & ICANON)));
   TEST(terml.oldconf_icrnl  == (0 != (tconf.c_iflag & ICRNL)));
   TEST(terml.oldconf_isig   == (0 != (tconf.c_lflag & ISIG)));
   TEST(terml.oldconf_ixon   == (0 != (tconf.c_iflag & IXON)));
   TEST(terml.oldconf_onlcr  == (0 != (tconf.c_oflag & ONLCR)));
   TEST(0 == terml.doclose);

   // TEST free_terminal: fd not closed
   TEST(0 == terml.doclose);
   TEST(0 == free_terminal(&terml));
   TEST(isfree_file(terml.input));
   TEST(isfree_file(terml.output));
   TEST(0 == terml.doclose);
   TEST(isvalid_file(sys_iochannel_STDIN));
   TEST(isvalid_file(sys_iochannel_STDOUT));

   // TEST init_terminal: ERROR
   for (unsigned i = 1; i <= 1; ++i) {
      init_testerrortimer(&s_terminal_errtimer, i, EINVAL);
      TEST(EINVAL == init_terminal(&terml));
      TEST(isfree_file(terml.input));
      TEST(isfree_file(terml.output));
      TEST(0 == terml.doclose);
   }

   // TEST free_terminal: NO ERROR possible (fd not closed)
   TEST(0 == init_terminal(&terml));
   TEST(0 == terml.doclose);
   init_testerrortimer(&s_terminal_errtimer, 1, EINVAL);
   TEST(0 == free_terminal(&terml));
   TEST(isfree_file(terml.input));
   TEST(isfree_file(terml.output));
   TEST(0 == terml.doclose);
   TEST(isvalid_file(sys_iochannel_STDIN));
   TEST(isvalid_file(sys_iochannel_STDOUT));
   init_testerrortimer(&s_terminal_errtimer, 0, EINVAL);

   static_assert(sys_iochannel_STDIN+1 == sys_iochannel_STDOUT, "used as bounds in for loop");
   for (stdfd = sys_iochannel_STDIN; stdfd <= sys_iochannel_STDOUT; ++stdfd) {
      // prepare
      memset(&terml, 0, sizeof(terml));
      oldstd = dup(stdfd);
      TEST(oldstd > 0);
      close(stdfd);

      // TEST init_terminal: open file
      TEST(! isvalid_file(stdfd));
      TEST(0 == init_terminal(&terml));
      TEST( isvalid_file(stdfd));
      TEST(stdfd == terml.input);
      TEST(stdfd == terml.output);
      TEST(terml.ctrl_lnext     == tconf.c_cc[VLNEXT]);
      TEST(terml.ctrl_susp      == tconf.c_cc[VSUSP]);
      TEST(terml.oldconf_vmin   == tconf.c_cc[VMIN]);
      TEST(terml.oldconf_vtime  == tconf.c_cc[VTIME]);
      TEST(terml.oldconf_echo   == (0 != (tconf.c_lflag & ECHO)));
      TEST(terml.oldconf_icanon == (0 != (tconf.c_lflag & ICANON)));
      TEST(terml.oldconf_icrnl  == (0 != (tconf.c_iflag & ICRNL)));
      TEST(terml.oldconf_isig   == (0 != (tconf.c_lflag & ISIG)));
      TEST(terml.oldconf_ixon   == (0 != (tconf.c_iflag & IXON)));
      TEST(terml.oldconf_onlcr  == (0 != (tconf.c_oflag & ONLCR)));
      TEST(0 != terml.doclose);

      // TEST free_terminal: fd closed
      TEST( isvalid_file(stdfd));
      TEST(0 == free_terminal(&terml));
      TEST(! isvalid_file(stdfd));
      TEST(isfree_file(terml.input));
      TEST(isfree_file(terml.output));
      TEST(0 == terml.doclose);

      // TEST init_terminal: ERROR
      for (unsigned i = 1; i <= 2; ++i) {
         init_testerrortimer(&s_terminal_errtimer, i, EINVAL);
         TEST(EINVAL == init_terminal(&terml));
         TEST(isfree_file(terml.input));
         TEST(isfree_file(terml.output));
         TEST(0 == terml.doclose);
      }

      // TEST free_terminal: ERROR (fd closed)
      for (unsigned i = 1; i <= 2; ++i) {
         TEST(0 == init_terminal(&terml));
         TEST( isvalid_file(stdfd));
         // test
         TEST(0 != terml.doclose);
         init_testerrortimer(&s_terminal_errtimer, i, EINVAL);
         TEST(EINVAL == free_terminal(&terml));
         TEST(! isvalid_file(stdfd));
         TEST(isfree_file(terml.input));
         TEST(isfree_file(terml.output));
         TEST(0 == terml.doclose);
      }

      // unprepare
      TEST(stdfd == dup2(oldstd, stdfd));
      TEST(0 == close(oldstd));
      oldstd = file_FREE;
      TEST( isvalid_file(stdfd));
   }

   return 0;
ONERR:
   if (! isfree_file(oldstd)) {
      dup2(oldstd, stdfd);
      close(oldstd);
   }
   return EINVAL;
}

static int test_enodata_typeterminal(void)
{
   uint8_t buffer[100] = {0};

   // TEST type_terminal: ENODATA
   unsetenv(ENVIRON_TERM);
   TEST(ENODATA == type_terminal(sizeof(buffer), buffer));
   TEST(0 == buffer[0]);

   return 0;
ONERR:
   return EINVAL;
}

static int test_query(void)
{
   terminal_t     terml  = terminal_FREE;
   terminal_t     terml2 = terminal_FREE; // keeps unitialized
   file_t         file   = file_FREE;
   int            pfd[2] = { sys_iochannel_FREE, sys_iochannel_FREE };
   uint8_t        name[100];
   uint8_t        type[100];
   struct timeval starttime;
   struct timeval endtime;
   struct
   itimerspec     exptime;
   timer_t        timerid;
   bool           istimer = false;

   // prepare
   file = dup(sys_iochannel_STDERR);
   TEST(0 < file);
   TEST(0 == pipe2(pfd, O_CLOEXEC));
   TEST(0 == init_terminal(&terml));
   sigevent_t sigev;
   sigev.sigev_notify = SIGEV_SIGNAL;
   sigev.sigev_signo  = SIGINT;
   sigev.sigev_value.sival_int = 0;
   TEST(0 == timer_create(CLOCK_MONOTONIC, &sigev, &timerid));
   istimer = true;

   // TEST input_terminal
   TEST(sys_iochannel_STDIN == input_terminal(&terml));
   for (sys_iochannel_t i = 1; i; i = i << 1) {
      terminal_t tx = { .input = i };
      TEST(i == input_terminal(&tx));
   }

   // TEST input_terminal
   TEST(sys_iochannel_STDOUT == output_terminal(&terml));
   for (sys_iochannel_t i = 1; i; i = i << 1) {
      terminal_t tx = { .output = i };
      TEST(i == output_terminal(&tx));
   }

   // TEST isutf8_terminal
   TEST(0 != isutf8_terminal(&terml));

   // TEST isutf8_terminal: ERROR
   init_testerrortimer(&s_terminal_errtimer, 1, EINVAL);
   TEST(0 == isutf8_terminal(&terml));

   // TEST pathname_terminal
   TEST(0 == pathname_terminal(&terml, sizeof(name), name));
   TEST(5 <  strlen((char*)name));
   TEST(0 == strncmp((char*)name, "/dev/", 5));

   // TEST pathname_terminal: ENOBUFS
   TEST(ENOBUFS == pathname_terminal(&terml, 5, name));

   // TEST pathname_terminal: EBADF
   TEST(EBADF == pathname_terminal(&terml2, sizeof(name), name));

   // TEST hascontrolling_terminal: true (false is tested in test_controlterm)
   TEST(0 != hascontrolling_terminal());

   // TEST is_terminal: true
   TEST(0 != is_terminal(sys_iochannel_STDIN));
   TEST(0 != is_terminal(sys_iochannel_STDOUT));
   TEST(0 != is_terminal(sys_iochannel_STDERR));
   TEST(0 != is_terminal(file));

   // TEST is_terminal: false
   TEST(0 == is_terminal(sys_iochannel_FREE));
   TEST(0 == is_terminal(pfd[0]));
   TEST(0 == is_terminal(pfd[1]));

   // TEST iscontrolling_terminal: true
   TEST(0 != iscontrolling_terminal(sys_iochannel_STDIN));
   TEST(0 != iscontrolling_terminal(sys_iochannel_STDOUT));
   TEST(0 != iscontrolling_terminal(sys_iochannel_STDERR));
   TEST(0 != iscontrolling_terminal(file));

   // TEST iscontrolling_terminal: false
   TEST(0 == iscontrolling_terminal(sys_iochannel_FREE));
   TEST(0 == iscontrolling_terminal(pfd[0]));
   TEST(0 == iscontrolling_terminal(pfd[1]));

   // TEST issizechange_terminal
   TEST(0 == issizechange_terminal());
   raise(SIGWINCH); // send signal to self
   TEST(0 != issizechange_terminal()); // removes signal from queue
   TEST(0 == issizechange_terminal());

   // TEST waitsizechange_terminal: return 0 (signal received)
   raise(SIGWINCH);
   TEST(0 == waitsizechange_terminal()); // removes signal from queue
   TEST(0 == issizechange_terminal());

   // TEST waitsizechange_terminal: return EINTR
   raise(SIGINT);
   sigset_t pending;
   TEST(0 == sigpending(&pending));
   TEST(1 == sigismember(&pending, SIGINT));
   TEST(EINTR == waitsizechange_terminal()); // removes signal from queue
   TEST(0 == sigpending(&pending));
   TEST(0 == sigismember(&pending, SIGINT));

   // TEST waitsizechange_terminal: test waiting
   memset(&exptime, 0, sizeof(exptime));
   exptime.it_value.tv_nsec = 1000000000/10;    // 10th of a second
   TEST(0 == timer_settime(timerid, 0, &exptime, 0)); // generate SIGINT after timeout
   TEST(0 == gettimeofday(&starttime, 0));
   TEST(EINTR == waitsizechange_terminal());
   TEST(0 == gettimeofday(&endtime, 0));
   unsigned elapsedms = (unsigned) (1000 * (endtime.tv_sec - starttime.tv_sec) + endtime.tv_usec / 1000 - starttime.tv_usec / 1000);
   TESTP(50 < elapsedms && elapsedms < 500, "elapsedms=%d", elapsedms);

   // TEST type_terminal
   memset(type, 255, sizeof(type));
   TEST(0 == type_terminal(sizeof(type), type));
   size_t len = strlen((char*)type);
   TEST(0 < len && len < sizeof(type));
   if (strcmp("xterm", (char*)type) && strcmp("linux", (char*)type)) {
      logwarning_unittest("unknown terminal type (not xterm, linux)");
   }

   // TEST type_terminal: ENOBUFS
   memset(type, 0, sizeof(type));
   TEST(ENOBUFS == type_terminal((uint16_t)len, type));
   TEST(0 == type[0]);

   // TEST type_terminal: ENODATA
   int err;
   TEST(0 == execasprocess_unittest(&test_enodata_typeterminal, &err));
   TEST(0 == err);

   // TEST ctrllnext_terminal
   TEST(ctrllnext_terminal(&terml) == terml.ctrl_lnext);
   for (uint8_t i = 1; i; i = (uint8_t)(i << 1)) {
      terml2.ctrl_lnext = i;
      TEST(i == ctrllnext_terminal(&terml2));
   }

   // TEST ctrlsusp_terminal
   TEST(ctrlsusp_terminal(&terml) == terml.ctrl_susp);
   for (uint8_t i = 1; i; i = (uint8_t)(i << 1)) {
      terml2.ctrl_susp = i;
      TEST(i == ctrlsusp_terminal(&terml2));
   }

   // unprepare
   istimer = false;
   TEST(0 == timer_delete(timerid));
   TEST(0 == close(file));
   TEST(0 == close(pfd[0]));
   TEST(0 == close(pfd[1]));
   TEST(0 == free_terminal(&terml));

   return 0;
ONERR:
   if (istimer) {
      timer_delete(timerid);
   }
   close(file);
   close(pfd[0]);
   close(pfd[1]);
   free_terminal(&terml);
   return EINVAL;
}

static int test_read(void)
{
   terminal_t     terml = terminal_FREE;
   struct winsize oldsize;
   uint16_t       colsize;
   uint16_t       rowsize;
   systimer_t     timer = systimer_FREE;
   uint64_t       duration_ms;
   int            fd[2];
   uint8_t        buf[10];

   // prepare
   TEST(0 == init_systimer(&timer, sysclock_MONOTONIC));
   TEST(0 == init_terminal(&terml));
   TEST(0 == configrawedit_terminal(&terml));
   TEST(0 == ioctl(terml.input, TIOCGWINSZ, &oldsize));
   tcflush(terml.input, TCIFLUSH); // discards any pressed keys

   // TEST tryread_terminal: waits 1/10th of a second
   TEST(0 == startinterval_systimer(timer, &(timevalue_t) { .seconds = 0, .nanosec = 1000000 }));
   TEST(0 == tryread_terminal(&terml, sizeof(buf), buf));
   TEST(0 == expirationcount_systimer(timer, &duration_ms));
   TESTP(50 <= duration_ms && duration_ms <= 250, "diration=%" PRIu64, duration_ms);

   // TEST tryread_terminal: EBADF
   terminal_t terml2 = terminal_FREE;
   TEST(0 == tryread_terminal(&terml2, sizeof(buf), buf));

   // TEST tryread_terminal: closed pipe
   TEST(0 == pipe2(fd, O_CLOEXEC));
   TEST(0 == close(fd[1]));
   terml2 = terml;
   terml2.input = fd[0];
   TEST(0 == tryread_terminal(&terml2, sizeof(buf), buf));
   TEST(0 == close(fd[0]));

   // TEST readsize_terminal
   colsize = 0;
   rowsize = 0;
   TEST(0 == readsize_terminal(&terml, &rowsize, &colsize));
   TEST(2 < colsize);
   TEST(2 < rowsize);
   TEST(oldsize.ws_col == colsize);
   TEST(oldsize.ws_row == rowsize);

   // TEST readsize_terminal: read changed size
   tcdrain(terml.input);
   struct winsize newsize = oldsize;
   newsize.ws_col = (unsigned short) (newsize.ws_col - 2);
   newsize.ws_row = (unsigned short) (newsize.ws_row - 2);
   TEST(0 == ioctl(terml.input, TIOCSWINSZ, &newsize)); // change size
   TEST(0 != issizechange_terminal()); // we received notification
   TEST(0 == readsize_terminal(&terml, &rowsize, &colsize));
   TEST(newsize.ws_col == colsize);
   TEST(newsize.ws_row == rowsize);
   TEST(0 == ioctl(terml.input, TIOCSWINSZ, &oldsize)); // change size
   TEST(0 != issizechange_terminal()); // we received notification
   TEST(0 == readsize_terminal(&terml, &rowsize, &colsize));
   TEST(oldsize.ws_col == colsize);
   TEST(oldsize.ws_row == rowsize);

   // unprepare
   TEST(0 == configrestore_terminal(&terml));
   TEST(0 == free_terminal(&terml));
   TEST(0 == free_systimer(&timer));

   // TEST readsize_terminal: EBADF
   TEST(EBADF == readsize_terminal(&terml, &rowsize, &colsize));

   return 0;
ONERR:
   configrestore_terminal(&terml);
   free_terminal(&terml);
   free_systimer(&timer);
   return EINVAL;
}

static int test_config(void)
{
   terminal_t     terml = terminal_FREE;
   struct termios oldconf;
   struct termios tconf;
   bool           isold = false;

   // prepare
   TEST(0 == init_terminal(&terml));
   TEST(0 == readconfig(&oldconf, terml.input));
   isold = true;

   // TEST configstore_terminal: line edit mode
   for (int i = 0; i <= 1; ++i) {
      terminal_t terml2;
      memset(&terml2, i ? 255 : 0, sizeof(terml2));
      terml2.input = terml.input;
      TEST(0 == configstore_terminal(&terml2));
      TEST(terml2.ctrl_lnext     == oldconf.c_cc[VLNEXT]);
      TEST(terml2.ctrl_susp      == oldconf.c_cc[VSUSP]);
      TEST(terml2.oldconf_vmin   == oldconf.c_cc[VMIN]);
      TEST(terml2.oldconf_vtime  == oldconf.c_cc[VTIME]);
      TEST(terml2.oldconf_echo   == (0 != (oldconf.c_lflag & ECHO)));
      TEST(terml2.oldconf_icanon == (0 != (oldconf.c_lflag & ICANON)));
      TEST(terml2.oldconf_icrnl  == (0 != (oldconf.c_iflag & ICRNL)));
      TEST(terml2.oldconf_isig   == (0 != (oldconf.c_lflag & ISIG)));
      TEST(terml2.oldconf_ixon   == (0 != (oldconf.c_iflag & IXON)));
      TEST(terml2.oldconf_onlcr  == (0 != (oldconf.c_oflag & ONLCR)));
   }

   // TEST configstore_terminal: ERROR
   for (int i = 0; i <= 1; ++i) {
      terminal_t terml2;
      terminal_t terml3;
      memset(&terml2, i ? 255 : 0, sizeof(terml2));
      memset(&terml3, i ? 255 : 0, sizeof(terml2));
      init_testerrortimer(&s_terminal_errtimer, 1, EINVAL);
      TEST(EINVAL == configstore_terminal(&terml2));
      TEST(0 == memcmp(&terml2, &terml3, sizeof(terml2))); // not changed
   }

   // TEST configrawedit_terminal
   TEST(0 == configrawedit_terminal(&terml));
   TEST(0 == readconfig(&tconf, terml.input));
   TEST(0 == (tconf.c_iflag & ICRNL));
   TEST(0 == (tconf.c_oflag & ONLCR));
   TEST(0 == (tconf.c_lflag & ICANON));
   TEST(0 == (tconf.c_lflag & ECHO));
   TEST(0 == (tconf.c_lflag & ISIG));
   TEST(0 == tconf.c_cc[VMIN]);
   TEST(1 == tconf.c_cc[VTIME]);
   TEST(oldconf.c_cc[VLNEXT] == tconf.c_cc[VLNEXT]);
   TEST(oldconf.c_cc[VSUSP]  == tconf.c_cc[VSUSP]);

   // TEST configstore_terminal: raw edit mode
   {
      terminal_t terml2;
      memset(&terml2, 255, sizeof(terml2));
      terml2.input = terml.input;
      TEST(0 == configstore_terminal(&terml2));
      TEST(terml2.ctrl_lnext     == oldconf.c_cc[VLNEXT]);
      TEST(terml2.ctrl_susp      == oldconf.c_cc[VSUSP]);
      TEST(terml2.oldconf_vmin   == 0);
      TEST(terml2.oldconf_vtime  == 1);
      TEST(terml2.oldconf_echo   == 0);
      TEST(terml2.oldconf_icanon == 0);
      TEST(terml2.oldconf_icrnl  == 0);
      TEST(terml2.oldconf_isig   == 0);
      TEST(terml2.oldconf_ixon   == 0);
      TEST(terml2.oldconf_onlcr  == 0);
   }

   // TEST configrestore_terminal
   TEST(0 == configrestore_terminal(&terml));
   TEST(0 == readconfig(&tconf, terml.input));
   TEST(oldconf.c_iflag == tconf.c_iflag);
   TEST(oldconf.c_oflag == tconf.c_oflag);
   TEST(oldconf.c_lflag == tconf.c_lflag);
   TEST(0 == memcmp(oldconf.c_cc, tconf.c_cc, sizeof(tconf.c_cc)))

   // TEST configrawedit_terminal, configrestore_terminal: VMIN
   if (0 == tconf.c_cc[VMIN]) {
      tconf.c_cc[VMIN] = 1;
      TEST(0 == writeconfig(&tconf, terml.input));
   }
   // sets VMIN to 0
   TEST(0 == configrawedit_terminal(&terml));
   TEST(0 == readconfig(&tconf, terml.input));
   TEST(0 == tconf.c_cc[VMIN]);
   for (int i = 2; i >= 0; --i) {
      terml.oldconf_vmin = (uint8_t) i;
      TEST(0 == configrestore_terminal(&terml));
      TEST(0 == readconfig(&tconf, terml.input));
      TEST(i == tconf.c_cc[VMIN]);
   }

   // TEST configrawedit_terminal, configrestore_terminal: VTIME
   if (tconf.c_cc[VTIME]) {
      tconf.c_cc[VTIME] = 0;
      TEST(0 == writeconfig(&tconf, terml.input));
   }
   // sets VTIME to 1
   TEST(0 == configrawedit_terminal(&terml));
   TEST(0 == readconfig(&tconf, terml.input));
   TEST(1 == tconf.c_cc[VTIME]);
   for (int i = 2; i >= 0; --i) {
      terml.oldconf_vtime = (uint8_t) i;
      TEST(0 == configrestore_terminal(&terml));
      TEST(0 == readconfig(&tconf, terml.input));
      TEST(i == tconf.c_cc[VTIME]);
   }

   // TEST configrawedit_terminal, configrestore_terminal: ICRNL
   if (! (tconf.c_iflag & ICRNL)) {
      tconf.c_iflag |= ICRNL;
      TEST(0 == writeconfig(&tconf, terml.input));
   }
   // clears ICRNL
   TEST(0 == configrawedit_terminal(&terml));
   TEST(0 == readconfig(&tconf, terml.input));
   TEST(0 == (tconf.c_iflag & ICRNL));
   for (int i = 0; i <= 1; ++i) {
      terml.oldconf_icrnl = (i != 0);
      TEST(0 == configrestore_terminal(&terml));
      TEST(0 == readconfig(&tconf, terml.input));
      TEST(i == (0 != (tconf.c_iflag & ICRNL)));
   }

   // TEST configrawedit_terminal, configrestore_terminal: ONLCR
   if (! (tconf.c_oflag & ONLCR)) {
      tconf.c_oflag |= ONLCR;
      TEST(0 == writeconfig(&tconf, terml.input));
   }
   // clears ONLCR
   TEST(0 == configrawedit_terminal(&terml));
   TEST(0 == readconfig(&tconf, terml.input));
   TEST(0 == (tconf.c_oflag & ONLCR));
   for (int i = 0; i <= 1; ++i) {
      terml.oldconf_onlcr = (i != 0);
      TEST(0 == configrestore_terminal(&terml));
      TEST(0 == readconfig(&tconf, terml.input));
      TEST(i == (0 != (tconf.c_oflag & ONLCR)));
   }

   // TEST configrawedit_terminal, configrestore_terminal: ICANON
   if (! (tconf.c_lflag & ICANON)) {
      tconf.c_lflag |= ICANON;
      TEST(0 == writeconfig(&tconf, terml.input));
   }
   // clears ICANON
   TEST(0 == configrawedit_terminal(&terml));
   TEST(0 == readconfig(&tconf, terml.input));
   TEST(0 == (tconf.c_lflag & ICANON));
   for (int i = 0; i <= 1; ++i) {
      terml.oldconf_icanon = (i != 0);
      TEST(0 == configrestore_terminal(&terml));
      TEST(0 == readconfig(&tconf, terml.input));
      TEST(i == (0 != (tconf.c_lflag & ICANON)));
   }

   // TEST configrawedit_terminal, configrestore_terminal: ECHO
   if (! (tconf.c_lflag & ECHO)) {
      tconf.c_lflag |= ECHO;
      TEST(0 == writeconfig(&tconf, terml.input));
   }
   // clears ECHO
   TEST(0 == configrawedit_terminal(&terml));
   TEST(0 == readconfig(&tconf, terml.input));
   TEST(0 == (tconf.c_lflag & ECHO));
   for (int i = 0; i <= 1; ++i) {
      terml.oldconf_echo = (i != 0);
      TEST(0 == configrestore_terminal(&terml));
      TEST(0 == readconfig(&tconf, terml.input));
      TEST(i == (0 != (tconf.c_lflag & ECHO)));
   }

   // TEST configrawedit_terminal, configrestore_terminal: ISIG
   if (! (tconf.c_lflag & ISIG)) {
      tconf.c_lflag |= ISIG;
      TEST(0 == writeconfig(&tconf, terml.input));
   }
   // clears ISIG
   TEST(0 == configrawedit_terminal(&terml));
   TEST(0 == readconfig(&tconf, terml.input));
   TEST(0 == (tconf.c_lflag & ISIG));
   for (int i = 0; i <= 1; ++i) {
      terml.oldconf_isig = (i != 0);
      TEST(0 == configrestore_terminal(&terml));
      TEST(0 == readconfig(&tconf, terml.input));
      TEST(i == (0 != (tconf.c_lflag & ISIG)));
   }

   // TEST configrawedit_terminal, configrestore_terminal: IXON
   if (! (tconf.c_iflag & IXON)) {
      tconf.c_iflag |= IXON;
      TEST(0 == writeconfig(&tconf, terml.input));
   }
   // clears IXON
   TEST(0 == configrawedit_terminal(&terml));
   TEST(0 == readconfig(&tconf, terml.input));
   TEST(0 == (tconf.c_iflag & IXON));
   for (int i = 0; i <= 1; ++i) {
      terml.oldconf_ixon = (i != 0);
      TEST(0 == configrestore_terminal(&terml));
      TEST(0 == readconfig(&tconf, terml.input));
      TEST(i == (0 != (tconf.c_iflag & IXON)));
   }

   // unprepare
   isold = false;
   TEST(0 == writeconfig(&oldconf, terml.input));
   TEST(0 == free_terminal(&terml));

   return 0;
ONERR:
   if (isold) {
      writeconfig(&oldconf, terml.input);
   }
   free_terminal(&terml);
   return EINVAL;
}

static int test_doremove1(void)
{
   resourceusage_t   usage = resourceusage_FREE;

   TEST(0 == init_resourceusage(&usage));

   // TEST hascontrolling_terminal: true; STDIN
   TEST(0 != iscontrolling_terminal(sys_iochannel_STDIN));
   TEST(0 != hascontrolling_terminal());

   // TEST removecontrolling_terminal: STDIN
   TEST(0 != iscontrolling_terminal(sys_iochannel_STDIN));
   removecontrolling_terminal();

   // TEST hascontrolling_terminal: false
   TEST(0 == hascontrolling_terminal());

   // TEST removecontrolling_terminal: ENXIO (already removed)
   TEST(ENXIO == removecontrolling_terminal());

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   return 0;
ONERR:
   return EINVAL;
}

static int test_doremove2(void)
{
   resourceusage_t   usage = resourceusage_FREE;

   TEST(0 == init_resourceusage(&usage));

   // TEST hascontrolling_terminal: true; open /dev/tty
   TEST(0 == iscontrolling_terminal(sys_iochannel_STDIN));
   TEST(0 != hascontrolling_terminal());

   // TEST removecontrolling_terminal: open /dev/tty
   TEST(0 == iscontrolling_terminal(sys_iochannel_STDIN));
   removecontrolling_terminal();

   // TEST hascontrolling_terminal: false
   TEST(0 == hascontrolling_terminal());

   // TEST removecontrolling_terminal: ENXIO (already removed)
   TEST(ENXIO == removecontrolling_terminal());

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   return 0;
ONERR:
   return EINVAL;
}

static int test_doremove3(void)
{
   resourceusage_t   usage = resourceusage_FREE;

   TEST(0 == init_resourceusage(&usage));

   // TEST hascontrolling_terminal: true
   TEST(0 != hascontrolling_terminal());

   // TEST setsid: chaning session id is same as removing control terminal
   TEST(getpid() == setsid());

   // TEST hascontrolling_terminal: false
   TEST(0 == hascontrolling_terminal());

   // TEST removecontrolling_terminal: ENXIO (already removed)
   TEST(ENXIO == removecontrolling_terminal());

   TEST(0 == same_resourceusage(&usage));
   TEST(0 == free_resourceusage(&usage));

   return 0;
ONERR:
   return EINVAL;
}

static int test_controlterm(void)
{
   int err;
   file_t   oldstdin = file_FREE;

   // TEST hascontrolling_terminal, removecontrolling_terminal: sys_iochannel_STDIN
   TEST(0 == execasprocess_unittest(&test_doremove1, &err));
   TEST(0 == execasprocess_unittest(&test_doremove3, &err));
   TEST(0 == err);

   // prepare
   oldstdin = dup(sys_iochannel_STDIN);
   TEST(oldstdin > 0);
   close(sys_iochannel_STDIN);

   // TEST hascontrolling_terminal, removecontrolling_terminal: open /dev/tty
   TEST(0 == execasprocess_unittest(&test_doremove2, &err));
   TEST(0 == execasprocess_unittest(&test_doremove3, &err));
   TEST(0 == err);

   // unprepare
   TEST(sys_iochannel_STDIN == dup2(oldstdin, sys_iochannel_STDIN));
   TEST(0 == close(oldstdin));
   oldstdin = file_FREE;

   return 0;
ONERR:
   if (! isfree_file(oldstdin)) {
      dup2(oldstdin, sys_iochannel_STDIN);
      close(oldstdin);
   }
   return EINVAL;
}


int unittest_io_terminal_terminal()
{
   if (test_helper())         goto ONERR;
   if (test_initfree())       goto ONERR;
   if (test_query())          goto ONERR;
   if (test_config())         goto ONERR;
   if (test_read())           goto ONERR;
   if (test_controlterm())    goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
