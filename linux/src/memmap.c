#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "../src/def_error_messages.h"
#include "../src/def_sizes.h"
#include "../src/memmap.h"

int var_glb; /* A global variable*/
int argDebug = 1;

unsigned int argRuns, argSecs;

Error error[DEF_SIZE_ERROR_INSTANCES];

MapFileRef mapfileref;
MapFileNode mapfilenode;

int setupMapFileDs()
{
   mapfileref.current = &mapfilenode;
   mapfileref.base = mapfileref.current;

   mapfilenode.prev = mapfileref.base;
   mapfilenode.next = NULL;
}

int setupErrorMessages()
{
   /*typedef struct {
      int number;
      char message[64];
      uint8_t log;
   } Error;*/

   //DEF_ERROR_1 "Could not find error message for error number, you shouldn't really see this.."
   error[1].number = 1;
   strcpy(error[1].message, DEF_ERROR_1);
   error[1].log = 0x0;

   //DEF_ERROR_10 "Missing argument"
   error[10].number = 10;
   strcpy(error[10].message, DEF_ERROR_10);
   error[10].log = 0x0;

   //DEF_ERROR_20 "Could not malloc"
   error[20].number = 20;
   strcpy(error[20].message, DEF_ERROR_20);
   error[20].log = 0x0;

   //DEF_ERROR_30 "IP address supplied is not a valid v4 address"
   error[30].number = 30;
   strcpy(error[30].message, DEF_ERROR_30);
   error[30].log = 0x0;

   //DEF_ERROR_31 "Domain is not valid"
   error[31].number = 31;
   strcpy(error[31].message, DEF_ERROR_31);
   error[31].log = 0x0;

   //DEF_ERROR_40 "Connection failed"
   error[40].number = 40;
   strcpy(error[40].message, DEF_ERROR_40);
   error[40].log = 0x0;

   //DEF_ERROR_50 "Could not resolve domain"
   error[50].number = 50;
   strcpy(error[50].message, DEF_ERROR_50);
   error[50].log = 0x0;

   //DEF_ERROR_60 "fd problem"
   error[60].number = 60;
   strcpy(error[60].message, DEF_ERROR_60);
   error[60].log = 0x0;

   return 0;
}

int debugPrintf(char msg[DEF_SIZE_DEBUG_MESSAGE], ...)
{
   if (argDebug == 1)
   {
      va_list args;
      va_start(args, msg);

      vprintf(msg, args);

      va_end(args);
   }

   return 0;
}

uint8_t exitError(int x)
{
   printf("Error %d: %s\n", error[x].number, error[x].message);

   exit(error[x].number);
}

int processArgs(int argc, char *argv[])
{
   /* arg 1 is number of runs, arg 2 is seconds to wait before
      sampling, arg 3 is binary to run and arg 4 ... N are arguments */
   /* ./memmap <runs> <secs> <binary> args */

   debugPrintf("processArgs argc == %d\n", argc);

   if (argc < 4)
   {
      exitError(10);
   }

   /* Assign globals */
   argRuns = strtoul(argv[1],NULL,10);
   argSecs = strtoul(argv[2],NULL,10);

   if ((argRuns <= 0) || (argSecs <= 0))
   {
      exitError(11);
   }

   debugPrintf("processArgs argRuns == %d\n", argRuns);
   debugPrintf("processArgs argSecs == %d\n", argSecs);

   return 0;
}

/* Adapted from http://stackoverflow.com/questions/2180079/how-can-i-copy-a-file-on-unix-using-c by caf (20140314) */
int cp(const char *from, const char *to)
{
   int fd_to, fd_from;
   char buf[4096];
   ssize_t nread;
   int saved_errno;

   fd_from = open(from, O_RDONLY);
   if (fd_from < 0)
      return -1;

   fd_to = open(to, O_WRONLY | O_CREAT | O_EXCL, 0666);
   if (fd_to < 0)
      goto out_error;

   while (nread = read(fd_from, buf, sizeof buf), nread > 0)
   {
      char *out_ptr = buf;
      ssize_t nwritten;

      do {
         nwritten = write(fd_to, out_ptr, nread);

         if (nwritten >= 0)
         {
            nread -= nwritten;
            out_ptr += nwritten;
         }
         else if (errno != EINTR)
         {
            goto out_error;
         }
      } while (nread > 0);
   }

   if (nread == 0)
   {
      if (close(fd_to) < 0)
      {
         fd_to = -1;
         goto out_error;
      }
      close(fd_from);

      /* Success! */
      return 0;
   }

   out_error:
      saved_errno = errno;

   close(fd_from);
   if (fd_to >= 0)
      close(fd_to);

   errno = saved_errno;
   return -1;
}

int addNode(int mapFile)
{
   /* add file to the data structure that needs creating.. */
   /* fd = num */
   /* linked list? na */

   /* new next */
   mapfilenode.next = malloc(sizeof(MapFileNode)); /* TODO check */
   /* prev becomes current */
   mapfilenode.prev = mapfileref.current;
   /* current becomes next */
   mapfileref.current = mapfilenode.next;
   /* next becomes NULL */
   mapfilenode.next = NULL;

   return 0;
}

int dumpProcMap(char *argv[], pid_t childPID)
{
   char from[DEF_SIZE_FILE_PATH], to[DEF_SIZE_FILE_PATH];
   time_t timer;
   char strTime[25];
   struct tm* tm_info;

   time(&timer);
   tm_info = localtime(&timer);

   strftime(strTime, 25, "%Y%m%d%H%M%S", tm_info);

   snprintf(from, sizeof(from), "/proc/%d/maps", childPID);

   debugPrintf("dumpProcMap from == %s\n", from);
   debugPrintf("dumpProcMap strTime == %s\n", strTime);

   snprintf(to,sizeof(to),"/tmp/memmap_%s_%d", strTime, childPID);

   /* Dump proc map in cwd. If non-zero returned, is 'to' arg fd */
   int mapFileNode = cp(from, to);

   if (mapFileNode <= 0)
   {
      addNode(mapFileNode);
   } else {
      exitError(60);
   }

   return 0;
}

int doFork(char *argv[])
{
   pid_t childPID;
   int var_lcl = 0, i = 0, childRetStatus = 0;

   /* Run specified times */
   for(i=0;i<argRuns;i++)
   {
      childPID = fork();

      if(childPID >= 0) /* fork success */
      {
         if(childPID == 0) /* child proc */
         {
            debugPrintf("doFork execv(%s, %x)\n",argv[3],&argv[3]);
            execv(argv[3], &argv[3]);
         }
         else /* parent proc */
         {
            var_lcl = 10;
            var_glb = 20;
            printf("\n Parent process :: var_lcl = [%d], var_glb[%d]\n", var_lcl, var_glb);
            debugPrintf("doFork argSecs == %d\n", argSecs);

            /* Dump process map */
            sleep(argSecs);
            dumpProcMap(argv, childPID);
            /* kill child */
            kill(childPID, SIGTERM);
         }
         /* waitfor? */
         waitpid(childPID, &childRetStatus, 0);
      }
      else // fork failed
      {
         printf("\n Fork failed, quitting!!!!!!\n");
         return 1;
      }
   }

   return 0;
}

int main(int argc, char *argv[])
{
   setupErrorMessages();
   setupMapFileDs();
   processArgs(argc,argv);

   doFork(argv);
}
