#include "readrouters.c"

static const int DEBUG = 1; 

/****************************************************************************/ 	
/* Given a neighboring router's name, look up and return that router's 		*/ 
/* (neighbor, socket) combination 											*/ 
/****************************************************************************/ 	
neighborSocket* GetNeighborSocket(char* name)
{
	int i = 0; 

	neighborSocket* neighbor; 

	for(i = 0; i < MAXLINKS; i++)
	{
		neighbor = &neighborSocketArray[i]; 

		if(neighbor->neighbor == 0)
			continue; 
		
		if(strncmp(neighbor->neighbor, name, 1) == 0)
		{
			return &neighborSocketArray[i]; 
		}
	}

	return 0; 
}

/****************************************************************************/ 
/* Given a router's name, look up and return that router's configuration	*/ 
/****************************************************************************/ 
routerInfo* GetRouterInfo(char* router)
{
	int i = 0; 

	routerInfo* tmpRouter = &routerInfoTable[i]; 

	for(i=0; i< MAXROUTERS; i++)
	{
		tmpRouter = &routerInfoTable[i]; 

		if(tmpRouter->baseport == 0)
			continue; 

		if(strncmp(tmpRouter->router, router, 1) == 0)
		{
			return &routerInfoTable[i]; 
		}


	}

}

/********************************************************/
/* Given a socket number 								*/ 
/* return the name of the router who's using that socket*/ 
/********************************************************/
char* GetRouterName(int socket)
{
	int i = 0; 

	neighborSocket* neighbor; 

	for(i = 0; i < MAXLINKS; i++)
	{
		neighbor = &neighborSocketArray[i]; 

		if(neighbor->neighbor == 0)
			continue; 
		
		if(neighbor->socket == socket)
		{
			return neighbor->neighbor; 
		}
	}

	return 0; 
}

/********************************************************/
/* Given a socket number send your routing table		*/ 
/* to that socket using a series of U messages    		*/ 
/********************************************************/
void SendRoutingTable(int socket)
{
	int j = 0; 
	char message[24]; 
	char receiverName[1]; 		// The receiving router
	linkInfo* routerLink; 

	char* tmpName = GetRouterName(socket);
	strncpy(receiverName, tmpName, 1); 
	receiverName[1] = '\0'; 

	printf("\nAttempting to send routing table to %s (socket #%d)", receiverName, socket); 

	/************************************************/ 
	/* Iterate through your routing table sending 	*/ 
	/* each row individually to neighbor->socket 	*/ 
	/* as a U message 								*/ 
	/************************************************/ 
	for(j=0; j < MAXROUTERS; j++)
	{
		routerLink = &linkInfoTable[j]; 

		/****************************************************************************/ 
		/* If routerLink->router == 0, then there is no direct link to this router 	*/ 
		/****************************************************************************/ 
		if(routerLink->router == 0)
			continue; 

		//Put the table entries into the message buffer 
		char* dest = routerLink->router; 


		int cost = routerLink->cost;

		memset(&message, 0, sizeof(message)); 

		snprintf(message, sizeof(message), "U %C %d", dest[0], cost);


		// for each of the connections, receive: 
		// 		- Router update messages: U dest cost 
		// 		- Link cost messages: L neighbor cost 
		// If neighborSock = -1 then connect() failed above 
		// printf("\nAttempting to send an update message...");
		ssize_t numBytes = send(socket, message, sizeof(message), 0); 

        if(numBytes < 0)
            DieWithSystemMessage("send() failed"); 
     	else if(numBytes != sizeof(message))
            DieWithUserMessage("send()", "sent unexpected number of bytes"); 

         printf("\nSuccessfully sent a %zu byte update message to Router %s via socket #%d: %s\n", numBytes, receiverName, socket, message); 
	}

}

/****************************************************************************/ 
/* Send a test message to the socket parameter  							*/
/****************************************************************************/  
void SendTestMessage(int socket)
{
	
	char receiverName[1]; 
    char* tmpName = GetRouterName(socket);
	strncpy(receiverName, tmpName, 1); 
	receiverName[1] = '\0'; 

	char testMessage[24] = "U D 4\0"; 


    /********************************************************/ 
   	/* Send a test message to this router via its socket        */  
   	/********************************************************/ 
   	ssize_t numBytes = send(socket, testMessage, sizeof(testMessage), 0); 

    if(numBytes < 0)
    {
        printf("\nsend() failed with socket #%d", socket); 
        return; 
    }
    else if(numBytes != sizeof(testMessage))
    {
        printf("\nsend(): sent unexpected number of bytes"); 
        return; 
    }

    printf("\nSuccessfully sent a %zu byte update message to Router %s via socket #%d: %s\n", numBytes, receiverName, socket, testMessage); 

}

/********************************************************************************/ 
/* Given the name of a router, determine if that router exists in the 			*/ 
/* routing table. If so, return a pointer to the router's entry. Else if the 	*/ 
/* router is not already in the routing table, add it and return a pointer  	*/ 
/* to it 																		*/ 
/********************************************************************************/ 
linkInfo* LookUpRouter(char* router)
{
	int i = 0; 

	if(DEBUG)
	{
		printf("\nLooking for router %s in my routing table... There are currently %d entries", router, linkcount); 
	}

	linkInfo* tmpLink = &linkInfoTable[i]; 

	for(i=0; i < linkcount - 1; i++)
	{
		printf("\nLooking at linkInfoTable[%d]", i); 
		tmpLink = &linkInfoTable[i]; 

		printf("\n 1"); 
		if(tmpLink == 0)
			break; 				// exit the for loop 

		printf(" 2"); 
		if(tmpLink->cost == 0 || tmpLink->router == 0)
			break; 				// exit the for loop

		printf(" 3"); 
		if(strncmp(tmpLink->router, router, 1) == 0)
		{
			printf(" 3.1"); 
			if(DEBUG)
			{
				printf("\nRouter %s is already in my routing table at linkInfoTable[%d]", router,i); 
			}
			return &linkInfoTable[i]; 
		}


	}

	/****************************************************/ 
	/* We've looked through all of our routing table 	*/ 
	/* and didn't find an entry for the provided router */ 
	/* So, we add it to the routing table and return 	*/ 
	/* a pointer to the newly added entry				*/ 
	/****************************************************/ 
	if(DEBUG)
		printf("\nFailed to find entry for router in routing table... Adding an entry for Router %s at linkInfoTable[%d]", router, i); 
	
	printf(" 4"); 
	strncpy(linkInfoTable[i+1].router, router, 1); 
	linkcount += 1; 

	printf("\nTesting for successful insert of Router %s", linkInfoTable[i+1].router); 

	return &linkInfoTable[i+1];


}






