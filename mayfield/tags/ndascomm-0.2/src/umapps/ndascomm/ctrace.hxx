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

#ifndef ASSERT_PARAM_ERROR
#define ASSERT_PARAM_ERROR ERROR_INVALID_PARAMETER
#endif
#define ASSERT_PARAM(pred) API_CALLEX(ASSERT_PARAM_ERROR, pred)
