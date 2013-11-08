#include <stdio.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include "DieWithMessage.c"
#include "router_utils.c"


static int MAX_DESCRIPTOR = 0; 				// Highest file descriptor 

static int MAX_MESSAGE_SIZE = 1024; 

static int connectedNeighborSocket[MAXPAIRS]; 

void SendRoutingTableToAllNeighbors(); 

void PrintRoutingTable(); 


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
    
    struct sockaddr_in neighborAddr;    // buffer for addresses neighboring router
    int neighborSock; 					// socket for neighboring router 
    socklen_t neighborAddrLength = sizeof(neighborAddr);	// Set length of client address structure (in-out parameter)
    char neighborName[1]; 
    int iterationCounter = 1; 
    int end_connection = 0; 
    int close_conn = 0; 
    int successfullyProcessedUpdate = 0; 
    int updatedRoutingTable = 0; 

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
		printf("\nCost = %d", cost); 

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

       		//SendTestMessage(servSock[i]); 
       		for(i=0; i < MAX_DESCRIPTOR + 1; i++)
       		{
       			if(servSock[i] != 0)
       			{
       				SendRoutingTable(servSock[i]); 
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
       				if(readyDescriptors == 0)
       				{
       					printf("\nThat was your last ready descriptor!"); 
       				}
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
					numBytes = recv(servSock[i], messageBuffer, sizeof(messageBuffer), 0);
					if (numBytes < 0)
					{
					 if (errno != EWOULDBLOCK)
					 {
					    perror("recv() failed"); 
					    //close_conn = 1;
					 }
					 break;
					}

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

							if(strncmp(&dest, router, 1) == 0)
							{
								if(DEBUG)
								{
									printf("\nI don't want to look for myself in my own routing table!"); 
									successfullyProcessedUpdate = 1; 
									updatedRoutingTable = 1; 
								}
								break; 
							}

							routingTableEntry* entry = LookUpRouter(&dest, router); 


							/************************************************************/
							/* If link->cost != cost, then we must update the table 	*/  
							/* If link->cost == 64, that indicates we had to add the new*/ 
							/* router to the table, hence updating it. 					*/ 
							/************************************************************/
							if(entry->cost != cost || entry->cost == 64)
							{
								/* We must update the routing table and send updates! 	*/ 
								entry->cost = cost; 

								/* Calculate next hop 				*/ 

								printf("\nRouter %s making change: \n\tDestination: %c\n\tCost: %d\n\tNext hop: %d", 
									router, dest, entry->cost, 0); 

								updatedRoutingTable = 1; 
							}
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

					if(successfullyProcessedUpdate && updatedRoutingTable)
					{
						if(DEBUG)
						{
							printf("\nSuccessfully processed update from Router %s via socket #%d", neighborName, servSock[i]); 
							printf("\n[TRIGGERED UPDATE] : Sending my routing table to all neighboring routers."); 
							//SendRoutingTableToAllNeighbors(servSock); 
						}

						break; 		// exit the receiving loop
					}
					

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

                	//servSock[i] = 0; 

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

void  SendRoutingTableToAllNeighbors(int servSock[])
{
	int i = 0; 

	for(i=0; i < MAX_DESCRIPTOR + 1; i++)
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

