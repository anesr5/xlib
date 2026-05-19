#ifndef _WIN32
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include <xlib/network.h>

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif

struct x_socket {
#ifdef _WIN32
    SOCKET handle;
#else
    int handle;
#endif
    int type;
    int family;
};

void x_socket_close(x_socket_t *socket);

#ifdef _WIN32
static int x_network_error_from_windows(int error)
{
    switch (error) {
    case 0:                  return 0;
    case WSAEWOULDBLOCK:     return EWOULDBLOCK;
    case WSAEINPROGRESS:     return EINPROGRESS;
    case WSAECONNRESET:      return ECONNRESET;
    case WSAECONNREFUSED:    return ECONNREFUSED;
    case WSAEADDRINUSE:      return EADDRINUSE;
    case WSAEADDRNOTAVAIL:   return EADDRNOTAVAIL;
    case WSAETIMEDOUT:       return ETIMEDOUT;
    case WSAENOTSOCK:        return ENOTSOCK;
    case WSAEINVAL:          return EINVAL;
    case WSAENOBUFS:         return ENOBUFS;
    case WSAEAFNOSUPPORT:    return EAFNOSUPPORT;
    case WSAEPROTONOSUPPORT: return EPROTONOSUPPORT;
    case WSAEACCES:          return EACCES;
    case WSAHOST_NOT_FOUND:
    case WSANO_DATA:         return ENOENT;
    default:                 return EIO;
    }
}



static INIT_ONCE x_network_init_once = INIT_ONCE_STATIC_INIT;
static int       x_network_startup_result;

static BOOL CALLBACK x_network_do_startup(PINIT_ONCE once, PVOID param, PVOID *ctx)
{
    WSADATA data;
    (void)once; (void)param; (void)ctx;
    x_network_startup_result = WSAStartup(MAKEWORD(2, 2), &data);
    return x_network_startup_result == 0 ? TRUE : FALSE;
}

static int x_network_startup(void)
{
    if (!InitOnceExecuteOnce(&x_network_init_once, x_network_do_startup, NULL, NULL)) {
        return x_network_startup_result != 0
            ? x_network_error_from_windows((DWORD)x_network_startup_result)
            : EIO;
    }
    return 0;
}
#else
static int x_network_startup(void)
{
    return 0;
}
#endif

static int x_socket_error(void)
{
#ifdef _WIN32
    return x_network_error_from_windows(WSAGetLastError());
#else
    return errno;
#endif
}

static int x_socket_copy_address(x_socket_address_t *address, const void *source, size_t length)
{
    if (address == NULL || source == NULL || length == 0U) {
        return EINVAL;
    }

    if (length > sizeof(address->storage)) {
        return EOVERFLOW;
    }

    memset(address->storage, 0, sizeof(address->storage));
    memcpy(address->storage, source, length);
    address->length = length;
    return 0;
}

static int x_socket_address_supported_family(const void *source)
{
    const struct sockaddr *sa = (const struct sockaddr *)source;

    return sa != NULL && (sa->sa_family == AF_INET || sa->sa_family == AF_INET6);
}

static int x_socket_set_ipv6_only_native(x_socket_t *socket, int enabled)
{
    int value = enabled ? 1 : 0;

    if (socket == NULL || socket->family != AF_INET6) {
        return EINVAL;
    }

    if (setsockopt(
            socket->handle,
            IPPROTO_IPV6,
            IPV6_V6ONLY,
            (const char *)&value,
            (socklen_t)sizeof(value)) != 0) {
        return x_socket_error();
    }

    return 0;
}

static int x_socket_create(x_socket_t **out_socket, int type, int family)
{
    x_socket_t *created;
    int error;
    int native_type;
    int protocol;

    if (out_socket == NULL || (type != X_SOCKET_TCP && type != X_SOCKET_UDP)) {
        return EINVAL;
    }

    if (family != AF_INET && family != AF_INET6) {
        return EINVAL;
    }

    *out_socket = NULL;

    error = x_network_startup();
    if (error != 0) {
        return error;
    }

    created = (x_socket_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }

    native_type = type == X_SOCKET_TCP ? SOCK_STREAM : SOCK_DGRAM;
    protocol = type == X_SOCKET_TCP ? IPPROTO_TCP : IPPROTO_UDP;

#ifdef _WIN32
    created->handle = socket(family, native_type, protocol);
    if (created->handle == INVALID_SOCKET) {
        error = x_socket_error();
        free(created);
        return error;
    }
#else
    /* Use SOCK_CLOEXEC where available so the fd is not inherited by child
     * processes spawned via x_process_start(). */
#if defined(SOCK_CLOEXEC)
    created->handle = socket(family, native_type | SOCK_CLOEXEC, protocol);
#else
    created->handle = socket(family, native_type, protocol);
#endif
    if (created->handle < 0) {
        error = errno;
        free(created);
        return error;
    }
#if !defined(SOCK_CLOEXEC)
    (void)fcntl(created->handle, F_SETFD, FD_CLOEXEC);
#endif
#endif

    created->type   = type;
    created->family = family;
    *out_socket = created;
    return 0;
}

int x_address_resolve(
    x_socket_address_t *address,
    const char *host,
    const char *service,
    int type,
    int passive)
{
    struct addrinfo hints;
    struct addrinfo *results;
    struct addrinfo *cursor;
    int error;
    int native_type;

    if (address == NULL || service == NULL || (type != X_SOCKET_TCP && type != X_SOCKET_UDP)) {
        return EINVAL;
    }

    error = x_network_startup();
    if (error != 0) {
        return error;
    }

    native_type = type == X_SOCKET_TCP ? SOCK_STREAM : SOCK_DGRAM;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = native_type;
    hints.ai_protocol = type == X_SOCKET_TCP ? IPPROTO_TCP : IPPROTO_UDP;
    hints.ai_flags    = passive ? AI_PASSIVE : 0;

    error = getaddrinfo(host, service, &hints, &results);
    if (error != 0) {
#ifdef _WIN32
        return x_network_error_from_windows(error);
#else
        /* Map EAI_* codes to meaningful errno values. */
        switch (error) {
        case EAI_AGAIN:   return EAGAIN;
#ifdef EAI_NONAME
        case EAI_NONAME:
#endif
#ifdef EAI_SERVICE
        case EAI_SERVICE:
#endif
            return ENOENT;
        case EAI_SYSTEM:  return errno;
        default:          return EINVAL;
        }
#endif
    }

    for (cursor = results; cursor != NULL; cursor = cursor->ai_next) {
        error = x_socket_copy_address(address, cursor->ai_addr, (size_t)cursor->ai_addrlen);
        if (error == 0) {
            freeaddrinfo(results);
            return 0;
        }
    }

    freeaddrinfo(results);
    return ENOENT;
}

int x_address_resolve_all(
    x_address_t **addresses,
    size_t *count,
    const char *host,
    const char *service,
    int type,
    int passive)
{
    struct addrinfo hints;
    struct addrinfo *results;
    struct addrinfo *cursor;
    x_address_t *created;
    size_t address_count = 0U;
    size_t index = 0U;
    int error;
    int native_type;

    if (addresses == NULL || count == NULL || service == NULL
        || (type != X_SOCKET_TCP && type != X_SOCKET_UDP)) {
        return EINVAL;
    }

    *addresses = NULL;
    *count = 0U;

    error = x_network_startup();
    if (error != 0) {
        return error;
    }

    native_type = type == X_SOCKET_TCP ? SOCK_STREAM : SOCK_DGRAM;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = native_type;
    hints.ai_protocol = type == X_SOCKET_TCP ? IPPROTO_TCP : IPPROTO_UDP;
    hints.ai_flags = passive ? AI_PASSIVE : 0;

    error = getaddrinfo(host, service, &hints, &results);
    if (error != 0) {
#ifdef _WIN32
        return x_network_error_from_windows(error);
#else
        switch (error) {
        case EAI_AGAIN: return EAGAIN;
#ifdef EAI_NONAME
        case EAI_NONAME:
#endif
#ifdef EAI_SERVICE
        case EAI_SERVICE:
#endif
            return ENOENT;
        case EAI_SYSTEM: return errno;
        default: return EINVAL;
        }
#endif
    }

    for (cursor = results; cursor != NULL; cursor = cursor->ai_next) {
        if (cursor->ai_addr != NULL
            && x_socket_address_supported_family(cursor->ai_addr)
            && (size_t)cursor->ai_addrlen <= sizeof(created->storage)) {
            ++address_count;
        }
    }

    if (address_count == 0U) {
        freeaddrinfo(results);
        return ENOENT;
    }

    created = (x_address_t *)calloc(address_count, sizeof(*created));
    if (created == NULL) {
        freeaddrinfo(results);
        return ENOMEM;
    }

    for (cursor = results; cursor != NULL; cursor = cursor->ai_next) {
        if (cursor->ai_addr == NULL
            || !x_socket_address_supported_family(cursor->ai_addr)
            || (size_t)cursor->ai_addrlen > sizeof(created[index].storage)) {
            continue;
        }

        (void)x_socket_copy_address(&created[index], cursor->ai_addr, (size_t)cursor->ai_addrlen);
        ++index;
    }

    freeaddrinfo(results);
    *addresses = created;
    *count = index;
    return 0;
}

void x_address_list_free(x_address_t *addresses)
{
    free(addresses);
}

int x_address_port(const x_socket_address_t *address, uint16_t *port)
{
    const struct sockaddr *sa;

    if (address == NULL || port == NULL || address->length == 0U) {
        return EINVAL;
    }

    sa = (const struct sockaddr *)address->storage;

    if (sa->sa_family == AF_INET) {
        if (address->length < sizeof(struct sockaddr_in)) {
            return EINVAL;
        }
        *port = ntohs(((const struct sockaddr_in *)address->storage)->sin_port);
        return 0;
    }

    if (sa->sa_family == AF_INET6) {
        if (address->length < sizeof(struct sockaddr_in6)) {
            return EINVAL;
        }
        *port = ntohs(((const struct sockaddr_in6 *)address->storage)->sin6_port);
        return 0;
    }

    return EAFNOSUPPORT;
}

int x_socket_tcp(x_socket_t **socket)
{
    return x_socket_create(socket, X_SOCKET_TCP, AF_INET);
}

int x_socket_udp(x_socket_t **socket)
{
    return x_socket_create(socket, X_SOCKET_UDP, AF_INET);
}

int x_socket_tcp6(x_socket_t **socket)
{
    return x_socket_create(socket, X_SOCKET_TCP, AF_INET6);
}

int x_socket_udp6(x_socket_t **socket)
{
    return x_socket_create(socket, X_SOCKET_UDP, AF_INET6);
}

int x_socket_tcp_dual_stack(x_socket_t **socket)
{
    x_socket_t *created = NULL;
    int error;

    error = x_socket_create(&created, X_SOCKET_TCP, AF_INET6);
    if (error != 0) {
        return error;
    }

    error = x_socket_set_ipv6_only_native(created, 0);
    if (error != 0) {
        x_socket_close(created);
        return error;
    }

    *socket = created;
    return 0;
}

int x_socket_udp_dual_stack(x_socket_t **socket)
{
    x_socket_t *created = NULL;
    int error;

    error = x_socket_create(&created, X_SOCKET_UDP, AF_INET6);
    if (error != 0) {
        return error;
    }

    error = x_socket_set_ipv6_only_native(created, 0);
    if (error != 0) {
        x_socket_close(created);
        return error;
    }

    *socket = created;
    return 0;
}

int x_socket_bind(x_socket_t *socket, const x_socket_address_t *address)
{
    if (socket == NULL || address == NULL || address->length == 0U) {
        return EINVAL;
    }

    if (bind(socket->handle, (const struct sockaddr *)address->storage, (socklen_t)address->length) != 0) {
        return x_socket_error();
    }

    return 0;
}

int x_socket_listen(x_socket_t *socket, int backlog)
{
    if (socket == NULL) {
        return EINVAL;
    }

    if (listen(socket->handle, backlog) != 0) {
        return x_socket_error();
    }

    return 0;
}

int x_socket_accept(x_socket_t *socket, x_socket_t **client)
{
    x_socket_t *accepted;
#ifdef _WIN32
    SOCKET handle;
#else
    int handle;
#endif

    if (socket == NULL || client == NULL) {
        return EINVAL;
    }

    *client = NULL;
    accepted = (x_socket_t *)calloc(1, sizeof(*accepted));
    if (accepted == NULL) {
        return ENOMEM;
    }

    handle = accept(socket->handle, NULL, NULL);
#ifdef _WIN32
    if (handle == INVALID_SOCKET) {
        int error = x_socket_error();
        free(accepted);
        return error;
    }
#else
    if (handle < 0) {
        int error = errno;
        free(accepted);
        return error;
    }
    (void)fcntl(handle, F_SETFD, FD_CLOEXEC);
#endif

    accepted->handle = handle;
    accepted->type = X_SOCKET_TCP;
    accepted->family = socket->family;
    *client = accepted;
    return 0;
}

int x_socket_connect(x_socket_t *socket, const x_socket_address_t *address)
{
    if (socket == NULL || address == NULL || address->length == 0U) {
        return EINVAL;
    }

    if (connect(socket->handle, (const struct sockaddr *)address->storage, (socklen_t)address->length) != 0) {
        return x_socket_error();
    }

    return 0;
}

int x_socket_shutdown(x_socket_t *socket, int how)
{
    int native_how;

    if (socket == NULL) {
        return EINVAL;
    }

    if (how == X_SOCKET_SHUTDOWN_READ) {
#ifdef _WIN32
        native_how = SD_RECEIVE;
#else
        native_how = SHUT_RD;
#endif
    } else if (how == X_SOCKET_SHUTDOWN_WRITE) {
#ifdef _WIN32
        native_how = SD_SEND;
#else
        native_how = SHUT_WR;
#endif
    } else if (how == X_SOCKET_SHUTDOWN_BOTH) {
#ifdef _WIN32
        native_how = SD_BOTH;
#else
        native_how = SHUT_RDWR;
#endif
    } else {
        return EINVAL;
    }

    if (shutdown(socket->handle, native_how) != 0) {
        return x_socket_error();
    }

    return 0;
}

int x_socket_peer_address(x_socket_t *socket, x_socket_address_t *address)
{
    struct sockaddr_storage peer;
    socklen_t peer_length = (socklen_t)sizeof(peer);

    if (socket == NULL || address == NULL) {
        return EINVAL;
    }

    if (getpeername(socket->handle, (struct sockaddr *)&peer, &peer_length) != 0) {
        return x_socket_error();
    }

    return x_socket_copy_address(address, &peer, (size_t)peer_length);
}

int x_socket_send(x_socket_t *socket, const void *buffer, size_t size, size_t *bytes_sent)
{
    int result;
    size_t chunk;

    if (socket == NULL || buffer == NULL) {
        return EINVAL;
    }

    if (bytes_sent != NULL) {
        *bytes_sent = 0U;
    }

    chunk = size > (size_t)INT_MAX ? (size_t)INT_MAX : size;
    result = send(socket->handle, (const char *)buffer, (int)chunk, 0);
    if (result < 0) {
        return x_socket_error();
    }

    if (bytes_sent != NULL) {
        *bytes_sent = (size_t)result;
    }

    return 0;
}

int x_socket_receive(x_socket_t *socket, void *buffer, size_t size, size_t *bytes_received)
{
    int result;
    size_t chunk;

    if (socket == NULL || buffer == NULL) {
        return EINVAL;
    }

    if (bytes_received != NULL) {
        *bytes_received = 0U;
    }

    chunk = size > (size_t)INT_MAX ? (size_t)INT_MAX : size;
    result = recv(socket->handle, (char *)buffer, (int)chunk, 0);
    if (result < 0) {
        return x_socket_error();
    }

    if (bytes_received != NULL) {
        *bytes_received = (size_t)result;
    }

    return 0;
}

int x_socket_send_to(
    x_socket_t *socket,
    const x_socket_address_t *address,
    const void *buffer,
    size_t size,
    size_t *bytes_sent)
{
    int result;
    size_t chunk;

    if (socket == NULL || address == NULL || buffer == NULL || address->length == 0U) {
        return EINVAL;
    }

    if (bytes_sent != NULL) {
        *bytes_sent = 0U;
    }

    chunk = size > (size_t)INT_MAX ? (size_t)INT_MAX : size;
    result = sendto(
        socket->handle,
        (const char *)buffer,
        (int)chunk,
        0,
        (const struct sockaddr *)address->storage,
        (socklen_t)address->length);
    if (result < 0) {
        return x_socket_error();
    }

    if (bytes_sent != NULL) {
        *bytes_sent = (size_t)result;
    }

    return 0;
}

int x_socket_receive_from(
    x_socket_t *socket,
    x_socket_address_t *address,
    void *buffer,
    size_t size,
    size_t *bytes_received)
{
    int result;
    size_t chunk;
    struct sockaddr_storage source;
    socklen_t source_length = (socklen_t)sizeof(source);

    if (socket == NULL || buffer == NULL) {
        return EINVAL;
    }

    if (bytes_received != NULL) {
        *bytes_received = 0U;
    }

    chunk = size > (size_t)INT_MAX ? (size_t)INT_MAX : size;
    result = recvfrom(
        socket->handle,
        (char *)buffer,
        (int)chunk,
        0,
        (struct sockaddr *)&source,
        &source_length);
    if (result < 0) {
        return x_socket_error();
    }

    if (address != NULL) {
        int error = x_socket_copy_address(address, &source, (size_t)source_length);
        if (error != 0) {
            return error;
        }
    }

    if (bytes_received != NULL) {
        *bytes_received = (size_t)result;
    }

    return 0;
}

int x_socket_local_address(x_socket_t *socket, x_socket_address_t *address)
{
    struct sockaddr_storage local;
    socklen_t local_length = (socklen_t)sizeof(local);

    if (socket == NULL || address == NULL) {
        return EINVAL;
    }

    if (getsockname(socket->handle, (struct sockaddr *)&local, &local_length) != 0) {
        return x_socket_error();
    }

    return x_socket_copy_address(address, &local, (size_t)local_length);
}

int x_socket_set_nonblocking(x_socket_t *socket, int enabled)
{
    if (socket == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    {
        u_long mode = enabled ? 1UL : 0UL;
        if (ioctlsocket(socket->handle, FIONBIO, &mode) != 0) {
            return x_socket_error();
        }
    }
#else
    {
        int flags = fcntl(socket->handle, F_GETFL, 0);
        if (flags < 0) {
            return errno;
        }

        if (enabled) {
            flags |= O_NONBLOCK;
        } else {
            flags &= ~O_NONBLOCK;
        }

        if (fcntl(socket->handle, F_SETFL, flags) != 0) {
            return errno;
        }
    }
#endif

    return 0;
}

int x_socket_set_reuse_address(x_socket_t *socket, int enabled)
{
    int value;

    if (socket == NULL) {
        return EINVAL;
    }

    value = enabled ? 1 : 0;
    if (setsockopt(socket->handle, SOL_SOCKET, SO_REUSEADDR, (const char *)&value, (socklen_t)sizeof(value)) != 0) {
        return x_socket_error();
    }

    return 0;
}

int x_socket_set_tcp_no_delay(x_socket_t *socket, int enabled)
{
    int value;

    if (socket == NULL || socket->type != X_SOCKET_TCP) {
        return EINVAL;
    }

    value = enabled ? 1 : 0;
    if (setsockopt(socket->handle, IPPROTO_TCP, TCP_NODELAY, (const char *)&value, (socklen_t)sizeof(value)) != 0) {
        return x_socket_error();
    }

    return 0;
}

uintptr_t x_socket_native_handle(x_socket_t *socket)
{
    if (socket == NULL) {
        return 0U;
    }

    return (uintptr_t)socket->handle;
}

void x_socket_close(x_socket_t *socket)
{
    if (socket == NULL) {
        return;
    }

#ifdef _WIN32
    closesocket(socket->handle);
#else
    close(socket->handle);
#endif

    free(socket);
}

int x_address_family(const x_socket_address_t *address)
{
    const struct sockaddr *sa;

    if (address == NULL || address->length == 0U) {
        return EINVAL;
    }

    sa = (const struct sockaddr *)address->storage;

    if (sa->sa_family == AF_INET) {
        return X_ADDRESS_FAMILY_IPV4;
    }

    if (sa->sa_family == AF_INET6) {
        return X_ADDRESS_FAMILY_IPV6;
    }

    return EAFNOSUPPORT;
}

int x_address_to_string(
    const x_socket_address_t *address,
    char *buffer,
    size_t buffer_size)
{
    const struct sockaddr *sa;
    const void *src;
    int af;

    if (address == NULL || buffer == NULL || buffer_size == 0U) {
        return EINVAL;
    }

    sa = (const struct sockaddr *)address->storage;
    af = sa->sa_family;

    if (af == AF_INET) {
        src = &((const struct sockaddr_in *)address->storage)->sin_addr;
    } else if (af == AF_INET6) {
        src = &((const struct sockaddr_in6 *)address->storage)->sin6_addr;
    } else {
        return EAFNOSUPPORT;
    }

#ifdef _WIN32
    if (InetNtopA(af, (PVOID)src, buffer, buffer_size) == NULL) {
        return x_socket_error();
    }
#else
    if (inet_ntop(af, src, buffer, (socklen_t)buffer_size) == NULL) {
        return errno;
    }
#endif

    return 0;
}

int x_address_from_string(
    x_socket_address_t *address,
    const char *host,
    uint16_t port,
    int family)
{
    int af;

    if (address == NULL || host == NULL) {
        return EINVAL;
    }

    if (family == X_ADDRESS_FAMILY_IPV4) {
        struct sockaddr_in sin;
        af = AF_INET;
        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_port   = htons(port);
#ifdef _WIN32
        if (InetPtonA(AF_INET, host, &sin.sin_addr) != 1) {
            return x_socket_error();
        }
#else
        if (inet_pton(AF_INET, host, &sin.sin_addr) != 1) {
            return EINVAL;
        }
#endif
        return x_socket_copy_address(address, &sin, sizeof(sin));
    }

    if (family == X_ADDRESS_FAMILY_IPV6) {
        struct sockaddr_in6 sin6;
        af = AF_INET6;
        (void)af;
        memset(&sin6, 0, sizeof(sin6));
        sin6.sin6_family = AF_INET6;
        sin6.sin6_port   = htons(port);
#ifdef _WIN32
        if (InetPtonA(AF_INET6, host, &sin6.sin6_addr) != 1) {
            return x_socket_error();
        }
#else
        if (inet_pton(AF_INET6, host, &sin6.sin6_addr) != 1) {
            return EINVAL;
        }
#endif
        return x_socket_copy_address(address, &sin6, sizeof(sin6));
    }

    return EINVAL;
}

static int x_socket_set_timeout(x_socket_t *socket, int optname, uint32_t milliseconds)
{
    if (socket == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    {
        DWORD ms = (DWORD)milliseconds;
        if (setsockopt(socket->handle, SOL_SOCKET, optname,
                       (const char *)&ms, (socklen_t)sizeof(ms)) != 0) {
            return x_socket_error();
        }
    }
#else
    {
        struct timeval tv;
        tv.tv_sec  = (time_t)(milliseconds / 1000U);
        tv.tv_usec = (suseconds_t)((milliseconds % 1000U) * 1000U);
        if (setsockopt(socket->handle, SOL_SOCKET, optname,
                       &tv, (socklen_t)sizeof(tv)) != 0) {
            return errno;
        }
    }
#endif

    return 0;
}

int x_socket_set_send_timeout(x_socket_t *socket, uint32_t milliseconds)
{
    return x_socket_set_timeout(socket, SO_SNDTIMEO, milliseconds);
}

int x_socket_set_receive_timeout(x_socket_t *socket, uint32_t milliseconds)
{
    return x_socket_set_timeout(socket, SO_RCVTIMEO, milliseconds);
}

static int x_socket_multicast_group(
    x_socket_t *socket,
    const char *group_address,
    const char *interface_address,
    int join)
{
    struct ip_mreq mreq;

    if (socket == NULL || group_address == NULL) {
        return EINVAL;
    }

    if (socket->type != X_SOCKET_UDP) {
        return EINVAL;
    }

    memset(&mreq, 0, sizeof(mreq));

#ifdef _WIN32
    if (InetPtonA(AF_INET, group_address, &mreq.imr_multiaddr) != 1) {
        return x_socket_error();
    }
    if (interface_address != NULL) {
        if (InetPtonA(AF_INET, interface_address, &mreq.imr_interface) != 1) {
            return x_socket_error();
        }
    } else {
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    }
#else
    if (inet_pton(AF_INET, group_address, &mreq.imr_multiaddr) != 1) {
        return EINVAL;
    }
    if (interface_address != NULL) {
        if (inet_pton(AF_INET, interface_address, &mreq.imr_interface) != 1) {
            return EINVAL;
        }
    } else {
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    }
#endif

    if (setsockopt(
            socket->handle,
            IPPROTO_IP,
            join ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP,
            (const char *)&mreq,
            (socklen_t)sizeof(mreq)) != 0) {
        return x_socket_error();
    }

    return 0;
}

int x_socket_join_multicast_group(
    x_socket_t *socket,
    const char *group_address,
    const char *interface_address)
{
    return x_socket_multicast_group(socket, group_address, interface_address, 1);
}

int x_socket_leave_multicast_group(
    x_socket_t *socket,
    const char *group_address,
    const char *interface_address)
{
    return x_socket_multicast_group(socket, group_address, interface_address, 0);
}

int x_socket_udp_bound(
    x_socket_t **socket,
    const char *host,
    const char *service)
{
    x_socket_t *created;
    x_socket_address_t address;
    int error;
    int family;

    if (socket == NULL || service == NULL) {
        return EINVAL;
    }

    *socket = NULL;

    error = x_address_resolve(&address, host, service, X_SOCKET_UDP, 1);
    if (error != 0) {
        return error;
    }

    family = x_address_family(&address);

    error = x_socket_create(&created, X_SOCKET_UDP,
                            family == X_ADDRESS_FAMILY_IPV6 ? AF_INET6 : AF_INET);
    if (error != 0) {
        return error;
    }

    error = x_socket_set_reuse_address(created, 1);
    if (error != 0) {
        x_socket_close(created);
        return error;
    }

    error = x_socket_bind(created, &address);
    if (error != 0) {
        x_socket_close(created);
        return error;
    }

    *socket = created;
    return 0;
}

int x_socket_would_block(int error)
{
    return error == EWOULDBLOCK
#ifdef EAGAIN
        || error == EAGAIN
#endif
        || error == EINPROGRESS;
}

int x_network_interfaces(x_network_interface_t **interfaces, size_t *count)
{
    x_network_interface_t *created = NULL;
    size_t created_count = 0U;
    int error;

    if (interfaces == NULL || count == NULL) {
        return EINVAL;
    }

    *interfaces = NULL;
    *count = 0U;

    error = x_network_startup();
    if (error != 0) {
        return error;
    }

#ifdef _WIN32
    {
        ULONG buffer_size = 15000U;
        IP_ADAPTER_ADDRESSES *adapters = NULL;
        IP_ADAPTER_ADDRESSES *adapter;
        ULONG result;

        for (;;) {
            adapters = (IP_ADAPTER_ADDRESSES *)malloc(buffer_size);
            if (adapters == NULL) {
                return ENOMEM;
            }

            result = GetAdaptersAddresses(
                AF_UNSPEC,
                GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
                NULL,
                adapters,
                &buffer_size);
            if (result != ERROR_BUFFER_OVERFLOW) {
                break;
            }

            free(adapters);
            adapters = NULL;
        }

        if (result != NO_ERROR) {
            free(adapters);
            return x_network_error_from_windows((int)result);
        }

        for (adapter = adapters; adapter != NULL; adapter = adapter->Next) {
            IP_ADAPTER_UNICAST_ADDRESS *unicast;

            for (unicast = adapter->FirstUnicastAddress; unicast != NULL; unicast = unicast->Next) {
                x_network_interface_t *next;
                struct sockaddr *address = unicast->Address.lpSockaddr;
                size_t address_length = (size_t)unicast->Address.iSockaddrLength;

                if (!x_socket_address_supported_family(address)) {
                    continue;
                }

                next = (x_network_interface_t *)realloc(created, (created_count + 1U) * sizeof(*created));
                if (next == NULL) {
                    free(created);
                    free(adapters);
                    return ENOMEM;
                }

                created = next;
                memset(&created[created_count], 0, sizeof(created[created_count]));
                if (adapter->AdapterName != NULL) {
                    strncpy(created[created_count].name, adapter->AdapterName, sizeof(created[created_count].name) - 1U);
                }
                created[created_count].flags = adapter->OperStatus == IfOperStatusUp ? 1U : 0U;
                (void)x_socket_copy_address(&created[created_count].address, address, address_length);
                ++created_count;
            }
        }

        free(adapters);
    }
#else
    {
        struct ifaddrs *ifaddrs_list = NULL;
        struct ifaddrs *cursor;

        if (getifaddrs(&ifaddrs_list) != 0) {
            return errno;
        }

        for (cursor = ifaddrs_list; cursor != NULL; cursor = cursor->ifa_next) {
            x_network_interface_t *next;
            socklen_t address_length;

            if (cursor->ifa_addr == NULL || !x_socket_address_supported_family(cursor->ifa_addr)) {
                continue;
            }

            if (cursor->ifa_addr->sa_family == AF_INET) {
                address_length = (socklen_t)sizeof(struct sockaddr_in);
            } else {
                address_length = (socklen_t)sizeof(struct sockaddr_in6);
            }

            next = (x_network_interface_t *)realloc(created, (created_count + 1U) * sizeof(*created));
            if (next == NULL) {
                free(created);
                freeifaddrs(ifaddrs_list);
                return ENOMEM;
            }

            created = next;
            memset(&created[created_count], 0, sizeof(created[created_count]));
            if (cursor->ifa_name != NULL) {
                strncpy(created[created_count].name, cursor->ifa_name, sizeof(created[created_count].name) - 1U);
            }
            created[created_count].flags = cursor->ifa_flags;
            (void)x_socket_copy_address(&created[created_count].address, cursor->ifa_addr, (size_t)address_length);
            ++created_count;
        }

        freeifaddrs(ifaddrs_list);
    }
#endif

    *interfaces = created;
    *count = created_count;
    return 0;
}

void x_network_interfaces_free(x_network_interface_t *interfaces)
{
    free(interfaces);
}
