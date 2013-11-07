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


/* Send a test message to the socket parameter */ 
void SendTestMessage(int socket)
{
	
	char receiverName[1]; 
    char* tmpName = GetRouterName(socket);
	strncpy(receiverName, tmpName, 1); 
	receiverName[1] = '\0'; 

	char testMessage[24] = "L B 4\0"; 


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
    int iterationCounter = 1; 
    int end_connection = 0; 
    int close_conn = 0; 
    int successfullyProcessedUpdate = 0; 

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


	/************************************/ 
	/* 	Initialize the master fd_set 	*/ 
	/************************************/ 
	FD_ZERO(&masterFD_Set);			// clear the master file descriptor set 
	MAX_DESCRIPTOR = receivingSocket;
	//FD_SET(0, &masterFD_Set); 		// add file descriptor 0 for keyboard to masterFD_Set


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
   	tv.tv_sec = 15;
    tv.tv_usec = 0;

    /*************************************************************/
	/* Loop waiting for incoming connects or for incoming data   */
	/* on any of the connected sockets.                          */
	/*************************************************************/
	do
	{
		if(DEBUG)
		{
			printf("\n\n===================================="); 
			printf("\n     Router %s  ", router); 
			printf("\n   Iteration %d ", iterationCounter++); 
			printf("\n====================================="); 
		}
			

		

		/*************************************************/ 
        /* Copy the masterFD_Set over to the temp FD set */ 
        /*************************************************/
        FD_ZERO(&rfds);
        //memcpy(&rfds, &masterFD_Set, sizeof(masterFD_Set));

        for(i=0; i < MAX_DESCRIPTOR + 1; i++)
        {
        	if(servSock[i] != 0)
        	{
        		FD_SET(servSock[i], &rfds); 
        		//SendTestMessage(servSock[i]); 
        		printf("\nSet socket #%d", servSock[i]); 
        	}
        		
        }

         /* Wait up to 30 seconds. */
   		tv.tv_sec = 15;
       	tv.tv_usec = 0;

       	if(DEBUG)
       	{
       		printf("\nMAX_DESCRIPTOR = %d", MAX_DESCRIPTOR); 
       		printf("\nselect() will wait for %zu seconds", tv.tv_sec); 
       		printf("\nWaiting on select()...");

       	}
       		


	   	retval = select(MAX_DESCRIPTOR+1, &rfds, NULL, NULL, &tv);

       	if(retval < 0)
       	{
       		printf("select() failed"); 
       		//continue; 		// go back to the beginning of the infinite loop
       	}
       	else if(retval == 0)
       	{
       		printf("select() timed out"); 
       		//continue; 		// go back to the beginning of the infinite loop

       		// Should I send the 30 second updates in here? 
       		if(DEBUG)
       		{
       			printf("\nSince %zu seconds have elapsed, I will send my routing table to each of my neighbors", tv.tv_sec); 
       		}
       		for(i=0; i < MAX_DESCRIPTOR + 1; i++)
       		{
       			if(servSock[i] != 0)
       			{
       				SendTestMessage(servSock[i]); 
       			}
       		}
       	}
       	else if(retval)
       		printf("Data is available now from %d socket(s).", retval);

     
       	readyDescriptors = retval;

       	/************************************************************/ 
       	/* Iterate through all selectable file descriptors 		*/ 
       	/************************************************************/
       	for(i=0; i<= MAX_DESCRIPTOR + 1 && readyDescriptors > 0; ++i)
       	{

       		memset(messageBuffer, 0, sizeof(messageBuffer));

       		if(DEBUG)
       		{
       			printf("\n-- Iteration %d.%d -- ", iterationCounter,i); 
       		}
       		/* Doesn't seem like FD_ISSSET is working as assumed */ 
       		if(FD_ISSET(servSock[i], &rfds))
       		{
       			readyDescriptors -= 1; 

       			if(DEBUG)
       			{
       				printf("\nSocket #%d is set in the servSock[] array. Processing socket #%d...", i, servSock[i]); 
       				printf("\nThere are %d ready descriptors left.", readyDescriptors); 
       			}

       			

       			

   				
				printf("\nDescriptor %d is readable.", servSock[i]); 
				/*************************************************/
				/* Receive all incoming data on this socket      */
				/* before we loop back and call select again.    */
				/*************************************************/
				do
				{
					/**********************************************/
					/* Receive data on this connection until the  */
					/* recv fails with EWOULDBLOCK.  If any other */
					/* failure occurs, we will close the          */
					/* connection.                                */
					/**********************************************/
					printf("\n1"); 
					numBytes = recv(servSock[i], messageBuffer, sizeof(messageBuffer), 0);
					printf(" 2"); 
					if (numBytes < 0)
					{
					 if (errno != EWOULDBLOCK)
					 {
					    perror("  recv() failed");
					    printf(" 4"); 
					    close_conn = 1;
					    printf(" 5"); 
					 }
					 break;
					}
					printf(" 3"); 

					/**********************************************/
					/* Check to see if the connection has been    */
					/* closed by the client                       */
					/**********************************************/
					if (numBytes == 0)
					{
					 printf("  Connection closed\n");
					 close_conn = 1;
					 break;
					}

					/**********************************************/
					/* Data was received                          */
					/**********************************************/
					char* tmpName = GetRouterName(servSock[i]);
					memset(neighborName, 0, sizeof(neighborName)); 
					strncpy(neighborName, tmpName, 1); 
					printf("\n%zu bytes received from socket #%d (Router %s)\n", numBytes, servSock[i], neighborName);


					/************************************************/ 
					/* Examine the received data to figure out how  */ 
					/* to process it. 								*/ 
					/************************************************/ 
					char messageType = messageBuffer[0];
					int cost = messageBuffer[4] - '0'; 

					switch(messageType)
					{
						case 'U':
							printf("\nReceived router update message: %s", messageBuffer); 
							char dest = messageBuffer[2]; 
							printf("\nDestination: %c // Cost: %d", dest, cost); 
							successfullyProcessedUpdate = 1; 

							/* What exactly do I update when I receive an update message? */ 
							break; 
						case 'L':
							printf("\nReceived link cost message: %s", messageBuffer); 
							char neighbor = messageBuffer[2]; 
							printf("\nNeighbor: %c // Cost: %d", neighbor, cost); 
							successfullyProcessedUpdate = 1; 
							break; 
						default: 
							printf("\nReceived a message from Router %s, but I have no idea how to process it", neighborName); 
							break; 
					}

					if(successfullyProcessedUpdate)
					{
						if(DEBUG)
						{
							printf("\nSuccessfully processed update from Router %s via socket #%d", neighborName, servSock[i]); 
						}

						break; 		// exit the receiving loop
					}
					

					/**********************************************/
					/* Echo the data back to the client           */
					/**********************************************/
					// numBytes = send(servSock[i], messageBuffer, numBytes, 0);
					// if (numBytes < 0)
					// {
					//  perror("  send() failed");
					//  close_conn = 1;
					//  break;
					// }
					// else if(numBytes == 24)
					// {
					// 	// We received the expected amount of bytes
					// 	break; 
					// }

				} while (1);

               /*************************************************/
               /* If the close_conn flag was turned on, we need */
               /* to clean up this active connection.  This     */
               /* clean up process includes removing the        */
               /* descriptor from the master set and            */
               /* determining the new maximum descriptor value  */
               /* based on the bits that are still turned on in */
               /* the master set.                               */
               /*************************************************/
               if (close_conn)
               {

                	servSock[i] = 0; 

               		if(DEBUG)
               		{
               			printf("\nClose Connection Flag Thrown!"); 
               		}
                  	
                  	if (i == MAX_DESCRIPTOR)
                  	{
                  		if(DEBUG)
                  		{
                  			printf("\nAhhhh boy... we must update MAX_DESCRIPTOR!\n"); 
                  		}
                  		int j;
                  		for(j = MAX_DESCRIPTOR - 1; j >= 0 && servSock[j] > 0; j--)
                  		{
                  			printf(" %d", j); 
                  		}

                  		MAX_DESCRIPTOR = j; 

                  		if(DEBUG)
                  		{
                  			printf("\nUpdated MAX_DESCRIPTOR to %d", MAX_DESCRIPTOR); 
                  		}
                  	}
               }




       		} /* End of if (FD_ISSET(i, &working_set)) */

       	} /* End of loop through selectable descriptors */


       				

       			

	} while(end_connection == 0);



	
	printf("\nPress any key to exit..."); 
	char ch = getchar(); 

	
	return 0; 
}

