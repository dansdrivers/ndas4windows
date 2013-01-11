#include "ndascommp.h"
#include "ndascommtransport.h"
#include "ndascommlsp.h"

HRESULT
NdasCommiLogin(
	__in HNDAS NdasHandle,
	__in const lsp_login_info_t* LspLoginInfo)
{
	lsp_status_t status = lsp_login(
		NdasHandle->LspHandle, 
		LspLoginInfo);
	return NdasCommTransportLspRequest(NdasHandle, &status, NULL);
}

HRESULT
NdasCommiLogout(
	__in HNDAS NdasHandle)
{
	lsp_status_t status = lsp_logout(NdasHandle->LspHandle);
	return NdasCommTransportLspRequest(NdasHandle, &status, NULL);
}

HRESULT
NdasCommiHandshake(
	__in HNDAS NdasHandle)
{
	lsp_status_t status = lsp_ata_handshake(NdasHandle->LspHandle);
	return NdasCommTransportLspRequest(NdasHandle, &status, NULL);
}

HRESULT
NdasCommiAtaCommand(
	__in HNDAS NdasHandle,
	__inout lsp_ide_register_param_t* IdeReg,
	__in_opt lsp_io_data_buffer_t* IoDataBuffer,
	__in_opt lsp_extended_command_t* Cdb)
{
	lsp_status_t status = lsp_ide_command(
		NdasHandle->LspHandle,
		IdeReg,
		IoDataBuffer,
		Cdb);
	return NdasCommTransportLspRequest(NdasHandle, &status, NULL);	
}

HRESULT
NdasCommiAtaRead(
	__in HNDAS NdasHandle,
	__in const lsp_large_integer_t* LogicalBlockAddress,
	__in USHORT TransferBlocks,
	__out LPVOID Buffer,
	__in size_t BufferLength)
{
	lsp_status_t lspStatus = lsp_ide_read(
		NdasHandle->LspHandle,
		LogicalBlockAddress,
		TransferBlocks,
		Buffer,
		BufferLength);

	return NdasCommTransportLspRequest(NdasHandle, &lspStatus, NULL);
}

HRESULT
NdasCommiAtaWrite(
	__in HNDAS NdasHandle,
	__in const lsp_large_integer_t* LogicalBlockAddress,
	__in USHORT TransferBlocks,
	__inout LPVOID Buffer,
	__in size_t BufferLength)
{
	lsp_status_t lspStatus = lsp_ide_write(
		NdasHandle->LspHandle,
		LogicalBlockAddress,
		TransferBlocks,
		Buffer,
		BufferLength);

	return NdasCommTransportLspRequest(NdasHandle, &lspStatus, NULL);
}

HRESULT
NdasCommiAtaVerify(
	__in HNDAS NdasHandle,
	__in const lsp_large_integer_t* LogicalBlockAddress,
	__in USHORT TransferBlocks,
	__reserved LPVOID Reserved1,
	__reserved size_t Reserved2)
{
	lsp_status_t lspStatus = lsp_ide_verify(
		NdasHandle->LspHandle,
		LogicalBlockAddress,
		TransferBlocks,
		Reserved1,
		Reserved2);

	return NdasCommTransportLspRequest(NdasHandle, &lspStatus, NULL);
}

HRESULT
NdasCommiTextCommand(
	__in HNDAS NdasHandle,
	__in UCHAR ParamType,
	__in UCHAR ParamVersion,
	__in const VOID* DataIn,
	__in USHORT DataInLength,
	__in VOID* DataOut,
	__in USHORT DataOutLength)
{
	lsp_status_t status = lsp_text_command(
		NdasHandle->LspHandle,
		ParamType,
		ParamVersion,
		DataIn,
		DataInLength,
		DataOut,
		DataOutLength);
	return NdasCommTransportLspRequest(NdasHandle, &status, NULL);
}

HRESULT
NdasCommiVendorCommand(
	__in HNDAS NdasHandle,
	__in lsp_uint16_t VendorId,
	__in lsp_uint8_t OpVersion,
	__in lsp_uint8_t OpCode,
	__in_bcount(ParameterLength) const lsp_uint8_t * Parameter,
	__in lsp_uint8_t ParameterLength,
	__in_opt const lsp_io_data_buffer_t* DataBuffer,
	lsp_extended_command_t *AhsRequest,
	lsp_extended_command_t *AhsResponse)
{
	lsp_status_t status = lsp_vendor_command(
		NdasHandle->LspHandle,
		VendorId,
		OpVersion,
		OpCode,
		Parameter,
		ParameterLength,
		DataBuffer,
		AhsRequest,
		AhsResponse);
	return NdasCommTransportLspRequest(NdasHandle, &status, NULL);
}
