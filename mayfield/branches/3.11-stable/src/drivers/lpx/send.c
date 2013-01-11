red;
        return -1;
    }

    //
    // Fill in TCP info, if requested.
    //

    if ( useTcp ) {

        tcpProtocolInfo = lpProtocolBuffer;

        tcpProtocolInfo->dwServiceFlags = XP_GUARANTEED_DELIVERY |
                                              XP_GUARANTEED_ORDER |
                                              XP_GRACEFUL_CLOSE |
//                                              XP_EXPEDITED_DATA |
                                              XP_FRAGMENTATION;
        tcpProtocolInfo->iAddressFamily = AF_UNSPEC;
        tcpProtocolInfo->iMaxSockAddr = sizeof(SOCKADDR_LPX);
        tcpProtocolInfo->iMinSockAddr = sizeof(SOCKADDR_LPX);
        tcpProtocolInfo->iSocketType = SOCK_STREAM;
        tcpProtocolInfo->iProtocol = IPPROTO_LPXTCP;
        tcpProtocolInfo->dwMessageSize = 0;
        tcpProtocolInfo->lpProtocol = (LPWSTR)
            ( (PBYTE)lpProtocolBuffer + *lpdwBufferLength -
                ( (wcslen( LPX_TCP_NAME ) + 1) * sizeof(WCHAR) ) );
        wcscpy( tcpProtocolInfo->lpProtocol, LPX_TCP_NAME );

        udpProtocolInfo = tcpProtocolInfo + 1;
        udpProtocolInfo->lpProtocol = (LPWSTR)
            ( (PBYTE)tcpProtocolInfo->lpProtocol -
                ( (wcslen( LPX_UDP_NAME ) + 1) * sizeof(WCHAR) ) );

    } else {

        udpProtocolInfo = lpProtocolBuffer;
        udpProtocolInfo->lpProtocol = (LPWSTR)
            ( (PBYTE)lpProtocolBuffer + *lpdwBufferLength -
                ( (wcslen( LPX_UDP_NAME ) + 1) * sizeof(WCHAR) ) );
    }

    //
    // Fill in UDP info, if requested.
    //

    if ( useUdp ) {

        udpProtocolInfo->dwServiceFlags = XP_CONNECTIONLESS |
                                              XP_MESSAGE_ORIENTED |
                                              XP_SUPPORTS_BROADCAST |
                                              XP_SUPPORTS_MULTICAST |
                                              XP_FRAGMENTATION;
        udpProtocolInfo->iAddressFamily = AF_UNSPEC;
        udpProtocolInfo->iMaxSockAddr = sizeof(SOCKADDR_LPX);
        udpProtocolInfo->iMinSockAddr = sizeof(SOCKADDR_LPX);
        udpProtocolInfo->iSocketType = SOCK_DGRAM;
        udpProtocolInfo->iProtocol = IPPROTO_LPXUDP;
        udpProtocolInfo->dwMessageSize = UDP_MESSAGE_SIZE;
        wcscpy( udpProtocolInfo->lpProtocol, LPX_UDP_NAME );
    }

    *lpdwBufferLength = bytesRequired;

    return (useTcp && useUdp) ? 2 : 1;

} // WSHEnumProtocols



BOOLEAN
IsTripleInList (
    IN PMAPPING_TRIPLE List,
    IN ULONG ListLength,
    IN INT AddressFamily,
    IN INT SocketType,
    IN INT Protocol
    )

/*++

Routine D