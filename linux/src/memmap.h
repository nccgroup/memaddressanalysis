#include <stdint.h>

#include "../src/def_sizes.h"

typedef struct Error Error;
typedef struct MapItemRef MapItemRef;
typedef struct MallocTableRef MallocTableRef;
typedef struct MallocTable MallocTable;
typedef struct MapItem MapItem;

/* Application error */
struct Error {
   int number;
   char message[DEF_SIZE_ERROR_MESSAGE_LENGTH];
   uint8_t log;
};

/* Linked list reference for MapItem, only base for now */
struct MapItemRef {
   MapItem *base;
   MapItem *current;
   MapItem *end;
   int itemCount;
};

/* Single item */
struct MapItem {
   int count; /* Number of page map occurences across processes */
   unsigned long long baseAddress, endAddress, offset;
   char permissions[1024], offsetStr[1024], device[1024], inode[1024], pathname[1024];

   MapItem *prev;
   MapItem *next;
};

/* Linked list reference for MallocTable, only base for now */
struct MallocTableRef {
   MallocTable *base;
};

struct MallocTable {
   void *mallocAddress;

   MallocTable *prev;
   MallocTable *next;
};

/* FUNCTIONS! */
int setupMapItemList(MapItem *mapItem,MapItemRef *mapItemRef,MallocTable *mallocTable);
int setupErrorMessages();
int debugPrintf(char msg[DEF_SIZE_DEBUG_MESSAGE], ...);
uint8_t exitError(int x);
int processArgs(int argc, char *argv[]);
void *addMalloc(int memSize, MallocTable *mallocTable);
int addItem(MapItem *mapItem,MapItemRef *mapItemRef,MallocTable *mallocTable);
int searchMapsLine();
int processMapsFile(MapItem *mapItem,MapItemRef *mapItemRef,MallocTable *mallocTable,pid_t childPID);
int doFork(MapItem *mapItem,MapItemRef *mapItemRef,MallocTable *mallocTable,char *argv[]);
int iterateLinkedList(MapItem *mapItem,MapItemRef *mapItemRef);
int identifyCommonStrings();
int outputCommonStrings();
