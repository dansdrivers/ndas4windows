#pragma once

namespace nstf
{
	const DWORD tHeartbeatListener = 0x00000010;

	const DWORD tError = 0;
	const DWORD tWarning = 1;
	const DWORD tInfo = 2;
	const DWORD tTrace = 3;
	const DWORD tNoise = 4;
}

const DWORD TCService      = 0x00000001;
const DWORD TCPnp          = 0x00000002;
const DWORD TCHeartbeat    = 0x00000010;
const DWORD TCEventMon     = 0x00000020;
const DWORD TCEventPub     = 0x00000040;
const DWORD TCAutoReg      = 0x00000080;
const DWORD TCDevice       = 0x00000100;
const DWORD TCUnitDevice   = 0x00000200;
const DWORD TCLogDevice    = 0x00000400;
const DWORD TCSysUtils     = 0x00001000;
const DWORD TCHixUtils     = 0x00002000;
const DWORD TCHixServer    = 0x00004000;
const DWORD TCHixClient    = 0x00008000;

const DWORD TCDeviceReg    = 0x00001000;
const DWORD TCLogDeviceMan = 0x00002000;
const DWORD TLError = 0;
const DWORD TLWarning = 1;
const DWORD TLInfo = 2;
const DWORD TLTrace = 3;
const DWORD TLNoise = 4;
