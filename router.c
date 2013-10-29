#include <stdio.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "readrouters.c"
#include "DieWithMessage.c"

static const int MAXPENDING = 5;                 // Maximum outstanding connection requests 

static const int DEBUG = 1; 

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

void CloseAllSockets()
{
	int i = 0; 
	while(i < MAXPAIRS)
	{
		neighborSocket* neighbor = &neighborSocketArray[i]; 

		if(neighbor->neighbor == 0)
			break; 

		close(neighbor->socket); 


		i++; 
	}
}


int main(int argc, char* argv[])
{
	char* directory; 					// directory to look for input files 
	char* router;						// name of this router 
	routerInfo* routerConfiguration; 	// name, host, baseport 
	linkInfo* routerLinks; 				// name, cost, local links, remote links		
	neighborSocket* neighbors; 			// name, socket 
	ssize_t numBytes = 0; 
	in_port_t receivingPort; 			// port for receiving messages 
	int receivingSocket;                 // Socket descriptor 
	fd_set rfds;						// set of file descriptors for select()
    struct timeval tv;					// timeout boundary 
    int retval;							// return value of select()
    struct sockaddr_in neighborAddr;    // buffer for addresses neighboring router
    int neighborSock; 					// socket for neighboring router 
    socklen_t neighborAddrLength = sizeof(neighborAddr);	// Set length of client address structure (in-out parameter)

	char messageBuffer[1024]; 

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
	

	// Read all routers in the system 
	routerConfiguration = readrouters(directory); 

	// Read the links for this router 
	routerLinks = readlinks(directory, router); 

	// Should I initialize my routing table here? 

	// Create connected datagram sockets for talking to neighbors
	// and provide us an array of (neighbor, socket) pairs
	// createConnections takes care of 
	// 		(1) socket
	// 		(2) bind
	// 		(3) connect 
	neighbors = createConnections(router); 

	if(DEBUG)
	{


		printf("\nSuccessfully constructed datagram sockets for each of my %d neighbors...", count); 

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

			// This is kind of a hacky way to find my baseport but it works... 
			if(strncmp(routerConfiguration->router, router, 1) == 0)
			{
				if(DEBUG)
				{
					// Set our baseport so we can bind a socket to it later 
					printf("\nSet my baseport to %d\n", routerConfiguration->baseport); 
				}
				
			}
			
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


			i++; 
		}
	}

	// Setup the receiving socket. Bind it to the local baseport
	// to receive L and P messages (but not U messages) 
   	// Create socket for incoming connections 

    if((receivingSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        DieWithSystemMessage("socket() failed"); 
    printf("\nSuccessfully created receiving socket (#%d)", receivingSocket); 

   	// Construct local address structure 
    struct sockaddr_in myAddr;                                         // local address
    memset(&myAddr, 0, sizeof(myAddr));                 			// zero out structure 
    myAddr.sin_family = AF_INET;                                      // IPV4 address family 
    myAddr.sin_addr.s_addr = htonl(INADDR_ANY);         				// any incoming interface 
    myAddr.sin_port = htons(receivingPort);                         // local port 

    // Bind to the local address 
    if(bind(receivingSocket, (struct sockaddr*) &myAddr, sizeof(myAddr)) < 0)
        DieWithSystemMessage("bind() failed"); 

    printf("\nSuccessfully bound to local address..."); 

    // Mark the socket so it will list for incoming connections 
    if(listen(receivingSocket, MAXPENDING) < 0)
        DieWithSystemMessage("listen() failed"); 

    printf("\nSuccessfully marked socket for listening..."); 


    



	for(;;)			// Run forever 
	{
		// Recycle these variables?
		routerConfiguration = &routerInfoTable[0]; 
		routerLinks = &linkInfoTable[0]; 

		// zero out the buffer, start from scratch 
		// memset(&neighborSocketArray, 0, sizeof(neighborSocketArray)); 
		// neighbors = createConnections(router); 


		printf("\n--\n--"); 
		printf("\nWaiting for updates from fellow routers in my network..."); 

        FD_ZERO(&rfds);			// clear the file descriptor set  
        FD_SET(0, &rfds);		// add a file descriptor to the set 

   		/* Wait up to 30 seconds. */
   		tv.tv_sec = 5;
       	tv.tv_usec = 0;

	   	retval = select(receivingSocket, &rfds, NULL, NULL, &tv);

       	if(retval == -1)
       		printf("\nselect() error"); 
       	else if(retval == 0)
       		printf("\nNone of my neighbors have updates for me..."); 
       	else if(retval)
       		printf("\nData is available now from %d socket(s).", retval); 

       	// Look through the existing connections to see if any of the 
       	// sockets have data 
    //    	for(i=0; i < MAXROUTERS; i++)
    //    	{
    //    		printf("\n%d", i); 
    //    		if(FD_ISSET(i, &rfds))
    //    		{
    //    			if(DEBUG)
    //    			{
    //    				printf("\nSocket #%d got somethin' to say!", i); 
    //    			}

   	// 			neighborAddrLength = sizeof(neighborAddr); 

   	// 			if((neighborSock = accept(receivingSocket, (struct sockaddr *)&neighborAddr, &neighborAddrLength)) < 0)
				// {
				// 	printf("\naccept failed()"); 
				// }
				// else
				// {
				// 	if(DEBUG)
				// 	{
				// 		printf("\nSuccessfully accepted neighborSock"); 
				// 	}
				// }
    //    		}
    //    	}

  //      	if((neighborSock = accept(receivingSocket, (struct sockaddr *)&neighborAddr, &neighborAddrLength)) < 0)
		// {
		// 	printf("\naccept failed()"); 
		// }
		// else
		// {
		// 	if(DEBUG)
		// 	{
		// 		printf("\nSuccessfully accepted neighborSock"); 
		// 	}
		// }


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
			numBytes = send(neighborSock, messageBuffer, sizeof(messageBuffer), 0); 

            if(numBytes < 0)
                DieWithSystemMessage("send() failed\n"); 
        	else if(numBytes != sizeof(messageBuffer))
                DieWithUserMessage("send()", "sent unexpected number of bytes"); 

            printf("\nSuccessfully sent a %zu byte update message to %s: %s\n", numBytes, dest, messageBuffer); 
            

            //CloseAllSockets(); 

            if(DEBUG)
            	printf("\nClosed socket #%d (Router %s)\n", neighborSock, dest); 

		}
       

  //      int i = 0; 

  //      for(i = 0; i < MAXPAIRS; i++)
  //      {
  //      			neighborSocket* neighbor = &neighborSocketArray[i]; 

  //      			if(neighbor->neighbor == 0)
		// 			break; 

		// 		// connect() 

  //      			// Receive datagrams in the format of: 
  //      			// 		- Router update messages: U dest cost
  //      			// 		- Link cost messages: L neighbor cost 


  //      			// Make appropriate changes to the routing table
  //      			// If there are changes, send the changes to neighbors
  //      			// using U-messages (known as a Triggered Update)

  //      			// Print an output message

  //      			// Send datagram updates to neighbors using U messages 
  //      			// 		- Router update messages: U dest cost 
  //      			// 		- Link cost message: L neighbor cost 
  //      			// Do I just create TCP/IP sockets like in Project 1 to send them? 
  //      			// Use createSockets to construct connected datagram sockets
		// 		// for talking to neighbors 



		// 		printf("\n%d.", i + 1); 
		// 		printf("\nNeighbor: %s", neighbor->neighbor); 
		// 		printf("\nSocket: %d", neighbor->socket); 
		// 		printf("\n"); 

				

				
		    
		// }

		// printf("\nFinished updating!"); 


		// Send your data to your neighbors 
		// Each router must send its entire routing table to its neighbors every 30 seconds 
	}



	
	printf("\nPress any key to exit..."); 
	char ch = getchar(); 

	
	return 0; 
}
