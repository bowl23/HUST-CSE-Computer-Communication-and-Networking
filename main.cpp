#include <WinSock2.h>
#include <iostream>
#include <WS2tcpip.h>
#include <cstdlib>
#include <time.h>
#include <stdio.h>
#pragma comment (lib,"wws2_32.lib")
using namespace std;

int getUdpSocket() {//���ɲ���ʼ��һ��udpsocket
	WORD ver = MAKEWORD(2, 2);
	WSADATA lpDATA;
	int err = WSAStartup(ver, &lpDATA);
	if (err != 0) 
		return -1;
	int udpsocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (udpsocket == INVALID_SOCKET) 
		return -2;
	return udpsocket;
}

sockaddr_in getAddr(const char* ip, int port) {//���������IP��ַ�Ͷ˿ںŹ����һ���洢�ŵ�ַ��sockaddr_in����
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.S_un.S_addr = inet_addr(ip);
	return addr;
}


char* RequestDownloadPack(char* content, int& datalen, int type) {//����RPQ���ݰ��������ļ�
	int len = strlen(content);
	char* buf = new char[len + 2 + 2 + type];
	buf[0] = 0x00;
	buf[1] = 0x01;
	memcpy(buf + 2, content, len);
	memcpy(buf + 2 + len, "\0", 1);
	if (type == 5)
		memcpy(buf + 2 + len + 1, "octet", 5);//�������ļ�
	else
		memcpy(buf + 2 + len + 1, "netascii", 8);//�ı��ļ�
	memcpy(buf + 2 + len + 1 + type, "\0", 1);
	datalen = len + 2 + 1 + type + 1;
	return buf;
}

char* RequestUploadPack(char* content, int& datalen, int type) {//����WRQ���ݰ����ϴ��ļ�
	int len = strlen(content);//��ȡ����
	char* buf = new char[len + 2 + 2 + type];//�ļ����ȣ�len��+TFTP���Ķε��ײ���2��+����"\0"(2)+�ļ����ͣ�type��
	//���ñ��Ķ��ײ�
	buf[0] = 0x00;
	buf[1] = 0x02;
	//�����ļ�����
	memcpy(buf + 2, content, len);
	//ĩβ��"\0"
	memcpy(buf + 2 + len, "\0", 1);
	//ĩβ����ļ�����
	if (type == 5)
		memcpy(buf + 2 + len + 1, "octet", 5);//�������ļ�
	else
		memcpy(buf + 2 + len + 1, "netascii", 8);//�ı��ļ�
	//	//ĩβ��"\0"
	memcpy(buf + 2 + len + 1 + type, "\0", 1);

	datalen = len + 2 + 1 + type + 1;//�����ܳ���
	return buf;
}

char* AckPack(short& no) {//����ACK���ݰ�
	char* ack = new char[4];
	ack[0] = 0x00;
	ack[1] = 0x04;
	no = htons(no);//�������ֽ���ת���������ֽ���
	memcpy(ack + 2, &no, 2);
	no = ntohs(no);
	return ack;
}


char* MakeData(short& no, FILE* f, int& datalen) {//����DATA���ݰ�
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
	else
		return NULL;
}

void print_time(FILE* fp) {//��־
	time_t t;
	time(&t);
	char stime[100];
	strcpy(stime, ctime(&t));
	*(strchr(stime, '\n')) = '\0';
	fprintf(fp, "[%s]", stime);
	return;
}




int main() {
	FILE* fp = fopen("TFTP_client.log", "a");//����־

	char commonbuf[2048];
	int buflen;//���ݳ���
	int Numbertokill;//��ʱ����
	int Killtime;
	clock_t start, end;//�ֱ��¼��ʼ�ͽ�����ʱ�䣬���ڼ��㴫������ 
	double runtime;
	SOCKET sock = getUdpSocket();//��ȡһ��UDP�׽���
	sockaddr_in addr;//��ʾinternet��ַ

	int recvTimeout = 1000;//1s
	int sendTimeout = 1000;

	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&recvTimeout, sizeof(int));
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&sendTimeout, sizeof(int));



	while (1) {
		//ѡ����
		printf("#######		1.�ϴ��ļ�			######");
		printf("#######		2.�����ļ�			######");
		printf("#######		0.�ر�TFTP�ͻ���		######");
		int choice;
		scanf("%d", &choice);



		//ѡ���ϴ��ļ�����
		if (choice == 1) {
			//��һ�����ݰ����Ƿ��򱾻���69�˿�
			addr = getAddr("127. 0 .0 .1 ", 69);

			//�û������ļ������ƣ������ڱ���name��
			printf(" ������Ҫ�ϴ��ļ���ȫ���� \n");
			char name[1000];
			scanf("%s", name);

			//�û�ѡ���ļ��ĸ�ʽ
			int type;
			printf(" ��ѡ���ϴ��ļ��ķ�ʽ�� 1. netascii 2.octet\n");
			scanf("%d%", &type);
			if (type == 1)//�ı��ļ�
				type = 8;
			else
				type = 5;//�������ļ�

			int datalen;
			char* sendData = RequestUploadPack(name, datalen, type);//���ô˺���������һ��WRQ���ݰ����洢��sendData��
			buflen = datalen;//��¼����

			Numbertokill = 1;//��ʾ���ݰ���ʱ�Ĵ���
			memcpy(commonbuf, sendData, datalen);//�����ݰ�����commonbuf

			int res = sendto(sock, sendData, datalen, 0, (sockaddr*)&addr, sizeof(addr));//��һ�ο�ʼ����WRQ��
			start = clock();//��ʼ��ʱ
			print_time(fp);
			fprintf(fp, "send WRQ for file:% s\n", name);
			Killtime = 1;//��ʾsendto�ĳ�ʱ��������recv_from���зֿ�����



			while (res != datalen) {//���sendto����ʧ�ܣ�����������sendto

				std::cout << "send WRQ failed:" << Killtime << "times" << std::endl;
				//�涨sendtoʧ��ʮ�ξ��Զ���������
				if (Killtime <= 10) {
					res = sendto(sock, commonbuf, buflen, 0, (sockaddr*)&addr, sizeof(addr));//�ش�
					Killtime++;//�ϴ�������һ
				}
				else
					break;
			}


			if (Killtime > 10)//����ʮ�ξ��Զ���������
				continue;

			delete[]sendData;
			FILE* f = fopen(name, "rb");

			if (f == NULL) {//���ļ�ʧ��
				std::cout << "Fi1e" << name << "open failed!" << std::endl;
				continue;
			}

			short block = 0;
			datalen = 0;
			int RST = 0;
			int Fullsize = 0;

			while (1) {
				char buf[1024];
				sockaddr_in server;//�ӷ��������ݰ��л�������Ķ˿ں���Ϣ

				int len = sizeof(server);
				res = recvfrom(sock, buf, 1024, 0, (sockaddr*)&server, &len);

				if (res == -1) {//δ�յ�����

					printf("%d ", Numbertokill);
					if (Numbertokill > 10) {//����ʮ��δ�յ���Ӧ
						printf("No acks get. transmission failed��\n");
						print_time(fp);
						fprintf(fp, "Upload file: %s failed. \n", name);//��ӡ������ʾ����������
						break;
					}

					int res = sendto(sock, commonbuf, buflen, 0, (sockaddr*)&addr, sizeof(addr));//�ش���һ�����ݰ�
					RST++;
					std::cout << "resend last blk" << std::endl;

					Killtime = 1;//ͬ�ϴ���sendto��ʱ�����
					while (res != buflen) {
						std::cout << "resend last blk failed:" << Killtime << "times" << std::endl;
						if (Killtime <= 10) {
							res = sendto(sock, commonbuf, buflen, 0, (sockaddr*)&addr, sizeof(addr));
							Killtime++;
						}
						else
							break;
					}
					if (Killtime > 10)
						break;
					Numbertokill++;
				}

				if (res > 0) {
					short flag;
					memcpy(&flag, buf, 2);
					flag = ntohs(flag);

					if (flag == 4) {//�յ�����ACK���ݰ�

						short no;
						memcpy(&no, buf + 2, 2);
						no = ntohs(no);
						if (no == block) {
							addr = server;
							if (feof(f) && datalen != 516) {//���һ�������ֽڳ��Ȳ�Ϊ516����˵���ļ���ȫ���ϴ����
								std::cout << "upload finished !" << std::endl;//��������
								end = clock();

								runtime = (double)(end - start) / CLOCKS_PER_SEC;//���㴫��ʱ��
								print_time(fp);
								printf("Average transmission rate: ��.21f kb/s \n", Fullsize / runtime / 1000);
								fprintf(fp, "Upload file: %s finished.resent times: %d;Fu11size: %d\n", name, RST, Fullsize);
								break;
							}

							block++;//û�ϴ��꣬��������һ��DATA��
							sendData = MakeData(block, f, datalen);
							buflen = datalen;
							Fullsize += datalen - 4;//fullsizeҪȥ�����ݰ���ͷ���ĳ���
							Numbertokill = 1;//���õ�ǰ���ݰ����ط�����
							memcpy(commonbuf, sendData, datalen);//����commonbuf�е����ݣ�׼����һ�ο��ܵ��ش�

							if (sendData == NULL) {//����ڹ������ݰ��Ĺ�����ʧ����
								std::cout << "Fi1e reading mistakes!" << std::endl;
								break;
							}//���ոչ����DATA�����ͳ�ȥ
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
							std::cout << "Pack No= " << block << std::endl;//���û������ǰ���ڴ�������ݰ����

						}
					}



					if (flag == 5) {//����ERROR��
						short errorcode;
						memcpy(&errorcode, buf + 2, 2);//���ERROR��ô�����
						errorcode = ntohs(errorcode);
						char strError[1024];
						int iter = 0;
						while (*(buf + iter + 4) != 0) {
							memcpy(strError + iter, buf + iter + 4, 1);
							++iter;
						}
						*(strError + iter + 1) = '\0';
						std::cout << "Error" << errorcode << "" << strError << std::endl;
						print_time(fp);
						fprintf(fp, "Error %d %s\n", errorcode, strError);
						break;
					}
				}
			}
			fclose(f);
		}


		//ѡ�������ļ�����
		if (choice == 2) {
			addr = getAddr("127.0.0.1", 69);
			printf("������Ҫ�����ļ���ȫ����\n");
			char name[1000];
			int type;
			scanf("%s", name);
			printf("��ѡ�������ļ��ķ�ʽ��1.netascii 2.octet\n");
			scanf("%d", &type);
			if (type == 1)
				type = 8;
			else
				type = 5;
			int datalen;
			char* sendData = RequestDownloadPack(name, datalen, type);
			buflen = datalen;
			Numbertokill = 1;
			memcpy(commonbuf, sendData, datalen);
			int res = sendto(sock, sendData, datalen, 0, (sockaddr*)&addr, sizeof(addr));
			start = clock();
			print_time(fp);
			Killtime = 1;
			while (res != datalen)
			{
				std::cout << "send RRQ failed:" << Killtime << "times" << std::endl;
				if (Killtime <= 10) {
					res = sendto(sock, commonbuf, buflen, 0, (sockaddr*)&addr, sizeof(addr));
					Killtime++;
				}
				else
					break;
			}
			if (Killtime > 10)
				continue;
			delete[]sendData;
			FILE* f = fopen(name, "wb");
			if (f == NULL) {
				std::cout << "File" << name << "open failed" << std::endl;
				continue;
			}
			int want_recv = 1;
			int RST = 0;
			int Fullsize = 0;
			while (1) {
				char buf[1024];
				sockaddr_in server;
				int len = sizeof(server);
				res = recvfrom(sock, buf, 1024, 0, (sockaddr*)&server, &len);
				if (res == -1)
					if (Numbertokill > 10) {
						printf("No block get.transmission failed\n");
						print_time(fp);
						fprintf(fp, "Download file: %s failed.\n", name);
						break;
					}
				int res = sendto(sock, commonbuf, buflen, 0, (sockaddr*)&addr, sizeof(addr));
				RST++;
				std::cout << "resend last blk" << std::endl;
				Killtime = 1;
				while (res != buflen)
				{
					std::cout << "resend last blk failed:" << Killtime << "time" << std::endl;
					if (Killtime <= 10) {
						res = sendto(sock, commonbuf, buflen, 0, (sockaddr*)&addr, sizeof(addr));
						Killtime++;
					}
					else
						break;
				}
				if (Killtime > 10)
					break;
				Numbertokill++;
				if (res > 0) {
					short flag;
					memcpy(&flag, buf, 2);
					flag = ntohs(flag);
					if (flag == 3) {
						addr = server;
						short no;
						memcpy(&no, buf + 2, 2);
						no = ntohs(no);
						std::cout << "Pack No=" << no << std::endl;
						char* ack = AckPack(no);
						int sendlen = sendto(sock, ack, 4, 0, (sockaddr*)&addr, sizeof(addr));
						Killtime = 1;
						while (sendlen != 4)
						{
							std::cout << "resend last ack failed:" << Killtime << "times" << std::endl;
							if (Killtime <= 10) {
								sendlen = sendto(sock, ack, 4, 0, (sockaddr*)&addr, sizeof(addr));
								Killtime++;
							}
							else
								break;
						}
						if (Killtime > 10)
							break;
						if (no == want_recv) {
							buflen = 4;
							Numbertokill = 1;
							memcpy(commonbuf, ack, 4);
							fwrite(buf + 4, res - 4, 1, f);
							Fullsize += res - 4;
							if (res - 4 >= 0 && res - 4 < 512) {
								std::cout << "download finished" << std::endl;
								end = clock();
								runtime = (double)(end - start) / CLOCKS_PER_SEC;
								print_time(fp);
								printf("Average transmission rate: &.2f kb/s\n", Fullsize / runtime / 1000);
								fprintf(fp, "Download file:%s finished.resent times:%d;Fullsize:%d\n", name, RST, Fullsize);
								break;
							}
							want_recv++;
						}
					}
					if (flag == 5) {
						short errorcode;
						memcpy(&errorcode, buf + 2, 2);
						errorcode = ntohs(errorcode);
						char strError[1024];
						int iter = 0;
						while (*(buf + iter + 4) != 0)
						{
							memcpy(strError + iter, buf + iter + 4, 1);
							++iter;
						}
						*(strError + iter + 1) = '\0';
						std::cout << "Error" << errorcode << " " << strError << std::endl;
						print_time(fp);
						fprintf(fp, "Error %d %s\n", errorcode, strError);
						break;
					}
				}
			}
			
			fclose(f);
		}
		

		//�رտͻ���
		if (choice == 0)
			break;

	}
	
	fclose(fp);//�ر���־

	return 0;
}
	


