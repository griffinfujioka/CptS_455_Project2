#include <stdio.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include "readrouters.c"
#include "DieWithMessage.c"

static const int MAXPENDING = 5;                 // Maximum outstanding connection requests 

static const int DEBUG = 1; 

static int MAX_DESCRIPTOR = 0; 				// Highest file descriptor 

static int MAX_MESSAGE_SIZE = 1024; 

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
/* Given a socket number 								*/ 
/* send your routing table to that socket    			*/ 
/********************************************************/
void SendRoutingTable(int socket)
{
	int j = 0; 
	char message[24]; 
	char receiverName[1]; 		// The router receiving of this message
	linkInfo* routerLink; 

	char* tmpName = GetRouterName(socket);
	strncpy(receiverName, tmpName, 1); 
	receiverName[1] = '\0'; 

	printf("\nAttempting to send routing table to %s (socket #%d)", receiverName, socket); 


	/* Iterate through your routing table sending 	*/ 
	/* each row individually to neighbor->socket 	*/ 
	/* as a U message 								*/ 
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

		snprintf( message, sizeof(message), "U %C %d", dest[0], cost);


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

         printf("\nSuccessfully sent a %zu byte update message on socket #%d: %s\n", numBytes, socket, message); 
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
	char messageBuffer[MAX_MESSAGE_SIZE]; 
	ssize_t numBytes = 0; 

	routerInfo* routerConfiguration; 	// name, host, baseport 
	linkInfo* routerLinks; 				// name, cost, local links, remote links		
	neighborSocket* neighbors; 			// name, socket 
	
	in_port_t receivingPort; 			// port for receiving messages 
	int receivingSocket;                 // Socket descriptor 
    
    struct sockaddr_in neighborAddr;    // buffer for addresses neighboring router
    int neighborSock; 					// socket for neighboring router 
    socklen_t neighborAddrLength = sizeof(neighborAddr);	// Set length of client address structure (in-out parameter)
    char neighborName[1]; 

    int on = 1; 
   

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

	// Do we need to setup the receiving socket? 
	// Setup the receiving socket. Bind it to the local baseport
	// to receive L and P messages (but not U messages) 
	// That means we have to: 
	// 		(1) socket() to create the socket 
	// 		(2) bind() the socket to a local address
	// 		(3) listen() for connections 
	// 		(4) accept() a connection 

	/********************************************/ 
   	/* Create socket for incoming connections 	*/ 
   	/********************************************/ 
   	if((receivingSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        DieWithSystemMessage("socket() failed"); 

    if(DEBUG)
    	printf("\nCreated socket %d for receiving...", receivingSocket); 

	/*************************************************************/
   	/* Allow socket descriptor to be reuseable                   */
   	/*************************************************************/
   	if(retval = setsockopt(receivingSocket, SOL_SOCKET,  SO_REUSEADDR,
                   (char *)&on, sizeof(on)) < 0)
   	{
    	perror("setsockopt() failed");
      	close(receivingSocket);
      	exit(-1);
   }

   	/********************************************/ 
   	/* Set socket to be non-blocking		 	*/ 
   	/********************************************/ 
	if(retval = ioctl(receivingSocket, FIONBIO, (char *)&on) < 0)
		DieWithSystemMessage("ioctl() failed"); 

    struct sockaddr_in servAddr;                                         // local address
    memset(&servAddr, 0, sizeof(servAddr));                 // zero out structure 
    servAddr.sin_family = AF_INET;                                        // IPV4 address family 
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);         // any incoming interface 
    servAddr.sin_port = htons(receivingPort);   

    /********************************************/ 
   	/* Bind to the local address 			 	*/ 
   	/********************************************/ 
    if(bind(receivingSocket, (struct sockaddr*) &servAddr, sizeof(servAddr)) < 0)
        DieWithSystemMessage("bind() failed"); 

    /********************************************/ 
   	/* Mark the socket so it will listen for 	*/ 
   	/* incoming connections 				 	*/ 
   	/********************************************/ 
    if(listen(receivingSocket, MAXROUTERS) < 0)
        DieWithSystemMessage("listen() failed");  

	/************************************/ 
	/* 	Initialize the master fd_set 	*/ 
	/************************************/ 
	FD_ZERO(&masterFD_Set);			// clear the master file descriptor set 
	MAX_DESCRIPTOR = receivingSocket;
	FD_SET(receivingSocket, &masterFD_Set); 		// add file descriptor 0 for keyboard to masterFD_Set


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
			receivingPort = routerConfiguration->baseport;
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
		if(neighbors->socket > MAX_DESCRIPTOR)
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

		printf("\nHighest file descriptor = %d", MAX_DESCRIPTOR); 
	}

	/* Wait up to 30 seconds. */
   	tv.tv_sec = 30;
    tv.tv_usec = 0;



	for(;;)			// Run forever 
	{
		// Do I need to setup the router for receiving within my infinite loop?
		// Do I need to invoke createConnections within my infinite loop? 

		printf("\nThere are %d connections in the neighborSocketArray", count); 


		printf("\n--\n--"); 
		printf("\nWaiting for updates from fellow routers in my network..."); 

		

		/************************************************/
		/* Zero out the messageBuffer and the tv		*/ 
		/************************************************/
		memset(messageBuffer, 0, MAX_MESSAGE_SIZE); 



		/*************************************************/ 
        /* Copy the masterFD_Set over to the temp FD set */ 
        /*************************************************/
        FD_ZERO(&rfds);			// clear the temp file descriptor set 
		FD_SET(0, &rfds);
		FD_SET(receivingSocket, &rfds); 
        memcpy(&rfds, &masterFD_Set, sizeof(masterFD_Set));  


        /****************************************************/ 
        /* This seems like a good place to setup triggered 	*/ 
        /* update. That is, be prepared to receive updates	*/ 
        /* from neighboring routers, update your routing 	*/ 
        /* table and send updates to your neighbors 		*/ 
        /****************************************************/ 


        /****************************************************/ 
        /* This for-loop ensures that update messages 		*/ 
        /* will be sent to neighboring routers every 30s 	*/ 
        /****************************************************/ 
        for(i=0; i < MAX_DESCRIPTOR + 1; i++)
        {
        	/******************************************************/ 
        	/* If servSock[i] != 0, then this socket is connected */ 
        	/* and we should send updates to it 				  */ 
        	/******************************************************/
        	if(servSock[i] != 0)
        	{
        		
        		

	        	/* Wait up to 30 seconds. */
		   		tv.tv_sec = 20;
		       	tv.tv_usec = 0;

		       	if(DEBUG)
		       	{
		       		printf("\nselect() will wait for %zu seconds", tv.tv_sec); 
		       		printf("\nWaiting on select()...");
		       	}
		       		


			   	retval = select(MAX_DESCRIPTOR + 1, &rfds, NULL, NULL, &tv);

		       	if(retval < 0)
		       	{
		       		printf("select() failed"); 
		       		//continue; 		// exit the loop 
		       	}
		       	else if(retval == 0)
		       	{
		       		printf("select() timed out"); 
		       		//continue; 		// exit the loop 
		       	}
		       	else if(retval)
		       		printf("Data is available now from %d socket(s).", retval);

		       	/* Send your entire routing table to this socket */ 
        		SendRoutingTable(servSock[i]); 
	       }

        	
        	
        }


       	/************************************************/
		/* Zero out the messageBuffer again				*/ 
		/************************************************/
		memset(messageBuffer, 0, MAX_MESSAGE_SIZE); 

       	/****************************************************/ 
       	/* One or more of the descriptors have data for us 	*/ 
       	/* We must determine which ones they are. 			*/ 
       	/****************************************************/ 
       	readyDescriptors = retval; 

       	for(i=0; i <= MAX_DESCRIPTOR && readyDescriptors > 0; i++)
       	{
       		if(DEBUG)
       		{
       			printf("\nChecking to see if FD %d has data for us", i); 
       		}
       			
       		/****************************************************/ 
       		/* Check to see if this descriptor is ready 			*/ 
       		/****************************************************/ 
       		if(FD_ISSET(servSock[i], &rfds))
       		{
       			if(DEBUG)
       			{
       				printf("\nFD #%d has data for us! servSock[%d] = %d", i, i, servSock[i]); 
       			}

       			if(servSock[i] == 0)
       				break; 
       			/****************************************************/ 
       			/* A descriptor was found that has data for us 		*/
       			/* One less has to be looked for, so we decrement 	*/
       			/* the readyDescriptors variable 					*/   
       			/****************************************************/ 
       			readyDescriptors--; 

       			// numBytes = 1;
       			// do
       			// {
       			// 	numBytes = recv(servSock[i], messageBuffer, sizeof(messageBuffer), 0); 

       			// 	if(numBytes < 0)
       			// 	{
       			// 		printf("\nrecv() failed"); 
       			// 		break;  
       			// 	}
       			// 	else if(numBytes == 0)
       			// 	{
       			// 		DieWithUserMessage("\nrecv()", "connection closed prematurely"); 
       			// 		break; 
       			// 	}

       			// 	/********************************************************/ 
       			// 	 Now that we know which socket the update came from	 
       			// 	/* we can determine which neighbor the update came from */ 
       			// 	/********************************************************/ 
       			// 	char* tempName = GetRouterName(servSock[i]); 
       			// 	strncpy(neighborName, tempName, 1); 


       			// 	if(DEBUG)
       			// 	{
       			// 		printf("\n%d: Received %zu bytes from socket #%d (Router %s)", i, numBytes, servSock[i], neighborName); 
       			// 		printf("\nReceived message: %s", messageBuffer); 
       			// 	}

       				

       			// } while(numBytes >= 0); 

   				/****************************/ 
       			/* 		Version 2 			*/ 
       			/****************************/ 
       			numBytes = recv(servSock[i], messageBuffer, sizeof(messageBuffer), 0); 

   				if(numBytes < 0)
   				{
   					printf("\nrecv() failed from socket #%d", servSock[i]); 
   					break;
   					 
   				}
   				else if(numBytes == 0)
   				{
   					printf("\nrecv()", "connection closed prematurely"); 
   					break;
   				}

   				char* tempName = GetRouterName(servSock[i]); 
   				strncpy(neighborName, tempName, 1); 


   				if(DEBUG)
   				{
   					printf("\n%d: Received %zu bytes from socket #%d (Router %s)", i, numBytes, servSock[i], neighborName); 
   					printf("\nReceived message: %s", messageBuffer); 
   				}

       			
       			/*************************************************/
               /* Accept all incoming connections that are      */
               /* queued up on the listening socket before we   */
               /* loop back and call select again.              */
               /*************************************************/
               // do
               // {
               //    /**********************************************/
               //    /* Accept each incoming connection.  If       */
               //    /* accept fails with EWOULDBLOCK, then we     */
               //    /* have accepted all of them.  Any other      */
               //    /* failure on accept will cause us to end the */
               //    /* server.                                    */
               //    /**********************************************/
               //    if(neighborSock = accept(receivingSocket, NULL, NULL) < 0)
               //    {
               //    	if (errno != EWOULDBLOCK)
               //       {
               //          perror("  accept() failed");
               //       }
               //       break;
               //    }

                  

               //    /**********************************************/
               //    /* Loop back up and accept another incoming   */
               //    /* connection                                 */
               //    /**********************************************/
               // } while (neighborSock != -1);

            	/**********************************************/
              /* Add the new incoming connection to the     */
              /* master read set                            */
              /**********************************************/
          //     printf("\nNew incoming connection: socket #%d\n", neighborSock);
          //     FD_SET(neighborSock, &rfds);
          //   if (neighborSock > MAX_DESCRIPTOR)
          //        MAX_DESCRIPTOR = neighborSock;

          //      printf("\nAccepted connection from neighbor socket #%d", neighborSock); 

       			// // /****************************************************/ 
       			// // /* Receive all incoming data from this socket 		*/
       			// // /* before we loop back and call select() again 		*/    
       			// // /****************************************************/ 
       			// numBytes = recv(servSock[i], messageBuffer, sizeof(messageBuffer), 0); 

       			// if(numBytes < 0)
          //           DieWithSystemMessage("recv() failed\n"); 
          //       else if(numBytes == 0)
          //           DieWithUserMessage("recv()", "connection closed prematurely\n"); 

          //       if(DEBUG)
          //       	printf("\nReceived %zu bytes from socket #%d", numBytes, i);

          //       ************************************************** 
       			// /* Process the received updates 					*/    
       			// /****************************************************/ 

       			// /********************************************************/ 
       			// /* Send an update message to this router via its socket	*/  
       			// /********************************************************/ 
       			// numBytes = send(i, messageBuffer, sizeof(messageBuffer), 0); 

	         //    if(numBytes < 0)
	         //        DieWithSystemMessage("send() failed"); 
	        	// else if(numBytes != sizeof(messageBuffer))
	         //        DieWithUserMessage("send()", "sent unexpected number of bytes"); 

	         //    printf("\nSuccessfully sent a %zu byte update message to socket #%d: %s\n", numBytes, i, messageBuffer); 

       		}
       		
       	}

       	printf("\nIterated through all possible routers.");
       
	}



	
	printf("\nPress any key to exit..."); 
	char ch = getchar(); 

	
	return 0; 
}
