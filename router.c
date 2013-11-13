#include <stdio.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include "DieWithMessage.c"
#include "router_utils.c"

static int MAXPENDING = 5; 					// Maximum incoming connections 

static int MAX_DESCRIPTOR = 0; 				// Highest file descriptor 

static int MAX_MESSAGE_SIZE = 1024; 

static int connectedNeighborSocket[MAXPAIRS]; 

void SendRoutingTableToAllNeighbors(); 

void PrintRoutingTable(); 

void PrintRoutingTableEntry(char* dest); 

int GetCostToNeighbor(char* neighbor); 

void UpdateLinkInfo(char* neighbor, int cost); 

void UpdateRoutingTable(char* neighbor, int cost); 


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
	int unconnectedSocket; 
    
    struct sockaddr_in neighborAddr;    // buffer for addresses neighboring router
    int neighborSock; 					// socket for neighboring router 
    socklen_t neighborAddrLength = sizeof(neighborAddr);	// Set length of client address structure (in-out parameter)
    char neighborName[1]; 				// buffer for the name of the neighbor we're communicating with 
    int iterationCounter = 1; 			// count the number of times the infinite loops has run 
    int end_connection = 0; 			
    int close_conn = 0; 				// Used as a flag for cleaning up connections 
    int successfullyProcessedUpdate = 0; // After receiving an update and successfully modifying our routing table, this flag is set 
    int updatedRoutingTable = 0; 		// Used to denote whether or not an update actually caused us to update our table 

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
	/* Initialize the routing table 		*/ 
	/************************************/ 
	i = 0; 
	for(i=0; i < MAXROUTERS; i++)
	{
		routingTable[i].dest = 0; 
		routingTable[i].cost = 0; 
		routingTable[i].nextHop = 0; 
	}

	

	/************************************/ 
	/* 	Read all routers in the network */ 
	/************************************/ 
	routerConfiguration = readrouters(directory); 

	/************************************/ 
	/* Read the links for this router 	*/  
	/************************************/ 
	routerLinks = readlinks(directory, router); 

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
				// Set our baseport so we can bind to an unconnected socket to it later 
				printf("\nMy name is %s and my host is %s", routerConfiguration->router, routerConfiguration->host); 
				printf("\nSet my baseport to %d\n", routerConfiguration->baseport); 
			}
			
		}
		
		i++; 
	}

	if(DEBUG)
	{
		printf("\n# Links = %d", linkcount); 
		printf("\nConnections: "); 
	}

	i = 0; 
	while(i < MAXLINKS)
	{
		routerLinks = &linkInfoTable[i]; 

		if(routerLinks->cost == 0)
			break; 

		int cost = routerLinks->cost; 

		/********************************************************************************/ 
		/* Initialize routing table using information provided to us by our neighbors 	*/ 
		/********************************************************************************/ 
		routingTable[routingTableEntries].dest =  malloc(strlen(routerLinks->router)+1);
		strncpy(routingTable[routingTableEntries].dest, routerLinks->router, 1); 
		routingTable[routingTableEntries].cost = cost;
		routingTable[routingTableEntries].nextHop = malloc(strlen(routerLinks->router)+1); 
		strncpy(routingTable[routingTableEntries].nextHop, routerLinks->router, 1); 
		
		if(DEBUG)
		{
			printf("\n%d.", i + 1); 
			printf("\nRouter: %s", routerLinks->router); 
			printf("\nCost: %d", routerLinks->cost); 
			printf("\nLocal link: %d", routerLinks->locallink); 
			printf("\nRemote link: %d", routerLinks->remotelink); 
			printf("\n"); 
			printf("\nSuccessfully added %s to my routing table with cost=%d and next hop= Router %s",
			routingTable[routingTableEntries].dest, routingTable[routingTableEntries].cost, routingTable[routingTableEntries].nextHop); 
			printf("\nThere are now %d entries in my routing table", routingTableEntries + 1); 
		}

		routingTableEntries += 1; 

		

		
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

	/****************************************************************/
	/* Create an unconnected UDP socket bound to the local baseport */ 
	/* for receiving L and P messages
	/****************************************************************/
	struct sockaddr_in uSocketAddr; 
	memset(&uSocketAddr, 0, sizeof(uSocketAddr)); 			// zero out the address structure
	uSocketAddr.sin_family = AF_INET;						// IPv4 address family 
	uSocketAddr.sin_addr.s_addr = htonl(INADDR_ANY); 		// any incoming interface
	uSocketAddr.sin_port = htons(receivingPort);			// local port 


	/* Create the datagram socket 	*/ 
    if( (unconnectedSocket = socket(PF_INET, SOCK_DGRAM, 0)) <  0)
    {
        DieWithSystemMessage("\nsocket() failed"); 
    }

    if(DEBUG)
    	printf("\nSuccessfully created unconnected socket #%d for L and P messages.", unconnectedSocket); 

    // Add the unconnectedSocket to the servSock[] array 	 
   	servSock[unconnectedSocket] = unconnectedSocket;


   	/* Update MAX_DESCRIPTOR if the unconnectedSocket is greater 	*/ 
    if(unconnectedSocket > MAX_DESCRIPTOR)
   	{
   		MAX_DESCRIPTOR = unconnectedSocket; 

   		if(DEBUG)
   			printf("\nUpdated MAX_DESCRIPTOR to %d", unconnectedSocket); 
   	}

    /* Bind socket to the local port 	*/ 
	if ((bind( unconnectedSocket, (struct sockaddr *)&uSocketAddr, sizeof(struct sockaddr_in) )) < 0 )
    {
        DieWithSystemMessage("\nbind() failed"); 
    }

    if(DEBUG)
    	printf("\nSuccessfully bound unconnected socked #%d to baseport %d", unconnectedSocket, receivingPort); 


    if(DEBUG)
    {
    	printf("\nSuccessfully setup unconnected socket #%d for L and P messages.", unconnectedSocket); 
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

		PrintRoutingTable(); 


			
		successfullyProcessedUpdate = 0; 
		updatedRoutingTable = 0; 
		

		/*************************************************/ 
        /* Copy the masterFD_Set over to the temp FD set */ 
        /*************************************************/
        FD_ZERO(&rfds);

        /* I'm not using the masterFD set socket setup earlier 	*/ 
        /* Instead I'm using servSock[] array to reinitialize 	*/ 
        /* rfds on each iteration. Kind of weird... 			*/ 


        /************************************************************/ 
        /* Set all sockets in the temporary file descriptor set 	*/ 
        /************************************************************/ 
        for(i=0; i < MAX_DESCRIPTOR + 1; i++)
        {
        	if(servSock[i] != 0)
        	{
        		FD_SET(servSock[i], &rfds);  
        		printf("\nSet socket #%d", servSock[i]); 
        	}
        		
        }

        /* Wait up to 30 seconds. */
        /* His discussion in class is making it seem like setting tv.tv_sec here is bad 	*/ 
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
       	}
       	else if(retval == 0)
       	{
       		printf("select() timed out"); 

       		if(DEBUG)
       		{
       			printf("\nSince %zu seconds have elapsed, I will send my routing table to each of my neighbors", tv.tv_sec); 
       		}

       		/************************************************************/ 
       		/* Since select timed out, meaning 30 seconds has passed 	*/ 
       		/* send your routing table to all of your neighbors 		*/ 
       		/************************************************************/ 
       		for(i=0; i < MAXROUTERS; i++)
       		{
       			if(servSock[i] != 0 && servSock[i] != unconnectedSocket)
       			{
       				SendRoutingTable(servSock[i]); 
       			}
       		}
       	}
       	else if(retval)
       	{
       		printf("Data is available now from %d socket(s).", retval);

     
	       	readyDescriptors = retval; 

	       	/************************************************************/ 
	       	/* Iterate through all selectable file descriptors 		*/ 
	       	/************************************************************/
	       	for(i=0; i<= MAXROUTERS - 1; ++i)
	       	{

	       		memset(messageBuffer, 0, sizeof(messageBuffer));

	       		if(DEBUG)
	       		{
	       			printf("\n-- Iteration %d.%d --", iterationCounter,i); 
	       		}
	       		 
	       		if(FD_ISSET(servSock[i], &rfds))
	       		{
	       			/* If select() says the socket is ready 	*/ 
	       			/* you really MUST read from the socket 	*/ 
	       			/* I think I'm accomplishing that by 		*/ 
	       			/* iterating through all of the fds using 	*/ 
	       			/* the for loop above 						*/ 
	       			readyDescriptors -= 1; 

	       			char* tmpName; 

	       			if(servSock[i] != unconnectedSocket)
	       			{
	       				tmpName = GetRouterName(servSock[i]);
						memset(neighborName, 0, sizeof(neighborName)); 
						strncpy(neighborName, tmpName, 1);
	       			}
	       				

	       			if(DEBUG)
	       			{
	       				if(servSock[i] != unconnectedSocket)
	       					printf("\nProcessing socket #%d...Router %s", i, neighborName); 
	       				else
	       					printf("\nReceived message on the unconnected socket, implying it's an L or P message"); 

	       				if(readyDescriptors == 0)
	       				{
	       					printf("\nThat was your last ready descriptor!"); 
	       					break; 			// exit the for loop
	       				}
	       			}



	   
					
					/*************************************************/
					/* Receive all incoming data on this socket      */
					/* before we loop back and call select again.    */
					/*************************************************/
					/**********************************************/
					/* Receive data on this connection until the  */
					/* recv fails If any other 					  */
					/* failure occurs, we will close the          */
					/* connection.                                */
					/**********************************************/
					numBytes = recv(servSock[i], messageBuffer, sizeof(messageBuffer), 0);
					if (numBytes < 0)
					{
						printf("\nrecv() failed");
					 	continue;
					}

					/**********************************************/
					/* Check to see if the connection has been    */
					/* closed by the client                       */
					/**********************************************/
					if (numBytes == 0)
					{
					 printf("  Connection closed\n");
					 close_conn = 1;
					 continue;
					}

					printf("\nDescriptor %d is readable.", servSock[i]); 

					/**********************************************/
					/* Data was received                          */
					/**********************************************/ 
					if(servSock[i] != unconnectedSocket)
						printf("\n%zu bytes received from socket #%d (Router %s)\n", numBytes, servSock[i], neighborName);


					/************************************************/ 
					/* Examine the received data to figure out how  */ 
					/* to process it. 								*/ 
					/************************************************/ 
					char messageType = messageBuffer[0];
					int cost = messageBuffer[4] - '0'; 
					// Calculate cost
					int t = 5; 
					while(messageBuffer[t] != '\0')
					{
						cost *= 10; 
						cost += messageBuffer[t] - '0'; 
						t++; 
					}

					int costToNeighbor = GetCostToNeighbor(neighborName); 

					

					

					switch(messageType)
					{
						case 'U':
							printf("\nReceived router update message: %s", messageBuffer); 
							char dest = messageBuffer[2]; 
							printf("\nDestination: %c // Cost: %d", dest, cost); 

							if(strncmp(&dest, router, 1) == 0)
							{
								if(DEBUG)
								{
									printf("\nI don't want to look for myself in my own routing table!"); 
									successfullyProcessedUpdate = 1; 
									updatedRoutingTable = 0; 
								}
								break; 
							}

							routingTableEntry* entry = LookUpRouter(&dest, router); 


							/************************************************************/
							/* The U message contains the cost of getting from the 		*/ 
							/* sending router to the destination router. Therefore 		*/ 
							/* The cost of getting from this router instance to the  	*/ 
							/* destination router must include the cost of the link from*/ 
							/* this router to the sending router. 						*/ 
							/*															*/ 
							/* If link->cost != cost, then we must update the table 	*/  
							/* If link->cost == 64, that indicates we had to add the new*/ 
							/* router to the table, hence updating it. 					*/ 
							/* 															*/ 
							/* Some cases left to consider and cover: 					*/ 
							/* 		(i) Consider the cost of using other neighbors by 	*/ 
							/* 			having their DVs saved and comparing the cost 	*/ 
							/* 			of using them instead. 							*/ 
							/* 		(ii) What happens if the cost changes for the 		*/ 
							/* 			router we're currently using as next hop? 		*/ 
							/************************************************************/
							if(entry->cost != cost || entry->cost == 64)
							{
								/* If the update cost is lower, we must update the routing table and send updates! 	*/ 
								if((costToNeighbor + cost) < entry->cost)
								{
									entry->cost = costToNeighbor + cost; 
									strncpy(entry->nextHop, neighborName, 1); 
								}
									

								printf("\nRouter %s making change: \n\tDestination: %c\n\tCost: %d\n\tNext hop: %s", 
									router, dest, entry->cost, entry->nextHop); 

								updatedRoutingTable = 1; 
							}

							successfullyProcessedUpdate = 1; 

							close_conn = 1; 
							break; 
						case 'L':
							printf("\nReceived L message: %s", messageBuffer); 
							char neighbor = messageBuffer[2]; 
							printf("\nNeighbor: %c // Cost: %d", neighbor, cost); 

							UpdateLinkInfo(&neighbor, cost); 

							UpdateRoutingTable(&neighbor, cost); 

							successfullyProcessedUpdate = 1; 
							updatedRoutingTable = 1; 
							break; 
						case 'P': 
							printf("\nReceived P message: %s", messageBuffer); 

							/* Start looking for a destination router at index 1. If messageBuffer[1] is NULL 		*/ 
							/* then we can assume now destination is provided, and print our entire routing table.	*/
							int i = messageBuffer[1]; 	

							while(messageBuffer[i] != '\0')
							{
								i++; 
							}

							if(i == 1)
								PrintRoutingTable(); 			// Print this router's routing table 
							else if(i == 2)
								PrintRoutingTableEntry(&messageBuffer[i]); 		// Print the routing table entry for messageBuffer[i], the destination 

							break; 
						default: 
							printf("\nReceived a message from Router %s, but what kind of message is %d?", neighborName, messageType); 
							break; 
					}

					if(successfullyProcessedUpdate && updatedRoutingTable)
					{
						if(DEBUG)
						{
							printf("\nSuccessfully processed update from Router %s via socket #%d", neighborName, servSock[i]); 
							printf("\n[TRIGGERED UPDATE] : Sending my routing table to all neighboring routers."); 
							SendRoutingTableToAllNeighbors(servSock); 
						}

						break; 		// exit the receiving loop
					}

	               /*************************************************/
	               /* If the close_conn flag was set, we need 		*/
	               /* to clean up this active connection.  This     */
	               /* clean up process includes removing the        */
	               /* descriptor from the master set and            */
	               /* determining the new maximum descriptor value  */
	               /* based on the bits that are still turned on in */
	               /* the master set.                               */
	               /*************************************************/
	               if (close_conn)
	               {
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
	    }


       			

	} while(end_connection == 0);



	
	printf("\nPress any key to exit..."); 
	char ch = getchar(); 

	
	return 0; 
}

void  SendRoutingTableToAllNeighbors(int servSock[])
{
	int i = 0; 

	for(i=0; i < MAX_DESCRIPTOR - 1; i++)
	{
		if(servSock[i] != 0)
		{
			SendRoutingTable(servSock[i]); 
		}
	}
}


void PrintRoutingTable()
{
	int i = 0; 
	printf("\nMy Routing Table: "); 
	printf("\nDestination\tCost\tNext Hop"); 
	for(i=0; i < routingTableEntries; i++)
	{
		printf("\n%s\t\t%d\t%s", 
			routingTable[i].dest, routingTable[i].cost, routingTable[i].nextHop); 
	}
}

void PrintRoutingTableEntry(char* dest)
{
	int i = 0; 

	printf("\nDestination\tCost\tNext Hop"); 
	for(i=0; i < routingTableEntries; i++)
	{
		if(strncmp(dest, routingTable[i].dest, 1) == 0)
		{
			printf("\n%s\t\t%d\t%s", 
				routingTable[i].dest, routingTable[i].cost, routingTable[i].nextHop); 
			return; 
		}
	}
}

int GetCostToNeighbor(char* neighbor)
{
	linkInfo* routerLink; 

	int i = 0; 

	while(i < MAXLINKS)
	{
		routerLink = &linkInfoTable[i]; 

		if(routerLink->cost == 0)
			break; 

		if(strncmp(neighbor, routerLink->router, 1) == 0)
		{
			int cost = routerLink->cost;
			printf("\nFound the cost of neighboring router %s to be %d", neighbor, cost); 
			return cost; 
		}

		i++; 

		
	}
}


void UpdateLinkInfo(char* neighbor, int cost)
{
	linkInfo* routerLink; 

	int i = 0; 

	printf("\nUpdating link info for router %s to cost %d", neighbor, cost); 

	while(i < MAXLINKS)
	{
		routerLink = &linkInfoTable[i]; 

		if(routerLink->cost == 0)
			break; 

		if(strncmp(neighbor, routerLink->router, 1) == 0)
		{
			routerLink->cost = cost; 
			printf("\nUpdated the cost of the link to neighbor %s to %d in my link info table", neighbor, cost); 
			return; 
		}

		i++; 

		
	}
}

void UpdateRoutingTable(char* neighbor, int cost)
{
	int i = 0; 
	for(i=0; i < routingTableEntries; i++)
	{
		if(strncmp(neighbor, routingTable[i].dest, 1) == 0)
		{
			routingTable[i].cost = cost; 
			printf("\nUpdated the cost of the link to neighbor %s to %d in my routing table", neighbor, cost); 
			return; 
		}
	}
}

