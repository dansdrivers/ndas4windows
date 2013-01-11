
MSI_OUTPUT_SUFFIX=fs-$(_NDAS_MSI_TARGET_DISPLAY_VERSION)
CANDLE_DEFINES=$(CANDLE_DEFINES) \
	-dCompressedPackage="no" \
	-dNdasSetupFlags_HAVE_NDASFAT="yes" \
	-dNdasSetupFlags_HAVE_NDASNTFS="yes" \
	-dUseNdasScMsm_FS="yes"
