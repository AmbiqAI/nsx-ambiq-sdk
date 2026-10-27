/* Public composition tests using the real lifecycle, coordinator and backend. */
static unsigned position(unsigned id) {
    for (unsigned i=0;i<count;++i) if(calls[i] == id) return i;
    assert(!"required operation missing");
    return 0;
}
static void output_count(unsigned expected) {
    nsx_low_power_printf("contract %u", expected);
    assert(print_calls == expected);
}
static void contract_startup(const char *scenario) {
    nsx_system_config_t cfg={.enable_cache=true, .perf_mode=NSX_PERF_LOW};
    bool minimal=strstr(scenario,"minimal") != NULL;
    bool staged=strstr(scenario,"staged") != NULL;
    cfg.skip_bsp_init=minimal;
    bool high=strstr(scenario,"high") != NULL;
    if(high) cfg.perf_mode=NSX_PERF_HIGH;
    cfg.debug.transport=strstr(scenario,"itm") ? NSX_DEBUG_ITM :
        strstr(scenario,"uart") ? NSX_DEBUG_UART : NSX_DEBUG_NONE;
    if(!strcmp(scenario,"default")) cfg=(nsx_system_config_t){0};
    if(!strcmp(scenario,"none-after-itm")) {
        nsx_debug_config_t itm_cfg={.transport=NSX_DEBUG_ITM}, none={0};
        assert(nsx_debug_init(&itm_cfg) == 0);
        unsigned before=count;
        assert(nsx_debug_init(NULL) == 0 && nsx_debug_init(&none) == 0);
        assert(count == before); output_count(1); return;
    }
    if(!strcmp(scenario,"fail-core")) failure=CORE;
    if(!strcmp(scenario,"fail-cache")) failure=CACHE;
    output_count(0);
    unsigned status;
    if(staged) {
        nsx_core_config_t core={.api=&nsx_core_V1_0_0};
        assert(nsx_core_init(&core) == 0 && nsx_core_initialized());
        status=minimal ? nsx_minimal_hw_init() : nsx_hw_init();
        assert(status == 0);
        assert(nsx_cache_enable() == 0);
        assert(nsx_debug_init(&cfg.debug) == 0);
        status=nsx_set_perf_mode(cfg.perf_mode);
    } else status=nsx_system_init(&cfg);
    assert(status == (failure ? 0x40+failure : 0));
    if(failure) {
        assert(calls[count-1] == failure);
        assert(!seen(DEBUG) && !seen(PERF) && !seen(PROFILE));
        assert(nsx_core_initialized() == (failure != CORE));
        if(failure == CORE) assert(!seen(BSP) && !seen(CACHE));
#if defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P) || defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO4L)
        unsigned before=count;
        assert(nsx_system_init(&cfg) == NSX_STATUS_STARTUP_REJECTED && count == before);
#endif
        output_count(0); return;
    }
    assert(nsx_core_initialized() && seen(CORE) == 1);
    if(minimal) {
        assert(!seen(BSP));
#if defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P) || defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO4L)
        assert(position(CACHE_CONFIG) < position(CACHE_ENABLE));
        assert(position(CACHE_ENABLE) < position(FPU));
#else
        assert(position(CPDLP) < position(SPOT) && position(SPOT) < position(FPU));
#endif
        assert(position(FPU) < position(STACK) && position(STACK) < position(CACHE));
    } else {
        assert(seen(BSP) == 1 && position(CORE) < position(BSP));
        if(cfg.enable_cache) assert(position(BSP) < position(CACHE));
    }
    assert(seen(CACHE) == (unsigned)cfg.enable_cache);
    if(cfg.debug.transport == NSX_DEBUG_NONE) {
        assert(!seen(DEBUG) && !seen(OTP_ON) && !seen(CRYPTO_ON));
        output_count(0);
    } else {
        assert(position(CACHE) < position(DEBUG));
        output_count(1);
        unsigned before=count;
        if(cfg.debug.transport == NSX_DEBUG_ITM) nsx_itm_printf_enable();
        else nsx_uart_printf_enable();
        nsx_debug_printf_enable();
        assert(count == before); output_count(2);
    }
#if !defined(AM_PART_APOLLO3) && !defined(AM_PART_APOLLO3P)
    assert(seen(PERF) == 1);
    if(high) {
#if defined(AM_PART_APOLLO330P) || defined(AM_PART_APOLLO510L)
        assert(selected_mode == AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE2);
#else
        assert(selected_mode == AM_HAL_PWRCTRL_MCU_MODE_HIGH_PERFORMANCE);
#endif
    }
    if(cfg.debug.transport != NSX_DEBUG_NONE) assert(position(DEBUG) < position(PERF));
    else if(cfg.enable_cache) assert(position(CACHE) < position(PERF));
#endif
    /* AP3's no-op system selector is an unresolved compatibility gap, not
     * a strict cross-family performance guarantee established by this test. */
    assert(!seen(PROFILE));
#if defined(AM_PART_APOLLO3) || defined(AM_PART_APOLLO3P) || defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO4L)
    unsigned before=count;
    assert(nsx_hw_init() == NSX_STATUS_STARTUP_REJECTED);
    assert(nsx_minimal_hw_init() == NSX_STATUS_STARTUP_REJECTED);
    assert(nsx_system_init(&cfg) == NSX_STATUS_STARTUP_REJECTED && count == before);
#endif
}
static void contract_performance(unsigned mode, unsigned expected, bool fail) {
    SystemCoreClock=777;
    uint32_t prior=SystemCoreClock;
    failure=fail ? PERF : 0;
    assert(nsx_set_perf_mode((nsx_perf_mode_e)mode) == (fail ? 0x40+PERF : 0));
    assert(count == 1 && calls[0] == PERF && selected_mode == expected);
    if(fail) assert(SystemCoreClock == prior);
    else {
        uint32_t hz=96000000u;
        if(mode) {
#if defined(AM_PART_APOLLO4P) || defined(AM_PART_APOLLO4L)
            hz=192000000u;
#elif defined(AM_PART_APOLLO330P) || defined(AM_PART_APOLLO510L)
            hz=mode==1 ? 192000000u : 250000000u;
#else
            hz=250000000u;
#endif
        }
        assert(SystemCoreClock == hz);
    }
}
