#include <WinSock2.h>
#include <iostream>
#include <WS2tcpip.h>
#include <cstdlib>
#include <time.h>
#include <stdio.h>
#pragma comment (lib,"wws2_32.lib")
using namespace std;

int getUdpSocket() {//生成并初始化一个udpsocket
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

sockaddr_in getAddr(const char* ip, int port) {//根据输入的IP地址和端口号构造出一个存储着地址的sockaddr_in类型
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.S_un.S_addr = inet_addr(ip);
	return addr;
}


char* RequestDownloadPack(char* content, int& datalen, int type) {//构造RPQ数据包，下载文件
	int len = strlen(content);
	char* buf = new char[len + 2 + 2 + type];
	buf[0] = 0x00;
	buf[1] = 0x01;
	memcpy(buf + 2, content, len);
	memcpy(buf + 2 + len, "\0", 1);
	if (type == 5)
		memcpy(buf + 2 + len + 1, "octet", 5);//二进制文件
	else
		memcpy(buf + 2 + len + 1, "netascii", 8);//文本文件
	memcpy(buf + 2 + len + 1 + type, "\0", 1);
	datalen = len + 2 + 1 + type + 1;
	return buf;
}

char* RequestUploadPack(char* content, int& datalen, int type) {//构造WRQ数据包，上传文件
	int len = strlen(content);//获取长度
	char* buf = new char[len + 2 + 2 + type];//文件长度（len）+TFTP报文段的首部（2）+两个"\0"(2)+文件类型（type）
	//设置报文段首部
	buf[0] = 0x00;
	buf[1] = 0x02;
	//复制文件内容
	memcpy(buf + 2, content, len);
	//末尾添"\0"
	memcpy(buf + 2 + len, "\0", 1);
	//末尾添加文件类型
	if (type == 5)
		memcpy(buf + 2 + len + 1, "octet", 5);//二进制文件
	else
		memcpy(buf + 2 + len + 1, "netascii", 8);//文本文件
	//	//末尾添"\0"
	memcpy(buf + 2 + len + 1 + type, "\0", 1);

	datalen = len + 2 + 1 + type + 1;//计算总长度
	return buf;
}

char* AckPack(short& no) {//构造ACK数据包
	char* ack = new char[4];
	ack[0] = 0x00;
	ack[1] = 0x04;
	no = htons(no);//将主机字节序转换成网络字节序
	memcpy(ack + 2, &no, 2);
	no = ntohs(no);
	return ack;
}


char* MakeData(short& no, FILE* f, int& datalen) {//构造DATA数据包
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

void print_time(FILE* fp) {//日志
	time_t t;
	time(&t);
	char stime[100];
	strcpy(stime, ctime(&t));
	*(strchr(stime, '\n')) = '\0';
	fprintf(fp, "[%s]", stime);
	return;
}




int main() {
	FILE* fp = fopen("TFTP_client.log", "a");//打开日志

	char commonbuf[2048];
	int buflen;//数据长度
	int Numbertokill;//超时次数
	int Killtime;
	clock_t start, end;//分别记录开始和结束的时间，用于计算传输速率 
	double runtime;
	SOCKET sock = getUdpSocket();//获取一个UDP套接字
	sockaddr_in addr;//表示internet地址

	int recvTimeout = 1000;//1s
	int sendTimeout = 1000;

	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&recvTimeout, sizeof(int));
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&sendTimeout, sizeof(int));



	while (1) {
		//选择功能
		printf("#######		1.上传文件			######");
		printf("#######		2.下载文件			######");
		printf("#######		0.关闭TFTP客户端		######");
		int choice;
		scanf("%d", &choice);



		//选择上传文件功能
		if (choice == 1) {
			//第一个数据包总是发向本机的69端口
			addr = getAddr("127. 0 .0 .1 ", 69);

			//用户输入文件的名称，储存在变量name中
			printf(" 请输入要上传文件的全名： \n");
			char name[1000];
			scanf("%s", name);

			//用户选择文件的格式
			int type;
			printf(" 请选择上传文件的方式： 1. netascii 2.octet\n");
			scanf("%d%", &type);
			if (type == 1)//文本文件
				type = 8;
			else
				type = 5;//二进制文件

			int datalen;
			char* sendData = RequestUploadPack(name, datalen, type);//调用此函数，构造一个WRQ数据包，存储在sendData里
			buflen = datalen;//记录长度

			Numbertokill = 1;//表示数据包超时的次数
			memcpy(commonbuf, sendData, datalen);//将数据包送入commonbuf

			int res = sendto(sock, sendData, datalen, 0, (sockaddr*)&addr, sizeof(addr));//第一次开始发送WRQ包
			start = clock();//开始计时
			print_time(fp);
			fprintf(fp, "send WRQ for file:% s\n", name);
			Killtime = 1;//表示sendto的超时次数，与recv_from超市分开计算



			while (res != datalen) {//如果sendto函数失败，则立即重新sendto

				std::cout << "send WRQ failed:" << Killtime << "times" << std::endl;
				//规定sendto失败十次就自动结束传输
				if (Killtime <= 10) {
					res = sendto(sock, commonbuf, buflen, 0, (sockaddr*)&addr, sizeof(addr));//重传
					Killtime++;//上传次数加一
				}
				else
					break;
			}


			if (Killtime > 10)//超过十次就自动结束传输
				continue;

			delete[]sendData;
			FILE* f = fopen(name, "rb");

			if (f == NULL) {//打开文件失败
				std::cout << "Fi1e" << name << "open failed!" << std::endl;
				continue;
			}

			short block = 0;
			datalen = 0;
			int RST = 0;
			int Fullsize = 0;

			while (1) {
				char buf[1024];
				sockaddr_in server;//从反馈的数据包中获得其分配的端口号信息

				int len = sizeof(server);
				res = recvfrom(sock, buf, 1024, 0, (sockaddr*)&server, &len);

				if (res == -1) {//未收到数据

					printf("%d ", Numbertokill);
					if (Numbertokill > 10) {//连续十次未收到响应
						printf("No acks get. transmission failed！\n");
						print_time(fp);
						fprintf(fp, "Upload file: %s failed. \n", name);//打印错误提示并结束传输
						break;
					}

					int res = sendto(sock, commonbuf, buflen, 0, (sockaddr*)&addr, sizeof(addr));//重传上一个数据包
					RST++;
					std::cout << "resend last blk" << std::endl;

					Killtime = 1;//同上处理sendto超时的情况
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

					if (flag == 4) {//收到的是ACK数据包

						short no;
						memcpy(&no, buf + 2, 2);
						no = ntohs(no);
						if (no == block) {
							addr = server;
							if (feof(f) && datalen != 516) {//最后一个包的字节长度不为516，则说明文件已全部上传完毕
								std::cout << "upload finished !" << std::endl;//结束传输
								end = clock();

								runtime = (double)(end - start) / CLOCKS_PER_SEC;//计算传输时间
								print_time(fp);
								printf("Average transmission rate: ％.21f kb/s \n", Fullsize / runtime / 1000);
								fprintf(fp, "Upload file: %s finished.resent times: %d;Fu11size: %d\n", name, RST, Fullsize);
								break;
							}

							block++;//没上传完，则制作下一个DATA包
							sendData = MakeData(block, f, datalen);
							buflen = datalen;
							Fullsize += datalen - 4;//fullsize要去除数据包中头部的长度
							Numbertokill = 1;//重置当前数据包的重发次数
							memcpy(commonbuf, sendData, datalen);//更新commonbuf中的内容，准备下一次可能的重传

							if (sendData == NULL) {//如果在构建数据包的过程中失败了
								std::cout << "Fi1e reading mistakes!" << std::endl;
								break;
							}//将刚刚构造的DATA报发送出去
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
							std::cout << "Pack No= " << block << std::endl;//向用户输出当前正在传输的数据包编号

						}
					}



					if (flag == 5) {//处理ERROR包
						short errorcode;
						memcpy(&errorcode, buf + 2, 2);//拆解ERROR获得错误码
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


		//选择下载文件功能
		if (choice == 2) {
			addr = getAddr("127.0.0.1", 69);
			printf("请输入要下载文件的全名：\n");
			char name[1000];
			int type;
			scanf("%s", name);
			printf("请选择下载文件的方式：1.netascii 2.octet\n");
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
		

		//关闭客户端
		if (choice == 0)
			break;

	}
	
	fclose(fp);//关闭日志

	return 0;
}
	


