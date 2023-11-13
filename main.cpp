#include <WinSock2.h>
#include <iostream>
#include <WS2tcpip.h>
#include <cstdlib>
#include <time.h>
#pragma comment (lib,"wws2_32.lib")
using namespace std;

int getUdpSocket() {
	WORD ver = MAKEWORD(2, 2);
	WSADATA lpDATA;
	int err = WSAStartup(ver, &lpDATA);
	if (err != 0) return -1;
	int udpsocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (udpsocket == INVALID_SOCKET) return -2;
	return udpsocket;
}

sockaddr_in getAddr(const char* ip, int port) {
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.S_un.S_addr = inet_addr(ip);
	return addr;
}


char* RequestDownloadPack(char* content, int& datalen, int type) {
	int len = strlen(content);
	char* buf = new char[len + 2 + 2 + type];
	buf[0] = 0x00;
	buf[1] = 0x01;
	memcpy(buf + 2, content, len);
	memcpy(buf + 2 + len, "\0", 1);
	if (type == 5)
		memcpy(buf + 2 + len + 1, "octet", 5);
	else
		memcpy(buf + 2 + len + 1, "netascii", 8);
	memcpy(buf + 2 + len + 1 + type, "\0", 1);
	datalen = len + 2 + 1 + type + 1;
	return buf;
}

char* RequestUploadPack(char* content, int& datalen, int type) {
	int len = strlen(content);
	char* buf = new char[len + 2 + 2 + type];
	buf[0] = 0x00;
	buf[1] = 0x02;
	memcpy(buf + 2, content, len);
	memcpy(buf + 2 + len, "\0", 1);
	if (type == 5)
		memcpy(buf + 2 + len + 1, "octet", 5);
	else
		memcpy(buf + 2 + len + 1, "netascii", 8);
	memcpy(buf + 2 + len + 1 + type, "\0", 1);
	datalen = len + 2 + 1 + type + 1;
	return buf;
}

char* AckPack(short& no) {
	char* ack = new char[4];
	ack[0] = 0x00;
	ack[1] = 0x04;
	no = htons(no);
	memcpy(ack + 2, &no, 2);
	no = ntohs(no);
	return ack;
}


char* MakeData(short& no, FILE* f, int& datalen) {
	char temp[512];
	int sum = fread(temp, 1, 512, f);
	if (!ferror(f)) {
		char* buf = new char[4 + sum];
		buf[0] = 0x00;
		buf[1] = 0x03;
		no = htons(no);
		memcpy(buf + 2, &no, 2);
		no = ntohs(no);
		memcpy(buf + 4, temp, sum);
		datalen = sum + 4;
		return buf;

	}
	return NULL;
}

void print_time(FILE* fp) {
	time_t t;
	time(&t);
	char stime[100];
	strcpy(stime, ctime(&t));
	*(strchr(stime, '\n')) = '\0';
	fprintf(fp, "[%s]", stime);
	return;
}




int main() {
	FILE* fp = fopen("TFTP_client.log", "a");
	char commonbuf[2048];
	int buflen;
	int Numbertokill;
	int Killtime;
	clock_t start, end;
	double runtime;
	SOCKET sock = getUdpSocket();
	sockaddr_in addr;
	int recvTimeout = 1000;
	int sendTimeout = 1000;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&recvTimeout, sizeof(int));
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&sendTimeout, sizeof(int));
	while (1) {
		printf("#######1.上传文件######");
		printf("#######2.下载文件######");
		printf("#######0.关闭TFTP客户端######");
		int choice;
		scanf("%d", &choice);
		if (choice == 1) {
			addr = getAddr("127. 0 .0 .1 ", 69);
			printf(" 请输入要上传文件的全名： \n");
			char name[1000];
			int type;
			scanf("%s", name);
			printf(" 请选择上传文件的方式： 1. netascii 2.octet\n");
			scanf("%d%", &type);
			if (type == 1)
				type = 8;
			else
				type = 5;
			int datalen;
			char* sendData = RequestUploadPack(name, datalen, type);
			buflen = datalen;
			Numbertokill = 1;
			memcpy(commonbuf, sendData, datalen);
			int res = sendto(sock, sendData, datalen, 0, (sockaddr*)&addr, sizeof(addr));
			start = clock();
			print_time(fp);
			fprintf(fp, "send WRQ for file:% s\n", name);
			Killtime = 1;



			while (res != datalen) {
				std::cout << "send WRQ failed:" << Killtime << "times" << std::endl;
				if (Killtime <= 10) {
					res = sendto(sock, commonbuf, buflen, 0, (sockaddr*)&addr, sizeof(addr));
					Killtime++;
				}
				else
					break;
			}
			if (Killtime > 10)
				continue;
			delete[] sendData;
			FILE* f = fopen(name, "rb");

			if (f == NULL) {
				std::cout << "Fi1e" << name << "open failed!" << std::endl;
				continue;
			}
			short block = 0;
			datalen = 0;
			int RST = 0;
			int Fullsize = 0;
			while (1) {
				char buf[1024];
				sockaddr_in server;

				int len = sizeof(server);
				res = recvfrom(sock, buf, 1024, 0, (sockaddr*)&server, &len);
				if (res == -1) {
					printf("%d ", Numbertokill);
					if (Numbertokill > 10) {

						printf("No acks get.transmission failed\n");
						print_time(fp);
						fprintf(fp, "Upload file: %s failed. \n", name);
						break;
					}
					int res = sendto(sock, commonbuf, buflen, 0, (sockaddr*)&addr, sizeof(addr));
					RST++;
					std::cout << "resend last blk" << std::endl;
					Killtime = 1;
					while (res != buflen) {
						std::cout << "resend last blk failed:" << Killtime << "times" << std::endl;
						if (Killtime <= 10) {
							res = sendto(sock, commonbuf, buflen, 0, (sockaddr*)&addr, sizeof(addr));
							Killtime++;
						}
						else
							break;
						if (Killtime > 10)
							break;
						Numbertokill++;
					}
					if (res > 0) {
						short flag;
						memcpy(&flag, buf, 2);
						flag = ntohs(flag);
						if (flag == 4) {

							short no;
							memcpy(&no, buf + 2, 2);
							no = ntohs(no);
							if (no == block) {
								addr = server;

								if (feof(f) && datalen != 516) {

									std::cout << "upload finished !" << std::endl;
									end = clock();

									runtime = (double)(end - start) / CLOCKS_PER_SEC;
									print_time(fp);
									printf("Average transmission rate: ％.21f kb/s \n", Fullsize / runtime / 1000);
									fprintf(fp, "Upload file: %s finished.resent times: %d;Fu11size: %d\n", name, RST, Fullsize);
									break;
								}
								block++;
								sendData = MakeData(block, f, datalen);
								buflen = datalen;
								Fullsize += datalen - 4;
								Numbertokill = 1;
								memcpy(commonbuf, sendData, datalen);
								if (sendData == NULL) {
									std::cout << "Fi1e read ing mistakes!" << std::endl;
									break;
								}
								int res = sendto(sock, sendData, datalen, 0, (sockaddr*)&addr, sizeof(addr));
								Killtime = 1;
								while (res != datalen) {
									std::cout << "send block" << block << "failed :" << Killtime << "times" << std::endl;
									if (Killtime <= 10) {
										res = sendto(sock, commonbuf, buflen, 0, (sockaddr*)&addr, sizeof(addr));
										Killtime++;
									}
									else
										break;
								}
								if (Killtime > 10)
									continue;
								std::cout << "Pack № = " << block << std::endl;
							}
						}
						if (flag == 5) {
							short errorcode;
							memcpy(&errorcode, buf + 2, 2);
							errorcode = ntohs(errorcode);
							char strError[1024];
							int iter = 0;
							while (*(buf + iter + 4) != 0) {
								memcpy(strError + iter, buf + iter + 4, 1);
							}

						}



					}






				}


			}
		}






	}

