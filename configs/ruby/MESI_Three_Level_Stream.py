# Copyright (c) 2006-2007 The Regents of The University of Michigan
# Copyright (c) 2009,2015 Advanced Micro Devices, Inc.
# Copyright (c) 2013 Mark D. Hill and David A. Wood
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Authors: Brad Beckmann
#          Nilay Vaish

import math
import m5
from m5.objects import *
from m5.defines import buildEnv
from .Ruby import create_topology, create_directories
from .Ruby import send_evicts
from common import FileSystemConfig

#
# Declare caches used by the protocol
#
class L0Cache(RubyCache): pass
class L1Cache(RubyCache): pass
class L2Cache(RubyCache): pass

def define_options(parser):
    parser.add_option("--num-clusters", type="int", default=1,
            help="number of clusters in a design in which there are shared\
            caches private to clusters")
    parser.add_option("--l0-transitions-per-cycle", type="int", default=32)
    parser.add_option("--l1-transitions-per-cycle", type="int", default=32)
    parser.add_option("--l2-transitions-per-cycle", type="int", default=4)
    return

def create_system(options, full_system, system, dma_ports, bootmem,
                  ruby_system):

    if buildEnv['PROTOCOL'] != 'MESI_Three_Level_Stream':
        fatal("This script requires the MESI_Three_Level_Stream protocol to be\
               built.")

    cpu_sequencers = []

    #
    # The ruby network creation expects the list of nodes in the system to be
    # consistent with the NetDest list.  Therefore the l1 controller nodes
    # must be listed before the directory nodes and directory nodes before
    # dma nodes, etc.
    #
    l0_cntrl_nodes = []
    l1_cntrl_nodes = []
    l2_cntrl_nodes = []
    dma_cntrl_nodes = []

    assert (options.num_cpus % options.num_clusters == 0)
    num_cpus_per_cluster = options.num_cpus // options.num_clusters

    assert (options.num_l2caches % options.num_clusters == 0)
    num_l2caches_per_cluster = options.num_l2caches // options.num_clusters

    l2_bits = int(math.log(num_l2caches_per_cluster, 2))
    block_size_bits = int(math.log(options.cacheline_size, 2))
    l2_select_low_bit = options.llc_select_low_bit
    assert(l2_select_low_bit >= block_size_bits)

    # ! Break the isolation. Assume MeshTopology and compute NumCoresPerRow.
    num_cores_per_row = options.num_cpus
    if options.gem_forge_stream_engine_enable_float_multicast:
        if options.topology not in ['Mesh_XY', 'MeshDirCorners_XY', 'MeshDir_XY']:
            print('So far MESI_Three_Level_Stream can only support MeshTopology for Multicast')
            assert(False)
        num_cores_per_row = options.num_cpus / options.mesh_rows


    #
    # Must create the individual controllers before the network to ensure the
    # controller constructors are called before the network constructor
    #
    for i in range(options.num_clusters):
        for j in range(num_cpus_per_cluster):
            #
            # First create the Ruby objects associated with this cpu
            #
            l0i_cache = L0Cache(size=options.l1i_size, assoc=options.l1i_assoc,
                is_icache=True,
                start_index_bit=block_size_bits,
                replacement_policy=BRRIPRP())

            l0d_cache = L0Cache(size=options.l1d_size, assoc=options.l1d_assoc, is_icache=False,
                start_index_bit=block_size_bits,
                replacement_policy=BRRIPRP(),
                dataAccessLatency=options.l1d_lat)

            prefetcher = RubyPrefetcher(
                num_streams=16,
                unit_filter=256,
                nonunit_filter=256,
                train_misses=5,
                num_startup_pfs=options.gem_forge_prefetch_dist,
                cross_page=True
            )

            bingo_prefetcher = RubyBingoPrefetcher(
                enabled=(options.gem_forge_prefetcher == 'bingo'),
            )

            # the ruby random tester reuses num_cpus to specify the
            # number of cpu ports connected to the tester object, which
            # is stored in system.cpu. because there is only ever one
            # tester object, num_cpus is not necessarily equal to the
            # size of system.cpu; therefore if len(system.cpu) == 1
            # we use system.cpu[0] to set the clk_domain, thereby ensuring
            # we don't index off the end of the cpu list.
            if len(system.cpu) == 1:
                clk_domain = system.cpu[0].clk_domain
            else:
                clk_domain = system.cpu[i].clk_domain

            l0_cntrl = L0Cache_Controller(
                version=i * num_cpus_per_cluster + j, Icache=l0i_cache,
                Dcache=l0d_cache,
                prefetcher=prefetcher,
                bingoPrefetcher=bingo_prefetcher,
                send_evictions=send_evicts(options),
                clk_domain=clk_domain,
                ruby_system=ruby_system,
                transitions_per_cycle=options.l0_transitions_per_cycle,
                number_of_TBEs=options.l1d_mshrs,
                request_latency=options.l1d_lat,
                response_latency=options.l1d_lat,
                llc_select_num_bits=l2_bits,
                llc_select_low_bit=l2_select_low_bit,
                num_cores_per_row=num_cores_per_row,
                enable_prefetch=(options.gem_forge_prefetcher == 'stride'),
                enable_stream_float=options.gem_forge_stream_engine_enable_float,
                enable_stream_subline=options.gem_forge_stream_engine_enable_float_subline,
                enable_stream_partial_config=options.gem_forge_stream_engine_enable_float_partial_config,
                enable_stream_idea_ack=options.gem_forge_stream_engine_enable_float_idea_ack,
                enable_stream_idea_end=options.gem_forge_stream_engine_enable_float_idea_end,
                enable_stream_idea_flow=options.gem_forge_stream_engine_enable_float_idea_flow,
                enable_stream_multicast=\
                    options.gem_forge_stream_engine_enable_float_multicast,
                stream_multicast_group_size=\
                    options.gem_forge_stream_engine_llc_multicast_group_size,
                stream_multicast_issue_policy=\
                    options.gem_forge_stream_engine_llc_multicast_issue_policy,
                mlc_stream_buffer_init_num_entries=\
                    options.gem_forge_stream_engine_mlc_stream_buffer_init_num_entries,
                enable_stream_range_sync=\
                    options.gem_forge_enable_stream_range_sync,
                enable_stream_float_mem=options.gem_forge_stream_engine_enable_float_mem,
                )

            cpu_seq = RubySequencer(version=i * num_cpus_per_cluster + j,
                                    icache=l0i_cache,
                                    clk_domain=clk_domain,
                                    dcache=l0d_cache,
                                    ruby_system=ruby_system,
                                    is_ideal=options.gem_forge_ideal_ruby,
                                    )
            if options.gem_forge_prefetcher == 'imp':
                if not options.gem_forge_prefetch_on_access:
                    raise ValueError('IMP must be used with PrefetchOnAccess.')
                cpu = system.cpu[i * num_cpus_per_cluster + j]
                if not hasattr(cpu, 'dtb'):
                    raise ValueError('IMP requires TLB to work with virtual address.')
                # IMP can be used with RubySequencer.
                cpu_seq.prefetcher = IndirectMemoryPrefetcher(
                    use_virtual_addresses=True,
                    index_queue_size=16,
                    on_inst=False,
                    prefetch_on_access=options.gem_forge_prefetch_on_access,
                    max_prefetch_distance=options.gem_forge_prefetch_dist,
                    streaming_distance=options.gem_forge_prefetch_dist,
                    pt_table_entries='64',
                    pt_table_assoc=64,
                )
                cpu_seq.prefetcher.registerTLB(cpu.dtb)

            l0_cntrl.sequencer = cpu_seq

            l1_cache = L1Cache(size=options.l1_5d_size,
                               assoc=options.l1_5d_assoc,
                               start_index_bit=block_size_bits,
                               is_icache=False)

            l1_prefetcher = RubyPrefetcher(
                num_streams=16,
                unit_filter=256,
                nonunit_filter=256,
                train_misses=5,
                num_startup_pfs=options.gem_forge_l2_prefetch_dist,
                cross_page=True,
                bulk_prefetch_size=options.gem_forge_l2_bulk_prefetch_size,
            )

            l1_cntrl = L1Cache_Controller(
                version=i * num_cpus_per_cluster + j,
                cache=l1_cache,
                prefetcher=l1_prefetcher,
                enable_prefetch=(options.gem_forge_l2_prefetcher == 'stride'),
                transitions_per_cycle=options.l1_transitions_per_cycle,
                number_of_TBEs=options.l1_5d_mshrs,
                l1_request_latency=options.l3_lat,
                l1_response_latency=options.l2_lat,
                # This is only used to send Unblock messages, so I kept it 1 cycle.
                to_l2_latency=1,
                l2_select_num_bits=l2_bits,
                l2_select_low_bit=l2_select_low_bit,
                num_cores_per_row=num_cores_per_row,
                cluster_id=i, ruby_system=ruby_system,
                # For the AbstractStreamAwareController
                llc_select_num_bits=l2_bits,
                llc_select_low_bit=l2_select_low_bit,
                enable_stream_float=options.gem_forge_stream_engine_enable_float,
                enable_stream_subline=options.gem_forge_stream_engine_enable_float_subline,
                enable_stream_partial_config=options.gem_forge_stream_engine_enable_float_partial_config,
                enable_stream_idea_ack=options.gem_forge_stream_engine_enable_float_idea_ack,
                enable_stream_idea_end=options.gem_forge_stream_engine_enable_float_idea_end,
                enable_mlc_stream_idea_pop_check_llc_progress=\
                    options.gem_forge_stream_engine_enable_float_idea_mlc_pop_check,
                enable_stream_idea_flow=options.gem_forge_stream_engine_enable_float_idea_flow,
                enable_stream_multicast=\
                    options.gem_forge_stream_engine_enable_float_multicast,
                stream_multicast_group_size=\
                    options.gem_forge_stream_engine_llc_multicast_group_size,
                stream_multicast_issue_policy=\
                    options.gem_forge_stream_engine_llc_multicast_issue_policy,
                mlc_stream_buffer_init_num_entries=\
                    options.gem_forge_stream_engine_mlc_stream_buffer_init_num_entries,
                mlc_stream_buffer_to_segment_ratio=\
                    options.gem_forge_stream_engine_mlc_stream_buffer_to_segment_ratio,
                enable_stream_range_sync=options.gem_forge_enable_stream_range_sync,
                enable_stream_float_mem=options.gem_forge_stream_engine_enable_float_mem,
                )

            exec("ruby_system.l0_cntrl%d = l0_cntrl"
                 % ( i * num_cpus_per_cluster + j))
            exec("ruby_system.l1_cntrl%d = l1_cntrl"
                 % ( i * num_cpus_per_cluster + j))

            #
            # Add controllers and sequencers to the appropriate lists
            #
            cpu_sequencers.append(cpu_seq)
            l0_cntrl_nodes.append(l0_cntrl)
            l1_cntrl_nodes.append(l1_cntrl)

            # Connect the L0 and L1 controllers
            l0_cntrl.mandatoryQueue=MessageBuffer()
            l0_cntrl.prefetchQueue = MessageBuffer()
            l0_cntrl.bufferToL1 = MessageBuffer(ordered = False)
            l1_cntrl.bufferFromL0 = l0_cntrl.bufferToL1
            l0_cntrl.bufferFromL1 = MessageBuffer(ordered=False)
            l1_cntrl.bufferToL0 = l0_cntrl.bufferFromL1

            # Connect the L1 controllers and the network
            l1_cntrl.requestToL2 = MessageBuffer()
            l1_cntrl.requestToL2.master = ruby_system.network.slave
            l1_cntrl.responseToL2 = MessageBuffer()
            l1_cntrl.responseToL2.master = ruby_system.network.slave
            l1_cntrl.unblockToL2 = MessageBuffer()
            l1_cntrl.unblockToL2.master = ruby_system.network.slave

            l1_cntrl.requestFromL2 = MessageBuffer()
            l1_cntrl.requestFromL2.slave = ruby_system.network.master
            l1_cntrl.responseFromL2 = MessageBuffer()
            l1_cntrl.responseFromL2.slave = ruby_system.network.master
            l1_cntrl.prefetchQueue = MessageBuffer()


        for j in range(num_l2caches_per_cluster):
            """
            Originally, bits [6, 7] are used to select LLC bank (block-level),
            bits [8, ...] are used to select set.
            Now bits [12, 12+n) are used to select bank (page-level),
            bits [6-11] and [12+n, ...] are used to select set.
            Since the index bits are broken down into two pieces, we need
            to inform the cache to skip the bank select bit.
            """
            l2_cache = L2Cache(size=options.l2_size,
                               assoc=options.l2_assoc,
                               start_index_bit=block_size_bits,
                               skip_index_start_bit=l2_select_low_bit,
                               skip_index_num_bits=l2_bits,
                               replacement_policy=BRRIPRP(),
                               query_stream_nuca=True,
                               )

            l2_cntrl = L2Cache_Controller(
                version=i * num_l2caches_per_cluster + j,
                L2cache=l2_cache,
                cluster_id=i,
                transitions_per_cycle=options.l2_transitions_per_cycle,
                ruby_system=ruby_system,
                number_of_TBEs=128,
                l2_request_latency=options.l3_lat,
                l2_response_latency=options.l3_lat,
                to_l1_latency=options.l3_lat,
                llc_select_low_bit=l2_select_low_bit,
                llc_select_num_bits=l2_bits,
                num_cores_per_row=num_cores_per_row,
                enable_stream_float=options.gem_forge_stream_engine_enable_float,
                enable_stream_subline=options.gem_forge_stream_engine_enable_float_subline,
                enable_stream_partial_config=options.gem_forge_stream_engine_enable_float_partial_config,
                enable_stream_idea_ack=options.gem_forge_stream_engine_enable_float_idea_ack,
                enable_stream_idea_end=options.gem_forge_stream_engine_enable_float_idea_end,
                enable_stream_idea_flow=options.gem_forge_stream_engine_enable_float_idea_flow,
                enable_stream_idea_store=\
                    options.gem_forge_stream_engine_enable_float_idea_store,
                enable_stream_compact_store=\
                    options.gem_forge_stream_engine_enable_float_compact_store,
                enable_stream_advance_migrate=\
                    options.gem_forge_stream_engine_enable_float_advance_migrate,
                enable_stream_multicast=\
                    options.gem_forge_stream_engine_enable_float_multicast,
                stream_multicast_group_size=\
                    options.gem_forge_stream_engine_llc_multicast_group_size,
                stream_multicast_issue_policy=\
                    options.gem_forge_stream_engine_llc_multicast_issue_policy,
                ind_stream_req_max_per_multicast_msg=\
                    options.gem_forge_stream_engine_llc_multicast_max_ind_req_per_message,
                ind_stream_req_multicast_group_size=\
                    options.gem_forge_stream_engine_llc_multicast_ind_req_bank_group_size,
                mlc_stream_buffer_init_num_entries=\
                    options.gem_forge_stream_engine_mlc_stream_buffer_init_num_entries,
                llc_stream_engine_issue_width=\
                    options.gem_forge_stream_engine_llc_stream_engine_issue_width,
                llc_stream_engine_migrate_width=\
                    options.gem_forge_stream_engine_llc_stream_engine_migrate_width,
                llc_stream_max_infly_request=\
                    options.gem_forge_stream_engine_llc_stream_max_infly_request,
                llc_stream_engine_compute_width=\
                    options.gem_forge_stream_engine_compute_width,
                llc_stream_engine_max_infly_computation=\
                    options.gem_forge_stream_engine_llc_max_infly_computation,
                llc_access_core_simd_delay=\
                    options.gem_forge_stream_engine_llc_access_core_simd_delay,
                has_scalar_alu=\
                    options.gem_forge_stream_engine_has_scalar_alu,
                mlc_generate_direct_range=\
                    options.gem_forge_stream_engine_mlc_generate_direct_range,
                enable_llc_stream_zero_compute_latency=\
                    options.gem_forge_enable_stream_zero_compute_latency,
                enable_stream_range_sync=\
                    options.gem_forge_enable_stream_range_sync,
                stream_atomic_lock_type=\
                    options.gem_forge_stream_atomic_lock_type,
                neighbor_stream_threshold=\
                    options.gem_forge_stream_engine_llc_neighbor_stream_threshold,
                neighbor_migration_delay=\
                    options.gem_forge_stream_engine_llc_neighbor_migration_delay,
                neighbor_migration_valve_type=\
                    options.gem_forge_stream_engine_llc_neighbor_migration_valve_type,
                enable_stream_float_mem=options.gem_forge_stream_engine_enable_float_mem,
                )

            exec("ruby_system.l2_cntrl%d = l2_cntrl"
                 % (i * num_l2caches_per_cluster + j))
            l2_cntrl_nodes.append(l2_cntrl)

            # Connect the L2 controllers and the network
            l2_cntrl.DirRequestFromL2Cache = MessageBuffer()
            l2_cntrl.DirRequestFromL2Cache.master = ruby_system.network.slave
            l2_cntrl.L1RequestFromL2Cache = MessageBuffer()
            l2_cntrl.L1RequestFromL2Cache.master = ruby_system.network.slave
            l2_cntrl.responseFromL2Cache = MessageBuffer()
            l2_cntrl.responseFromL2Cache.master = ruby_system.network.slave

            l2_cntrl.unblockToL2Cache = MessageBuffer()
            l2_cntrl.unblockToL2Cache.slave = ruby_system.network.master
            l2_cntrl.L1RequestToL2Cache = MessageBuffer()
            l2_cntrl.L1RequestToL2Cache.slave = ruby_system.network.master
            l2_cntrl.responseToL2Cache = MessageBuffer()
            l2_cntrl.responseToL2Cache.slave = ruby_system.network.master

            # ! Sean: StreamAwareCache
            l2_cntrl.streamMigrateToL2Cache = MessageBuffer()
            l2_cntrl.streamMigrateToL2Cache.master = ruby_system.network.slave
            l2_cntrl.streamMigrateFromL2Cache = MessageBuffer()
            l2_cntrl.streamMigrateFromL2Cache.slave = ruby_system.network.master

            l2_cntrl.streamIndirectToL2Cache = MessageBuffer()
            l2_cntrl.streamIndirectToL2Cache.master = ruby_system.network.slave
            l2_cntrl.streamIndirectFromL2Cache = MessageBuffer()
            l2_cntrl.streamIndirectFromL2Cache.slave = ruby_system.network.master

    # Run each of the ruby memory controllers at a ratio of the frequency of
    # the ruby system
    # clk_divider value is a fix to pass regression.
    ruby_system.memctrl_clk_domain = DerivedClockDomain(
            clk_domain = ruby_system.clk_domain, clk_divider=3)

    mem_dir_cntrl_nodes, rom_dir_cntrl_node = create_directories(
        options, bootmem, ruby_system, system)
    dir_cntrl_nodes = mem_dir_cntrl_nodes[:]
    if rom_dir_cntrl_node is not None:
        dir_cntrl_nodes.append(rom_dir_cntrl_node)
    for dir_cntrl in dir_cntrl_nodes:

        # Increase the number of TBEs.
        dir_cntrl.number_of_TBEs = 8192

        # Connect the directory controllers and the network
        dir_cntrl.requestToDir = MessageBuffer()
        dir_cntrl.requestToDir.slave = ruby_system.network.master
        dir_cntrl.responseToDir = MessageBuffer()
        dir_cntrl.responseToDir.slave = ruby_system.network.master
        dir_cntrl.responseFromDir = MessageBuffer()
        dir_cntrl.responseFromDir.master = ruby_system.network.slave
        dir_cntrl.requestFromDir = MessageBuffer()
        dir_cntrl.requestFromDir.master = ruby_system.network.slave
        dir_cntrl.requestToMemory = MessageBuffer()
        dir_cntrl.responseFromMemory = MessageBuffer()

        # ! Sean: StreamAwareCache
        dir_cntrl.streamMigrateFromMem = MessageBuffer()
        dir_cntrl.streamMigrateFromMem.master = ruby_system.network.slave
        dir_cntrl.streamMigrateToMem = MessageBuffer()
        dir_cntrl.streamMigrateToMem.slave = ruby_system.network.master

        dir_cntrl.streamIndirectFromMem = MessageBuffer()
        dir_cntrl.streamIndirectFromMem.master = ruby_system.network.slave
        dir_cntrl.streamIndirectToMem = MessageBuffer()
        dir_cntrl.streamIndirectToMem.slave = ruby_system.network.master


        dir_cntrl.llc_select_low_bit = l2_select_low_bit
        dir_cntrl.llc_select_num_bits = l2_bits
        dir_cntrl.num_cores_per_row = num_cores_per_row
        dir_cntrl.enable_stream_float = options.gem_forge_stream_engine_enable_float
        dir_cntrl.enable_stream_subline = options.gem_forge_stream_engine_enable_float_subline
        dir_cntrl.enable_stream_partial_config = options.gem_forge_stream_engine_enable_float_partial_config
        dir_cntrl.enable_stream_idea_ack = options.gem_forge_stream_engine_enable_float_idea_ack
        dir_cntrl.enable_stream_idea_end = options.gem_forge_stream_engine_enable_float_idea_end
        dir_cntrl.enable_stream_idea_flow = options.gem_forge_stream_engine_enable_float_idea_flow
        dir_cntrl.enable_stream_idea_store = options.gem_forge_stream_engine_enable_float_idea_store
        dir_cntrl.enable_stream_compact_store = options.gem_forge_stream_engine_enable_float_compact_store
        dir_cntrl.enable_stream_advance_migrate = options.gem_forge_stream_engine_enable_float_advance_migrate
        dir_cntrl.enable_stream_multicast = options.gem_forge_stream_engine_enable_float_multicast
        dir_cntrl.stream_multicast_group_size = options.gem_forge_stream_engine_llc_multicast_group_size
        dir_cntrl.stream_multicast_issue_policy = options.gem_forge_stream_engine_llc_multicast_issue_policy
        dir_cntrl.mlc_stream_buffer_init_num_entries = options.gem_forge_stream_engine_mlc_stream_buffer_init_num_entries
        dir_cntrl.llc_stream_engine_issue_width = options.gem_forge_stream_engine_mc_issue_width
        dir_cntrl.llc_stream_engine_migrate_width = options.gem_forge_stream_engine_llc_stream_engine_migrate_width
        dir_cntrl.llc_stream_max_infly_request = options.gem_forge_stream_engine_mc_stream_max_infly_request
        dir_cntrl.llc_stream_engine_compute_width = options.gem_forge_stream_engine_compute_width
        dir_cntrl.llc_stream_engine_max_infly_computation = options.gem_forge_stream_engine_llc_max_infly_computation
        dir_cntrl.llc_access_core_simd_delay = options.gem_forge_stream_engine_llc_access_core_simd_delay
        dir_cntrl.mlc_generate_direct_range = options.gem_forge_stream_engine_mlc_generate_direct_range
        dir_cntrl.has_scalar_alu=options.gem_forge_stream_engine_has_scalar_alu
        dir_cntrl.enable_llc_stream_zero_compute_latency = options.gem_forge_enable_stream_zero_compute_latency
        dir_cntrl.enable_stream_range_sync = options.gem_forge_enable_stream_range_sync
        dir_cntrl.stream_atomic_lock_type = options.gem_forge_stream_atomic_lock_type
        dir_cntrl.neighbor_stream_threshold = options.gem_forge_stream_engine_mc_neighbor_stream_threshold
        dir_cntrl.neighbor_migration_delay = options.gem_forge_stream_engine_mc_neighbor_migration_delay
        dir_cntrl.enable_stream_float_mem = options.gem_forge_stream_engine_enable_float_mem
        dir_cntrl.reuse_buffer_lines_per_core = options.gem_forge_stream_engine_mc_reuse_buffer_lines_per_core
        dir_cntrl.ind_stream_req_max_per_multicast_msg = options.gem_forge_stream_engine_llc_multicast_max_ind_req_per_message
        dir_cntrl.ind_stream_req_multicast_group_size = options.gem_forge_stream_engine_llc_multicast_ind_req_bank_group_size

    for i, dma_port in enumerate(dma_ports):
        #
        # Create the Ruby objects associated with the dma controller
        #
        dma_seq = DMASequencer(version=i, ruby_system=ruby_system)

        dma_cntrl = DMA_Controller(version=i,
                                   dma_sequencer=dma_seq,
                                   transitions_per_cycle=options.ports,
                                   ruby_system=ruby_system)

        exec("ruby_system.dma_cntrl%d = dma_cntrl" % i)
        exec("ruby_system.dma_cntrl%d.dma_sequencer.slave = dma_port" % i)
        dma_cntrl_nodes.append(dma_cntrl)

        # Connect the dma controller to the network
        dma_cntrl.mandatoryQueue = MessageBuffer()
        dma_cntrl.responseFromDir = MessageBuffer(ordered=True)
        dma_cntrl.responseFromDir.slave = ruby_system.network.master
        dma_cntrl.requestToDir = MessageBuffer()
        dma_cntrl.requestToDir.master = ruby_system.network.slave

    all_cntrls = l0_cntrl_nodes + \
                 l1_cntrl_nodes + \
                 l2_cntrl_nodes + \
                 dir_cntrl_nodes + \
                 dma_cntrl_nodes

    # Create the io controller and the sequencer
    if full_system:
        io_seq = DMASequencer(version=len(dma_ports), ruby_system=ruby_system)
        ruby_system._io_port = io_seq
        io_controller = DMA_Controller(version=len(dma_ports),
                                       dma_sequencer=io_seq,
                                       ruby_system=ruby_system)
        ruby_system.io_controller = io_controller

        # Connect the dma controller to the network
        io_controller.mandatoryQueue = MessageBuffer()
        io_controller.responseFromDir = MessageBuffer(ordered=True)
        io_controller.responseFromDir.slave = ruby_system.network.master
        io_controller.requestToDir = MessageBuffer()
        io_controller.requestToDir.master = ruby_system.network.slave

        all_cntrls = all_cntrls + [io_controller]
    # Register configuration with filesystem
    else:
        for i in range(options.num_clusters):
            for j in range(num_cpus_per_cluster):
                FileSystemConfig.register_cpu(physical_package_id=0,
                                              core_siblings=range(options.num_cpus),
                                              core_id=i*num_cpus_per_cluster+j,
                                              thread_siblings=[])

                FileSystemConfig.register_cache(level=0,
                                                idu_type='Instruction',
                                                size='4096B',
                                                line_size=options.cacheline_size,
                                                assoc=1,
                                                cpus=[i*num_cpus_per_cluster+j])
                FileSystemConfig.register_cache(level=0,
                                                idu_type='Data',
                                                size='4096B',
                                                line_size=options.cacheline_size,
                                                assoc=1,
                                                cpus=[i*num_cpus_per_cluster+j])

                FileSystemConfig.register_cache(level=1,
                                                idu_type='Unified',
                                                size=options.l1d_size,
                                                line_size=options.cacheline_size,
                                                assoc=options.l1d_assoc,
                                                cpus=[i*num_cpus_per_cluster+j])

            FileSystemConfig.register_cache(level=2,
                                            idu_type='Unified',
                                            size=str(MemorySize(options.l2_size) * \
                                                   num_l2caches_per_cluster)+'B',
                                            line_size=options.cacheline_size,
                                            assoc=options.l2_assoc,
                                            cpus=[n for n in range(i*num_cpus_per_cluster, \
                                                                     (i+1)*num_cpus_per_cluster)])

    # ! Sean: StreamAwareCache
    # ! Add one virtual network for stream migration request.
    # ! Add one virtual network for stream indirect request.
    ruby_system.network.number_of_virtual_networks = 5
    topology = create_topology(all_cntrls, options)
    return (cpu_sequencers, mem_dir_cntrl_nodes, topology)
