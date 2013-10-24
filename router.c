#include <stdio.h>
#include "readrouters.c"
#include "DieWithMessage.c"

int main(int argc, char* argv[])
{
	char* directory; 
	char* router;
	routerInfo* routerConfiguration; 
	linkInfo* routerLinks; 
	routerInfo* tmp;

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

	// Read the configuration of the router's local links from the files in the directory 
	// Open the directory, read each of the files 
	// Open the file for reading 
	routerConfiguration = readrouters(directory); 

	routerLinks = readlinks(directory, router); 

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


	
	return 0; 
}
