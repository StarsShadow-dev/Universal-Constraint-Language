#include <string.h>

#include "main.h"
#include "types.h"
#include "globals.h"

int main(int argc, char **argv) {
	allocateGlobals();
	
	linkedList_Node *linkedList = NULL;
	
	char *msg1 = "Hello, World";
	char *node1 = linkedList_addNode(&linkedList, strlen(msg1) + 1);
	strcpy(node1, msg1);
	
	char *msg2 = "testing";
	char *node2 = linkedList_addNode(&linkedList, strlen(msg2) + 1);
	strcpy(node2, msg2);
	
	printf("node1: %s\n", linkedList->data);
	printf("node2: %s\n", linkedList->next->data);
	
	linkedList_freeList(&linkedList);
	
	deallocateGlobals();
	return 0;
}
