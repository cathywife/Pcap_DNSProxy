// This code is part of Pcap_DNSProxy
// Pcap_DNSProxy, A local DNS server base on WinPcap and LibPcap.
// Copyright (C) 2012-2014 Chengr28
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


#include "Pcap_DNSProxy.h"

extern Configuration Parameter;
extern timeval ReliableSocketTimeout, UnreliableSocketTimeout;
extern std::vector<AddressRange> *AddressRangeUsing;
extern std::deque<DNSCacheData> DNSCacheList;
extern std::mutex DNSCacheListLock, AddressRangeLock;
extern DNSCurveConfiguration DNSCurveParameter;
extern AlternateSwapTable AlternateSwapList;

//Local DNS server initialization
size_t __fastcall MonitorInit()
{
//Get Hop Limits/TTL with normal DNS request.
	if (Parameter.DNSTarget.IPv6.sin6_family != NULL)
	{
		std::thread IPv6TestDoaminThread(DomainTestRequest, AF_INET6);
		IPv6TestDoaminThread.detach();
	}
	if (Parameter.DNSTarget.IPv4.sin_family != NULL)
	{
		std::thread IPv4TestDoaminThread(DomainTestRequest, AF_INET);
		IPv4TestDoaminThread.detach();
	}

//Get Hop Limits/TTL with ICMP Echo.
	if (Parameter.ICMPOptions.ICMPSpeed > 0)
	{
		if (Parameter.DNSTarget.IPv4.sin_family != NULL)
		{
			std::thread ICMPThread(ICMPEcho);
			ICMPThread.detach();
		}

		if (Parameter.DNSTarget.IPv6.sin6_family != NULL)
		{
			std::thread ICMPv6Thread(ICMPv6Echo);
			ICMPv6Thread.detach();
		}
	}

//Set Preferred DNS servers switcher.
	if (Parameter.DNSTarget.Alternate_IPv6.sin6_family != NULL || Parameter.DNSTarget.Alternate_IPv4.sin_family != NULL || 
		Parameter.DNSTarget.Alternate_Local_IPv6.sin6_family != NULL || Parameter.DNSTarget.Alternate_Local_IPv4.sin_family != NULL || 
		DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.sin6_family != NULL || DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.sin_family != NULL)
	{
		std::thread AlternateServerSwitcherThread(AlternateServerSwitcher);
		AlternateServerSwitcherThread.detach();
	}

	SOCKET_DATA LocalhostData = {0};
//Set localhost socket(IPv6/UDP).
	LocalhostData.Socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	Parameter.LocalSocket[0] = LocalhostData.Socket;
	if (Parameter.OperationMode == LISTEN_PROXYMODE) //Proxy Mode
		((PSOCKADDR_IN6)&LocalhostData.SockAddr)->sin6_addr = in6addr_loopback;
	else //Server Mode, Priavte Mode and Custom Mode
		((PSOCKADDR_IN6)&LocalhostData.SockAddr)->sin6_addr = in6addr_any;
	LocalhostData.SockAddr.ss_family = AF_INET6;
	((PSOCKADDR_IN6)&LocalhostData.SockAddr)->sin6_port = Parameter.ListenPort;
	LocalhostData.AddrLen = sizeof(sockaddr_in6);

	std::thread IPv6UDPMonitor(UDPMonitor, LocalhostData);
	memset(&LocalhostData, 0, sizeof(SOCKET_DATA));

//Set localhost socket(IPv6/TCP).
	LocalhostData.Socket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	Parameter.LocalSocket[2U] = LocalhostData.Socket;
	if (Parameter.OperationMode == LISTEN_PROXYMODE) //Proxy Mode
		((PSOCKADDR_IN6)&LocalhostData.SockAddr)->sin6_addr = in6addr_loopback;
	else //Server Mode, Priavte Mode and Custom Mode
		((PSOCKADDR_IN6)&LocalhostData.SockAddr)->sin6_addr = in6addr_any;
	LocalhostData.SockAddr.ss_family = AF_INET6;
	((PSOCKADDR_IN6)&LocalhostData.SockAddr)->sin6_port = Parameter.ListenPort;
	LocalhostData.AddrLen = sizeof(sockaddr_in6);

	std::thread IPv6TCPMonitor(TCPMonitor, LocalhostData);
	memset(&LocalhostData, 0, sizeof(SOCKET_DATA));

//Set localhost socket(IPv4/UDP).
	LocalhostData.Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	Parameter.LocalSocket[1U] = LocalhostData.Socket;
	if (Parameter.OperationMode == LISTEN_PROXYMODE) //Proxy Mode
		((PSOCKADDR_IN)&LocalhostData.SockAddr)->sin_addr.S_un.S_addr = htonl(INADDR_LOOPBACK);
	else //Server Mode, Priavte Mode and Custom Mode
		((PSOCKADDR_IN)&LocalhostData.SockAddr)->sin_addr.S_un.S_addr = INADDR_ANY;
	LocalhostData.SockAddr.ss_family = AF_INET;
	((PSOCKADDR_IN)&LocalhostData.SockAddr)->sin_port = Parameter.ListenPort;
	LocalhostData.AddrLen = sizeof(sockaddr_in);

	std::thread IPv4UDPMonitor(UDPMonitor, LocalhostData);
	memset(&LocalhostData, 0, sizeof(SOCKET_DATA));

//Set localhost socket(IPv4/TCP).
	LocalhostData.Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	Parameter.LocalSocket[3U] = LocalhostData.Socket;
	if (Parameter.OperationMode == LISTEN_PROXYMODE) //Proxy Mode
		((PSOCKADDR_IN)&LocalhostData.SockAddr)->sin_addr.S_un.S_addr = htonl(INADDR_LOOPBACK);
	else //Server Mode, Priavte Mode and Custom Mode
		((PSOCKADDR_IN)&LocalhostData.SockAddr)->sin_addr.S_un.S_addr = INADDR_ANY;
	LocalhostData.SockAddr.ss_family = AF_INET;
	((PSOCKADDR_IN)&LocalhostData.SockAddr)->sin_port = Parameter.ListenPort;
	LocalhostData.AddrLen = sizeof(sockaddr_in);
			
	std::thread IPv4TCPMonitor(TCPMonitor, LocalhostData);
	memset(&LocalhostData, 0, sizeof(SOCKET_DATA));
/*
//Unblocking Mode
	ULONG SocketMode = 1U;
	if (ioctlsocket(Parameter.LocalhostSocket ,FIONBIO, &SocketMode) == SOCKET_ERROR)
		return EXIT_FAILURE;

//Preventing other sockets from being forcibly bound to the same address and port.
	int Val = 1;
	if (setsockopt(Parameter.LocalhostSocket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (PSTR)&Val, sizeof(int)) == SOCKET_ERROR)
		return EXIT_FAILURE;
*/
//Join threads.
	if (IPv6UDPMonitor.joinable())
		IPv6UDPMonitor.join();
	if (IPv4UDPMonitor.joinable())
		IPv4UDPMonitor.join();
	if (IPv6TCPMonitor.joinable())
		IPv6TCPMonitor.join();
	if (IPv4TCPMonitor.joinable())
		IPv4TCPMonitor.join();

	return EXIT_SUCCESS;
}

//Local DNS server with UDP protocol
size_t __fastcall UDPMonitor(const SOCKET_DATA LocalhostData)
{
//Socket initialization
	if (LocalhostData.Socket == INVALID_SOCKET)
	{
		PrintError(WINSOCK_ERROR, L"UDP Monitor socket initialization error", WSAGetLastError(), nullptr, NULL);
		return EXIT_FAILURE;
	}

//Check parameter.
	if (Parameter.DNSTarget.IPv6.sin6_family == NULL && LocalhostData.AddrLen == sizeof(sockaddr_in6) || //IPv6
		Parameter.DNSTarget.IPv4.sin_family == NULL && LocalhostData.AddrLen == sizeof(sockaddr_in)) //IPv4
	{
		closesocket(LocalhostData.Socket);
		return EXIT_FAILURE;
	}

//Bind socket to port.
	if (bind(LocalhostData.Socket, (PSOCKADDR)&LocalhostData.SockAddr, LocalhostData.AddrLen) == SOCKET_ERROR)
	{
		closesocket(LocalhostData.Socket);
		PrintError(WINSOCK_ERROR, L"Bind UDP Monitor socket error", WSAGetLastError(), nullptr, NULL);

		return EXIT_FAILURE;
	}

//Set socket timeout.
	if (setsockopt(LocalhostData.Socket, SOL_SOCKET, SO_SNDTIMEO, (PSTR)&UnreliableSocketTimeout, sizeof(timeval)) == SOCKET_ERROR || 
		setsockopt(LocalhostData.Socket, SOL_SOCKET, SO_RCVTIMEO, (PSTR)&UnreliableSocketTimeout, sizeof(timeval)) == SOCKET_ERROR)
	{
		closesocket(LocalhostData.Socket);
		PrintError(WINSOCK_ERROR, L"Set UDP socket timeout error", WSAGetLastError(), nullptr, NULL);

		return EXIT_FAILURE;
	}

	std::shared_ptr<char> Buffer(new char[PACKET_MAXSIZE*BUFFER_RING_MAXNUM]());
//Start Monitor.
	SSIZE_T RecvLen = 0;
	size_t Index[] = {0, 0};

	in6_addr *IPv6Addr = nullptr;
	in_addr *IPv4Addr = nullptr;
	dns_hdr *pdns_hdr = nullptr;
	while (true)
	{
		memset(Buffer.get() + PACKET_MAXSIZE * Index[0], 0, PACKET_MAXSIZE);
		if (Parameter.EDNS0Label) //EDNS0 Label
			RecvLen = recvfrom(LocalhostData.Socket, Buffer.get() + PACKET_MAXSIZE * Index[0], PACKET_MAXSIZE - sizeof(dns_edns0_label), NULL, (PSOCKADDR)&LocalhostData.SockAddr, (PINT)&LocalhostData.AddrLen);
		else 
			RecvLen = recvfrom(LocalhostData.Socket, Buffer.get() + PACKET_MAXSIZE * Index[0], PACKET_MAXSIZE, NULL, (PSOCKADDR)&LocalhostData.SockAddr, (PINT)&LocalhostData.AddrLen);

	//Check address(es).
		if (LocalhostData.AddrLen == sizeof(sockaddr_in6)) //IPv6
		{
			IPv6Addr = &((PSOCKADDR_IN6)&LocalhostData.SockAddr)->sin6_addr;
			if (memcmp(IPv6Addr, &Parameter.DNSTarget.IPv6.sin6_addr, sizeof(in6_addr)) == 0 || 
				memcmp(IPv6Addr, &Parameter.DNSTarget.Alternate_IPv6.sin6_addr, sizeof(in6_addr)) == 0 || 
				memcmp(IPv6Addr, &Parameter.DNSTarget.Local_IPv6.sin6_addr, sizeof(in6_addr)) == 0 || 
				memcmp(IPv6Addr, &Parameter.DNSTarget.Alternate_Local_IPv6.sin6_addr, sizeof(in6_addr)) == 0 || 
		//Check Private Mode(IPv6).
				(Parameter.OperationMode == LISTEN_PRIVATEMODE && 
				!(IPv6Addr->u.Byte[0] >= 0xFC && IPv6Addr->u.Byte[0] <= 0xFD || //Unique Local Unicast address/ULA(FC00::/7, Section 2.5.7 in RFC 4193)
				IPv6Addr->u.Byte[0] == 0xFE && IPv6Addr->u.Byte[1U] >= 0x80 && IPv6Addr->u.Byte[1U] <= 0xBF || //Link-Local Unicast Contrast address(FE80::/10, Section 2.5.6 in RFC 4291)
				IPv6Addr->u.Word[6U] == 0 && IPv6Addr->u.Word[7U] == htons(0x0001))) || //Loopback address(::1, Section 2.5.3 in RFC 4291)
		//Check Custom Mode(IPv6).
				(Parameter.OperationMode == LISTEN_CUSTOMMODE && !CustomModeFilter(IPv6Addr, AF_INET6)))
					continue;
		}
		else { //IPv4
			IPv4Addr = &((PSOCKADDR_IN)&LocalhostData.SockAddr)->sin_addr;
			if (IPv4Addr->S_un.S_addr == Parameter.DNSTarget.IPv4.sin_addr.S_un.S_addr || 
				IPv4Addr->S_un.S_addr == Parameter.DNSTarget.Alternate_IPv4.sin_addr.S_un.S_addr || 
				IPv4Addr->S_un.S_addr == Parameter.DNSTarget.Local_IPv4.sin_addr.S_un.S_addr || 
				IPv4Addr->S_un.S_addr == Parameter.DNSTarget.Alternate_Local_IPv4.sin_addr.S_un.S_addr ||
		//Check Private Mode(IPv4).
				(Parameter.OperationMode == LISTEN_PRIVATEMODE && 
				!(IPv4Addr->S_un.S_un_b.s_b1 == 0x0A || //Private class A address(10.0.0.0/8, Section 3 in RFC 1918)
				IPv4Addr->S_un.S_un_b.s_b1 == 0x7F || //Loopback address(127.0.0.0/8, Section 3.2.1.3 in RFC 1122)
				IPv4Addr->S_un.S_un_b.s_b1 == 0xAC && IPv4Addr->S_un.S_un_b.s_b2 >= 0x10 && IPv4Addr->S_un.S_un_b.s_b2 <= 0x1F || //Private class B address(172.16.0.0/16, Section 3 in RFC 1918)
				IPv4Addr->S_un.S_un_b.s_b1 == 0xC0 && IPv4Addr->S_un.S_un_b.s_b2 == 0xA8)) || //Private class C address(192.168.0.0/24, Section 3 in RFC 1918)
		//Check Custom Mode(IPv4).
				(Parameter.OperationMode == LISTEN_CUSTOMMODE && !CustomModeFilter(IPv4Addr, AF_INET)))
					continue;
		}

	//UDP Truncated check
		if (RecvLen > (SSIZE_T)(Parameter.EDNS0PayloadSize - sizeof(dns_edns0_label)))
		{
			if (Parameter.EDNS0Label || //EDNS0 Lebal
				RecvLen > (SSIZE_T)Parameter.EDNS0PayloadSize)
			{
			//Make packet(s) with EDNS0 Lebal.
				dns_edns0_label *EDNS0 = nullptr;
				pdns_hdr = (dns_hdr *)(Buffer.get() + PACKET_MAXSIZE * Index[0]);
				pdns_hdr->Flags = htons(DNS_SQRNE_TC);
			
				if (pdns_hdr->Additional == 0)
				{
					pdns_hdr->Additional = htons(0x0001);
					EDNS0 = (dns_edns0_label *)(Buffer.get() + PACKET_MAXSIZE * Index[0] + RecvLen);
					EDNS0->Type = htons(DNS_EDNS0_RECORDS);
					EDNS0->UDPPayloadSize = htons((uint16_t)Parameter.EDNS0PayloadSize);
					RecvLen += sizeof(dns_edns0_label);
				}
				else {
					EDNS0 = (dns_edns0_label *)(Buffer.get() + PACKET_MAXSIZE * Index[0] + RecvLen - sizeof(dns_edns0_label));
					if (EDNS0->Type == htons(DNS_EDNS0_RECORDS))
						EDNS0->UDPPayloadSize = htons((uint16_t)Parameter.EDNS0PayloadSize);
				}

			//Send request.
				sendto(LocalhostData.Socket, Buffer.get() + PACKET_MAXSIZE * Index[0], (int)RecvLen, NULL, (PSOCKADDR)&LocalhostData.SockAddr, LocalhostData.AddrLen);
				continue;
			}
		}

	//Receive process.
		pdns_hdr = (dns_hdr *)(Buffer.get() + PACKET_MAXSIZE * Index[0]);
		if (RecvLen >= (SSIZE_T)(sizeof(dns_hdr) + 1U + sizeof(dns_qry)))
		{
		//EDNS0 Label
			if (Parameter.EDNS0Label)
			{
				dns_edns0_label *EDNS0 = nullptr;
				if (pdns_hdr->Additional == 0) //No additional
				{
					pdns_hdr->Additional = htons(0x0001);
					EDNS0 = (dns_edns0_label *)(Buffer.get() + PACKET_MAXSIZE * Index[0] + RecvLen);
					EDNS0->Type = htons(DNS_EDNS0_RECORDS);
					EDNS0->UDPPayloadSize = htons((uint16_t)Parameter.EDNS0PayloadSize);
					RecvLen += sizeof(dns_edns0_label);
				}
				else { //Already EDNS0 Lebal
					EDNS0 = (dns_edns0_label *)(Buffer.get() + PACKET_MAXSIZE * Index[0] + RecvLen - sizeof(dns_edns0_label));
					EDNS0->Type = htons(DNS_EDNS0_RECORDS);
					EDNS0->UDPPayloadSize = htons((uint16_t)Parameter.EDNS0PayloadSize);
				}

			//DNSSEC
				if (Parameter.DNSSECRequest)
				{
					pdns_hdr->FlagsBits.AD = ~pdns_hdr->FlagsBits.AD; //Local DNSSEC Server validate
					pdns_hdr->FlagsBits.CD = ~pdns_hdr->FlagsBits.CD; //Client validate
					EDNS0->Z_Bits.DO = ~EDNS0->Z_Bits.DO; //Accepts DNSSEC security RRs
				}
			}

		//Request process
			if (LocalhostData.AddrLen == sizeof(sockaddr_in6)) //IPv6
			{
				std::thread RecvProcess(RequestProcess, Buffer.get() + PACKET_MAXSIZE * Index[0], RecvLen, LocalhostData, IPPROTO_UDP, Index[1U]);
				RecvProcess.detach();
			}
			else { //IPv4
				std::thread RecvProcess(RequestProcess, Buffer.get() + PACKET_MAXSIZE * Index[0], RecvLen, LocalhostData, IPPROTO_UDP, Index[1U] + QUEUE_MAXLENGTH);
				RecvProcess.detach();
			}

			Index[0] = (Index[0] + 1U) % BUFFER_RING_MAXNUM;
			Index[1U] = (Index[1U] + 1U) % QUEUE_MAXLENGTH;
		}
		else { //Incorrect packets
			pdns_hdr->Flags = htons(DNS_SQRNE_SF);
			sendto(LocalhostData.Socket, Buffer.get() + PACKET_MAXSIZE * Index[0], (int)RecvLen, NULL, (PSOCKADDR)&LocalhostData.SockAddr, LocalhostData.AddrLen);
		}
	}

	closesocket(LocalhostData.Socket);
	return EXIT_SUCCESS;
}

//Local DNS server with TCP protocol
size_t __fastcall TCPMonitor(const SOCKET_DATA LocalhostData)
{
//Socket initialization
	if (LocalhostData.Socket == INVALID_SOCKET)
	{
		PrintError(WINSOCK_ERROR, L"TCP Monitor socket initialization error", WSAGetLastError(), nullptr, NULL);
		return EXIT_FAILURE;
	}

//Check parameter.
	if (Parameter.DNSTarget.IPv6.sin6_family == NULL && LocalhostData.AddrLen == sizeof(sockaddr_in6) || //IPv6
		Parameter.DNSTarget.IPv4.sin_family == NULL && LocalhostData.AddrLen == sizeof(sockaddr_in)) //IPv4
	{
		closesocket(LocalhostData.Socket);
		return EXIT_FAILURE;
	}

//Set socket timeout.
	if (setsockopt(LocalhostData.Socket, SOL_SOCKET, SO_SNDTIMEO, (PSTR)&ReliableSocketTimeout, sizeof(timeval)) == SOCKET_ERROR || 
		setsockopt(LocalhostData.Socket, SOL_SOCKET, SO_RCVTIMEO, (PSTR)&ReliableSocketTimeout, sizeof(timeval)) == SOCKET_ERROR)
	{
		closesocket(LocalhostData.Socket);
		PrintError(WINSOCK_ERROR, L"Set TCP socket timeout error", WSAGetLastError(), nullptr, NULL);

		return EXIT_FAILURE;
	}

//Bind socket to port.
	if (bind(LocalhostData.Socket, (PSOCKADDR)&LocalhostData.SockAddr, LocalhostData.AddrLen) == SOCKET_ERROR)
	{
		closesocket(LocalhostData.Socket);
		PrintError(WINSOCK_ERROR, L"Bind TCP Monitor socket error", WSAGetLastError(), nullptr, NULL);

		return EXIT_FAILURE;
	}

//Listen request from socket.
	if (listen(LocalhostData.Socket, SOMAXCONN) == SOCKET_ERROR)
	{
		closesocket(LocalhostData.Socket);
		PrintError(WINSOCK_ERROR, L"TCP Monitor socket listening initialization error", WSAGetLastError(), nullptr, NULL);

		return EXIT_FAILURE;
	}

//Start Monitor.
	SOCKET_DATA ClientData = {0};
	size_t Index = 0;
	in6_addr *IPv6Addr = nullptr;
	in_addr *IPv4Addr = nullptr;
	ClientData.AddrLen = LocalhostData.AddrLen;
	while (true)
	{
		memset(&ClientData, 0, sizeof(SOCKET_DATA) - sizeof(int));
		ClientData.Socket = accept(LocalhostData.Socket, (PSOCKADDR)&ClientData.SockAddr, (PINT)&(LocalhostData.AddrLen));
		if (ClientData.Socket == INVALID_SOCKET)
			continue;

	//Check address(es).
		if (LocalhostData.AddrLen == sizeof(sockaddr_in6)) //IPv6
		{
			IPv6Addr = &((PSOCKADDR_IN6)&LocalhostData.SockAddr)->sin6_addr;
			if (memcmp(IPv6Addr, &Parameter.DNSTarget.IPv6.sin6_addr, sizeof(in6_addr)) == 0 || 
				memcmp(IPv6Addr, &Parameter.DNSTarget.Alternate_IPv6.sin6_addr, sizeof(in6_addr)) == 0 || 
				memcmp(IPv6Addr, &Parameter.DNSTarget.Local_IPv6.sin6_addr, sizeof(in6_addr)) == 0 || 
				memcmp(IPv6Addr, &Parameter.DNSTarget.Alternate_Local_IPv6.sin6_addr, sizeof(in6_addr)) == 0 || 
		//Check Private Mode(IPv6).
				(Parameter.OperationMode == LISTEN_PRIVATEMODE && 
				!(IPv6Addr->u.Byte[0] >= 0xFC && IPv6Addr->u.Byte[0] <= 0xFD || //Unique Local Unicast address/ULA(FC00::/7, Section 2.5.7 in RFC 4193)
				IPv6Addr->u.Byte[0] == 0xFE && IPv6Addr->u.Byte[1U] >= 0x80 && IPv6Addr->u.Byte[1U] <= 0xBF || //Link-Local Unicast Contrast address(FE80::/10, Section 2.5.6 in RFC 4291)
				IPv6Addr->u.Word[6U] == 0 && IPv6Addr->u.Word[7U] == htons(0x0001))) || //Loopback address(::1, Section 2.5.3 in RFC 4291)
		//Check Custom Mode(IPv6).
				(Parameter.OperationMode == LISTEN_CUSTOMMODE && !CustomModeFilter(IPv6Addr, AF_INET6)))
			{
				closesocket(ClientData.Socket);
				continue;
			}
		}
		else { //IPv4
			IPv4Addr = &((PSOCKADDR_IN)&ClientData.SockAddr)->sin_addr;
			if (IPv4Addr->S_un.S_addr == Parameter.DNSTarget.IPv4.sin_addr.S_un.S_addr || 
				IPv4Addr->S_un.S_addr == Parameter.DNSTarget.Alternate_IPv4.sin_addr.S_un.S_addr || 
				IPv4Addr->S_un.S_addr == Parameter.DNSTarget.Local_IPv4.sin_addr.S_un.S_addr || 
				IPv4Addr->S_un.S_addr == Parameter.DNSTarget.Alternate_Local_IPv4.sin_addr.S_un.S_addr || 
		//Check Private Mode(IPv4).
				(Parameter.OperationMode == LISTEN_PRIVATEMODE && 
				!(IPv4Addr->S_un.S_un_b.s_b1 == 0x0A || //Private class A address(10.0.0.0/8, Section 3 in RFC 1918)
				IPv4Addr->S_un.S_un_b.s_b1 == 0x7F || //Loopback address(127.0.0.0/8, Section 3.2.1.3 in RFC 1122)
				IPv4Addr->S_un.S_un_b.s_b1 == 0xAC && IPv4Addr->S_un.S_un_b.s_b2 >= 0x10 && IPv4Addr->S_un.S_un_b.s_b2 <= 0x1F || //Private class B address(172.16.0.0/16, Section 3 in RFC 1918)
				IPv4Addr->S_un.S_un_b.s_b1 == 0xC0 && IPv4Addr->S_un.S_un_b.s_b2 == 0xA8)) || //Private class C address(192.168.0.0/24, Section 3 in RFC 1918)
		//Check Custom Mode(IPv4).
				(Parameter.OperationMode == LISTEN_CUSTOMMODE && !CustomModeFilter(IPv4Addr, AF_INET)))
			{
				closesocket(ClientData.Socket);
				continue;
			}
		}

	//Accept process.
		std::thread TCPReceiveThread(TCPReceiveProcess, ClientData, Index);
		TCPReceiveThread.detach();

		Index = (Index + 1U) % QUEUE_MAXLENGTH;
	}

	closesocket(LocalhostData.Socket);
	return EXIT_SUCCESS;
}

//Custom Mode addresses filter
inline bool __fastcall CustomModeFilter(const void *pAddr, const uint16_t Protocol)
{
	size_t Index = 0;

//IPv6
	if (Protocol == AF_INET6)
	{
		auto Addr = (in6_addr *)pAddr;
	//Permit
		if (Parameter.IPFilterOptions.Type)
		{
			std::unique_lock<std::mutex> AddressRangeMutex(AddressRangeLock);
			for (auto AddressRangeIter:*AddressRangeUsing)
			{
			//Check level.
				if (Parameter.IPFilterOptions.IPFilterLevel != 0 && AddressRangeIter.Level < Parameter.IPFilterOptions.IPFilterLevel)
					continue;

			//Check address.
				for (Index = 0;Index < sizeof(in6_addr) / sizeof(uint16_t);Index++)
				{
					if (ntohs(Addr->u.Word[Index]) > ntohs(AddressRangeIter.Begin.IPv6.u.Word[Index]) && ntohs(Addr->u.Word[Index]) < ntohs(AddressRangeIter.End.IPv6.u.Word[Index]))
					{
						return true;
					}
					else if (Addr->u.Word[Index] == AddressRangeIter.Begin.IPv6.u.Word[Index] || Addr->u.Word[Index] == AddressRangeIter.End.IPv6.u.Word[Index])
					{
						if (Index == sizeof(in6_addr) / sizeof(uint16_t) - 1U)
							return true;
						else 
							continue;
					}
					else {
						return false;
					}
				}
			}
		}
	//Deny
		else {
			std::unique_lock<std::mutex> AddressRangeMutex(AddressRangeLock);
			for (auto AddressRangeIter:*AddressRangeUsing)
			{
			//Check level.
				if (Parameter.IPFilterOptions.IPFilterLevel != 0 && AddressRangeIter.Level < Parameter.IPFilterOptions.IPFilterLevel)
					continue;

			//Check address.
				for (Index = 0;Index < sizeof(in6_addr) / sizeof(uint16_t);Index++)
				{
					if (ntohs(Addr->u.Word[Index]) > ntohs(AddressRangeIter.Begin.IPv6.u.Word[Index]) && ntohs(Addr->u.Word[Index]) < ntohs(AddressRangeIter.End.IPv6.u.Word[Index]))
					{
						return false;
					}
					else if (Addr->u.Word[Index] == AddressRangeIter.Begin.IPv6.u.Word[Index] || Addr->u.Word[Index] == AddressRangeIter.End.IPv6.u.Word[Index])
					{
						if (Index == sizeof(in6_addr) / sizeof(uint16_t) - 1U)
							return false;
						else 
							continue;
					}
					else {
						return true;
					}
				}
			}
		}
	}
//IPv4
	else {
		auto Addr = (in_addr *)pAddr;
	//Permit
		if (Parameter.IPFilterOptions.Type)
		{
			std::unique_lock<std::mutex> AddressRangeMutex(AddressRangeLock);
			for (auto AddressRangeIter:*AddressRangeUsing)
			{
			//Check level.
				if (Parameter.IPFilterOptions.IPFilterLevel != 0 && AddressRangeIter.Level < Parameter.IPFilterOptions.IPFilterLevel)
					continue;

			//Check address.
				if (Addr->S_un.S_un_b.s_b1 > AddressRangeIter.Begin.IPv4.S_un.S_un_b.s_b1 && Addr->S_un.S_un_b.s_b1 < AddressRangeIter.End.IPv4.S_un.S_un_b.s_b1)
				{
					return true;
				}
				else if (Addr->S_un.S_un_b.s_b1 == AddressRangeIter.Begin.IPv4.S_un.S_un_b.s_b1 || Addr->S_un.S_un_b.s_b1 == AddressRangeIter.End.IPv4.S_un.S_un_b.s_b1)
				{
					if (Addr->S_un.S_un_b.s_b2 > AddressRangeIter.Begin.IPv4.S_un.S_un_b.s_b2 && Addr->S_un.S_un_b.s_b2 < AddressRangeIter.End.IPv4.S_un.S_un_b.s_b2)
					{
						return true;
					}
					else if (Addr->S_un.S_un_b.s_b2 == AddressRangeIter.Begin.IPv4.S_un.S_un_b.s_b2 || Addr->S_un.S_un_b.s_b2 == AddressRangeIter.End.IPv4.S_un.S_un_b.s_b2)
					{
						if (Addr->S_un.S_un_b.s_b3 > AddressRangeIter.Begin.IPv4.S_un.S_un_b.s_b3 && Addr->S_un.S_un_b.s_b3 < AddressRangeIter.End.IPv4.S_un.S_un_b.s_b3)
						{
							return true;
						}
						else if (Addr->S_un.S_un_b.s_b3 == AddressRangeIter.Begin.IPv4.S_un.S_un_b.s_b3 || Addr->S_un.S_un_b.s_b3 == AddressRangeIter.End.IPv4.S_un.S_un_b.s_b3)
						{
							if (Addr->S_un.S_un_b.s_b4 >= AddressRangeIter.Begin.IPv4.S_un.S_un_b.s_b4 && Addr->S_un.S_un_b.s_b4 <= AddressRangeIter.End.IPv4.S_un.S_un_b.s_b4)
							{
								return true;
							}
							else {
								return false;
							}
						}
						else {
							return false;
						}
					}
					else {
						return false;
					}
				}
				else {
					return false;
				}
			}
		}
	//Deny
		else {
			std::unique_lock<std::mutex> AddressRangeMutex(AddressRangeLock);
			for (auto AddressRangeIter:*AddressRangeUsing)
			{
			//Check level.
				if (Parameter.IPFilterOptions.IPFilterLevel != 0 && AddressRangeIter.Level < Parameter.IPFilterOptions.IPFilterLevel)
					continue;

			//Check address.
				if (Addr->S_un.S_un_b.s_b1 > AddressRangeIter.Begin.IPv4.S_un.S_un_b.s_b1 && Addr->S_un.S_un_b.s_b1 < AddressRangeIter.End.IPv4.S_un.S_un_b.s_b1)
				{
					return false;
				}
				else if (Addr->S_un.S_un_b.s_b1 == AddressRangeIter.Begin.IPv4.S_un.S_un_b.s_b1 || Addr->S_un.S_un_b.s_b1 == AddressRangeIter.End.IPv4.S_un.S_un_b.s_b1)
				{
					if (Addr->S_un.S_un_b.s_b2 > AddressRangeIter.Begin.IPv4.S_un.S_un_b.s_b2 && Addr->S_un.S_un_b.s_b2 < AddressRangeIter.End.IPv4.S_un.S_un_b.s_b2)
					{
						return false;
					}
					else if (Addr->S_un.S_un_b.s_b2 == AddressRangeIter.Begin.IPv4.S_un.S_un_b.s_b2 || Addr->S_un.S_un_b.s_b2 == AddressRangeIter.End.IPv4.S_un.S_un_b.s_b2)
					{
						if (Addr->S_un.S_un_b.s_b3 > AddressRangeIter.Begin.IPv4.S_un.S_un_b.s_b3 && Addr->S_un.S_un_b.s_b3 < AddressRangeIter.End.IPv4.S_un.S_un_b.s_b3)
						{
							return false;
						}
						else if (Addr->S_un.S_un_b.s_b3 == AddressRangeIter.Begin.IPv4.S_un.S_un_b.s_b3 || Addr->S_un.S_un_b.s_b3 == AddressRangeIter.End.IPv4.S_un.S_un_b.s_b3)
						{
							if (Addr->S_un.S_un_b.s_b4 >= AddressRangeIter.Begin.IPv4.S_un.S_un_b.s_b4 && Addr->S_un.S_un_b.s_b4 <= AddressRangeIter.End.IPv4.S_un.S_un_b.s_b4)
							{
								return false;
							}
							else {
								return true;
							}
						}
						else {
							return true;
						}
					}
					else {
						return true;
					}
				}
				else {
					return true;
				}
			}
		}
	}

	return true;
}

//Alternate DNS servers switcher
inline void __fastcall AlternateServerSwitcher(void)
{
	size_t Index = 0, RangeTimer[ALTERNATE_SERVERNUM] = {0}, SwapTimer[ALTERNATE_SERVERNUM] = {0};

//Switcher
//Minimum supported system of GetTickCount64() is Windows Vista.
	while (true)
	{
	//Pcap Requesting check
		for (Index = 0;Index < QUEUE_MAXLENGTH * QUEUE_PARTNUM;Index++)
		{
			if (AlternateSwapList.PcapAlternateTimeout[Index] != 0 && 
			#ifdef _WIN64
				GetTickCount64() > RELIABLE_SOCKET_TIMEOUT * TIME_OUT + AlternateSwapList.PcapAlternateTimeout[Index]) //Check in TIME_OUT.
			#else //x86
				GetTickCount() > RELIABLE_SOCKET_TIMEOUT * TIME_OUT + AlternateSwapList.PcapAlternateTimeout[Index]) //Check in TIME_OUT.
			#endif
			{
				AlternateSwapList.PcapAlternateTimeout[Index] = 0;
				if (Index >= 0 && Index < QUEUE_MAXLENGTH * (QUEUE_PARTNUM - 3U) || Index >= QUEUE_MAXLENGTH * (QUEUE_PARTNUM - 2U) && Index < QUEUE_MAXLENGTH * (QUEUE_PARTNUM - 1U)) //IPv6
					AlternateSwapList.TimeoutTimes[2U]++;
				else //IPv4
					AlternateSwapList.TimeoutTimes[3U]++;
			}
		}

	//Complete Requesting check
		for (Index = 0;Index < ALTERNATE_SERVERNUM;Index++)
		{
		//Reset TimeoutTimes out of alternate time range.
		#ifdef _WIN64
			if (GetTickCount64() >= Parameter.AlternateOptions.AlternateTimeRange + RangeTimer[Index])
			{
				RangeTimer[Index] = GetTickCount64();
		#else //x86
			if (GetTickCount() >= Parameter.AlternateOptions.AlternateTimeRange + RangeTimer[Index])
			{
				RangeTimer[Index] = GetTickCount();
		#endif
				AlternateSwapList.TimeoutTimes[Index] = 0;
				continue;
			}

		//Reset alternate switching.
			if (AlternateSwapList.Swap[Index])
			{
			#ifdef _WIN64
				if (GetTickCount64() >= Parameter.AlternateOptions.AlternateResetTime + SwapTimer[Index])
			#else //x86
				if (GetTickCount() >= Parameter.AlternateOptions.AlternateResetTime + SwapTimer[Index])
			#endif
				{
					AlternateSwapList.Swap[Index] = false;
					AlternateSwapList.TimeoutTimes[Index] = 0;
					SwapTimer[Index] = 0;
				}
			}
			else {
			//Mark alternate switching.
				if (AlternateSwapList.TimeoutTimes[Index] >= Parameter.AlternateOptions.AlternateTimes)
				{
					AlternateSwapList.Swap[Index] = true;
					AlternateSwapList.TimeoutTimes[Index] = 0;
				#ifdef _WIN64
					SwapTimer[Index] = GetTickCount64();
				#else //x86
					SwapTimer[Index] = GetTickCount();
				#endif
				}
			}
		}

		Sleep(TIME_OUT);
	}

	return;
}

//DNS Cache timers monitor
void __fastcall DNSCacheTimerMonitor(const size_t CacheType)
{
	size_t Time = 0;
	while (true)
	{
	//Minimum supported system of GetTickCount64() is Windows Vista.
	#ifdef _WIN64
		Time = GetTickCount64();
	#else //x86
		Time = GetTickCount();
	#endif

		if (CacheType == CACHE_TIMER)
		{
			std::unique_lock<std::mutex> DNSCacheListMutex(DNSCacheListLock);
			while (!DNSCacheList.empty() && Time >= Parameter.CacheParameter + DNSCacheList.front().Time)
				DNSCacheList.pop_front();
			DNSCacheList.shrink_to_fit();
		}
		else { //CACHE_QUEUE
			std::unique_lock<std::mutex> DNSCacheListMutex(DNSCacheListLock);
			while (!DNSCacheList.empty() && Time >= DNSCacheList.front().Time + Parameter.HostsDefaultTTL * TIME_OUT)
				DNSCacheList.pop_front();
			DNSCacheList.shrink_to_fit();
		}

		Sleep(TIME_OUT); //Time between checks.
	}

	return;
}
