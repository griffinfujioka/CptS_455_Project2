#include <stdio.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "readrouters.c"
#include "DieWithMessage.c"

int main(int argc, char* argv[])
{
	char* directory; 
	char* router;
	routerInfo* routerConfiguration; 
	linkInfo* routerLinks; 
	routerInfo* tmp;
	neighborSocket* neighbors; 

	int i = 0; 

	if(argc != 3)
	{
		printf("Invalid number of arguments!\n"); 
		DieWithUserMessage("Parameters", "<Directory of configuration files> <Single-letter router name corresponding to an entry in the list of routers>"); 
		return 0; 
	}

	printf("Welcome to the Distance-Vector Routing Program!\n"); 

	if(argv[1] != 0)
	{
		printf("\nDirectory: %s", argv[1]);
		directory = argv[1];  
	}

	if(argv[2] != 0)
	{
		printf("\nRouter name: %s", argv[2]); 
		router = argv[2]; 
	}

	// Read all routers in the system 
	routerConfiguration = readrouters(directory); 

	// Read the links for this router 
	routerLinks = readlinks(directory, router); 

	// Create connected datagram sockets for talking to neighbors
	// and provide us an array of (neighbor, socket) pairs



	neighbors = createConnections(router); 

	printf("\nRouters in the system: ");

	while(i < MAXROUTERS)
	{
		
		routerConfiguration = &routerInfoTable[i];
		if(routerConfiguration->baseport == 0)
			break; 

		printf("\n%d. ", i + 1); 
		printf("\nRouter: %s", routerConfiguration->router); 
		printf("\nHost: %s", routerConfiguration->host); 
		printf("\nBase port: %d", routerConfiguration->baseport); 
		printf("\n"); 
		i++; 
	}

	printf("\nConnections: "); 
	i = 0; 
	while(i < MAXLINKS)
	{
		routerLinks = &linkInfoTable[i]; 

		if(routerLinks->cost == 0)
			break; 

		printf("\n%d.", i + 1); 
		printf("\nRouter: %s", routerLinks->router); 
		printf("\nCost: %d", routerLinks->cost); 
		printf("\nLocal link: %d", routerLinks->locallink); 
		printf("\nRemote link: %d", routerLinks->remotelink); 
		printf("\n"); 
		i++; 

	}

	printf("\nNeighbor sockets: "); 
	i = 0; 
	while(i < MAXPAIRS)
	{
		neighbors = &neighborSocketArray[i]; 

		if(neighbors->neighbor == 0)
			break; 

		printf("\n%d.", i + 1); 
		printf("\nNeighbor: %s", neighbors->neighbor); 
		printf("\nSocket: %d", neighbors->socket); 
		printf("\n"); 

		// Use createSockets to construct connected datagram sockets
		// for talking to neighbors 


		i++; 

	}


	for(;;)			// Run forever 
	{
		printf("\nWaiting for updates from fellow routers in my network..."); 

		fd_set rfds;			// a set of file descriptors 
        struct timeval tv;		// timeout boundary 
        int retval;

        FD_ZERO(&rfds);			// clear the set 
        FD_SET(0, &rfds);		// add a file descriptor to the set 

       /* Wait up to 30 seconds. */
       tv.tv_sec = 30;
       tv.tv_usec = 0;

       // Check which routers have updates for you, 
       // I.e., their sockets contain data 


       retval = select(1, &rfds, NULL, NULL, &tv);

       if(retval == -1)
       	printf("\nNone of my neighbors have updates for me..."); 
       else if(retval)
       	printf("\nData is available now from %d socket(s).", retval); 
       

       

       int i = 0; 

       for(i = 0; i < MAXPAIRS; i++)
       {
       		if(FD_ISSET(i, &rfds))
       		{
       			// This socket has data to share 
       			neighborSocket* neighbor = &neighborSocketArray[i]; 
       		}	
       } 

       //printf("\nNeighbor %s has something to say.", neighbor->neighbor); 



		// Send your data to your neighbors 
		// Each router must send its entire routing table to its neighbors every 30 seconds 

		// Use select to find out which socket descriptors have available messages, 
		// issue recv() calls only for those descriptors

	}

	



	
	return 0; 
}
