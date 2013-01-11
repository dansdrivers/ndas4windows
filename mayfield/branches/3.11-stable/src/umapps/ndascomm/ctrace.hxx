#ifdef CTRACE_INCLUDED_ABF012
#error CTRACE.HXX should not be nested
#endif
#define CTRACE_INCLUDED_ABF012

static void DbgPrintA(LPCSTR szFormat, ... );

void DbgPrintA(LPCSTR szFormat, ... )
{
	va_list ap;
	static CHAR buffer[1024];
	va_start(ap, szFormat);
	::StringCchVPrintfA(buffer, sizeof(buffer) / sizeof(buffer[0]), szFormat, ap);
	::OutputDebugStringA(buffer);
	va_end(ap);
}

#ifdef _DEBUG
#define DBGTRACE(e,stmt) ::DbgPrintA("%s(%d): error %u(%08x): %s\n", __FILE__, __LINE__, e, e, stmt)
#else
#define DBGTRACE(e,stmt) __noop;
#endif

#define LSP_ERROR_BASE 0xECC00000
#define API_CALL(_stmt_) \
	do { \
		if (!(_stmt_)) { \
			DBGTRACE(::GetLastError(), #_stmt_); \
			return FALSE; \
		} \
	} while(0)

#define API_CALLEX(_err_, _stmt_) \
	do { \
		if (!(_stmt_)) { \
			DBGTRACE((_err_),#_stmt_); \
			::SetLastError(_err_); \
			return FALSE; \
		} \
	} while(0)

#define API_CALL_JMP(_label_, _stmt_) \
	do { \
		if (!(_stmt_)) { \
			DBGTRACE(::GetLastError(), #_stmt_); \
			goto _label_; \
		} \
	} while(0)

#define API_CALLEX_JMP(_err_, _label_, _stmt_) \
	do { \
		if (!(_stmt_)) { \
			DBGTRACE((_err_),#_stmt_); \
			::SetLastError(_err_); \
			goto _label_; \
		} \
	} while(0)

#define LSP_CALL(_stmt_) \
	do { \
		lsp_error_t lsp_err_ = (_stmt_); \
		if (LSP_SUCCESS != lsp_err_) { \
			DBGTRACE((LSP_ERROR_BASE | (lsp_err_)), #_stmt_); \
			::SetLastError(LSP_ERROR_BASE | (lsp_err_)); \
			return FALSE; \
		} \
	} while(0)

#define LSP_CALL_JMP(_label_, _stmt_) \
	do { \
		lsp_error_t lsp_err_ = (_stmt_); \
		if (LSP_SUCCESS != lsp_err_) { \
			DBGTRACE((LSP_ERROR_BASE | (lsp_err_)), #_stmt_); \
			::SetLastError(LSP_ERROR_BASE | (lsp_err_)); \
			goto _label_; \
		} \
	} while(0)

#define LSP_CALLEX(_err_, _stmt_) \
	do { \
		lsp_error_t lsp_err_ = (_stmt_); \
		if (LSP_SUCCESS != lsp_err_) { \
			DBGTRACE((_err_),#_stmt_); \
			::SetLastError(_err_); \
			return FALSE; \
		} \
	} while(0)

//
// APITRACE macro execute the "statement (including a function)"
// and if the execution is unsuccessful, it logs the error code
// to the debug tracer.
//
// BOOL MyFunction()
// {
//   BOOL fSuccess;
//   APITRACE( fSuccess = ReadFile(...) );
//   return fSuccess;
// }
// 
#define APITRACE(_stmt_) \
	do { \
		if (!(_stmt_)) { \
			DWORD saved__ = GetLastError(); \
			DBGTRACE(GetLastError(), #_stmt_); \
			SetLastError(saved__); \
		} \
	} while(0)

#define APITRACE_SE(_err_, _stmt_) \
	do { \
		if (!(_stmt_)) { \
			DBGTRACE(GetLastError(), #_stmt_); \
			SetLastError(_err_); \
		} \
	} while(0)

//
// APITRACE_RET macro invokes APITRACE macro.
// If _stmt_ is not successful, execute return FALSE;
// This macro simplifies WINAPI style function implementation
//
// BOOL MyFunction()
// {
//   APITRACE_RET( ReadFile(...) );
//   return TRUE;
// }
//
#define APITRACE_RET(_stmt_) \
	do { \
		BOOL success__; \
		APITRACE(success__ = (_stmt_)); \
		if (!success__) return FALSE; \
	} while(0)

#define APITRACE_SE_RET(_err_,_stmt_) \
	do { \
		BOOL success__; \
		APITRACE_SE((_err_), success__ = (_stmt_)); \
		if (!success__) return FALSE; \
	} while(0)

#define SOCKTRACE(_stmt_) \
	do \
	{ \
		int sockret__ = (_stmt_); \
		if (SOCKET_ERROR == sockret__) \
		{ \
			DWORD saved__ = WSAGetLastError(); \
			DBGTRACE(WSAGetLastError(), #_stmt_); \
			WSASetLastError(saved__); \
		} \
	} while(0)

#define LSPTRACE(_stmt_) \
	do \
	{ \
		lsp_error_t lsp_err__ = (_stmt_); \
		if (LSP_SUCCESS != lsp_err_) { \
			DBGTRACE((LSP_ERROR_BASE | (lsp_err__)), #_stmt_); \
			::SetLastError(LSP_ERROR_BASE | (lsp_err__)); \
			return FALSE; \
		} \
	} while(0)

#define SOCK_CALL(stmt) \
	do \
	{ \
		int err = stmt; \
		if (0 != err) \
		{ \
		DBGTRACE(WSAGetLastError(), #stmt); \
		return err; \
		} \
	} while(0)

#ifndef ASSERT_PARAM_ERROR
#define ASSERT_PARAM_ERROR ERROR_INVALID_PARAMETER
#endif
#define ASSERT_PARAM(pred) APITRACE_SE_RET(ASSERT_PARAM_ERROR, pred)

