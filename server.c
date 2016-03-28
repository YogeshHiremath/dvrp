#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdlib.h>
//#include <conio.h>

#define MAXBUFLEN 1024
#define STDIN 0

FILE *topology;

struct tableEntry {
	int id;
	int nextHop;
	int cost;
	char *ipAddress;
	int port;
};

char *ipList[10] = { 0 };
struct topTable {
	struct tableEntry *list[10];
	int myID;
	char *ipAddress;
	int port;
};

void *get_in_addr (struct sockaddr *sa)
{
	if(sa->sa_family == AF_INET){
		return &(((struct sockaddr_in*) sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*) sa)->sin6_addr);
}

void printTable();
void printCostTable();
void tostring(char str[], int num);
struct tableEntry createTableEntry(int id, char *ipAddress, int port, int nextHop, int cost);
void sendUDPPacket(char *ipAddress, int port, char *buf, int len);
void invalidArgs();
void invalidFile();
void sendPacketsToNeighbors();
void updateTables(int src, int dst, int cst);
void disableLink(int servID);
struct topTable t;
struct tableEntry te[10];
char *tempString[50];
int serverSockfd;
fd_set master, read_fds;
int fdmax;
int costTableData[20][20];
int noOfClients, noOfNeighbors;
int recvdPackets;
int timeoutCount[10];
int initVector[10];
void updateCostTable(int a[][2], int len);
void printInitVector();
int timeoutCount[10];
int nxtH[10];

int main(int argc, char* argv[])
{
	if(argc < 4)
	{
		invalidArgs();
		return(0);
	}
	//START Read from file and initializations
	int j;
	for(j = 0;j<=10;j++){
		initVector[j]=-1;
		timeoutCount[j] = 0;
		nxtH[j]=0;
	}
	topology = fopen (argv[2], "rt");
	for (j=0; j < 10; j++){
		t.list[j] = NULL;
	}
	char line[128];
	if(fgets(line, 80, topology) != NULL){
		//store noOfClients
		noOfClients = atoi(line);
		int x, y;
		for(x=0;x<noOfClients+1;x++)
			for(y=0;y<noOfClients+1;y++){
				if(x == y)
					costTableData[x][y] = 0;
				else
					costTableData[x][y] = -1;
			}
//		printf("noOfClients: %d\n", noOfClients);
		if(fgets(line, 80, topology) != NULL){
			//store noOfClients
			noOfNeighbors = atoi(line);
			int i;

//			for(i=0;i<noOfClients;i++){
//				te[i] = NULL;
//			}
			int counter = 0;
			for(i=0;i<noOfClients;i++){
				if(fgets(line, 80, topology) != NULL){
//					printf("%s", line);
					tempString[counter] = strtok(line," ");
					if(tempString[counter] != NULL){
						te[i].id = atoi(tempString[counter]);
//						printf("%d\n",te[i].id);
						counter++;
						tempString[counter] = strtok (NULL," ");
						char *thisIPAddress = tempString[counter];
						ipList[i] = malloc(strlen(thisIPAddress) + 1);
						strcpy(ipList[i], thisIPAddress);
//						printf("%s\n %d",ipList[i], i);
						te[i].ipAddress = ipList[i];
						counter++;
						tempString[counter] = strtok (NULL," ");
						te[i].port = atoi(tempString[counter]);
						
						te[i].nextHop = 0;
						te[i].cost = -1;
//						struct tableEntry tk = &createTableEntry(1, "te.ipAddress", "te.port", 2, 5);
						t.list[i] = &te[i];
//						printf("%d\n",te[i].id);
//						printTable(t);
						counter++;
					}
					else
						invalidFile();
				}
				else
					invalidFile();
			}
			int dest, destCost;
			for(i=0;i<noOfNeighbors;i++){
				if(fgets(line, 80, topology) != NULL){
					counter++;
					tempString[counter] = strtok(line," ");
					t.myID = atoi(tempString[counter]);
					initVector[t.myID] = 0;
					int jk;
					for(jk=0;jk<noOfClients;jk++)
					{
						if(te[jk].id == t.myID){
							te[jk].cost = 0;
						}
					}
					counter++;
					tempString[counter] = strtok(NULL," ");
					dest = atoi(tempString[counter]);
					counter++;
					tempString[counter] = strtok(NULL," ");
					destCost = atoi(tempString[counter]);
					for(jk=0;jk<noOfClients;jk++)
					{
						if(te[jk].id == dest){
							te[jk].cost = destCost;
							te[jk].nextHop = dest;
							initVector[jk+1] = destCost;
						}
					}
					if(dest != t.myID){
						costTableData[t.myID][dest] = destCost;
					}
//					printCostTable();
//					printf("%s", tempString[counter]);
				}
				else{
					invalidFile();
				}
			}
//			printf("\nnoOfNeighbors: %d\n", noOfNeighbors);
		}
		else
			invalidFile();
		//
		
		initVector[t.myID] = 0;
	}
	else{
		invalidFile();
	}
	int jkm;
	for(jkm = 0; jkm<noOfClients;jkm++){
		if(te[jkm].id == t.myID){
			t.port = te[jkm].port;
			t.ipAddress = te[jkm].ipAddress;
		}
	}
//	printInitVector();
	fclose(topology);
//	printTable(t);
	for(jkm = 0;jkm<=10;jkm++){
		timeoutCount[jkm]=0;
	}
	//END Read from file and initializations
	
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	struct sockaddr_storage their_addr[noOfClients];
	char buf[MAXBUFLEN];
	socklen_t addr_len;
	char s[INET6_ADDRSTRLEN];
	char portStr[10];
	sprintf(portStr, "%d", t.port);
	memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
	
//	printf("Port %d\n", t.port);
	if ((rv = getaddrinfo(NULL, portStr, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}
	
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((serverSockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("listener: socket");
			continue;
		}

		if (bind(serverSockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(serverSockfd);
			perror("listener: bind");
			continue;
		}

		break;
	}
	
	if (p == NULL) {
		fprintf(stderr, "listener: failed to bind socket\n");
		return 2;
	}

	freeaddrinfo(servinfo);
	FD_SET(STDIN, &master);
	FD_SET(serverSockfd, &master);
	struct timeval tv;
	tv.tv_sec = atoi(argv[4]);
	tv.tv_usec = 0;
	fdmax = serverSockfd;
	
	printf("Listening on port: %s\n", portStr);
	recvdPackets = 0;

	//Code to wait for datagrams, user input and timeouts
	for(;;) {
		read_fds = master; // copy it
		rv = select(fdmax+1, &read_fds, NULL, NULL, &tv);
		if (rv == -1) {
			perror("select");
		}
		else if (rv == 0){
			tv.tv_sec = atoi(argv[4]);
			sendPacketsToNeighbors();
			for(j = 1;j<=noOfClients;j++){
				timeoutCount[j]++;
				initVector[t.myID] = 0;
				if(timeoutCount[j] == 3 && initVector[j] != -1){
					
					initVector[j] = -1;
					if(j!=t.myID)
						printf("%d timed out\n", j);
				}
			}
			printf(".......................................~~~......................................\n");
			initVector[t.myID] = 0;
		}
		else {
			// run through the existing connections looking for data to read
			int i;
			for(i = 0; i <= fdmax; i++) {
				if (FD_ISSET(i, &read_fds)) {
					if(i == STDIN)
					{
						char command[50];
						fgets(command, sizeof(command), stdin);
						command[strlen(command)-1] = '\0';
						char * pch;
						
						pch = strtok (command," ");
//						pch[strlen(pch)-1] = '\0';
						if(strcasecmp ("update", pch) == 0)
						{
//							pch = strtok (command," ");
							pch = strtok (NULL," ");
//							printf("%s\n", command);
							if(atoi(pch) != t.myID)
								printf("Invalid params for update\n");
							else{
								char sssss[512];
								int updateNode;
								pch = strtok (NULL," ");
								updateNode = atoi(pch);
								if(initVector[updateNode] != -1){
									pch = strtok (NULL," ");
									if(strcasecmp ("inf", pch) == 0)
										initVector[updateNode] = -1;
									else
										initVector[updateNode] = atoi(pch);
									sprintf(sssss, "updt %d %d", t.myID, initVector[updateNode]);
									sendUDPPacket(te[updateNode-1].ipAddress, te[updateNode-1].port, sssss, strlen(sssss));
									printf("Sent update packet to neighbor: %d \n UPDATE : SUCCESS\n", updateNode);
								// Send update cost to corresponding node
								}
								else
									printf("Cannot update non-neighbor");
							}
						}
						if(strcasecmp ("display", pch) == 0)
						{
							printTable();
							printf("DISPLAY : SUCCESS\n");
						}
						if(strcasecmp ("disable", pch) == 0)
						{
							int servID;
							pch = strtok (NULL," ");
//							printf("%s", command);
							servID = atoi(pch);
							if(servID == t.myID)
								printf("DISABLE : Cannot disable self\n");
							else{
								if(initVector[servID]!= -1){
									disableLink(servID);
									printf("DISABLE : SUCCESS\n");
								}
								else
									printf("DISABLE : Cannot disable non-neighbor\n");
							}
//							displayHelp();
						}
						if(strcasecmp ("step", pch) == 0)
						{
//							displayHelp();
							sendPacketsToNeighbors();
							printf("STEP : SUCCESS\n");
						}
						if(strcasecmp ("packets", pch) == 0)
						{
							printf("No of DV packets received = %d\n", recvdPackets);
							recvdPackets = 0;
							printf("PACKETS : SUCCESS\n");
//							displayHelp();
						}
						if(strcasecmp ("crash", pch) == 0)
						{
							printf("Exiting....\n");
							printf("CRASH : SUCCESS\n");
//							displayHelp();
							return 0;
						}
						if(strcasecmp ("iv", pch) == 0)
						{
							printInitVector();
//							displayHelp();
//							return 0;
						}
					}
					else if (i == serverSockfd) {
//						recvdPackets++;
						// handle new connections
						addr_len = sizeof their_addr;
						if ((numbytes = recvfrom(serverSockfd, buf, MAXBUFLEN-1 , 0,
							(struct sockaddr *)&their_addr, &addr_len)) == -1) {
							perror("recvfrom");
						}

//						printf("listener: got packet from %s\n",
//							inet_ntop(their_addr[0].ss_family,
//								get_in_addr((struct sockaddr *)&their_addr),
//								s, sizeof s));
//						printf("listener: packet is %d bytes long\n", numbytes);
						buf[numbytes] = '\0';
						char * pch;
						int noOfUpdates;
						pch = strtok (buf," ");
						if(strcasecmp ("updt", pch) == 0){
							int ntoUpdate, costToUpdate;
							pch = strtok (NULL," ");
							ntoUpdate = atoi(pch);
							pch = strtok (NULL," ");
							costToUpdate = atoi(pch);
							initVector[ntoUpdate] = costToUpdate;
							printf("Received update packet from: %d",ntoUpdate);
						}
						else{
							recvdPackets++;
							noOfUpdates = atoi(pch);
							int rcvdDV[noOfUpdates][2];
							pch = strtok (NULL," ");
							pch = strtok (NULL," ");
							
							for(jkm=0;jkm<noOfUpdates;jkm++){
								pch = strtok (NULL," ");
								pch = strtok (NULL," ");
								pch = strtok (NULL," ");
								pch = strtok (NULL," ");
								rcvdDV[jkm][1] = atoi(pch);
								pch = strtok (NULL," ");
								rcvdDV[jkm][0] = atoi(pch);
								if(rcvdDV[jkm][0] == 0)
									printf("Received DV packet from: %d",rcvdDV[jkm][1]);
//								printf("%d \t", rcvdDV[jkm][0]);
							}
							printf("\n");
							updateCostTable(rcvdDV, noOfUpdates);
							
//							printf("listener: packet contains \"%s\"\n", buf);
						}
					} 
				} 
			} 
		} 
	}
	return 0;
}

void invalidArgs()
{
	printf("Correct usage: ./server -t <topology-file-name> -i <routing-update-interval>\n");
}

void invalidFile()
{
	printf("Correct usage: ./server -t <topology-file-name> -i <routing-update-interval>\n");
}

void printInitVector(){
	int i;
	for(i = 1; i<=5;i++){
		printf("%d\t", initVector[i]);
	}
	printf("\n");
}

void printTable(){
	int i;
	printf("Current routing table:\n");
//	printf("If unreachable next hop is 0 and distance is inf:\n\n");
	printf("Dest.\tnextHop\tCost\n");
//	for(i = 0; i<10;i++){
//		if(t.list[i] != NULL){
//			struct tableEntry te= *t.list[i];
//			if(te.cost != -1)
//				printf("%d\t%d\t%d\n", te.id, te.nextHop, te.cost);
//			else
//				printf("%d\t%d\tinf\n", te.id, te.nextHop);
//		}
//	}
	for(i = 1; i<=noOfClients;i++){
		if(t.myID == i)
			nxtH[i] = i;
		if(costTableData[t.myID][i] != -1)
			printf("%d\t%d\t%d\n", i, nxtH[i], costTableData[t.myID][i]);
		else
			printf("%d\t%d\tinf\n", i, nxtH[i]);
	}
}

void printCostTable(){
	int i,j;
	printf("\n");
	for(i=1;i<=noOfClients;i++){
		for(j=1;j<=noOfClients;j++){
			if(costTableData[i][j] != -1)
				printf("%d\t", costTableData[i][j]);
			else
				printf("inf\t");
		}
		printf("\n");
	}
	
}

struct tableEntry createTableEntry(int id, char *ipAddress, int port, int nextHop, int cost){
	struct tableEntry t;
	t.cost = cost;
	t.id = id;
	t.nextHop = nextHop;
	t.ipAddress = ipAddress;
	t.port = port;
	return t;
}
//
void sendUDPPacket(char *ipAddress, int port, char *buf, int len){
	int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
	char portStr[10];
//	printf("%s\n", portStr);
	sprintf(portStr, "%d", port);
    if ((rv = getaddrinfo(ipAddress, portStr, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    }

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
    }

    if ((numbytes = sendto(sockfd, buf, strlen(buf), 0,
             p->ai_addr, p->ai_addrlen)) == -1) {
        perror("talker: sendto");
    }

    freeaddrinfo(servinfo);

//    printf("talker: sent %d bytes to %s:%s\n", numbytes, ipAddress, portStr);
    close(sockfd);
}

void updateTables(int src, int dst, int cst){
	if(cst == -1){
		costTableData[src][dst] = cst;
		te[dst-1].nextHop = 0;
		te[dst-1].cost = -1;
	}
	else if(costTableData[src][dst] > cst || costTableData[src][dst] == -1){
		costTableData[src][dst] = cst;
		te[dst-1].nextHop = dst;
		te[dst-1].cost = cst;
		printf("%d-->%d : updated to %d\n", src, dst, cst);
	}
	printTable();
	printCostTable();
	
	printf("%d-->%d : updated to %d\n", src, dst, cst);
}

void disableLink(int servID){
	printf("%d disabled\n", servID);
	initVector[servID] = -1;
	char sssss[512];
	sprintf(sssss, "updt %d -1", t.myID);
	sendUDPPacket(te[servID-1].ipAddress, te[servID-1].port, sssss, strlen(sssss));
	printf("Sent disable packet to neighbor: %d \n", servID);
}

void sendPacketsToNeighbors(){
	int jkm;
	for(jkm=1;jkm<=noOfClients;jkm++){
		if(initVector[jkm] > 0){
			char sendBuffer[MAXBUFLEN], abc[20];
			strcpy(sendBuffer,"");
			sprintf(abc, "%d", noOfClients);
			strcat(sendBuffer, abc);
			strcat(sendBuffer," ");
			sprintf(abc, "%d", t.port);
			strcat(sendBuffer, abc);
			strcat(sendBuffer," ");
			strcat(sendBuffer, t.ipAddress);
			
			//					printf("%s\n", sendBuffer);
			int i;
			for(i=1;i<=noOfClients;i++){
				strcat(sendBuffer," ");
				strcat(sendBuffer, te[i-1].ipAddress);
				strcat(sendBuffer," ");
				sprintf(abc, "%d", te[i-1].port);
				strcat(sendBuffer, abc);
				strcat(sendBuffer," 0x0");
				
				strcat(sendBuffer," ");
				sprintf(abc, "%d", i);
				strcat(sendBuffer, abc);
				strcat(sendBuffer," ");
				sprintf(abc, "%d", costTableData[t.myID][i]);
				strcat(sendBuffer, abc);
				//				printf("Hello\n");
			}
			//					strcat(sendBuffer, abc);
			//					strcat(sendBuffer," ");
			//					strcat(sendBuffer, t.ipAddress);
			//			printf("%s\n", sendBuffer);
			sendUDPPacket(te[jkm-1].ipAddress, te[jkm-1].port, sendBuffer, strlen(sendBuffer));
			printf("Sent DV packet to neighbor: %d \n", jkm);
		}
	}
}

void updateCostTable(int a[][2], int len){
	int j, row, i, x;
	row = -2;
	
	//printf("in upDate :\n");
	for(j=0;j<len;j++)
	{
		//printf("%d\t", a[j][0]);
		if(a[j][0] == 0)
			row = j+1;
	}
	//printf("\n%d\n", row);
	timeoutCount[row] = 0;
	
	for(i=1;i<=noOfClients;i++){
		costTableData[row][i] = a[i-1][0];
	}
	int min;
	for(i=1;i<=noOfClients;i++){
		min = -1;
		int x = 65535;
		for(j=1;j<=noOfClients;j++){
			if(initVector[j] != -1 && initVector[j] != -1){
				if(j == t.myID){
					//look into initVector
					//distance from myself to myself + myself to i
					x = initVector[j] + initVector[i];
					
				}
				else
				{
					//look in costTableData
					//distance from myself to peer + peer to i
					if(costTableData[j][i] == -1)
						x = -1;
					else
						x = initVector[j] + costTableData[j][i];
				}
				if(x != -1)
					if(min > x || min == -1)
					{
						min = x;
						nxtH[i] = j;
					}
			}
		}
		costTableData[t.myID][i] = min;
		costTableData[t.myID][t.myID] = 0;
	}
	
//	printCostTable();
	
//	for(i=1;i<=noOfClients;i++)
//	{
//		int min = -1;
//		for(j=1;j<=noOfClients;j++)
//		{
//			if(initVector[j] != -1 && a[j][0] != -1)
//				x = initVector[j] + a[j][0];
//			else
//				x = -1;
//			
//			if(x != -1 && x < min){
//				min = x;
//			}
//				
//			
//		}
		
//		costTableData[t.myID][i] = min;
//		costTableData[t.myID][j] = getDist(a);
//	}
	
//	printCostTable();
	
//	row = -2;
//	for(j=0;j<len;j++)
//	{
//		printf("%d\t", a[j][0]);
//		if(a[j][0] == 0)
//			row = j+1;
//	}
//	printf("\n%d\n", row);
	
	
//	for(j=1;j<=noOfClients;j++){
//		if(noOfClients != row){
////			if(costTableData[t.myID][j] != -1)
//			costTableData[row][j] = a[j-1][0];
////			costTableData[row][j] = a[j-1][0] + costTableData[t.myID][j];
//			if(costTableData[row][j]<0)
//				costTableData[row][j] = -1;
//		}
//	}
//	for(j=1;j<=noOfClients;j++){
//		int min = costTableData[t.myID][j];
//		for(i=1;i<=noOfClients;i++){
//			if((costTableData[t.myID][i] + costTableData[i][j]) < min && costTableData[i][j]>0){
//				min = costTableData[i][j] + costTableData[t.myID][i];
//			}
//			if(min < 0){
//				min = costTableData[i][j] + costTableData[t.myID][i];
//			}
//		}
//		if(min != costTableData[t.myID][j])
//			costTableData[t.myID][j] = min;
//	}
//	printCostTable();
//	for(j=1;j<=noOfClients;j++){
//		int min = costTableData[row][j];
//		for(i=1;i<=noOfClients;i++){
//			if(min>costTableData[i][j] && costTableData[i][j] != -1){
//				min = costTableData[i][j];
//				te[t.myID-1].nextHop = i;
//			}
//		}
//		costTableData[row][j] = min;
//	}
//	printCostTable();
}
