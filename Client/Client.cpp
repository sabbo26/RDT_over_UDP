#include <iostream>
#include <string>
#include <WS2tcpip.h>
#include <fstream>
#include <iphlpapi.h>
#include <unordered_map>
#include <chrono>
#include <ctime>
#define SIZE 500

#pragma comment (lib , "ws2_32.lib")

using namespace std;

struct data_packet {
	/*Header*/
	bool fin;
	uint16_t cksum;
	uint16_t len ;
	uint32_t seqno;
	/*Data*/
	char data[SIZE];
};

struct ack_packet{
	uint16_t cksum;
	uint16_t len;
	uint32_t ackno;

};

void parse_client_in( string* server_IP , int* server_port_num , string* file_name  ) {


	ifstream client_in("client.in.txt");

	string line ;

	getline(client_in, *server_IP );

	getline(client_in, line);

	*server_port_num = stoi(line);

	getline(client_in, *file_name);

	client_in.close();
}


bool check_similarity( ifstream* model , ifstream* received ) {

	char buffer[10240];

	ZeroMemory(buffer,10240);

	model->read(buffer, 10240);

	string temp_1(buffer);

	ZeroMemory(buffer, 10240);

	received->read(buffer, 10240);

	string temp_2(buffer);


	return temp_1._Equal(temp_2);

}


void GBN( SOCKET my_socket  , string file_name ) {

	auto start = chrono::system_clock::now();

	unordered_map<int, data_packet> buffer;

	bool finish = FALSE;

	sockaddr_in server_addr;

	int size_of_server_addr = sizeof(server_addr);

	ofstream output(file_name);

	uint32_t ackno = 0 ;

	while (true)
	{

		data_packet pkt;

		ZeroMemory(&pkt, sizeof(pkt));

		ZeroMemory(&server_addr, sizeof(server_addr));

		void* ptr = &pkt;

		int num_of_bytes = recvfrom(my_socket, (char*) ptr , sizeof(pkt) , 0, (SOCKADDR*)&server_addr, &size_of_server_addr);

		if (num_of_bytes == SOCKET_ERROR) {

			cerr << "Error while receiving data packets from the server !!" << WSAGetLastError << endl;

			closesocket(my_socket);

			output.close();

			WSACleanup();

			exit(-1);

		}

		if (pkt.seqno == ackno) {

			cout << "data packet with in order seqno "<< ackno << " received.. \n" << endl;

			cout << "size of packet is " << pkt.len << " bytes\n" << endl;

			buffer[ackno] = pkt;
			
			while (buffer.contains(ackno))
			{
				pkt = buffer[ackno];

				string data = string(pkt.data);

				if (pkt.fin)
					finish = TRUE;

				output << data;

				ackno += data.size();

			}

		}
		else if ( pkt.seqno > ackno )  {
			cout << "data packet with out of order seqno " << pkt.seqno << " insted of "<< ackno << " received.. \n" << endl;
			cout << "buffering ... \n" << endl;
			if (pkt.seqno > ackno) {
				buffer[pkt.seqno] = pkt;
			}
		}

		ack_packet pkt_2;

		pkt_2.ackno = ackno;

		pkt_2.len = 8;

		ptr = &pkt_2;

		num_of_bytes = sendto(my_socket, (char*) ptr , sizeof(pkt_2) , 0, (SOCKADDR*)&server_addr,
			size_of_server_addr);

		if (num_of_bytes == SOCKET_ERROR) {

			cerr << "Error while sending ack packets from the server !!" << WSAGetLastError << endl;

			closesocket(my_socket);

			output.close();

			WSACleanup();

			exit(-1);

		}
		else {
			cout << "ack " << ackno << " sent to server...\n" << endl;
		}
		if (finish) {

			cout << "transmission ended .. \n" << endl;

			chrono::duration<double> elapsed = chrono::system_clock::now() - start;

			cout << "connection duration is " << elapsed.count() << " seconds\n" << endl;

			cout << "throughput is " << (((double)ackno) / elapsed.count()) << " byte/s \n" << endl;

			output.close();

			ifstream temp_1(file_name);

			ifstream temp_2("file_at_server.txt");
			
			if (check_similarity(&temp_2, &temp_1))
				cout << "the two files are the same ... done !!\n" << endl;
			else
				cout << "the two files are not the same ... there's a problem !!\n" << endl;
			closesocket(my_socket);
			WSACleanup();
			exit(0);
		}
	}


}




void main() {

	// setting IP address and port number of server

	string server_IP  ;

	int server_port_num ;

	string file_name ;

	parse_client_in(&server_IP, &server_port_num, &file_name);

	// initialize winsock

	WSADATA data;

	WORD version = MAKEWORD(2, 2);

	int state = WSAStartup(version, &data);

	if (state != 0) {
		// error initializing winsock
		cerr << "Error while initializing WSA !! " << endl;
		return;

	}

	// creating a UDP socket to send datagrams to server

	SOCKET my_socket = socket(AF_INET, SOCK_DGRAM ,  IPPROTO_UDP );

	if (my_socket == INVALID_SOCKET) {
		// error while creating socket
		cerr << "Error while creating client socket !!" << WSAGetLastError << endl;
		WSACleanup();
		return;

	}

	// setting addr for server

	sockaddr_in server_addr;

	ZeroMemory(&server_addr, sizeof(server_addr));

	server_addr.sin_family = AF_INET;

	server_addr.sin_port = htons(server_port_num);

	inet_pton(AF_INET, server_IP.c_str() , &server_addr.sin_addr);

	// sending file name to the server

	int num_of_bytes = sendto(my_socket, file_name.c_str() ,
		file_name.size() + 1 , 0, (SOCKADDR*)&server_addr, sizeof(server_addr));
	
	if (num_of_bytes == SOCKET_ERROR) {

		cerr << "Error while sending file name to the server !!" << WSAGetLastError << endl;
		
		closesocket(my_socket);

		WSACleanup();
		
		return;

	}
	else {
		cout << "file name sent successfully.. "  << endl;
	}
	// getting first ack message from server

	ack_packet pkt;

	ZeroMemory(&pkt, sizeof(pkt));

	void* ptr = &pkt;

	int size_of_server_addr = sizeof(server_addr);

	ZeroMemory(&server_addr, size_of_server_addr);

	num_of_bytes = recvfrom(my_socket, (char*) ptr , sizeof(pkt) , 0, (SOCKADDR*)&server_addr, & size_of_server_addr );

	if (num_of_bytes == SOCKET_ERROR) {

		cerr << "Error receiving first ack from the server !!" << WSAGetLastError << endl;

		closesocket(my_socket);

		WSACleanup();

		return;

	}
	else {
		cout << "first ack received from server successfully..." << endl;
	}

	// receiving file from server
	
	GBN(my_socket,file_name);

	closesocket(my_socket);

	WSACleanup();

}
