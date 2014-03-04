#include <stdint.h>

#include "../src/def_sizes.h"

typedef struct Error Error;
typedef struct MapFileRef MapFileRef;
typedef struct MapFileNode MapFileNode;

/* Application error */
struct Error {
   int number;
   char message[DEF_SIZE_ERROR_MESSAGE_LENGTH];
   uint8_t log;
};

/* Linked list pointer etc */
struct MapFileRef {
   MapFileNode *base;
   MapFileNode *current;
};

/* Single node */
struct MapFileNode {
   int fd;
   MapFileNode *prev;
   MapFileNode *next;
};
