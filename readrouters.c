#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "readrouters.h"

// Maximum number of routers considered
#define MAXROUTERS 10

// Maximum number of links considered
#define MAXLINKS 10

// Maximum number of neighbors considered
#define MAXPAIRS 10

// Table to store Router Info
static routerInfo routerInfoTable[MAXROUTERS];
// Count to store the number of routers in InfoTable
static int routercount = 0;

// Table to store link info
static linkInfo linkInfoTable[MAXLINKS];
// Count to store the number of links in InfoTable
static int linkcount = 0;

// Array to store Neighbor-socket pairs
static neighborSocket neighborSocketArray[MAXPAIRS];
// Count to store the number of neighbor-socket pairs
static int count = 0;

/*************************************************************
   This function reads the router information from a file
   and populates the routerInfoTable.
   It takes the path-name for the router info file as argument
   When done, static variable routercount contains the number of routerinfo 
   stored in the table
   Returns a pointer to the routerInfoTable
**************************************************************/
routerInfo* readrouters(char* path)
{
	FILE* fp;
	char* filename;
	char* delim = " ";
	char* line;
	char* word;
	
	if(routercount != 0) 
	{
		return routerInfoTable;
	}

	if(path == NULL)
	{
		filename = (char*)malloc(10);
		strncpy(filename,".",2);
	}
	else
	{
		filename = (char*)malloc(strlen(path)+9);
		strncpy(filename,path,strlen(path)+9);
	}
	
	strcat(filename,"/routers");

	fp = fopen(filename,"r");

	if(fp==NULL)
	{	
		perror("Unable to open Router Information File");
		exit(0);
	}

	line = (char*)malloc(101);
	word = (char*)malloc(15);
	while(fgets(line,100,fp))
	{
		if(line[0] == '#' || line[0] == '\n') continue;

		word = strsep(&line,delim);
		routerInfoTable[routercount].router = malloc(strlen(word)+1);
		strncpy(routerInfoTable[routercount].router,word,strlen(word)+1);

		word = strsep(&line,delim);
		routerInfoTable[routercount].host = malloc(strlen(word)+1);
		strncpy(routerInfoTable[routercount].host,word,strlen(word)+1);

		routerInfoTable[routercount].baseport = atoi(line);

		routercount++;		
	}

	fclose(fp);

	return routerInfoTable;
}

/*************************************************************
   This function reads the link information from a file
   and populates the linkInfoTable.
   It takes the path-name for the link info file and the 
   router name as argument
   When done, static variable linkcount contains the number of routerinfo 
   stored in the table
   Returns a pointer to the linkInfoTable
**************************************************************/
linkInfo* readlinks(char* path, char* router)
{
	FILE* fp;
	char* filename;
	char* delim = " ";
	char* line;
	char* word;
	
	if(linkcount != 0) 
	{
		return linkInfoTable;
	}

	if(path == NULL)
	{
		filename = (char*)malloc(strlen(router)+7);
		strncpy(filename,".",2);
	}
	else
	{
		filename = (char*)malloc(strlen(path)+strlen(router)+6);
		strncpy(filename,path,strlen(path)+strlen(router)+6);
	}
	
	strncat(filename,"/",2);
	strncat(filename,router,strlen(router)+1);
	strncat(filename,".cfg",5);

	fp = fopen(filename,"r");

	if(fp==NULL)
	{	
		perror("Unable to open Link Information File");
		exit(0);
	}

	line = (char*)malloc(101);
	while(fgets(line,100,fp))
	{
		if(line[0] == '#' || line[0] == '\n') continue;
		
		word = strsep(&line,delim);
		linkInfoTable[linkcount].router = malloc(strlen(word)+1);
		strncpy(linkInfoTable[linkcount].router,word,strlen(word)+1);
		
		word = strsep(&line,delim);
		linkInfoTable[linkcount].cost = atoi(word);

		word = strsep(&line,delim);
		linkInfoTable[linkcount].locallink = atoi(word);

		linkInfoTable[linkcount].remotelink = atoi(line);

		linkcount++;
	}
	
	fclose(fp);

	return linkInfoTable;
}

/*************************************************************
   This function creates datagram sockets for
   all routers directly connected to this router (input argument)

   Populates the neighborSocketArray   
   When done, static variable count contains the number of neighbor routers
   stored in the array
   Returns a pointer to the neighborSocketArray
**************************************************************/
neighborSocket* createConnections(char* routerName)
{
	int i,j;
	int socket;
	char *sourcehost, *desthost;
	int port, sourceport, destport;

	if (count != 0)
	{
		return neighborSocketArray;
	}

	// Find details for routerName from routerInfoTable
	for(i=0;i<routercount;i++)
	{
		if(strcmp(routerName,routerInfoTable[i].router) == 0)
			break;
	}

	// Get source hostname for routerName
	sourcehost = (char*)malloc(strlen(routerInfoTable[i].host)+1);
	strcpy(sourcehost,routerInfoTable[i].host);

	// Getbaseport for routerName
	port = routerInfoTable[i].baseport;

	// For all routers directly connected to me (present in linkInfoTable)
	for(i=0;i<linkcount;i++)
	{
		// Find details for this connected router
		for(j=0;j<routercount;j++)
		{
			if(strcmp(linkInfoTable[i].router,routerInfoTable[j].router) == 0)
				break;
		}

		// Get the destination host name
		desthost = (char*)malloc(strlen(routerInfoTable[j].host)+1);
		strcpy(desthost,routerInfoTable[j].host);

		// Calculate destination port number (destination baseport + remotelink)
		destport = routerInfoTable[j].baseport + linkInfoTable[i].remotelink;

		// Calculate source port number (source baseport + locallink)
		sourceport = port + linkInfoTable[i].locallink;

		// Create connected Datagram socket and get socket descriptor
		socket = createSocket(sourcehost, sourceport, desthost, destport);
		
		// Store neighbor router name and socket descriptor in neighborSocketArray
		neighborSocketArray[count].neighbor = (char*)malloc(strlen(linkInfoTable[i].router)+1);
		strcpy(neighborSocketArray[count].neighbor,linkInfoTable[i].router);
		neighborSocketArray[count].socket = socket;

		// Increment the neighborSocket Pair count
		count++;
	}

	return neighborSocketArray;
}

/*************************************************************
   This function creates a connected Datagram socket and returns
   the socket descriptor

   Takes as input - the source host name, source port, 
					the destination host name and destination port
**************************************************************/
int createSocket(char* host, int port, char* dest, int destport)
{
	struct sockaddr_in sin; 
	struct hostent *hp;          
	int sockfd;

	// Get IP address of source
	hp = gethostbyname(host);

	// build address data structure of INADDR(all interfaces) paired with source port  
	bzero( (char*)&sin, sizeof(sin) );
	sin.sin_family = AF_INET;
	bcopy( hp->h_addr, (char*)&(sin.sin_addr), hp->h_length );
	sin.sin_port = htons(port);

	// Create datagram socket
    if( (sockfd = socket(PF_INET, SOCK_DGRAM, 0) ) <  0)
    {
        perror( "Error creating socket" );
        exit(1);
    }

	// Bind socket to local port
	if ((bind( sockfd, (struct sockaddr *)&sin, sizeof(struct sockaddr_in) )) < 0 )
    {
        perror( "Error binding socket" );
        exit(1);
    }

	// Get IP address of destination
	hp = gethostbyname(dest);

	// build address data structure of INADDR(all interfaces) paired with destination port
	bzero( (char*)&sin, sizeof(sin) );
	sin.sin_family = AF_INET;
	bcopy( hp->h_addr, (char*)&(sin.sin_addr), hp->h_length );
	sin.sin_port = htons(destport);

	// Connect socket to remote port
    if ((connect(sockfd, (struct sockaddr *)&sin, sizeof(struct sockaddr))) < 0 )
	{
		perror("Error connecting to remote host");
		exit(1);
	}

	// Return socket descriptor
	return sockfd;
}
