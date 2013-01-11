#ifndef LSP_IDE_DEF_H_INCLUDED
#define LSP_IDE_DEF_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/* 

ATA command code table

ND: Non-data command
PI: PIO data-in command
PO: PIO data-out command
DM: DMA command
DMQ: DMA QUEUED command
DR: DEVICE RESET command
DD: EXECUTE DEVICE DIAGNOSTIC command
P: PACKET command
VS: Vendor specific

M=Mandatory O=Optional N=Use prohibited
F=unless CFA, vendor specific

*/

typedef enum _lsp_ide_cmd_t {
	/* ND F N */ LSP_IDE_CMD_CFA_ERASE_SECTORS = 0xC0,
	/* ND O N */ LSP_IDE_CMD_CFA_REQUEST_EXTENDED_ERROR_CODE = 0x03,
	/* PI O N */ LSP_IDE_CMD_CFA_TRANSLATE_SECTOR = 0x87,
	/* PO O N */ LSP_IDE_CMD_CFA_WRITE_MULTIPLE_WITHOUT_ERASE = 0xCD,
	/* PO O N */ LSP_IDE_CMD_CFA_WRITE_SECTORS_WITHOUT_ERASE = 0x38,
	/* ND O N */ LSP_IDE_CMD_CHECK_MEDIA_TYPE_CARD = 0xD1,
	/* ND M M */ LSP_IDE_CMD_CHECK_POWER_MODE = 0xE5,
	/* ND O O */ LSP_IDE_CMD_CONFIGURE_STREAM = 0x51,
	/* ND/P O O */ LSP_IDE_CMD_DEVICE_CONFIGURATION = 0xB1,
	/* DR N M */ LSP_IDE_CMD_DEVICE_RESET = 0x08,
	/* PO O N */ LSP_IDE_CMD_DEVICE_DOWNLOAD_MICROCODE = 0x92,
	/* DD M M */ LSP_IDE_CMD_EXECUTE_DEVICE_DIAGNOSTIC = 0x90,
	/* ND M O */ LSP_IDE_CMD_FLUSH_CACHE = 0xE7,
	/* ND O N */ LSP_IDE_CMD_FLUSH_CACHE_EXT = 0xEA,
	/* ND O O */ LSP_IDE_CMD_GET_MEDIA_STATUS = 0xDA,
	/* PI M M */ LSP_IDE_CMD_IDENTIFY_DEVICE = 0xEC,
	/* PI N M */ LSP_IDE_CMD_IDENTIFY_PACKET_DEVICE = 0xA1,
	/* ND M O */ LSP_IDE_CMD_IDLE = 0xE3,
	/* ND M M */ LSP_IDE_CMD_IDLE_IMMEDIATE = 0xE1,
	/* ND O N */ LSP_IDE_CMD_MEDIA_EJECT = 0xED,
	/* ND O N */ LSP_IDE_CMD_MEDIA_LOCK = 0xDE,
	/* ND O N */ LSP_IDE_CMD_MEDIA_UNLOCK = 0xDF,
	/* ND O M */ LSP_IDE_CMD_NOP = 0x00,
	/* P  N M */ LSP_IDE_CMD_PACKET = 0xA0,
	/* PI O N */ LSP_IDE_CMD_READ_BUFFER = 0xE4,
	/* DM M N */ LSP_IDE_CMD_READ_DMA = 0xC8,
	/* DM O N */ LSP_IDE_CMD_READ_DMA_EXT = 0x25,
	/* DMQ O N */ LSP_IDE_CMD_READ_DMA_QUEUED = 0xC7,
	/* DMQ O N */ LSP_IDE_CMD_READ_DMA_QUEUED_EXT = 0x26,
	/* PI O O */ LSP_IDE_CMD_READ_LOG_EXT = 0x2F,
	/* PI M N */ LSP_IDE_CMD_READ_MULTIPLE = 0xC4,
	/* PI O N */ LSP_IDE_CMD_READ_MULTIPLE_EXT = 0x29,
	/* ND O O */ LSP_IDE_CMD_READ_NATIVE_MAX_ADDRESS = 0xF8,
	/* ND O N */ LSP_IDE_CMD_READ_NATIVE_MAX_ADDRESS_EXT = 0x27,
	/* PI M M */ LSP_IDE_CMD_READ_SECTORS = 0x20,
	/* PI O N */ LSP_IDE_CMD_READ_SECTORS_EXT = 0x24,
	/* DM O N */ LSP_IDE_CMD_READ_STREAM_DMA_EXT = 0x2A,
	/* PI O N */ LSP_IDE_CMD_READ_STREAM_EXT = 0x2B,
	/* ND M N */ LSP_IDE_CMD_READ_VERIFY_SECTORS = 0x40,
	/* ND O N */ LSP_IDE_CMD_READ_VERIFY_SECTORS_EXT = 0x42,
	/* PO O O */ LSP_IDE_CMD_SECURITY_DISABLE_PASSWORD = 0xF6,
	/* ND O O */ LSP_IDE_CMD_SECURITY_ERASE_PREPARE = 0xF3,
	/* PO O O */ LSP_IDE_CMD_SECURITY_ERASE_UNIT = 0xF4,
	/* ND O O */ LSP_IDE_CMD_SECURITY_FREEZE_LOCK = 0xF5,
	/* PO O O */ LSP_IDE_CMD_SECURITY_SET_PASSWORD = 0xF1,
	/* PO O O */ LSP_IDE_CMD_SECURITY_UNLOCK = 0xF2,
	/* P/DMQ O O */ LSP_IDE_CMD_SERVICE = 0xA2,
	/* ND M M */ LSP_IDE_CMD_SET_FEATURES = 0xEF,
	/* ND O O */ LSP_IDE_CMD_SET_MAX = 0xF9,
	/* ND O N */ LSP_IDE_CMD_SET_MAX_ADDRESS_EXT = 0x37,
	/* ND M N */ LSP_IDE_CMD_SET_MULTIPLE_MODE = 0xC6,
	/* ND M M */ LSP_IDE_CMD_SLEEP = 0xE6,
	/* ND/P O N */ LSP_IDE_CMD_SMART = 0xB0,
	/* ND M O */ LSP_IDE_CMD_STANDBY = 0xE2,
	/* ND M M */ LSP_IDE_CMD_STANDBY_IMMEDIATE = 0xE0,
	/* PO O N */ LSP_IDE_CMD_WRITE_BUFFER = 0xE8,
	/* DM M N */ LSP_IDE_CMD_WRITE_DMA = 0xCA,
	/* DM O N */ LSP_IDE_CMD_WRITE_DMA_EXT = 0x35,
	/* DM O N */ LSP_IDE_CMD_WRITE_DMA_FUA_EXT = 0x3D,
	/* DMQ O N */ LSP_IDE_CMD_WRITE_DMA_QUEUED = 0xCC,
	/* DMQ O N */ LSP_IDE_CMD_WRITE_DMA_QUEUED_EXT = 0x36,
	/* DMQ O N */ LSP_IDE_CMD_WRITE_DMA_QUEUED_FUA_EXT = 0x3E,
	/* PO O O */ LSP_IDE_CMD_WRITE_LOG_EXT = 0x3F,
	/* PO M N */ LSP_IDE_CMD_WRITE_MULTIPLE = 0xC5,
	/* PO O N */ LSP_IDE_CMD_WRITE_MULTIPLE_EXT = 0x39,
	/* PO O N */ LSP_IDE_CMD_WRITE_MULTIPLE_FUA_EXT = 0xCE,
	/* PO M N */ LSP_IDE_CMD_WRITE_SECTORS = 0x30,
	/* PO O N */ LSP_IDE_CMD_WRITE_SECTORS_EXT = 0x34,
	/* DM O N */ LSP_IDE_CMD_WRITE_STREAM_DMA_EXT = 0x3A,
	/* PO O N */ LSP_IDE_CMD_WRITE_STREAM_EXT = 0x3B
} lsp_ide_cmd_t;

typedef enum _lsp_ide_device_configuration_feature_t {
	LSP_IDE_DEVICE_CONFIGURATION_RESTORE = 0xC0,
	LSP_IDE_DEVICE_CONFIGURATION_FREEZE_LOCK = 0xC1,
	LSP_IDE_DEVICE_CONFIGURATION_IDENTIFY = 0xC2,
	LSP_IDE_DEVICE_CONFIGURATION_SET = 0xC3
} lsp_ide_device_configuration_feature_t;

typedef enum _lsp_ide_set_features_feature_t {
	LSP_IDE_SET_FEATURES_ENABLE_8BIT_PIO = 0x01,
	LSP_IDE_SET_FEATURES_ENABLE_WRITE_CACHE = 0x02,
	LSP_IDE_SET_FEATURES_SET_TRANSFER_MODE = 0x03,
	LSP_IDE_SET_FEATURES_OBSOLETE_04 = 0x04,
	/* APM: Advanced Power Management */
	LSP_IDE_SET_FEATURES_ENABLE_APM = 0x05,
	LSP_IDE_SET_FEATURES_ENABLE_POWER_UP_IN_STANDBY = 0x06,
	LSP_IDE_SET_FEATURES_POWER_UP_IN_STANDBY_DEVICE_SPIN_UP = 0x07,
	LSP_IDE_SET_FEATURES_RESERVED_09 = 0x09,
	LSP_IDE_SET_FEATURES_ENABLE_CFA_POWER_MODE_1 = 0x0A,
	LSP_IDE_SET_FEATURES_ENABLE_SATA_FEATURE = 0x10,
	LSP_IDE_SET_FEATURES_RESERVED_20_TECHSUPPORT = 0x20,
	LSP_IDE_SET_FEATURES_RESERVED_21_TECHSUPPORT = 0x21,
	LSP_IDE_SET_FEATURES_DISABLE_MEDIA_STATUS_NOTIFICATION = 0x31,
	LSP_IDE_SET_FEATURES_OBSOLET_33 = 0x33,
	/* AAM: Automatic Acoustic Management */
	LSP_IDE_SET_FEATURES_ENABLE_AAM_FEATURE_SET = 0x42,
	LSP_IDE_SET_FEATURES_SET_MAX_HOST_INTERFACE_SECTOR_TIMES = 0x43,
	LSP_IDE_SET_FEATURES_OBSOLETE_44 = 0x44,
	LSP_IDE_SET_FEATURES_OBSOLETE_54 = 0x54,
	LSP_IDE_SET_FEATURES_DISABLE_READ_LOOK_AHEAD = 0x55,
	LSP_IDE_SET_FEATURES_ENABLE_RELEASE_INT = 0x5D,
	LSP_IDE_SET_FEATURES_ENABLE_SERVICE_INT = 0x5E,
	LSP_IDE_SET_FEATURES_DISABLE_REVERTING_TO_POWER_ON_DEFAULTS = 0x66,
	LSP_IDE_SET_FEATURES_OBSOLETE_77 = 0x77,
	LSP_IDE_SET_FEATURES_DISABLE_8BIT_PIO = 0x81,
	LSP_IDE_SET_FEATURES_DISABLE_WRITE_CACHE = 0x82,
	LSP_IDE_SET_FEATURES_OBSOLETE_84 = 0x84,
	LSP_IDE_SET_FEATURES_DISABLE_APM = 0x85,
	LSP_IDE_SET_FEATURES_DISABLE_POWER_UP_IN_STANDBY = 0x86,
	LSP_IDE_SET_FEATURES_OBSOLETE_88 = 0x88,
	LSP_IDE_SET_FEATURES_RESERVED_89 = 0x89,
	LSP_IDE_SET_FEATURES_DISABLE_CFA_POWER_MODE_1 = 0x8A,
	LSP_IDE_SET_FEATURES_DISABLE_SATA_FEATURE = 0x90,
	LSP_IDE_SET_FEATURES_ENABLE_MEDIA_STATUS_NOTIFICATION = 0x95,
	LSP_IDE_SET_FEATURES_OBSOLETE_99 = 0x99,
	LSP_IDE_SET_FEATURES_OBSOLETE_9A = 0x9A,
	LSP_IDE_SET_FEATURES_ENABLE_READ_LOOK_AHEAD = 0xAA,
	LSP_IDE_SET_FEATURES_OBSOLETE_AB = 0xAB,
	LSP_IDE_SET_FEATURES_OBSOLETE_BB = 0xBB,
	LSP_IDE_SET_FEATURES_DISABLE_AAM = 0xC2,
	LSP_IDE_SET_FEATURES_ENABLE_REVERTING_TO_POWER_ON_DEFAULTS = 0xCC,
	LSP_IDE_SET_FEATURES_DISABLE_RELEASE_INT = 0xDD,
	LSP_IDE_SET_FEATURES_DISABLE_SERVICE_INT = 0xDE,
	LSP_IDE_SET_FEATURES_OBSOLETE_E0 = 0xE0,
	/* F0h-FFh: reserved for CFA */
	LSP_IDE_SET_FEATURES_CFA_F0 = 0xF0,
	LSP_IDE_SET_FEATURES_CFA_FF = 0xFF
} lsp_ide_set_features_feature_t;

typedef enum _lsp_cmd_set_max_feature_t {
	LSP_IDE_SET_MAX_OBSOLETE = 0x00,
	LSP_IDE_SET_MAX_SET_PASSWORD = 0x01,
	LSP_IDE_SET_MAX_LOCK = 0x02,
	LSP_IDE_SET_MAX_UNLOCK = 0x03,
	LSP_IDE_SET_MAX_FREEZE_LOCK = 0x04
} lsp_cmd_set_max_feature_t;

/* required to use lsp_htoles(x) in idereg sector counter register bits(7:3)*/
typedef enum _lsp_cmd_set_transfer_mode_t {
	LSP_IDE_TRANSFER_MODE_PIO_DEFAULT = 0x00,
	LSP_IDE_TRANSFER_MODE_PIO_FLOW_CONTROL = 0x01 << 3,
	LSP_IDE_TRANSFER_MODE_SINGLEWORD_DMA = 0x02 << 3,
	LSP_IDE_TRANSFER_MODE_MULTIWORD_DMA = 0x04 << 3,
	LSP_IDE_TRANSFER_MODE_ULTRA_DMA = 0x08 << 3,
	LSP_IDE_TRANSFER_MODE_RESERVED_1 = 0x10 << 3,
	LSP_IDE_TRANSFER_MODE_UNSPECIFIED = 0xFF /* not a valid value */
} lsp_cmd_set_transfer_mode_t;

typedef enum _lsp_cmd_smart_feature_t {
	LSP_IDE_SMART_READ_DATA = 0xD0,
	LSP_IDE_SMART_OBSOLETE_D1 = 0xD1,
	LSP_IDE_SMART_ENABLE_DISABLE_ATTRIBUTE_AUTOSAVE = 0xD2,
	LSP_IDE_SMART_OBSOLETE_D3 = 0xD3,
	LSP_IDE_SMART_EXECUTE_OFFLINE_IMMEDIATE = 0xD4,
	LSP_IDE_SMART_READ_LOG = 0xD5,
	LSP_IDE_SMART_WRITE_LOG = 0xD6,
	LSP_IDE_SMART_OBSOLETE_D7 = 0xD7,
	LSP_IDE_SMART_ENABLE_OPERATIONS = 0xD8,
	LSP_IDE_SMART_DISABLE_OPERATIONS = 0xD9,
	LSP_IDE_SMART_RETURN_STATUS = 0xDA,
	LSP_IDE_SMART_OBSOLETE_DB = 0xDB
} lsp_cmd_smart_feature_t;

/* IDENTIFY device data (response to 0xEC) */

#if !defined(__GNUC__)
#pragma pack(push, _lsp_ide_identify_data_t, 1)
#endif

typedef struct _lsp_ide_identify_data_t {

	/* word 0 */
	struct {
#if defined(__BIG_ENDIAN__)
		lsp_uint8_t removable_media : 1;
		lsp_uint8_t fixed_device : 1;
		lsp_uint8_t retired_2 : 3;
		lsp_uint8_t response_incomplete : 1;
		lsp_uint8_t retired_3 : 1;
		lsp_uint8_t reserved_1 : 1;

		lsp_uint8_t device_type : 1;
		lsp_uint8_t retired_1 : 7;
#else
		lsp_uint8_t reserved_1 : 1;
		lsp_uint8_t retired_3 : 1;
		lsp_uint8_t response_incomplete : 1;
		lsp_uint8_t retired_2 : 3;
		lsp_uint8_t fixed_device : 1;
		lsp_uint8_t removable_media : 1;

		lsp_uint8_t retired_1 : 7;
		lsp_uint8_t device_type : 1;
#endif
	} __lsp_attr_packed__ general_configuration;

	/* word 1 */
	lsp_uint16_t num_cylinders;
	/* word 2 */
	lsp_uint16_t reserved_word_2;
	/* word 3 */
	lsp_uint16_t num_heads;
	/* word 4-5 */
	lsp_uint16_t retired_1[2];
	/* word 6 */
	lsp_uint16_t num_sectors_per_track;
	/* word 7-9 */
	lsp_uint16_t vendor_unique_1[3];
	/* word 10-19 */
	lsp_uint8_t	 serial_number[20];
	/* word 20-21 */
	lsp_uint16_t retired_2[2];
	/* word 22 */
	lsp_uint16_t obsolete_1;
	/* word 23-26 */
	lsp_uint8_t	 firmware_revision[8];
	/* word 27-46 */
	lsp_uint8_t	 model_number[40];
	/* word 47 */
	lsp_uint8_t	 maximum_block_transfer;
	lsp_uint8_t	 vendor_unique_2;
	/* word 48 */
	lsp_uint16_t reserved_word_48;

	/* word 49-50 */
	struct {
#if defined(__BIG_ENDIAN__)
		lsp_uint8_t reserved_byte_49;

		lsp_uint8_t reserved_2 : 2;
		lsp_uint8_t standyby_timer_support : 1;
		lsp_uint8_t reserved_1 : 1;
		lsp_uint8_t iordy_supported : 1;
		lsp_uint8_t iordy_disable : 1;
		lsp_uint8_t lba_supported : 1;
		lsp_uint8_t dma_supported : 1;
#else
		lsp_uint8_t reserved_byte_49;

		lsp_uint8_t dma_supported : 1;
		lsp_uint8_t lba_supported : 1;
		lsp_uint8_t iordy_disable : 1;
		lsp_uint8_t iordy_supported : 1;
		lsp_uint8_t reserved_1 : 1;
		lsp_uint8_t standyby_timer_support : 1;
		lsp_uint8_t reserved_2 : 2;
#endif
		lsp_uint16_t reserved_word_50;
	} __lsp_attr_packed__ capabilities;

	/* word 51-52 */
	lsp_uint16_t obsolete_words_51[2];

	/* word 53 */
#if defined(__BIG_ENDIAN__)
	lsp_uint8_t reserved_3_1 : 5;
	lsp_uint8_t translation_fields_valid : 3;

	lsp_uint8_t reserved_3_2 : 8;
#else
	lsp_uint8_t translation_fields_valid : 3;
	lsp_uint8_t reserved_3_1 : 5;

	lsp_uint8_t reserved_3_2 : 8;
#endif

	/* word 54 */
	lsp_uint16_t number_of_current_cylinders;
	/* word 55 */
	lsp_uint16_t number_of_current_heads;
	/* word 56 */
	lsp_uint16_t current_sectors_per_track;

	/* word 57-58 */
	lsp_uint32_t current_sector_capacity;

	/* word 59 */
#if defined(__BIG_ENDIAN__)
	lsp_uint8_t	 current_multi_sector_setting;

	lsp_uint8_t	 reserved_byte_59 : 7;
	lsp_uint8_t	 multi_sector_setting_valid : 1;
#else
	lsp_uint8_t	 current_multi_sector_setting;

	lsp_uint8_t	 multi_sector_setting_valid : 1;
	lsp_uint8_t	 reserved_byte_59 : 7;
#endif

	/* word 60-61 */
	/* aka user_addressable_sectors */
	lsp_uint32_t lba28_capacity;

	/* word 62 */
	lsp_uint8_t singleword_dma_support : 8;
	lsp_uint8_t singleword_dma_active : 8;

	/* word 63 */
	lsp_uint8_t multiword_dma_support : 8;
	lsp_uint8_t multiword_dma_active : 8;

	/* word 64 */
	lsp_uint8_t advanced_pio_modes : 8;
	lsp_uint8_t reserved_byte_64 : 8;

	/* word 65 */
	lsp_uint16_t minimum_mwx_fer_cycle_time;
	/* word 66 */
	lsp_uint16_t recommended_mwx_fer_cycle_time;
	/* word 67 */
	lsp_uint16_t minimum_pio_cycle_time;
	/* word 68 */
	lsp_uint16_t minimum_pio_cycle_time_iordy;

	/* word 69-74 */
	lsp_uint16_t reserved_words_69[6];

	/* word 75 */
#if defined(__BIG_ENDIAN__)
	lsp_uint8_t reserved_word_75_1 : 3;
	lsp_uint8_t queue_depth : 5;

	lsp_uint8_t reserved_word_75_2 : 8;
#else
	lsp_uint8_t queue_depth : 5;
	lsp_uint8_t reserved_word_75_1 : 3;

	lsp_uint8_t reserved_word_75_2 : 8;
#endif

	/* word 76-79 */
	lsp_uint16_t reserved_words_76[4];

	/* word 80 */
	lsp_uint16_t major_revision;
	/* word 81 */
	lsp_uint16_t minor_revision;

	struct {

		/* word 82 */
#if defined(__BIG_ENDIAN__)
		lsp_uint8_t release_interrupt : 1;
		lsp_uint8_t look_ahead : 1;
		lsp_uint8_t write_cache : 1;
		lsp_uint8_t no_packet_clear_to_zero : 1;
		lsp_uint8_t power_management : 1;
		lsp_uint8_t removable_media_feature : 1;
		lsp_uint8_t security_mode : 1;
		lsp_uint8_t smart_commands : 1;

		lsp_uint8_t obsolete2 : 1;
		lsp_uint8_t nop : 1;
		lsp_uint8_t read_buffer : 1;
		lsp_uint8_t write_buffer : 1;
		lsp_uint8_t obsolete1 : 1;
		lsp_uint8_t host_protected_area : 1;
		lsp_uint8_t device_reset : 1;
		lsp_uint8_t service_interrupt : 1;
#else
		lsp_uint8_t smart_commands : 1;
		lsp_uint8_t security_mode : 1;
		lsp_uint8_t removable_media_feature : 1;
		lsp_uint8_t power_management : 1;
		lsp_uint8_t no_packet_clear_to_zero : 1;
		lsp_uint8_t write_cache : 1;
		lsp_uint8_t look_ahead : 1;
		lsp_uint8_t release_interrupt : 1;

		lsp_uint8_t service_interrupt : 1;
		lsp_uint8_t device_reset : 1;
		lsp_uint8_t host_protected_area : 1;
		lsp_uint8_t obsolete1 : 1;
		lsp_uint8_t write_buffer : 1;
		lsp_uint8_t read_buffer : 1;
		lsp_uint8_t nop : 1;
		lsp_uint8_t obsolete2 : 1;
#endif

		/* word 83 */
#if defined(__BIG_ENDIAN__)
		lsp_uint8_t reserved_86_83_7 : 1;
		lsp_uint8_t manual_power_up : 1;
		lsp_uint8_t power_up_in_standby : 1;
		lsp_uint8_t msn : 1;
		lsp_uint8_t advanced_pm : 1;
		lsp_uint8_t cfa : 1;
		lsp_uint8_t dma_queued : 1;
		lsp_uint8_t download_microcode : 1;

		lsp_uint8_t reserved_86_83_15 : 1;
		lsp_uint8_t reserved_86_83_14 : 1;
		lsp_uint8_t flush_cache_ext : 1;
		lsp_uint8_t flush_cache : 1;
		lsp_uint8_t device_config_overlay : 1;
		lsp_uint8_t big_lba : 1;
		lsp_uint8_t acoustics : 1;
		lsp_uint8_t set_max : 1;
#else
		lsp_uint8_t download_microcode : 1;
		lsp_uint8_t dma_queued : 1;
		lsp_uint8_t cfa : 1;
		lsp_uint8_t advanced_pm : 1;
		lsp_uint8_t msn : 1;
		lsp_uint8_t power_up_in_standby : 1;
		lsp_uint8_t manual_power_up : 1;
		lsp_uint8_t reserved_86_83_7 : 1;

		lsp_uint8_t set_max : 1;
		lsp_uint8_t acoustics : 1;
		lsp_uint8_t big_lba : 1;
		lsp_uint8_t device_config_overlay : 1;
		lsp_uint8_t flush_cache : 1;
		lsp_uint8_t flush_cache_ext : 1;
		lsp_uint8_t reserved_86_83_14 : 1;
		lsp_uint8_t reserved_86_83_15 : 1;
#endif

		/* word 84 */
#if defined(__BIG_ENDIAN__)
		lsp_uint8_t write_queued_fua : 1;
		lsp_uint8_t write_fua : 1;
		lsp_uint8_t gp_logging : 1;
		lsp_uint8_t streaming_feature : 1;
		lsp_uint8_t media_card_pass_through : 1;
		lsp_uint8_t media_serial_number : 1;
		lsp_uint8_t smart_self_test : 1;
		lsp_uint8_t smart_error_log : 1;

		lsp_uint8_t valid_clear_to_zero : 1;
		lsp_uint8_t valid_set_to_one : 1;
		lsp_uint8_t idle_with_unload_feature : 1;
		lsp_uint8_t reserved_for_tech_report_87_84_12 : 1;
		lsp_uint8_t reserved_for_tech_report_87_84_11 : 1;
		lsp_uint8_t urg_write_stream : 1;
		lsp_uint8_t urg_read_stream : 1;
		lsp_uint8_t wwn_64_bit : 1;
#else
		lsp_uint8_t smart_error_log : 1;
		lsp_uint8_t smart_self_test : 1;
		lsp_uint8_t media_serial_number : 1;
		lsp_uint8_t media_card_pass_through : 1;
		lsp_uint8_t streaming_feature : 1;
		lsp_uint8_t gp_logging : 1;
		lsp_uint8_t write_fua : 1;
		lsp_uint8_t write_queued_fua : 1;

		lsp_uint8_t wwn_64_bit : 1;
		lsp_uint8_t urg_read_stream : 1;
		lsp_uint8_t urg_write_stream : 1;
		lsp_uint8_t reserved_for_tech_report_87_84_11 : 1;
		lsp_uint8_t reserved_for_tech_report_87_84_12 : 1;
		lsp_uint8_t idle_with_unload_feature : 1;
		lsp_uint8_t valid_set_to_one : 1;
		lsp_uint8_t valid_clear_to_zero : 1;
#endif

	} __lsp_attr_packed__ command_set_support; 

	struct {

		/* word 85 */
#if defined(__BIG_ENDIAN__)
		lsp_uint8_t release_interrupt : 1;
		lsp_uint8_t look_ahead : 1;
		lsp_uint8_t write_cache : 1;
		lsp_uint8_t no_packet_clear_to_zero : 1;
		lsp_uint8_t power_management : 1;
		lsp_uint8_t removable_media_feature : 1;
		lsp_uint8_t security_mode : 1;
		lsp_uint8_t smart_commands : 1;

		lsp_uint8_t obsolete2 : 1;
		lsp_uint8_t nop : 1;
		lsp_uint8_t read_buffer : 1;
		lsp_uint8_t write_buffer : 1;
		lsp_uint8_t obsolete1 : 1;
		lsp_uint8_t host_protected_area : 1;
		lsp_uint8_t device_reset : 1;
		lsp_uint8_t service_interrupt : 1;
#else
		lsp_uint8_t smart_commands : 1;
		lsp_uint8_t security_mode : 1;
		lsp_uint8_t removable_media_feature : 1;
		lsp_uint8_t power_management : 1;
		lsp_uint8_t no_packet_clear_to_zero : 1;
		lsp_uint8_t write_cache : 1;
		lsp_uint8_t look_ahead : 1;
		lsp_uint8_t release_interrupt : 1;

		lsp_uint8_t service_interrupt : 1;
		lsp_uint8_t device_reset : 1;
		lsp_uint8_t host_protected_area : 1;
		lsp_uint8_t obsolete1 : 1;
		lsp_uint8_t write_buffer : 1;
		lsp_uint8_t read_buffer : 1;
		lsp_uint8_t nop : 1;
		lsp_uint8_t obsolete2 : 1;
#endif

		/* word 86 */
#if defined(__BIG_ENDIAN__)
		lsp_uint8_t reserved_86_83_7 : 1;
		lsp_uint8_t manual_power_up : 1;
		lsp_uint8_t power_up_in_standby : 1;
		lsp_uint8_t msn : 1;
		lsp_uint8_t advanced_pm : 1;
		lsp_uint8_t cfa : 1;
		lsp_uint8_t dma_queued : 1;
		lsp_uint8_t download_microcode : 1;

		lsp_uint8_t reserved_86_83_15 : 1;
		lsp_uint8_t reserved_86_83_14 : 1;
		lsp_uint8_t flush_cache_ext : 1;
		lsp_uint8_t flush_cache : 1;
		lsp_uint8_t device_config_overlay : 1;
		lsp_uint8_t big_lba : 1;
		lsp_uint8_t acoustics : 1;
		lsp_uint8_t set_max : 1;
#else
		lsp_uint8_t download_microcode : 1;
		lsp_uint8_t dma_queued : 1;
		lsp_uint8_t cfa : 1;
		lsp_uint8_t advanced_pm : 1;
		lsp_uint8_t msn : 1;
		lsp_uint8_t power_up_in_standby : 1;
		lsp_uint8_t manual_power_up : 1;
		lsp_uint8_t reserved_86_83_7 : 1;

		lsp_uint8_t set_max : 1;
		lsp_uint8_t acoustics : 1;
		lsp_uint8_t big_lba : 1;
		lsp_uint8_t device_config_overlay : 1;
		lsp_uint8_t flush_cache : 1;
		lsp_uint8_t flush_cache_ext : 1;
		lsp_uint8_t reserved_86_83_14 : 1;
		lsp_uint8_t reserved_86_83_15 : 1;
#endif

		/* word 87 */
#if defined(__BIG_ENDIAN__)
		lsp_uint8_t write_queued_fua : 1;
		lsp_uint8_t write_fua : 1;
		lsp_uint8_t gp_logging : 1;
		lsp_uint8_t streaming_feature : 1;
		lsp_uint8_t media_card_pass_through : 1;
		lsp_uint8_t media_serial_number : 1;
		lsp_uint8_t smart_self_test : 1;
		lsp_uint8_t smart_error_log : 1;

		lsp_uint8_t valid_clear_to_zero : 1;
		lsp_uint8_t valid_set_to_one : 1;
		lsp_uint8_t idle_with_unload_feature : 1;
		lsp_uint8_t reserved_for_tech_report_87_84_12 : 1;
		lsp_uint8_t reserved_for_tech_report_87_84_11 : 1;
		lsp_uint8_t urg_write_stream : 1;
		lsp_uint8_t urg_read_stream : 1;
		lsp_uint8_t wwn_64_bit : 1;
#else
		lsp_uint8_t smart_error_log : 1;
		lsp_uint8_t smart_self_test : 1;
		lsp_uint8_t media_serial_number : 1;
		lsp_uint8_t media_card_pass_through : 1;
		lsp_uint8_t streaming_feature : 1;
		lsp_uint8_t gp_logging : 1;
		lsp_uint8_t write_fua : 1;
		lsp_uint8_t write_queued_fua : 1;

		lsp_uint8_t wwn_64_bit : 1;
		lsp_uint8_t urg_read_stream : 1;
		lsp_uint8_t urg_write_stream : 1;
		lsp_uint8_t reserved_for_tech_report_87_84_11 : 1;
		lsp_uint8_t reserved_for_tech_report_87_84_12 : 1;
		lsp_uint8_t idle_with_unload_feature : 1;
		lsp_uint8_t valid_set_to_one : 1;
		lsp_uint8_t valid_clear_to_zero : 1;
#endif

	} __lsp_attr_packed__ command_set_active;

	/* word 88 */
	lsp_uint8_t ultra_dma_support : 8;
	lsp_uint8_t ultra_dma_active  : 8;

	/* word 89-92 */
	lsp_uint16_t reserved_word_89[4];

	/* word 93 */
	lsp_uint16_t hardware_reset_result;

	/* word 94 */
	lsp_uint8_t current_acoustic_value : 8;
	lsp_uint8_t recommended_acoustic_value : 8;

	/* word 95-99 */
	lsp_uint16_t reserved_word_95[5];

	/* word 100-103 */
	/* 268,435,455 (0x0FFFFFFF) is maximum LBA in 28-bit addressing mode */
	/* 281,474,976,710,655 is the maximum LBA in 48-bit addressing mode */
	/* aka max_48_bit_lba_lsw,msw */
	lsp_uint32_t lba48_capacity_lsw;
	lsp_uint32_t lba48_capacity_msw;

	/* word 104 */
	lsp_uint16_t streaming_transfer_time;
	/* word 105 */
	lsp_uint16_t reserved_word_105;

	/* word 106 */
	struct {
#if defined(__BIG_ENDIAN__)
		lsp_uint8_t reserved_0_1 : 4;
		lsp_uint8_t logical_sectors_per_physical_sector_pwr2 : 4;

		lsp_uint8_t cleared_to_zero : 1;
		lsp_uint8_t set_to_one : 1;
		lsp_uint8_t multiple_logical_sectors_per_physical_sector : 1;
		lsp_uint8_t logical_sector_longer_than_256_words : 1;
		lsp_uint8_t reserved_0_2 : 4;
#else
		/* If bit 14 of word 106 is set to one and bit 15 of word 106 is 
		cleared to zero, the contents of word 106 contain
		valid information. If not, information is not valid in this word. */
		lsp_uint8_t logical_sectors_per_physical_sector_pwr2 : 4;
		lsp_uint8_t reserved_0_1 : 4;

		lsp_uint8_t reserved_0_2 : 4;
		lsp_uint8_t logical_sector_longer_than_256_words : 1;
		lsp_uint8_t multiple_logical_sectors_per_physical_sector : 1;
		lsp_uint8_t set_to_one : 1;
		lsp_uint8_t cleared_to_zero : 1;
#endif
	} __lsp_attr_packed__ physical_logical_sector_size;

	/* word 107 */
	lsp_uint16_t inter_seek_delay;
	/* word 108-111 */
	lsp_uint16_t world_wide_name[4];
	/* word 112-115 */
	lsp_uint16_t reserved_for_world_wide_name128[4];
	/* word 116 */
	lsp_uint16_t reserved_for_tlc_technical_report;

	/* word 117-118 */
	lsp_uint32_t words_per_logical_sector;

	/* word 119: ATA8-ACS */
	struct {
#if defined(__BIG_ENDIAN__)
		lsp_uint8_t reserved_1 : 2;
		lsp_uint8_t freefall_control : 1;
		lsp_uint8_t segmented_for_download_microcode : 1;
		lsp_uint8_t rw_dma_ext_gpl : 1;
		lsp_uint8_t write_uncorrectable_ext : 1;
		lsp_uint8_t write_read_verify : 1;
		lsp_uint8_t reserved_for_drq_technical_report : 1;

		lsp_uint8_t clear_to_zero : 1;
		lsp_uint8_t set_to_one : 1;
		lsp_uint8_t reserved_0 : 6;
#else
		lsp_uint8_t reserved_for_drq_technical_report : 1;
		lsp_uint8_t write_read_verify : 1;
		lsp_uint8_t write_uncorrectable_ext : 1;
		lsp_uint8_t rw_dma_ext_gpl : 1;
		lsp_uint8_t segmented_for_download_microcode : 1;
		lsp_uint8_t freefall_control : 1;
		lsp_uint8_t reserved_1 : 2;

		lsp_uint8_t reserved_0 : 6;
		lsp_uint8_t set_to_one : 1;
		lsp_uint8_t clear_to_zero : 1;
#endif
	} __lsp_attr_packed__ command_set_support_ext;

	/* word 120: ATA8-ACS */
	struct {
#if defined(__BIG_ENDIAN__)
		lsp_uint8_t reserved_1 : 2;
		lsp_uint8_t freefall_control : 1;
		lsp_uint8_t segmented_for_download_microcode : 1;
		lsp_uint8_t rw_dma_ext_gpl : 1;
		lsp_uint8_t write_uncorrectable_ext : 1;
		lsp_uint8_t write_read_verify : 1;
		lsp_uint8_t reserved_for_drq_technical_report : 1;

		lsp_uint8_t clear_to_zero : 1;
		lsp_uint8_t set_to_one : 1;
		lsp_uint8_t reserved_0 : 6;
#else
		lsp_uint8_t reserved_for_drq_technical_report : 1;
		lsp_uint8_t write_read_verify : 1;
		lsp_uint8_t write_uncorrectable_ext : 1;
		lsp_uint8_t rw_dma_ext_gpl : 1;
		lsp_uint8_t segmented_for_download_microcode : 1;
		lsp_uint8_t freefall_control : 1;
		lsp_uint8_t reserved_1 : 2;

		lsp_uint8_t reserved_0 : 6;
		lsp_uint8_t set_to_one : 1;
		lsp_uint8_t clear_to_zero : 1;
#endif
	} __lsp_attr_packed__ command_set_active_ext;

	/* word 121-126 */
	lsp_uint16_t reserved_for_expanded_supportand_active[6];

	/* word 127 */
#if defined(__BIG_ENDIAN__)
	lsp_uint8_t reserved_word_127_1 : 6;
	lsp_uint8_t msn_support : 2;

	lsp_uint8_t reserved_word_127_2 : 8;
#else
	lsp_uint8_t msn_support : 2;
	lsp_uint8_t reserved_word_127_1 : 6;

	lsp_uint8_t reserved_word_127_2 : 8;
#endif

	/* word 128 */
	struct {
#if defined(__BIG_ENDIAN__)
		lsp_uint8_t reserved_0 : 2;
		lsp_uint8_t enhanced_security_erase_supported : 1;
		lsp_uint8_t security_count_expired : 1;
		lsp_uint8_t security_frozen : 1;
		lsp_uint8_t security_locked : 1;
		lsp_uint8_t security_enabled : 1;
		lsp_uint8_t security_supported : 1;

		lsp_uint8_t reserved_1 : 7;
		lsp_uint8_t security_level : 1;
#else
		lsp_uint8_t security_supported : 1;
		lsp_uint8_t security_enabled : 1;
		lsp_uint8_t security_locked : 1;
		lsp_uint8_t security_frozen : 1;
		lsp_uint8_t security_count_expired : 1;
		lsp_uint8_t enhanced_security_erase_supported : 1;
		lsp_uint8_t reserved_0 : 2;

		lsp_uint8_t security_level : 1;
		lsp_uint8_t reserved_1 : 7;
#endif
	} __lsp_attr_packed__ security_status;

	/* word 129-159 */
	lsp_uint16_t reserved_word_129[31];

	/* word 160 */
	struct {
#if defined(__BIG_ENDIAN__)
		lsp_uint8_t maximum_current_in_ma_lsb : 8;

		lsp_uint8_t word_160_supported : 1;
		lsp_uint8_t reserved_0 : 1;
		lsp_uint8_t cfa_power_mode_1_required : 1;
		lsp_uint8_t cfa_power_mode_1_disabled : 1;
		lsp_uint8_t maximum_current_in_ma_msb : 4;
#else
		lsp_uint8_t maximum_current_in_ma_lsb : 8;

		lsp_uint8_t maximum_current_in_ma_msb : 4;
		lsp_uint8_t cfa_power_mode_1_disabled : 1;
		lsp_uint8_t cfa_power_mode_1_required : 1;
		lsp_uint8_t reserved_0 : 1;
		lsp_uint8_t word_160_supported : 1;
#endif
	} __lsp_attr_packed__ cfa_power_mode_1;

	/* word 161-175 */
	lsp_uint16_t reserved_for_cfa[15];
	/* word 176-205 */
	lsp_uint16_t current_media_serial_number[30];

	/* word 206 */
	lsp_uint16_t reserved_word_206;
	/* word 207-208 */
	lsp_uint16_t reserved_word_207[2];

	/* word 209 */
	struct {
#if defined(__BIG_ENDIAN__)
		lsp_uint8_t alignment_of_logical_within_physical_lsb: 8;

		lsp_uint8_t reserved_0: 1;
		lsp_uint8_t word_209_supported: 1;
		lsp_uint8_t alignment_of_logical_within_physical_msb: 6;
#else
		lsp_uint8_t alignment_of_logical_within_physical_lsb: 8;

		lsp_uint8_t alignment_of_logical_within_physical_msb: 6;
		lsp_uint8_t word_209_supported: 1;
		lsp_uint8_t reserved_0: 1;
#endif
	} __lsp_attr_packed__ block_alignment;

	/* word 210-211 */
	lsp_uint32_t write_read_verify_sector_count_mode_3_only;
	/* word 212-213 */
	lsp_uint32_t write_read_verify_sector_count_mode_2_only;

	/* word 214: ATA-8 */
	struct {
#if defined(__BIG_ENDIAN__)
		lsp_uint8_t reserved_1: 3;
		lsp_uint8_t nv_cache_feature_set_enabled: 1;
		lsp_uint8_t reserved_0: 3;
		lsp_uint8_t nv_cache_power_mode_enabled: 1;

		lsp_uint8_t nv_cache_feature_set_version: 4;
		lsp_uint8_t nv_cache_power_mode_version: 4;
#else
		lsp_uint8_t nv_cache_power_mode_enabled: 1;
		lsp_uint8_t reserved_0: 3;
		lsp_uint8_t nv_cache_feature_set_enabled: 1;
		lsp_uint8_t reserved_1: 3;

		lsp_uint8_t nv_cache_power_mode_version: 4;
		lsp_uint8_t nv_cache_feature_set_version: 4;
#endif
	} __lsp_attr_packed__ nv_cache_capabilities;

	/* word 215 */
	lsp_uint16_t nv_cache_size_lsw;
	/* word 216 */
	lsp_uint16_t nv_cache_size_msw;
	/* word 217 */
	lsp_uint16_t nv_cache_read_speed;
	/* word 218 */
	lsp_uint16_t nv_cache_write_speed;

	/* word 219 */
	struct {
		lsp_uint8_t nv_cache_estimated_time_to_spin_up_in_sec;
		lsp_uint8_t reserved;
	} __lsp_attr_packed__ nv_cache_options;

	/* word 220-254 */
	lsp_uint16_t reserved_word_220[35];

	/* word 255 */
	lsp_uint8_t signature : 8;
	lsp_uint8_t check_sum : 8;

}  __lsp_attr_packed__ lsp_ide_identify_device_data_t;

LSP_C_ASSERT_SIZEOF(lsp_ide_identify_device_data_t, 512);

#if !defined(__GNUC__)
#pragma pack (pop, _lsp_ide_identify_data_t)

#pragma pack(push, _lsp_ide_identify_packet_device_data_t, 16)
#endif

typedef struct _lsp_ide_identify_packet_device_data_t {

	/* word 0 */
	struct {
#if defined(__BIG_ENDIAN__)
		lsp_uint8_t removable_media : 1;
		lsp_uint8_t drq_response_time : 2;
		lsp_uint8_t reserved_2 : 2;
		lsp_uint8_t response_incomplete : 1;
		lsp_uint8_t command_packet_type : 2; /* 00: 12-byte, 01: 16-byte, 1x: Reserved */

		lsp_uint8_t atapi_device : 2;
		lsp_uint8_t reserved_1 : 1;
		lsp_uint8_t command_packet_set : 5;
#else
		lsp_uint8_t command_packet_type : 2; /* 00: 12-byte, 01: 16-byte, 1x: Reserved */
		lsp_uint8_t response_incomplete : 1;
		lsp_uint8_t reserved_2 : 2;
		lsp_uint8_t drq_response_time : 2;
		lsp_uint8_t removable_media : 1;

		lsp_uint8_t command_packet_set : 5;
		lsp_uint8_t reserved_1 : 1;
		lsp_uint8_t atapi_device : 2;
#endif
	} __lsp_attr_packed__ general_configuration;

	/* word 1-9 */
	lsp_uint16_t reserved_word_1;
	lsp_uint16_t reserved_word_2;
	lsp_uint16_t reserved_word_3;
	lsp_uint16_t reserved_word_4;
	lsp_uint16_t reserved_word_5;
	lsp_uint16_t reserved_word_6;
	lsp_uint16_t reserved_word_7;
	lsp_uint16_t reserved_word_8;
	lsp_uint16_t reserved_word_9;
	/* word 10-19 */
	lsp_uint8_t	 serial_number[20];
	/* word 20-22 */
	lsp_uint16_t reserved_word_20;
	lsp_uint16_t reserved_word_21;
	lsp_uint16_t reserved_word_22;
	/* word 23-26 */
	lsp_uint8_t	 firmware_revision[8];
	/* word 27-46 */
	lsp_uint8_t	 model_number[40];
	/* word 47-48 */
	lsp_uint16_t reserved_word_47;
	lsp_uint16_t reserved_word_48;

	/* word 49-50 */
	struct {
#if defined(__BIG_ENDIAN__)
		lsp_uint8_t vendor_specific;

		lsp_uint8_t interleaved_dma_supported : 1;
		lsp_uint8_t command_queuing_supported : 1;
		lsp_uint8_t overlapped_operation_supported : 1;
		lsp_uint8_t ata_software_request_required : 1;
		lsp_uint8_t iordy_supported : 1;
		lsp_uint8_t iordy_may_be_disabled : 1;
		lsp_uint8_t lba_supported : 1;
		lsp_uint8_t dma_supported : 1;
#else
		lsp_uint8_t vendor_specific;

		lsp_uint8_t dma_supported : 1;
		lsp_uint8_t lba_supported : 1;
		lsp_uint8_t iordy_may_be_disabled : 1;
		lsp_uint8_t iordy_supported : 1;
		lsp_uint8_t ata_software_request_required : 1;
		lsp_uint8_t overlapped_operation_supported : 1;
		lsp_uint8_t command_queuing_supported : 1;
		lsp_uint8_t interleaved_dma_supported : 1;
#endif

#if defined(__BIG_ENDIAN__)
		lsp_uint8_t reserved_50_7_2 : 6;
		lsp_uint8_t obsolete_50_1 : 1;
		lsp_uint8_t device_specific_standby_timer : 1;

		lsp_uint8_t cleared_to_zero : 1;
		lsp_uint8_t set_to_one : 1;
		lsp_uint8_t reserved_50_13_8 : 6;
#else
		lsp_uint8_t device_specific_standby_timer : 1;
		lsp_uint8_t obsolete_50_1 : 1;
		lsp_uint8_t reserved_50_7_2 : 6;

		lsp_uint8_t reserved_50_13_8 : 6;
		lsp_uint8_t set_to_one : 1;
		lsp_uint8_t cleared_to_zero : 1;
#endif
	} __lsp_attr_packed__ capabilities;

	/* word 51 */
	lsp_uint8_t vendor_specific_word_51_7_0 : 8;
	lsp_uint8_t pio_data_transfer_mode : 8;

	/* word 52 */
	lsp_uint16_t reserved_word_52;

	/* word 53 */
#if defined(__BIG_ENDIAN__)
	lsp_uint8_t reserved_word_53_7_3 : 5;
	lsp_uint8_t word_88_valid : 1;
	lsp_uint8_t word_64_70_valid : 1;
	lsp_uint8_t word_54_58_valid : 1;

	lsp_uint8_t reserved_word_53_15_8 : 8;
#else
	lsp_uint8_t word_54_58_valid : 1;
	lsp_uint8_t word_64_70_valid : 1;
	lsp_uint8_t word_88_valid : 1;
	lsp_uint8_t reserved_word_53_7_3 : 5;

	lsp_uint8_t reserved_word_53_15_8 : 8;
#endif

	/* word 54-62 */
	lsp_uint16_t reserved_word_54_to_62[9];

	/* word 63 */
	lsp_uint8_t multiword_dma_support : 8;
	lsp_uint8_t multiword_dma_active : 8;

	/* word 64 */
	lsp_uint8_t advanced_pio_modes : 8;
	lsp_uint8_t reserved_byte_64 : 8;

	/* word 65 */
	lsp_uint16_t minimum_mwx_fer_cycle_time;
	/* word 66 */
	lsp_uint16_t recommended_mwx_fer_cycle_time;
	/* word 67 */
	lsp_uint16_t minimum_pio_cycle_time;
	/* word 68 */
	lsp_uint16_t minimum_pio_cycle_time_iordy;

	/* word 69-70 */
	lsp_uint16_t reserved_word_69_to_70[2];

	/* word 71 */
	lsp_uint16_t packet_command_to_bus_release_time;

	/* word 72 */
	lsp_uint16_t service_command_to_busy_clear_time;

	/* word 73-74 */
	lsp_uint16_t reserved_word_73_to_74[2];

	/* word 75 */
#if defined(__BIG_ENDIAN__)
	lsp_uint8_t reserved_word_75_7_5 : 3;
	lsp_uint8_t queue_depth : 5;

	lsp_uint8_t reserved_word_75_15_8 : 8;
#else
	lsp_uint8_t queue_depth : 5;
	lsp_uint8_t reserved_word_75_7_5 : 3;

	lsp_uint8_t reserved_word_75_15_8 : 8;
#endif

	/* word 76-79 */
	lsp_uint16_t reserved_word_76_to_79[4];

	/* word 80 */
	lsp_uint16_t major_revision;
	/* word 81 */
	lsp_uint16_t minor_revision;

	struct {

		/* word 82 */
#if defined(__BIG_ENDIAN__)
		lsp_uint8_t release_interrupt : 1;
		lsp_uint8_t look_ahead : 1;
		lsp_uint8_t write_cache : 1;
		lsp_uint8_t packet_command : 1;
		lsp_uint8_t power_management : 1;
		lsp_uint8_t removable_media_feature : 1;
		lsp_uint8_t security_mode : 1;
		lsp_uint8_t smart_commands : 1;

		lsp_uint8_t obsolete2 : 1;
		lsp_uint8_t nop : 1;
		lsp_uint8_t read_buffer : 1;
		lsp_uint8_t write_buffer : 1;
		lsp_uint8_t obsolete1 : 1;
		lsp_uint8_t host_protected_area : 1;
		lsp_uint8_t device_reset : 1;
		lsp_uint8_t service_interrupt : 1;
#else
		lsp_uint8_t smart_commands : 1;
		lsp_uint8_t security_mode : 1;
		lsp_uint8_t removable_media_feature : 1;
		lsp_uint8_t power_management : 1;
		lsp_uint8_t packet_command : 1;
		lsp_uint8_t write_cache : 1;
		lsp_uint8_t look_ahead : 1;
		lsp_uint8_t release_interrupt : 1;

		lsp_uint8_t service_interrupt : 1;
		lsp_uint8_t device_reset : 1;
		lsp_uint8_t host_protected_area : 1;
		lsp_uint8_t obsolete1 : 1;
		lsp_uint8_t write_buffer : 1;
		lsp_uint8_t read_buffer : 1;
		lsp_uint8_t nop : 1;
		lsp_uint8_t obsolete2 : 1;
#endif

		/* word 83 */
#if defined(__BIG_ENDIAN__)
		lsp_uint8_t reserved_2 : 1;
		lsp_uint8_t manual_power_up : 1;
		lsp_uint8_t power_up_in_standby : 1;
		lsp_uint8_t msn : 1;
		lsp_uint8_t advanced_pm : 1;
		lsp_uint8_t cfa : 1;
		lsp_uint8_t dma_queued : 1;
		lsp_uint8_t download_microcode : 1;

		lsp_uint8_t reserved_3 : 2;
		lsp_uint8_t flush_cache_ext : 1;
		lsp_uint8_t flush_cache : 1;
		lsp_uint8_t device_config_overlay : 1;
		lsp_uint8_t big_lba : 1;
		lsp_uint8_t acoustics : 1;
		lsp_uint8_t set_max : 1;
#else
		lsp_uint8_t download_microcode : 1;
		lsp_uint8_t dma_queued : 1;
		lsp_uint8_t cfa : 1;
		lsp_uint8_t advanced_pm : 1;
		lsp_uint8_t msn : 1;
		lsp_uint8_t power_up_in_standby : 1;
		lsp_uint8_t manual_power_up : 1;
		lsp_uint8_t reserved_2 : 1;

		lsp_uint8_t set_max : 1;
		lsp_uint8_t acoustics : 1;
		lsp_uint8_t big_lba : 1;
		lsp_uint8_t device_config_overlay : 1;
		lsp_uint8_t flush_cache : 1;
		lsp_uint8_t flush_cache_ext : 1;
		lsp_uint8_t reserved_3 : 2;
#endif

		/* word 84 */
#if defined(__BIG_ENDIAN__)
		lsp_uint8_t write_queued_fua : 1;
		lsp_uint8_t write_fua : 1;
		lsp_uint8_t gp_logging : 1;
		lsp_uint8_t streaming_feature : 1;
		lsp_uint8_t media_card_pass_through : 1;
		lsp_uint8_t media_serial_number : 1;
		lsp_uint8_t smart_self_test : 1;
		lsp_uint8_t smart_error_log : 1;

		lsp_uint8_t reserved_4 : 2;
		lsp_uint8_t idle_with_unload_feature : 1;
		lsp_uint8_t reserved_for_tech_report : 2;
		lsp_uint8_t urg_write_stream : 1;
		lsp_uint8_t urg_read_stream : 1;
		lsp_uint8_t wwn_64_bit : 1;
#else
		lsp_uint8_t smart_error_log : 1;
		lsp_uint8_t smart_self_test : 1;
		lsp_uint8_t media_serial_number : 1;
		lsp_uint8_t media_card_pass_through : 1;
		lsp_uint8_t streaming_feature : 1;
		lsp_uint8_t gp_logging : 1;
		lsp_uint8_t write_fua : 1;
		lsp_uint8_t write_queued_fua : 1;

		lsp_uint8_t wwn_64_bit : 1;
		lsp_uint8_t urg_read_stream : 1;
		lsp_uint8_t urg_write_stream : 1;
		lsp_uint8_t reserved_for_tech_report : 2;
		lsp_uint8_t idle_with_unload_feature : 1;
		lsp_uint8_t reserved_4 : 2;
#endif

	} __lsp_attr_packed__ command_set_support;

	struct {

		/* word 85 */
#if defined(__BIG_ENDIAN__)
		lsp_uint8_t release_interrupt : 1;
		lsp_uint8_t look_ahead : 1;
		lsp_uint8_t write_cache : 1;
		lsp_uint8_t packet_command : 1;
		lsp_uint8_t power_management : 1;
		lsp_uint8_t removable_media_feature : 1;
		lsp_uint8_t security_mode : 1;
		lsp_uint8_t smart_commands : 1;

		lsp_uint8_t obsolete2 : 1;
		lsp_uint8_t nop : 1;
		lsp_uint8_t read_buffer : 1;
		lsp_uint8_t write_buffer : 1;
		lsp_uint8_t obsolete1 : 1;
		lsp_uint8_t host_protected_area : 1;
		lsp_uint8_t device_reset : 1;
		lsp_uint8_t service_interrupt : 1;
#else
		lsp_uint8_t smart_commands : 1;
		lsp_uint8_t security_mode : 1;
		lsp_uint8_t removable_media_feature : 1;
		lsp_uint8_t power_management : 1;
		lsp_uint8_t packet_command : 1;
		lsp_uint8_t write_cache : 1;
		lsp_uint8_t look_ahead : 1;
		lsp_uint8_t release_interrupt : 1;

		lsp_uint8_t service_interrupt : 1;
		lsp_uint8_t device_reset : 1;
		lsp_uint8_t host_protected_area : 1;
		lsp_uint8_t obsolete1 : 1;
		lsp_uint8_t write_buffer : 1;
		lsp_uint8_t read_buffer : 1;
		lsp_uint8_t nop : 1;
		lsp_uint8_t obsolete2 : 1;
#endif

		/* word 86 */
#if defined(__BIG_ENDIAN__)
		lsp_uint8_t reserved_2 : 1;
		lsp_uint8_t manual_power_up : 1;
		lsp_uint8_t power_up_in_standby : 1;
		lsp_uint8_t msn : 1;
		lsp_uint8_t advanced_pm : 1;
		lsp_uint8_t cfa : 1;
		lsp_uint8_t dma_queued : 1;
		lsp_uint8_t download_microcode : 1;

		lsp_uint8_t reserved_3 : 2;
		lsp_uint8_t flush_cache_ext : 1;
		lsp_uint8_t flush_cache : 1;
		lsp_uint8_t device_config_overlay : 1;
		lsp_uint8_t big_lba : 1;
		lsp_uint8_t acoustics : 1;
		lsp_uint8_t set_max : 1;
#else
		lsp_uint8_t download_microcode : 1;
		lsp_uint8_t dma_queued : 1;
		lsp_uint8_t cfa : 1;
		lsp_uint8_t advanced_pm : 1;
		lsp_uint8_t msn : 1;
		lsp_uint8_t power_up_in_standby : 1;
		lsp_uint8_t manual_power_up : 1;
		lsp_uint8_t reserved_2 : 1;

		lsp_uint8_t set_max : 1;
		lsp_uint8_t acoustics : 1;
		lsp_uint8_t big_lba : 1;
		lsp_uint8_t device_config_overlay : 1;
		lsp_uint8_t flush_cache : 1;
		lsp_uint8_t flush_cache_ext : 1;
		lsp_uint8_t reserved_3 : 2;
#endif

		/* word 87 */
#if defined(__BIG_ENDIAN__)
		lsp_uint8_t write_queued_fua : 1;
		lsp_uint8_t write_fua : 1;
		lsp_uint8_t gp_logging : 1;
		lsp_uint8_t streaming_feature : 1;
		lsp_uint8_t media_card_pass_through : 1;
		lsp_uint8_t media_serial_number : 1;
		lsp_uint8_t smart_self_test : 1;
		lsp_uint8_t smart_error_log : 1;

		lsp_uint8_t reserved_4 : 2;
		lsp_uint8_t idle_with_unload_feature : 1;
		lsp_uint8_t reserved_for_tech_report : 2;
		lsp_uint8_t urg_write_stream : 1;
		lsp_uint8_t urg_read_stream : 1;
		lsp_uint8_t wwn_64_bit : 1;
#else
		lsp_uint8_t smart_error_log : 1;
		lsp_uint8_t smart_self_test : 1;
		lsp_uint8_t media_serial_number : 1;
		lsp_uint8_t media_card_pass_through : 1;
		lsp_uint8_t streaming_feature : 1;
		lsp_uint8_t gp_logging : 1;
		lsp_uint8_t write_fua : 1;
		lsp_uint8_t write_queued_fua : 1;

		lsp_uint8_t wwn_64_bit : 1;
		lsp_uint8_t urg_read_stream : 1;
		lsp_uint8_t urg_write_stream : 1;
		lsp_uint8_t reserved_for_tech_report : 2;
		lsp_uint8_t idle_with_unload_feature : 1;
		lsp_uint8_t reserved_4 : 2;
#endif

	} __lsp_attr_packed__ command_set_active;

	/* word 88 */
	lsp_uint8_t ultra_dma_support : 8;
	lsp_uint8_t ultra_dma_active  : 8;

	/* word 89-92 */
	lsp_uint16_t reserved_word_89_to_92[4];

	/* word 93 */
	lsp_uint16_t hardware_reset_result;

	/* word 94 */
	lsp_uint8_t current_acoustic_value : 8;
	lsp_uint8_t recommended_acoustic_value : 8;

	/* word 95-107 */
	lsp_uint16_t reserved_word_95_to_107[13];

	/* word 108-111 */
	lsp_uint16_t world_wide_name[4];

	/* word 112-115 */
	lsp_uint16_t reserved_for_world_wide_name128[4];

	/* word 116-124 */
	lsp_uint16_t reserved_word_116_to_124[9];

	/* word 125 */
	lsp_uint16_t atapi_zero_byte_count_behavior;

	/* word 126 */
	lsp_uint16_t obsolete_126;

	/* word 127 */
#if defined(__BIG_ENDIAN__)
	lsp_uint8_t reserved_word_127_1 : 6;
	lsp_uint8_t msn_support : 2;

	lsp_uint8_t reserved_word_127_2 : 8;
#else
	lsp_uint8_t msn_support : 2;
	lsp_uint8_t reserved_word_127_1 : 6;

	lsp_uint8_t reserved_word_127_2 : 8;
#endif

	/* word 128 */
	struct {
#if defined(__BIG_ENDIAN__)
		lsp_uint8_t reserved_0 : 2;
		lsp_uint8_t enhanced_security_erase_supported : 1;
		lsp_uint8_t security_count_expired : 1;
		lsp_uint8_t security_frozen : 1;
		lsp_uint8_t security_locked : 1;
		lsp_uint8_t security_enabled : 1;
		lsp_uint8_t security_supported : 1;

		lsp_uint8_t reserved_1 : 7;
		lsp_uint8_t security_level : 1;
#else
		lsp_uint8_t security_supported : 1;
		lsp_uint8_t security_enabled : 1;
		lsp_uint8_t security_locked : 1;
		lsp_uint8_t security_frozen : 1;
		lsp_uint8_t security_count_expired : 1;
		lsp_uint8_t enhanced_security_erase_supported : 1;
		lsp_uint8_t reserved_0 : 2;

		lsp_uint8_t security_level : 1;
		lsp_uint8_t reserved_1 : 7;
#endif
	} __lsp_attr_packed__ security_status;

	/* word 129-159 */
	lsp_uint16_t reserved_word_129_to_159[31];

	/* word 160 */
	struct {
#if defined(__BIG_ENDIAN__)
		lsp_uint8_t maximum_current_in_ma_lsb : 8;

		lsp_uint8_t word_160_supported : 1;
		lsp_uint8_t reserved_0 : 1;
		lsp_uint8_t cfa_power_mode_1_required : 1;
		lsp_uint8_t cfa_power_mode_1_disabled : 1;
		lsp_uint8_t maximum_current_in_ma_msb : 4;
#else
		lsp_uint8_t maximum_current_in_ma_lsb : 8;

		lsp_uint8_t maximum_current_in_ma_msb : 4;
		lsp_uint8_t cfa_power_mode_1_disabled : 1;
		lsp_uint8_t cfa_power_mode_1_required : 1;
		lsp_uint8_t reserved_0 : 1;
		lsp_uint8_t word_160_supported : 1;
#endif
	} __lsp_attr_packed__ cfa_power_mode_1;

	/* word 161-175 */
	lsp_uint16_t reserved_for_cfa[15];

	/* word 176-254 */
	lsp_uint16_t reserved_word_176_to_254[79];

	/* word 255 */
	lsp_uint8_t signature : 8;
	lsp_uint8_t check_sum : 8;

} __lsp_attr_packed__ lsp_ide_identify_packet_device_data_t;

LSP_C_ASSERT_SIZEOF(lsp_ide_identify_packet_device_data_t, 512);

#if !defined(__GNUC__)
#pragma pack (pop, _lsp_ide_identify_packet_device_data_t)
#endif

#ifdef __cplusplus
}
#endif

#endif /* LSP_IDE_DEF_H_INCLUDED */
