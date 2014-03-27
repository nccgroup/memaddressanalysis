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

int var_glb, argDebug = 1;

unsigned int argRuns, argSecs;

Error error[DEF_SIZE_ERROR_INSTANCES];

int setupMallocTable(MallocTable *mallocTable)
{
/*   mallocTable = malloc(sizeof(MallocTable));

   debugPrintf("setupMallocTable mallocTable == %x\n",mallocTable);
   if (NULL == mallocTable)
   {
      // malloc problem, bail
      exitError(20);
   }
*/

   mallocTable->prev = NULL;
   mallocTable->next = NULL;
}

int setupMapItemList(MapItem *mapItem,MapItemRef *mapItemRef,MallocTable *mallocTable)
{
   mapItem = addMalloc(sizeof(MapItem),mallocTable);
   mapItemRef->base = mapItem;

   mapItem->prev = NULL;
   mapItem->next = NULL;
}

int setupErrorMessages()
{
   //DEF_ERROR_1 "Could not find error message for error number, you shouldn't really see this.."
   error[1].number = 1;
   strcpy(error[1].message,DEF_ERROR_1);
   error[1].log = 0x0;

   //DEF_ERROR_10 "Missing argument"
   error[10].number = 10;
   strcpy(error[10].message,DEF_ERROR_10);
   error[10].log = 0x0;

   //DEF_ERROR_20 "Could not malloc"
   error[20].number = 20;
   strcpy(error[20].message,DEF_ERROR_20);
   error[20].log = 0x0;

   //DEF_ERROR_30 "IP address supplied is not a valid v4 address"
   error[30].number = 30;
   strcpy(error[30].message,DEF_ERROR_30);
   error[30].log = 0x0;

   //DEF_ERROR_31 "Domain is not valid"
   error[31].number = 31;
   strcpy(error[31].message,DEF_ERROR_31);
   error[31].log = 0x0;

   //DEF_ERROR_40 "Connection failed"
   error[40].number = 40;
   strcpy(error[40].message,DEF_ERROR_40);
   error[40].log = 0x0;

   //DEF_ERROR_50 "Could not resolve domain"
   error[50].number = 50;
   strcpy(error[50].message,DEF_ERROR_50);
   error[50].log = 0x0;

   //DEF_ERROR_60 "fd problem"
   error[60].number = 60;
   strcpy(error[60].message,DEF_ERROR_60);
   error[60].log = 0x0;

   //DEF_ERROR_70 "NULL pointer exception"
   error[70].number = 70;
   strcpy(error[70].message,DEF_ERROR_70);
   error[70].log = 0x0;

   return 0;
}

int debugPrintf(char msg[DEF_SIZE_DEBUG_MESSAGE],...)
{
   if (1 == argDebug)
   {
      va_list args;
      va_start(args,msg);

      vprintf(msg,args);

      va_end(args);
   }

   return 0;
}

uint8_t exitError(int x)
{
   printf("Error %d: %s\n",error[x].number,error[x].message);

   exit(error[x].number); /* exit(x) ? */
}

int processArgs(int argc,char *argv[])
{
   /* arg 1 is number of runs, arg 2 is seconds to wait before
      sampling, arg 3 is binary to run and arg 4 ... N are arguments */
   /* ./memmap <runs> <secs> <binary> args */
   /* I know it's pikey I'll fix it later maybe (not) */

   debugPrintf("processArgs argc == %d\n",argc);

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

   debugPrintf("processArgs argRuns == %d\n",argRuns);
   debugPrintf("processArgs argSecs == %d\n",argSecs);

   return 0;
}

/* TODO Make generic */
void *addMalloc(int memSize,MallocTable *mallocTable)
{
   void *doMalloc = NULL;

   /* malloc struct */
   MallocTable *oldMallocTable;
   /* store for next list item prev assignment */
   oldMallocTable = mallocTable;

   /* new next */
   mallocTable->next = malloc(sizeof(MallocTable));

   if (NULL == mallocTable->next)
   {
      /* malloc problem, bail */
      exitError(20);
   }

   /* DO MALLOC */
   doMalloc = malloc(memSize);

   if (NULL == doMalloc)
   {
      /* malloc problem, bail */
      exitError(20);
   } else {
      mallocTable->mallocAddress = doMalloc;
   }

   /* current becomes next */
   mallocTable = mallocTable->next;
   /* prev becomes current */
   mallocTable->prev = oldMallocTable;
   /* next becomes NULL */
   mallocTable->next = NULL;

   return doMalloc;
}

MapItem *processMapItemString(MapItem mapItem,char *mapItemStr)
{
   /* Nasty function is nasty */
   /* address           perms offset  dev   inode       pathname */
   /* 7f0255821000-7f025582c000 r--s 00000000 fe:02 134446                     /var/cache/fontconfig/d62e99ef547d1d24cdb1bd22ec1a2976-le64.cache-4 */

   unsigned long long baseAddress, endAddress, offset;
   char permissions[1024], offsetStr[1024], device[1024], inode[1024], pathname[1024];
   char *strCursor = mapItemStr, *pathnameStr;
   int desiredTempMapItemCount;

   debugPrintf("processMapItemString mapItemStr == %s",mapItemStr);

   /*** BASE ADDRESS ***/
   baseAddress = strtoull(mapItemStr,&strCursor,16);
   debugPrintf("processMapItemString strtoull base address == %p\n",(void *) baseAddress);
   debugPrintf("processMapItemString strCursor == %p\n",(void *) strCursor);

   mapItemStr = strCursor + 1;
   debugPrintf("processMapItemString mapItemStr == %s\n",mapItemStr);

   /*** END ADDRESS ***/
   endAddress = strtoull(mapItemStr,&strCursor,16);
   debugPrintf("processMapItemString strtoull end address == %p\n",(void *) endAddress);

   /* size_t strcspn(const char *s,const char *reject); */
   mapItemStr = strCursor + 1;

   /*** PERMISSIONS ***/
   strcpy(permissions,mapItemStr); /* TODO validate char * large enough */

   desiredTempMapItemCount = strcspn(mapItemStr," ");
   permissions[desiredTempMapItemCount] = '\0';

   debugPrintf("processMapItemString permissions == %s\n",permissions);

   /*** OFFSET ***/
   strcpy(offsetStr,(mapItemStr + desiredTempMapItemCount + 1)); /* TODO validate offset large enough */
   strCursor = offsetStr;
   offset = strtoull(offsetStr,&strCursor,16);

   debugPrintf("processMapItemString offset == %llx\n",offset);

   /*** DEVICE ***/
   strcpy(device,strCursor + 1); /* TODO validate char * large enough */

   desiredTempMapItemCount = strcspn(device," ");
   device[desiredTempMapItemCount] = '\0';

   debugPrintf("processMapItemString device == %s\n",device);

   /*** INODE ***/
   strcpy(inode,(device + desiredTempMapItemCount + 1)); /* TODO validate char * large enough */

   desiredTempMapItemCount = strcspn(device," ");
   inode[desiredTempMapItemCount] = '\0';

   debugPrintf("processMapItemString inode == %s\n",inode);

   /*** PATHNAME ***/
   strcpy(pathname,(inode + desiredTempMapItemCount + 1)); /* TODO validate char * large enough */
   pathnameStr = pathname;

   while(isspace(*pathnameStr))
   {
      pathnameStr++;
   }

   debugPrintf("processMapItemString pathname == %s\n",pathnameStr);
   debugPrintf("\n");

   /* Shove into data structure */
   mapItem.baseAddress = baseAddress;
   mapItem.endAddress = endAddress;
   strcpy(mapItem.permissions,permissions);
   mapItem.offset = offset;
   strcpy(mapItem.device,device);
   strcpy(mapItem.inode,inode); /* TODO inode should be a number */
   strcpy(mapItem.pathname,pathnameStr);

   return 0;
}

/* Add item to the list, and populates with string (which should be from /proc/[pid]/maps file */
/* context var can be 't' or nothing. If 't'(emp), then don't add new list item first, because this */
/* is a temporary data struct */
int addItem(MapItem *mapItem,MapItemRef *mapItemRef,MallocTable *mallocTable)
{
   MapItem *oldMapItem;
   /* store for next list item prev assignment */
   oldMapItem = mapItem;

   /* new next */
   debugPrintf("addItem mallocTable == %x\n",mallocTable);
   mapItem->next = addMalloc(sizeof(MapItem),mallocTable);
   debugPrintf("addItem mapItem->next == %x\n",mapItem->next);
   /* current becomes next */
   mapItem = mapItem->next;
   /* prev becomes current */
   mapItem->prev = oldMapItem;
   /* next becomes NULL */
   mapItem->next = NULL;
   /* Set end item so we know */
   mapItemRef->end = mapItem;

   return 0;
}

/* Should return non-zero if item in list */
int searchMapsLine()
{
   /* Process maps line */
   /* For each item in list */
      /* Perform comparison */
      /* return 1 if item found */

   return  1;
}

/* Open maps file and read each line */
int processMapsFile(MapItem *mapItem,MapItemRef *mapItemRef,MallocTable *mallocTable,pid_t childPID)
{
   /* Open file */
   FILE *fd;
   MapItem *tempMapItem;
   char buf[4096], mapsFilenameString[32];
   ssize_t nread;
   int saved_errno;

   /* Get maps filename                                 -1 ? */
   snprintf(mapsFilenameString,sizeof(mapsFilenameString),"/proc/%d/maps",childPID);

   debugPrintf("processMapsFile mapsFilenameString == %s\n",mapsFilenameString);

   fd = fopen(mapsFilenameString,"r");
   if (fd < 0)
   {
      exitError(60);
   }

   debugPrintf("processMapsFile fd == %x\n",fd);

   /* Iterate each line */
   while (fgets(buf,sizeof(buf),fd) != NULL) /* TODO check buffer overflow potential */
   {
      //debugPrintf("processMapsFile buf == %s",buf);
      /* Process the string and add to the list item */
      /* Can only approximate boundaries for maps file parameters */
      /* Would be nice to know what the variables were .. */
      processMapItemString(*mapItem,buf);

      addItem(mapItem,mapItemRef,mallocTable);
   }

   return 0;
}

int compareMapsList(MapItemRef *mapItemRef)
{
   MapItem *currentMapItem, *endMapItem;

   endMapItem = mapItemRef->end;
   currentMapItem = mapItemRef->base;
   /* Comparison paramters. Base address and permissions (and pathname/heap/stack?) */

   /* Starting from beginning, iterate over list to n-1 */
   while(&currentMapItem != &endMapItem)
   {
      /* if baseAddress is the same */
      if (currentMapItem->baseAddress == endMapItem->baseAddress)
      {
         /* if permissions are the same */
         if (strcmp(currentMapItem->permissions,endMapItem->permissions) == 0)
         {
            /* if pathname/heap/stack is the same? */
            currentMapItem->count++;

            /* Get rid of the last item in the list TODO */
            currentMapItem = mapItemRef->end;
            currentMapItem = currentMapItem->prev;
            currentMapItem->next = NULL;

            return 0;
         }
      }

      /* Next */
      currentMapItem = currentMapItem->next;
   }

   return 0;
}

int doFork(MapItem *mapItem,MapItemRef *mapItemRef,MallocTable *mallocTable,char *argv[])
{
   pid_t childPID;
   int var_lcl = 0, i = 0, childRetStatus = 0;

   /* Run specified times */
   for(i=0;i<argRuns;i++)
   {
      childPID = fork();

      if(childPID >= 0) /* fork success */
      {
         if(0 == childPID) /* child proc */
         {
            debugPrintf("doFork execv(%s, %x)\n",argv[3],&argv[3]);
            execv(argv[3],&argv[3]);
         }
         else /* parent proc */
         {
            debugPrintf("doFork parent\n");
            debugPrintf("doFork argSecs == %d\n",argSecs);

            sleep(argSecs);
            /* Process maps file */
            processMapsFile(mapItem,mapItemRef,mallocTable,childPID);
            /* Current process maps file will always be added to the end of the list as data struct */
            /* If a match is found in the list, then the count for that item is incremented */
            /* and the last item is dereferenced. Otherwise, keep the last item and move on */
            compareMapsList(mapItemRef);
            /* kill child */
            kill(childPID,SIGTERM);
         }
         /* waitfor? */
         waitpid(childPID,&childRetStatus,0);
      }
      else /* fork failed */
      {
         printf("\n Fork failed, quitting!!!!!!\n");
         return 1;
      }
   }

   return 0;
}

int iterateLinkedList(MapItem *mapItem,MapItemRef *mapItemRef)
{
   mapItem = mapItemRef->base;

   debugPrintf("iterateLinkedList mapItem->prev: %x\n",mapItem->next);
   while(mapItem->next != NULL)
   {
      /* address           perms offset  dev   inode       pathname */
      printf("iterateLinkedList Base Address: %llx\n",mapItem->baseAddress);
      printf("iterateLinkedList End Address: %llx\n",mapItem->endAddress);
      printf("iterateLinkedList Permissions: %s\n",mapItem->permissions);
      printf("iterateLinkedList Offset: %llx\n",mapItem->offset);
      printf("iterateLinkedList Device: %s\n",mapItem->device);
      printf("iterateLinkedList Inode: %s\n",mapItem->inode);
      printf("iterateLinkedList Pathname: %s\n",mapItem->pathname);
      printf("iterateLinkedList Count: %d\n",mapItem->count);
      debugPrintf("iterateLinkedList next: %x\n",mapItem->next);
      printf("\n");
      mapItem = mapItem->next;
      printf("iterateLinkedList Base Address: %llx\n",mapItem->baseAddress);
      printf("iterateLinkedList End Address: %llx\n",mapItem->endAddress);
      printf("iterateLinkedList Permissions: %s\n",mapItem->permissions);
      printf("iterateLinkedList Offset: %llx\n",mapItem->offset);
      printf("iterateLinkedList Device: %s\n",mapItem->device);
      printf("iterateLinkedList Inode: %s\n",mapItem->inode);
      printf("iterateLinkedList Pathname: %s\n",mapItem->pathname);
      printf("iterateLinkedList Count: %d\n",mapItem->count);
      debugPrintf("iterateLinkedList next: %d\n",mapItem->next);
      printf("\n");
   }

   return 0;
}

int main(int argc,char *argv[])
{
   MapItem mapItem;
   MapItemRef mapItemRef;
   MallocTable mallocTable;

   setupErrorMessages();
   setupMallocTable(&mallocTable);
   setupMapItemList(&mapItem,&mapItemRef,&mallocTable);
   processArgs(argc,argv);

   doFork(&mapItem,&mapItemRef,&mallocTable,argv);
   iterateLinkedList(&mapItem,&mapItemRef);
   debugPrintf("main mapItemRef.base == %p\n",mapItemRef.base);
   debugPrintf("main mapItemRef.end == %p\n",mapItemRef.end);
}
