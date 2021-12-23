#include <iostream>
#include <WS2tcpip.h>
#include <thread>
#include <fstream>
#include <filesystem>
#include <stdlib.h>
#include <Windows.h>
#include <deque>
#include <chrono>
#include <ctime>
#include <unordered_set>
#include "CongestionControl.h"
#pragma comment (lib,"ws2_32.lib")

#define MSS 1460

#define SIZE 500

#define HEADER 9

#define TIMEOUT 1

using namespace std;

struct data_packet {
	/*Header*/
	bool fin;
	uint16_t cksum;
	uint16_t len;
	uint32_t seqno;
	/*Data*/
	char data[SIZE];
};

struct ack_packet {
	uint16_t cksum;
	uint16_t len;
	uint32_t ackno;

};

void parse_server_in( int* server_port_num, int* seed , double* plp) {


	ifstream server_in("server.in.txt");

	string line;

	getline(server_in, line);

	*server_port_num = stoi(line);

	getline(server_in, line);

	*seed = stoi(line);

	getline(server_in, line );

	*plp = stod(line);
}


void send_ack_packet( SOCKET my_socket , sockaddr_in* client_addr , int size_of_client , int ackno ) {

	ack_packet pkt;

	ZeroMemory(&pkt, sizeof(pkt));

	pkt.ackno = ackno ;

	pkt.len = 8;

	void* ptr = &pkt;

	int num_of_bytes = sendto(my_socket, (char*)ptr, sizeof(pkt), 0,
		(SOCKADDR*)client_addr, size_of_client);

	if (num_of_bytes == SOCKET_ERROR) {
		cerr << "Error in sending ack to client !! " << WSAGetLastError << endl;
		closesocket(my_socket);
		WSACleanup();
		exit(-1);
	}

}


void resend_packet( SOCKET my_socket , data_packet* pkt , ifstream* reader
					, sockaddr_in* client_addr , int size_of_client ) {

	void* ptr = nullptr ;

	ptr = pkt;

	//cout << "sending data packet with seqno " << pkt->seqno << " to client" << endl;

	//cout << "size of packet is " << pkt->len << " bytes" << endl;
	int num_of_bytes = sendto(my_socket, (char*)ptr, sizeof(*pkt), 0
		, (SOCKADDR*)client_addr, size_of_client);

	if (num_of_bytes == SOCKET_ERROR) {
		cerr << "Error in sending data packets to client !! " << WSAGetLastError << endl;
		reader->close();
		closesocket(my_socket);
		WSACleanup();
		exit(-1);
	}
}

void GBN (SOCKET my_socket , sockaddr_in* client_addr , int size_of_client , string file_name , double plp , double seed ) {


	// sending data to the client

	ofstream summary("cwnd_size.txt");      // used for plots of cwnd size

	summary << "cwnd_size\n";

	int num_of_sent_packets = 0;

	int num_of_time_outs = 0;            // to print number of timeouts after finishing

	int num_of_3_dup_acks = 0;			 // to print number of times 3 dup acks received after finishing

	srand(seed);                         // set the seed of pseudo random number generator

	bool send_packet ;                   // determine to send packet or not

	auto start = chrono::system_clock::now() ;       // timer for first packet (unacked) at cwnd 

	fd_set my_set;               // to use it with select()

	FD_ZERO(&my_set);

	FD_SET(my_socket, &my_set);

	timeval wait_time;       // waiting time used with select

	wait_time.tv_sec = 0;

	wait_time.tv_usec = 1;

	CongestionControl* controller = new CongestionControl();   // congestion control for connection

	deque<data_packet> dequ;           // dequeue for cwnd

	void* ptr;                        // used in recvfrom and sendto to send structures over socket

	int num_of_bytes;              // number of bytes after sent or received after sendto or recvfrom

    uint32_t send_base = 0 ;         // start of cwnd

	uint32_t send_cwnd = MSS ;      // cwnd size

	uint32_t next_seqno = 0 ;       // next sequence number to send

	ifstream reader(file_name);     // file to send
	
	bool finish = false ;           // to indicate end of transmission


	while (true)
	{
		// sending data packet to the client

		if ( send_base + send_cwnd > next_seqno  && !finish)
		{

			int data_bytes = min(send_cwnd + send_base - next_seqno, SIZE + HEADER) - HEADER - 1;

			if (data_bytes > 0) {

				data_packet data_pkt;

				ZeroMemory(&data_pkt, sizeof(data_pkt));

				data_pkt.fin = false;

				data_pkt.seqno = next_seqno;

				reader.read(data_pkt.data, data_bytes);

				int num_of_read_bytes = string(data_pkt.data).size();

				data_pkt.len = HEADER + num_of_read_bytes + 1;

				next_seqno += num_of_read_bytes;

				char temp[4];

				ZeroMemory(temp, 4);

				reader.read(temp, 1);

				// try to read extra character to check for end of file

				if (string(temp).size() != 1) {
					// reached end of the file
					finish = true;
					data_pkt.fin = true;
				}
				else 
					reader.seekg(-1, ios_base::cur);

				dequ.push_front(data_pkt);

				// start time if not started

				if (dequ.size() == 1)
					start = chrono::system_clock::now();

				cout << "sending data packet with seqno " << data_pkt.seqno << " to client\n" << endl;

				cout << "size of packet is " << data_pkt.len << " bytes\n" << endl;

				ptr = &data_pkt;

				// increase number of sent packets even if it wasn't sent as we simulate loss

				num_of_sent_packets++;

				send_packet = (rand() % 100) > plp ;

				if ( send_packet ) {
					num_of_bytes = sendto(my_socket, (char*)ptr, sizeof(data_pkt), 0
						, (SOCKADDR*)client_addr, size_of_client);

					if (num_of_bytes == SOCKET_ERROR) {
						cerr << "Error in sending data packets to client !! " << WSAGetLastError << endl;
						reader.close();
						closesocket(my_socket);
						WSACleanup();
						exit(-1);
					}
				}
			}



		}
		
		fd_set copy = my_set;

		// to avoid the case if all acks are lost so we don't block on recvfrom

		int ready = select(my_socket + 1, &copy, nullptr, nullptr, &wait_time);

		if (ready > 0) {
			ack_packet ack_pkt;

			ZeroMemory(&ack_pkt, sizeof(ack_pkt));

			ptr = &ack_pkt;

			num_of_bytes = recvfrom(my_socket, (char*)ptr, sizeof(ack_pkt), 0,
				(SOCKADDR*)client_addr, &size_of_client);

			if (num_of_bytes == SOCKET_ERROR) {
				cerr << "Error in receiving ack packets from client !! " << WSAGetLastError << endl;
				reader.close();
				summary.close();
				closesocket(my_socket);
				WSACleanup();
				exit(-1);
			}

			cout << "ack packet with ackno " << ack_pkt.ackno << " received ...\n" << endl;

			if (ack_pkt.ackno > send_base && ack_pkt.ackno <= next_seqno) {

				// cumulative ack

				send_base = ack_pkt.ackno;   

				while (!dequ.empty() && (dequ.back().seqno < ack_pkt.ackno))
					dequ.pop_back();

				// reset timer if there're unacked packets in cwnd

				if (!dequ.empty())
					start = chrono::system_clock::now();

				// every data packet is acked and end of file reached .....

				if (dequ.empty() && finish) {
					cout << "transmission ended ...\n" << endl;
					cout << "number of sent packets : " << num_of_sent_packets << "\n" << endl;
					cout << "number of timeouts : " << num_of_time_outs << "\n" << endl;
					cout << "number of 3 dup acks : " << num_of_3_dup_acks << "\n" << endl;
					reader.close();
					summary.close();
					closesocket(my_socket);
					WSACleanup();
					exit(0);
				}

				// update cwnd size and state of congestion controller

				controller->new_ack();

				send_cwnd = controller->get_cwnd_size();

				summary << send_cwnd << "\n";

			}

			// duplicate ack arrived

			// resend if 3 dup acks received

			else if (ack_pkt.ackno == send_base) {  
				int x = controller->duplicate_ack(send_base);
				send_cwnd = controller->get_cwnd_size();
				summary << send_cwnd << "\n";
				if (x != -1) {
					num_of_3_dup_acks++;
					num_of_sent_packets++;
					resend_packet(my_socket, &(dequ.back()), &reader, client_addr, size_of_client);
					cout << "three duplicate acks .. resending packet " <<
						dequ.back().seqno << " needed by client... \n" << endl;
				}
			}
		}

		chrono::duration<double> elapsed = chrono::system_clock::now() - start;

		// resend on timeout and reset timer

		// update cwnd size and controller state

		if (elapsed.count() >= TIMEOUT && !dequ.empty() ) {
			num_of_time_outs++;
			cout << "resending data packet with seqno " << dequ.back().seqno << " because of time out ...  \n" << endl;
			controller->timeout();
			send_cwnd = controller->get_cwnd_size();
			summary << send_cwnd << "\n";
			num_of_sent_packets++;
			resend_packet(my_socket, & (dequ.back()) ,&reader , client_addr , size_of_client);
			start = chrono::system_clock::now();
		}

	}

}

void stop_and_wait(SOCKET my_socket, sockaddr_in* client_addr, int size_of_client, string file_name, double plp, double seed) {

	// sending data to the client

	auto start = chrono::system_clock::now();

	int num_of_sent_packets = 0;

	int num_of_time_outs = 0;            // to print number of timeouts after finishing

	int num_of_3_dup_acks = 0;			 // to print number of times 3 dup acks received after finishing

	fd_set my_set;               // to use it with select()

	FD_ZERO(&my_set);

	FD_SET(my_socket, &my_set);

	srand(seed);

	bool send_packet ;

	timeval wait_time;       // waiting time used with select

	wait_time.tv_sec = 0;

	wait_time.tv_usec = 1;

	void* ptr;                        // used in recvfrom and sendto to send structures over socket

	int num_of_bytes;              // number of bytes after sent or received after sendto or recvfrom

	uint32_t next_seqno = 0;       // next sequence number to send

	ifstream reader(file_name);     // file to send

	bool finish = false;           // to indicate end of transmission


	while (true)
	{
		// sending data packet to the client

		data_packet data_pkt;

		ZeroMemory(&data_pkt, sizeof(data_pkt));

		data_pkt.fin = false;

		data_pkt.seqno = next_seqno;

		reader.read(data_pkt.data, SIZE - 1);

		int num_of_read_bytes = string(data_pkt.data).size();

		data_pkt.len = HEADER + num_of_read_bytes + 1;

		next_seqno += num_of_read_bytes;

		// try to read extra character to check for end of file

		char temp[4];

		ZeroMemory(temp, 4);

		reader.read(temp, 1);

		if (string(temp).size() != 1) {
			// reached end of the file
			finish = true;
			data_pkt.fin = true;
		}
		else
			reader.seekg(-1, ios_base::cur);

		cout << "sending data packet with seqno " << data_pkt.seqno << " to client\n" << endl;

		cout << "size of packet is " << data_pkt.len << " bytes\n" << endl;

		ptr = &data_pkt;

		num_of_sent_packets++;

		send_packet = (rand() % 100) > plp;

		if (send_packet) {
			num_of_bytes = sendto(my_socket, (char*)ptr, sizeof(data_pkt), 0
				, (SOCKADDR*)client_addr, size_of_client);

			if (num_of_bytes == SOCKET_ERROR) {
				cerr << "Error in sending data packets to client !! " << WSAGetLastError << endl;
				reader.close();
				closesocket(my_socket);
				WSACleanup();
				exit(-1);
			}
		}

		start = chrono::system_clock::now();

		while (true) {

			fd_set copy = my_set;
			
			int ready = select(my_socket + 1, &copy, nullptr, nullptr, &wait_time);

			if (ready > 0) {
				ack_packet ack_pkt;

				ZeroMemory(&ack_pkt, sizeof(ack_pkt));

				ptr = &ack_pkt;

				num_of_bytes = recvfrom(my_socket, (char*)ptr, sizeof(ack_pkt), 0,
					(SOCKADDR*)client_addr, &size_of_client);

				if (num_of_bytes == SOCKET_ERROR) {
					cerr << "Error in receiving ack packets from client !! " << WSAGetLastError << endl;
					reader.close();
					closesocket(my_socket);
					WSACleanup();
					exit(-1);
				}

				cout << "ack packet with ackno " << ack_pkt.ackno << " received ...\n" << endl;

				if (ack_pkt.ackno == next_seqno) {

					if (finish) {
						cout << "transmission ended ...\n" << endl;
						cout << "number of sent packets : " << num_of_sent_packets << "\n" << endl;
						cout << "number of timeouts : " << num_of_time_outs << "\n" << endl;
						cout << "number of 3 dup acks : " << num_of_3_dup_acks << "\n" << endl;
						reader.close();
						closesocket(my_socket);
						WSACleanup();
						exit(0);
					}
					break;
				}

			}
			chrono::duration<double> elapsed = chrono::system_clock::now() - start;
			if ( elapsed.count() > TIMEOUT ) {
				// resend after time out
				num_of_time_outs++;
				cout << "resending data packet with seqno " << data_pkt.seqno << " because of time out ...  \n" << endl;
				resend_packet(my_socket, &data_pkt, &reader, client_addr, size_of_client);
				num_of_sent_packets++;
				start = chrono::system_clock::now();
			}
		}

	}

}

void main() {

	// setting IP address and port number of server

	int server_port_num;

	int seed;

	double plp;

	parse_server_in( &server_port_num, &seed , &plp);


	// initialize winsock to use it

	WSADATA data;

	WORD version = MAKEWORD(2, 2);

	int state = WSAStartup(version, &data);

	if (state != 0) {

		cerr << "Error while initializing winsock !! " << endl;
		return;
	}

	// // creating a UDP socket to send datagrams to client

	SOCKET my_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (my_socket == INVALID_SOCKET) {
		// error while creating socket
		cerr << "Error while creating server socket !!" << WSAGetLastError << endl;
		WSACleanup();
		return;

	}

	// bind IP address and port number to the socket

	string server_IP = "127.0.0.1";

	sockaddr_in binding;

	ZeroMemory(&binding, sizeof(binding));

	binding.sin_family = AF_INET;

	binding.sin_port = htons(server_port_num);

	inet_pton(AF_INET, server_IP.c_str(), &binding.sin_addr);

	state = bind(my_socket, (SOCKADDR*)&binding, sizeof(binding));

	if (state != 0) {

		cerr << "Error binding to IP and port number !!" << WSAGetLastError << endl;
		closesocket(my_socket);
		WSACleanup();
		return;
	}


	// accept new connections and create threads to serve clients

	sockaddr_in client_addr;

	int size_of_client = sizeof(client_addr);

	char file_name[100] ;

	char client_ip[20];

	while (true) {

		ZeroMemory(&client_addr,  size_of_client );

		ZeroMemory(file_name, sizeof(file_name));

		ZeroMemory(client_ip, sizeof(client_ip));
		
		int num_of_bytes = recvfrom(my_socket, file_name , 100 , 0, (SOCKADDR*)&client_addr, &size_of_client);

		if (num_of_bytes == SOCKET_ERROR) {
			cerr << "Error in receiving data at server socket !! " << WSAGetLastError << endl;
			closesocket(my_socket);
			WSACleanup();
			return;
		}

		inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, 20 );

		cout << "client " << string(client_ip) << " sent a datagram to request " << string(file_name) << endl;

		// sending ack before sending data packets of file

		send_ack_packet(my_socket, &client_addr, size_of_client, 0 );

		//stop_and_wait(my_socket , &client_addr, size_of_client ,  file_name , plp , seed );

		GBN(my_socket, &client_addr, size_of_client, file_name, plp, seed);

	}

	closesocket(my_socket);

	WSACleanup();


}