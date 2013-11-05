#include <stdio.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "readrouters.c"
#include "DieWithMessage.c"

static const int MAXPENDING = 5;                 // Maximum outstanding connection requests 

static const int DEBUG = 1; 

static int MAX_DESCRIPTOR = 0; 				// Highest file descriptor 

static int connectedNeighborSocket[MAXPAIRS]; 

// Given a neighboring router's name, look up and return that router's 
// (neighbor, socket) combination 
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

// Given a router's name, look up and return that router's configuration
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


int main(int argc, char* argv[])
{
	/* Directory to look for input files */ 
	char* directory; 	

	/* Name of this router instance */ 				
	char* router;		

	/* temp set of file descriptors for select() */ 	
	fd_set rfds;		

	/* master file descriptor set */ 
	/* will be shadow copied into rfds on each iteration */ 
	fd_set masterFD_Set; 

	/* array of all file descriptors */ 
	 int servSock[MAXROUTERS]; 

	struct timeval tv;					// timeout boundary 
	int retval;							// return value of select()
    int readyDescriptors = 0; 			// number of descriptors with available data 
	char messageBuffer[1024]; 
	ssize_t numBytes = 0; 

	routerInfo* routerConfiguration; 	// name, host, baseport 
	linkInfo* routerLinks; 				// name, cost, local links, remote links		
	neighborSocket* neighbors; 			// name, socket 
	
	in_port_t receivingPort; 			// port for receiving messages 
	int receivingSocket;                 // Socket descriptor 
    
    struct sockaddr_in neighborAddr;    // buffer for addresses neighboring router
    int neighborSock; 					// socket for neighboring router 
    socklen_t neighborAddrLength = sizeof(neighborAddr);	// Set length of client address structure (in-out parameter)
   

	int i = 0; 

	if(argc != 3)
	{
		printf("Invalid number of arguments!\n"); 
		DieWithUserMessage("Parameters", "<Directory of configuration files> <Single-letter router name corresponding to an entry in the list of routers>"); 
		return 0; 
	}

	printf("Welcome to the Distance-Vector Routing Program!\n"); 

	if(DEBUG)
	{
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
	}
	

	/************************************/ 
	/* 	Read all routers in the network */ 
	/************************************/ 
	routerConfiguration = readrouters(directory); 

	/************************************/ 
	/* Read the links for this router 	*/  
	/************************************/ 
	routerLinks = readlinks(directory, router); 

	// Should I initialize my routing table here or am I good to go?  

	/****************************************************************/ 
	/* Create connected datagram sockets for talking to neighbors  	*/ 
	/* and provide us an array of (neighbor, socket) pairs			*/ 
	/* createConnections takes care of 								*/ 
	/* 		(1) socket 												*/ 
	/* 		(2) bind 												*/ 
	/* 		(3) connect 											*/ 
	/* Which means we (as a client) need to take care of: 						*/ 
	/* 		(1) send/recv 											*/ 
	/****************************************************************/ 
	neighbors = createConnections(router); 

	/************************************/ 
	/* 	Initialize the master fd_set 	*/ 
	/************************************/ 
	FD_ZERO(&masterFD_Set);			// clear the master file descriptor set 
	FD_ZERO(&rfds);					// clear the temp file descriptor set  
	FD_SET(0, &masterFD_Set); 		// add file descriptor 0 for keyboard to masterFD_Set


	if(DEBUG)
	{
		printf("\nSuccessfully constructed datagram sockets for each of my %d neighbors...", count); 
		printf("\nRouters in the system: ");
	}

	while(i < MAXROUTERS)
	{
		
		routerConfiguration = &routerInfoTable[i];
		if(routerConfiguration->baseport == 0)
			break; 

		if(DEBUG)
		{
			printf("\n%d. ", i + 1); 
			printf("\nRouter: %s", routerConfiguration->router); 
			printf("\nHost: %s", routerConfiguration->host); 
			printf("\nBase port: %d", routerConfiguration->baseport); 
			printf("\n");
		}
		 

		// This is kind of a hacky way to find my baseport but it works... 
		if(strncmp(routerConfiguration->router, router, 1) == 0)
		{
			if(DEBUG)
			{
				// Set our baseport so we can bind a socket to it later 
				printf("\nMy name is %s and my host is %s", routerConfiguration->router, routerConfiguration->host); 
				printf("\nSet my baseport to %d\n", routerConfiguration->baseport); 
			}
			
		}
		
		i++; 
	}

	if(DEBUG)
		printf("\nConnections: "); 
	i = 0; 
	while(i < MAXLINKS)
	{
		routerLinks = &linkInfoTable[i]; 

		if(routerLinks->cost == 0)
			break; 

		if(DEBUG)
		{
			printf("\n%d.", i + 1); 
			printf("\nRouter: %s", routerLinks->router); 
			printf("\nCost: %d", routerLinks->cost); 
			printf("\nLocal link: %d", routerLinks->locallink); 
			printf("\nRemote link: %d", routerLinks->remotelink); 
			printf("\n"); 
		}
		
		i++; 

	}

	if(DEBUG)
	{
		printf("\nNeighbor sockets: "); 
	}
	
	i = 0; 
	while(i < MAXPAIRS)
	{
		neighbors = &neighborSocketArray[i]; 



		

		if(neighbors->neighbor == 0)
		{
			servSock[i] == 0; 
			break; 
		}

		/**********************************************/ 
		/* Fill the servSock[] array with the sockets */ 
		/**********************************************/ 
		servSock[neighbors->socket] = neighbors->socket; 
		FD_SET(neighbors->socket, &masterFD_Set); 	// add the socket file descriptor to the master FD set
		MAX_DESCRIPTOR = neighbors->socket; 
		

		if(DEBUG)
		{
			printf("\n%d.", i + 1); 
			printf("\nNeighbor: %s", neighbors->neighbor); 
			printf("\nSocket: %d", neighbors->socket); 
			printf("\nAdded socket %d to master file descriptor set", neighbors->socket); 
			printf("\nUpdated MAX_DESCRIPTOR to %d", MAX_DESCRIPTOR); 
			printf("\n"); 
		}


		i++; 
	}

	/***********************************************/ 
	/* Print out all the sockets in servSock array */ 
	/***********************************************/ 
	if(DEBUG)
	{
		for(i=0; i < MAX_DESCRIPTOR + 1; i++)
		{
			printf("\nservSock[%d] : %d", i, servSock[i]); 
		}
	}


	
	// Do we need to setup the receiving socket? 
	// Setup the receiving socket. Bind it to the local baseport
	// to receive L and P messages (but not U messages) 
	// That means we have to: 
	// 		(1) socket() to create the socket 
	// 		(2) bind() the socket to a local address
	// 		(3) listen() for connections 
	// 		(4) accept() a connection 
   	// Create socket for incoming connections 



	for(;;)			// Run forever 
	{
		// Do I need to setup the router for receiving within my infinite loop?
		// Do I need to invoke createConnections within my infinite loop? 

		printf("\nThere are %d connections in the neighborSocketArray", count); 


		printf("\n--\n--"); 
		printf("\nWaiting for updates from fellow routers in my network..."); 

		/*************************************************/ 
        /* Copy the masterFD_Set over to the temp FD set */ 
        /*************************************************/
        FD_ZERO(&rfds);			// clear the temp file descriptor set  

        for(i=0; i < MAX_DESCRIPTOR + 1; i++)
        {
        	if(servSock[i] != 0)
        	{
        		FD_SET(servSock[i], &rfds); 

        		if(DEBUG)
        			printf("\nAdded FD #%d to temporary file descriptor set", servSock[i]); 
        	}
        	
        }


   		/* Wait up to 30 seconds. */
   		tv.tv_sec = 30;
       	tv.tv_usec = 0;

       	if(DEBUG)
       		printf("\nWaiting on select()..."); 

	   	retval = select(MAX_DESCRIPTOR + 1, &rfds, NULL, NULL, &tv);

       	if(retval < 0)
       	{
       		printf("select() failed"); 
       		continue; 		// exit the loop 
       	}
       	else if(retval == 0)
       	{
       		printf("select() timed out"); 
       		continue; 		// exit the loop 
       	}
       	else if(retval)
       		printf("Data is available now from %d socket(s).", retval);

       	/****************************************************/ 
       	/* One or more of the descriptors have data for us 	*/ 
       	/* We must determine which ones they are. 			*/ 
       	/****************************************************/ 
       	readyDescriptors = retval; 

       	for(i=0; i <= MAX_DESCRIPTOR && readyDescriptors > 0; ++i)
       	{
       		if(DEBUG)
       			printf("\nChecking to see if FD %d has data for us", i); 
       		/****************************************************/ 
       		/* Check to see if this descriptor is ready 			*/ 
       		/****************************************************/ 
       		if(FD_ISSET(servSock[i], &rfds))
       		{
       			if(DEBUG)
       			{
       				printf("\nFD #%d has data for us!", i); 
       			}
       			/****************************************************/ 
       			/* A descriptor was found that has data for us 		*/
       			/* One less has to be looked for, so we decrement 	*/
       			/* the readyDescriptors variable 					*/   
       			/****************************************************/ 
       			readyDescriptors--; 

       			/****************************************************/ 
       			/* Receive all incoming data from this socket 		*/
       			/* before we loop back and call select() again 		*/    
       			/****************************************************/ 
       			numBytes = recv(i, messageBuffer, sizeof(messageBuffer), 0); 

       			if(numBytes < 0)
                    DieWithSystemMessage("recv() failed\n"); 
                else if(numBytes == 0)
                    DieWithUserMessage("recv()", "connection closed prematurely\n"); 

                if(DEBUG)
                	printf("\nReceived %zu bytes from socket #%d", numBytes, i);

                /****************************************************/ 
       			/* Process the received updates 					*/    
       			/****************************************************/ 

       			/********************************************************/ 
       			/* Send an update message to this router via its socket	*/  
       			/********************************************************/ 
       			numBytes = send(i, messageBuffer, sizeof(messageBuffer), 0); 

	            if(numBytes < 0)
	                DieWithSystemMessage("send() failed"); 
	        	else if(numBytes != sizeof(messageBuffer))
	                DieWithUserMessage("send()", "sent unexpected number of bytes"); 

	            printf("\nSuccessfully sent a %zu byte update message to socket #%d: %s\n", numBytes, i, messageBuffer); 

       		}
       		
       	}


       	i = 0; 
       
       	// for each of my neighboring connections
       	// 		- recv some messages  
        for(i=0; i< MAXROUTERS; i++)		// count = number of neighbor routers with connections 
		{
			linkInfo* routerLink = &linkInfoTable[i]; 

			// Check to see if the router deserves an update 
			if(routerLink->router == 0)
				continue; 

			printf("\a"); 
			// Need neighborSocket for socket # 
			neighborSocket* neighbor = GetNeighborSocket(routerLink->router); 

			if(neighbor->neighbor == 0)
				continue; 
			else
			{
				neighborSock = neighbor->socket; 

				if(DEBUG)
				{
					printf("\nNeighbor: %s", neighbor->neighbor); 
					printf("\nSocket: %d", neighbor->socket); 
					printf("\nneighborSock: %d", neighborSock); 
				}
				
			}

			// Need routerInfo for baseport 
			routerInfo* router = GetRouterInfo(neighbor->neighbor); 


			// Setup the neighborAddr 
			memset(&neighborAddr, 0, sizeof(neighborAddr));                 			// zero out structure 
		    neighborAddr.sin_family = AF_INET;                                      // IPV4 address family 
		    neighborAddr.sin_addr.s_addr = htonl(INADDR_ANY);         				// any incoming interface 
		    neighborAddr.sin_port = htons(router->baseport);  
		    neighborAddrLength = sizeof(neighborAddr);               

		    printf("\nneighborSock: %d", neighborSock);
		    printf("\nneighborAddr.sin_port: %d", neighborAddr.sin_port); 
		    printf("\nneighborAddrLength: %d", neighborAddrLength); 


			// Put the table entries into the message buffer 
			char* dest = routerLink->router; 
			int cost = routerLink->cost;

			memset(&messageBuffer, 0, sizeof(messageBuffer)); 

			snprintf( messageBuffer, sizeof(messageBuffer), "U %C %d", dest[0], cost);


			// for each of the connections, receive: 
			// 		- Router update messages: U dest cost 
			// 		- Link cost messages: L neighbor cost 
			// If neighborSock = -1 then connect() failed above 
			printf("\nAttempting to send an update message...");
			// numBytes = send(neighborSock, messageBuffer, sizeof(messageBuffer), 0); 

   //          if(numBytes < 0)
   //              DieWithSystemMessage("send() failed"); 
   //      	else if(numBytes != sizeof(messageBuffer))
   //              DieWithUserMessage("send()", "sent unexpected number of bytes"); 

   //          printf("\nSuccessfully sent a %zu byte update message to %s: %s\n", numBytes, dest, messageBuffer); 
            

            // Should I close all sockets before re-entering the infinite loop?
            //CloseAllSockets(); 

            if(DEBUG)
            	printf("\nClosed socket #%d (Router %s)\n", neighborSock, dest); 

		}
       
	}



	
	printf("\nPress any key to exit..."); 
	char ch = getchar(); 

	
	return 0; 
}
