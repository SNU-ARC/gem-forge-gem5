/*
 * Copyright (c) 2020 Inria
 * Copyright (c) 2016 Georgia Institute of Technology
 * Copyright (c) 2008 Princeton University
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef __MEM_RUBY_NETWORK_GARNET2_0_INPUTUNIT_HH__
#define __MEM_RUBY_NETWORK_GARNET2_0_INPUTUNIT_HH__

#include <iostream>
#include <vector>
#include <deque>

#include "mem/ruby/common/Consumer.hh"
#include "mem/ruby/network/garnet2.0/CommonTypes.hh"
#include "mem/ruby/network/garnet2.0/CreditLink.hh"
#include "mem/ruby/network/garnet2.0/NetworkLink.hh"
#include "mem/ruby/network/garnet2.0/Router.hh"
#include "mem/ruby/network/garnet2.0/VirtualChannel.hh"
#include "mem/ruby/network/garnet2.0/flitBuffer.hh"

class InputUnit : public Consumer
{
  public:
    InputUnit(int id, PortDirection direction, Router *router);
    ~InputUnit() = default;

    void wakeup();
    void print(std::ostream& out) const {};

    inline PortDirection get_direction() { return m_direction; }

    inline void
    set_vc_idle(int vc, Cycles curTime)
    {
        virtualChannels[vc].set_idle(curTime);
    }

    inline void
    set_vc_active(int vc, Cycles curTime)
    {
        virtualChannels[vc].set_active(curTime);
    }

    inline void
    grant_outport(int vc, int outport)
    {
        virtualChannels[vc].set_outport(outport);
    }

    inline void
    grant_outvc(int vc, int outvc)
    {
        virtualChannels[vc].set_outvc(outvc);
    }

    inline int
    get_outport(int invc)
    {
        return virtualChannels[invc].get_outport();
    }

    inline int
    get_outvc(int invc)
    {
        return virtualChannels[invc].get_outvc();
    }

    inline Cycles
    get_enqueue_time(int invc)
    {
        return virtualChannels[invc].get_enqueue_time();
    }

    void increment_credit(int in_vc, bool free_signal, Cycles curTime);

    inline flit*
    peekTopFlit(int vc)
    {
        return virtualChannels[vc].peekTopFlit();
    }

    inline flit*
    getTopFlit(int vc)
    {
        return virtualChannels[vc].getTopFlit();
    }

    inline bool
    need_stage(int vc, flit_stage stage, Cycles time)
    {
        return virtualChannels[vc].need_stage(stage, time);
    }

    inline bool
    isReady(int invc, Cycles curTime)
    {
        return virtualChannels[invc].isReady(curTime);
    }

    flitBuffer* getCreditQueue() { return &creditQueue; }

    inline void
    set_in_link(NetworkLink *link)
    {
        m_in_link = link;
    }

    inline int get_inlink_id() { return m_in_link->get_id(); }

    inline void
    set_credit_link(CreditLink *credit_link)
    {
        m_credit_link = credit_link;
    }

    double get_buf_read_activity(unsigned int vnet) const
    { return m_num_buffer_reads[vnet]; }
    double get_buf_write_activity(unsigned int vnet) const
    { return m_num_buffer_writes[vnet]; }

    uint32_t functionalWrite(Packet *pkt);
    void resetStats();

  private:
    Router *m_router;
    int m_id;
    PortDirection m_direction;
    int m_vc_per_vnet;
    std::vector<int> m_vnet_busy_count;
    NetworkLink *m_in_link;
    CreditLink *m_credit_link;
    flitBuffer creditQueue;

    // Input Virtual channels
    std::vector<VirtualChannel> virtualChannels;

    // Statistical variables
    std::vector<double> m_num_buffer_writes;
    std::vector<double> m_num_buffer_reads;

    struct MulticastDuplicateBuffer {
        enum StateE {
            Invalid,
            Buffering,
        };
        InputUnit *inputUnit;
        StateE state = Invalid;
        RouteInfo route;
        MsgPtr msg = nullptr;
        int readyFlits = 0;
        std::deque<flit *> flits;
        MulticastDuplicateBuffer(InputUnit *_inputUnit)
            : inputUnit(_inputUnit) {}
        void allocate(const RouteInfo &route, MsgPtr msg) {
            assert(this->state == Invalid);
            this->route = route;
            this->msg = msg;
            this->state = Buffering;
        }
        void push(flit *f) {
            assert(this->state == Buffering);
            // Set the flag indicating this is a duplicate flit.
            f->setMulticastDuplicate(true);
            this->flits.push_back(f);
            auto type = f->get_type();
            if (type == TAIL_ || type == HEAD_TAIL_) {
                this->readyFlits += f->get_size();
                this->inputUnit->totalReadyMulitcastFlits += f->get_size();
                assert(this->readyFlits == this->flits.size());
                this->state = Invalid;
                inputUnit->duplicateMulticastMsgToNetworkInterface(*this);
            }
        }
        flit *peek() {
            assert(this->isReady());
            auto f = this->flits.front();
            return f;
        }
        flit *pop() {
            assert(this->isReady());
            auto f = this->flits.front();
            this->flits.pop_front();
            this->readyFlits--;
            this->inputUnit->totalReadyMulitcastFlits--;
            return f;
        }
        void setVCForFrontMsg(int vc) {
            assert(this->isReady());
            auto frontFlitType = this->flits.front()->get_type();
            assert(frontFlitType == HEAD_ || frontFlitType == HEAD_TAIL_);
            assert(this->readyFlits >= this->flits.front()->get_size());
            int i = 0;
            int n = this->flits.front()->get_size();
            auto iter = this->flits.begin();
            auto end = this->flits.end();
            while (i < n) {
                assert(iter != end);
                (*iter)->set_vc(vc);
                ++i;
                ++iter;
            }
        }
        bool isReady() const {
            return this->readyFlits > 0;
        }
        bool isBuffering() const {
            return this->state == Buffering;
        }
    };

    std::vector<MulticastDuplicateBuffer> multicastBuffers;
    int totalReadyMulitcastFlits = 0;
    // Used for round robin.
    int currMulticastBufferIdx = 0;

    // Group destination by routing out port.
    using PortToDestinationMap = std::map<int, std::vector<MachineID>>;
    PortToDestinationMap groupDestinationByRouting(
        flit* inflyFlit,
        const std::vector<MachineID> &destMachineIDs);
    flit *selectFlit();
    void allocateMulticastBuffer(flit *f);
    void duplicateMulitcastFlit(flit *f);
    int calculateVCForMulticastDuplicateFlit(int vnet);
    void duplicateMulticastMsgToNetworkInterface(
        MulticastDuplicateBuffer &buffer);
};

#endif // __MEM_RUBY_NETWORK_GARNET2_0_INPUTUNIT_HH__
