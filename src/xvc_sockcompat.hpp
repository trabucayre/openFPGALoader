// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2022 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 *
 * Small portability shim used by xvc_client.* and xvc_server.* so the
 * Xilinx Virtual Cable (XVC) code can be compiled both on POSIX systems
 * (native BSD sockets) and on Windows (Winsock2), without scattering
 * #ifdef _WIN32 blocks all over the XVC implementation files.
 */

#ifndef SRC_XVC_SOCKCOMPAT_HPP_
#define SRC_XVC_SOCKCOMPAT_HPP_

#include <string>

#if defined(_WIN32) || defined(_WIN64)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

/* Winsock send()/recv() require char* buffers and have no read()/write()
 * equivalent on raw SOCKET handles: wrap them so call sites stay identical
 * on every platform. */
#define xvc_close_socket(s)    closesocket(s)
#define xvc_recv(fd, buf, len) recv((fd), reinterpret_cast<char *>(buf), \
		static_cast<int>(len), 0)
#define xvc_send(fd, buf, len) send((fd), reinterpret_cast<const char *>(buf), \
		static_cast<int>(len), 0)

/*!
 * \brief initialize Winsock (no-op on POSIX)
 * \return true on success
 */
inline bool xvc_socket_init()
{
	WSADATA wsaData;
	return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
}

/*!
 * \brief release Winsock resources (no-op on POSIX)
 */
inline void xvc_socket_cleanup()
{
	WSACleanup();
}

/*!
 * \brief return a human readable string for the last socket error
 */
inline std::string xvc_last_socket_error()
{
	return "WSA error " + std::to_string(WSAGetLastError());
}

#else  // POSIX

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR   (-1)

#define xvc_close_socket(s)    close(s)
#define xvc_recv(fd, buf, len) read((fd), (buf), (len))
#define xvc_send(fd, buf, len) write((fd), (buf), (len))

inline bool xvc_socket_init() { return true; }
inline void xvc_socket_cleanup() {}

inline std::string xvc_last_socket_error()
{
	return std::string(strerror(errno));
}

#endif  // _WIN32 || _WIN64

#endif  // SRC_XVC_SOCKCOMPAT_HPP_
