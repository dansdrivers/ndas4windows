#pragma once
#include <lspx/lsp.h>
#include <lspx/lsp_util.h>

typedef struct _NDASCOMM_HANDLE *HNDAS;

HRESULT
NdasCommiLogin(
	__in HNDAS NdasHandle,
	__in const lsp_login_info_t* LspLoginInfo);

HRESULT
NdasCommiLogout(
	__in HNDAS NdasHandle);

HRESULT
NdasCommiHandshake(
	__in HNDAS NdasHandle);

HRESULT
NdasCommiAtaCommand(
	__in HNDAS NdasHandle,
	__inout lsp_ide_register_param_t* IdeReg,
	__in_opt lsp_io_data_buffer_t* IoDataBuffer,
	__in_opt lsp_extended_command_t* Cdb);

HRESULT
NdasCommiAtaRead(
	__in HNDAS NdasHandle,
	__in const lsp_large_integer_t* LogicalBlockAddress,
	__in USHORT TransferBlocks,
	__out LPVOID Buffer,
	__in size_t BufferLength);

HRESULT
NdasCommiAtaWrite(
	__in HNDAS NdasHandle,
	__in const lsp_large_integer_t* LogicalBlockAddress,
	__in USHORT TransferBlocks,
	__inout LPVOID Buffer,
	__in size_t BufferLength);

HRESULT
NdasCommiAtaVerify(
	__in HNDAS NdasHandle,
	__in const lsp_large_integer_t* LogicalBlockAddress,
	__in USHORT TransferBlocks,
	__reserved LPVOID Reserved1,
	__reserved size_t Reserved2);

HRESULT
NdasCommiTextCommand(
	__in HNDAS NdasHandle,
	__in UCHAR ParamType,
	__in UCHAR ParamVersion,
	__in const VOID* DataIn,
	__in USHORT DataInLength,
	__in VOID* DataOut,
	__in USHORT DataOutLength);

HRESULT
NdasCommiVendorCommand(
	__in HNDAS NdasHandle,
	__in lsp_uint16_t VendorId,
	__in lsp_uint8_t OpVersion,
	__in lsp_uint8_t OpCode,
	__in_bcount(ParameterLength) const lsp_uint8_t * Parameter,
	__in lsp_uint8_t ParameterLength,
	__in_opt const lsp_io_data_buffer_t* DataBuffer);
