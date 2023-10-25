
from m5.objects import *

def initializeADFA(options):
    adfa = AbstractDataFlowAccelerator()
    adfa.adfaCoreIssueWidth = options.gem_forge_adfa_core_issue_width
    adfa.adfaEnableSpeculation = options.gem_forge_adfa_enable_speculation
    adfa.adfaBreakIVDep = options.gem_forge_adfa_break_iv_dep
    adfa.adfaBreakRVDep = options.gem_forge_adfa_break_rv_dep
    adfa.adfaBreakUnrollableControlDep = \
        options.gem_forge_adfa_break_unrollable_ctr_dep
    adfa.adfaNumBanks = options.gem_forge_adfa_num_banks
    adfa.adfaNumPortsPerBank = options.gem_forge_adfa_num_ports_per_bank
    adfa.adfaNumCores = options.gem_forge_adfa_num_cores
    adfa.adfaEnableTLS = (options.gem_forge_adfa_enable_tls == 1)
    adfa.adfaIdealMem = (options.gem_forge_adfa_ideal_mem == 1)
    return adfa

def initializeIdealPrefetcher(options):
    idealPrefetcher = IdealPrefetcher()
    idealPrefetcher.enableIdealPrefetcher = options.gem_forge_ideal_prefetcher
    idealPrefetcher.idealPrefetcherDistance = options.gem_forge_ideal_prefetcher_distance
    return idealPrefetcher

def initializeStreamEngine(options):
    se = StreamEngine()
    se.streamEngineIsOracle = (
        options.gem_forge_stream_engine_is_oracle != 0)
    se.defaultRunAheadLength = \
        options.gem_forge_stream_engine_default_run_ahead_length
    se.totalRunAheadLength = \
        options.gem_forge_stream_engine_total_run_ahead_length
    se.totalRunAheadBytes = \
        options.gem_forge_stream_engine_total_run_ahead_bytes
    se.maxNumElementsPrefetchForAtomic = \
        options.gem_forge_stream_engine_max_num_elements_prefetch_for_atomic
    se.throttling = options.gem_forge_stream_engine_throttling
    se.streamEngineEnableLSQ = options.gem_forge_stream_engine_enable_lsq
    se.streamEngineForceNoFlushPEB = options.gem_forge_stream_engine_force_no_flush_peb
    se.streamEngineEnableCoalesce = options.gem_forge_stream_engine_enable_coalesce
    se.streamEngineEnableMerge = options.gem_forge_stream_engine_enable_merge

    se.streamEngineEnableFloat = options.gem_forge_stream_engine_enable_float
    se.streamEngineFloatPolicy = options.gem_forge_stream_engine_float_policy
    se.streamEngineEnableFloatIndirect = \
        options.gem_forge_stream_engine_enable_float_indirect
    se.streamEngineEnableFloatPseudo = \
        options.gem_forge_stream_engine_enable_float_pseudo
    se.streamEngineEnableFloatCancel = \
        options.gem_forge_stream_engine_enable_float_cancel
    if options.gem_forge_stream_engine_enable_float_indirect:
        assert(options.gem_forge_stream_engine_enable_float)
    if options.gem_forge_stream_engine_enable_float_pseudo:
        assert(options.gem_forge_stream_engine_enable_float_indirect)
    se.mlc_stream_buffer_init_num_entries = \
        options.gem_forge_stream_engine_mlc_stream_buffer_init_num_entries

    se.streamEngineEnableMidwayFloat = \
        options.gem_forge_stream_engine_enable_midway_float
    se.streamEngineMidwayFloatElementIdx = \
        options.gem_forge_stream_engine_midway_float_element_idx

    se.computeWidth =\
        options.gem_forge_stream_engine_compute_width
    # So far we reuse the LLC SIMD delay parameter.
    simd_delay =\
        options.gem_forge_stream_engine_llc_access_core_simd_delay
    if simd_delay >= 2:
        # Half the latency for LLC SIMD Delay, as we are closer to core.
        simd_delay = simd_delay // 2
    se.computeSIMDDelay = simd_delay
    se.hasScalarALU = options.gem_forge_stream_engine_has_scalar_alu
    se.computeMaxInflyComputation =\
        options.gem_forge_stream_engine_llc_max_infly_computation
    se.enableZeroComputeLatency =\
        options.gem_forge_enable_stream_zero_compute_latency
    se.enableRangeSync =\
        options.gem_forge_enable_stream_range_sync
    se.enableFloatIndirectReduction =\
        options.gem_forge_enable_stream_float_indirect_reduction
    se.enableFloatTwoLevelIndirectStoreCompute =\
        options.gem_forge_enable_stream_float_two_level_indirect_store_compute
    se.enableFineGrainedNearDataComputing =\
        options.gem_forge_stream_engine_enable_fine_grained_near_data_computing

    se.enableFloatMem =\
        options.gem_forge_stream_engine_enable_float_mem
    se.floatLevelPolicy = options.gem_forge_stream_engine_float_level_policy

    return se

def initializePSPFrontend(options):
    psp = PSPFrontend()
    # Pass parameters
    psp.totalPatternTableEntries = options.gem_forge_psp_frontend_total_pattern_table_entries

    return psp

def initializeEmptyGemForgeAcceleratorManager(options):
    has_accelerator = False
    if options.gem_forge_adfa_enable:
        has_accelerator = True
    if options.gem_forge_ideal_prefetcher:
        has_accelerator = True
    # accelerators.append(SpeculativePrecomputationManager(options))
    if options.gem_forge_stream_engine_enable:
        has_accelerator = True
    if options.gem_forge_psp_frontend_enable:
        has_accelerator = True
    if has_accelerator:
        return GemForgeAcceleratorManager(accelerators=list())
    elif options.gem_forge_idea_inorder_cpu:
        # IdeaInorderCPU is implemented in CPU delegator, which is
        # dependent on accelManaguer.
        return GemForgeAcceleratorManager(accelerators=list())
    else:
        # Disable this in default.
        return NULL

def initializeGemForgeAcceleratorManager(options):
    accelerators = list()
    if options.gem_forge_adfa_enable:
        accelerators.append(initializeADFA(options))
    if options.gem_forge_ideal_prefetcher:
        accelerators.append(initializeIdealPrefetcher(options))
    # accelerators.append(SpeculativePrecomputationManager(options))
    if options.gem_forge_stream_engine_enable:
        accelerators.append(initializeStreamEngine(options))
    if options.gem_forge_psp_frontend_enable:
        accelerators.append(initializePSPFrontend(options))
    if accelerators:
        return GemForgeAcceleratorManager(accelerators=accelerators)
    elif options.gem_forge_idea_inorder_cpu:
        # IdeaInorderCPU is implemented in CPU delegator, which is
        # dependent on accelManaguer.
        return GemForgeAcceleratorManager(accelerators=accelerators)
    else:
        # Disable this in default.
        return NULL
