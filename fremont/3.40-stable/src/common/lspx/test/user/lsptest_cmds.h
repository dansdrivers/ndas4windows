#pragma once

extern int lsptest_show_idereg_in;
extern int lsptest_show_idereg_out;

int
lsptest_login(
	lsptest_context_t* context,
	const lsp_login_info_t* login_info);

int
lsptest_logout(
	lsptest_context_t* context);

int
lspsh_ata_handshake(
	lsptest_context_t* context);

int 
lsptest_ide_write(
	lsptest_context_t* scontext,
	lsp_large_integer_t* location,
	void* buf,
	size_t len);

int 
lsptest_set_features(
	lsptest_context_t* scontext,
	lsp_ide_register_param_t* reg_in);

int 
lsptest_set_feature_enable_automatic_acoustic_management(
	lsptest_context_t* scontext,
	lsp_uint8_t level);

int 
lsptest_set_feature_disable_automatic_acoustic_management(
	lsptest_context_t* scontext);

int 
lsptest_smart_read_log(
	lsptest_context_t* scontext);

int 
lsptest_smart_read_data(
	lsptest_context_t* scontext);

int 
lsptest_smart_return_status(
	lsptest_context_t* scontext);

int lspsh_ata_idle(lsptest_context_t* context);
int lspsh_ata_idle_immediate(lsptest_context_t* scontext);

int lspsh_ata_identify(lsptest_context_t* scontext);

int lspsh_ata_identify_packet_device(lsptest_context_t* context);

typedef enum _lspsh_read_mode_t {
	lspsh_read_bin,
	lspsh_read_text,
	lspsh_read_mbr,
} lspsh_read_mode_t;

int ndavs_disp_mbr(void* buf);

int lsptest_read(
	lsptest_context_t* context,
	lsp_large_integer_t* location,
	FILE* out,
	size_t sectors,
	lspsh_read_mode_t parse_mode);

int lsptest_write(
	lsptest_context_t* scontext,
	lsp_large_integer_t* location,
	FILE* in,
	size_t sector_count);

int lsptest_read_native_max_address(lsptest_context_t* scontext);
int lsptest_read_native_max_address_ext(lsptest_context_t* scontext);

int lspsh_ata_check_power_mode(lsptest_context_t* scontext);
int lspsh_ata_device_reset(lsptest_context_t* context);
int lspsh_ata_execute_device_diagnostic(lsptest_context_t* context);
int lspsh_ata_flush_cache(lsptest_context_t* scontext);
int lspsh_ata_flush_cache_ext(lsptest_context_t* scontext);

int lspsh_ata_sleep(lsptest_context_t* context);
int lspsh_ata_standby(lsptest_context_t* context);
int lspsh_ata_standby_immediate(lsptest_context_t* context);

int lspsh_text_target_list(lsptest_context_t* context);

int lspsh_text_target_data(
	__in lsptest_context_t* context,
	__in int read,
	__in_bcount(8) const unsigned char* data);
