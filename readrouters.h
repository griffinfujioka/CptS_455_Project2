// Structure to store Router Info 
// Contains the host-name and the base port
typedef struct RouterInfo
{
	char* router;
	char* host;
	int baseport;
} routerInfo; 

// Structure to store the Link Info
// Contains the cost, the local interface and the remote interface
typedef struct LinkInfo
{
	char* router;
	int cost;
	int locallink;
	int remotelink;
} linkInfo;

// Structure to store the Neighbor-Socker Pair
// Contains the neighbor router and socket descriptor
typedef struct NeighborSocket
{
	char* neighbor;
	int socket;
} neighborSocket;

// Function  to read the router information from a file
// and return a pointer to the routerInfoTable.
routerInfo* readrouters(char* path);

// Function  to read the link information from a file
// and return a pointer to the linkInfoTable.
linkInfo* readlinks(char* path, char* router);

// Function to create connected Datagram connections to
// all routers directly connected to this router
neighborSocket* createConnections(char* routerName);

// Function to create a connected Datagram socket and return
// the socket descriptor
int createSocket(char* host, int port, char* dest, int destport);