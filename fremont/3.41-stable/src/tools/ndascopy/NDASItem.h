#pragma once

class CNDASItem
{
public:
	int status;
	BYTE m_DeviceAddress[6], m_LocalAddress[6];

	CNDASItem(const BYTE DeviceAddress[], const BYTE LocalAddress[]) {
		memcpy(m_DeviceAddress, DeviceAddress, sizeof(m_DeviceAddress));
		memcpy(m_LocalAddress, LocalAddress, sizeof(m_LocalAddress));
		m_buffer_using = 0;
	};
	~CNDASItem(void);

	int m_buffer_using;
};
