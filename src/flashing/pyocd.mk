flash:
ifeq ($(PROBE_ID),)
	$(error PROBE_ID not specified. Run pyocd list to get unique ID for probe)
endif
	@pyocd load -u $(PROBE_ID) -t $(CORE_MCU_MODEL) $(FLASH_BINARY_DIR)/$(TARGET).hex