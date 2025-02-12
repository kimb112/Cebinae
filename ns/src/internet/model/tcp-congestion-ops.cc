/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015 Natale Patriciello <natale.patriciello@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include "tcp-congestion-ops.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include <iostream>
#include <fstream>


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpCongestionOps");

NS_OBJECT_ENSURE_REGISTERED (TcpCongestionOps);

TypeId
TcpCongestionOps::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpCongestionOps")
    .SetParent<Object> ()
    .SetGroupName ("Internet")
  ;
  return tid;
}

TcpCongestionOps::TcpCongestionOps () : Object ()
{
}

TcpCongestionOps::TcpCongestionOps (const TcpCongestionOps &other) : Object (other)
{
}

TcpCongestionOps::~TcpCongestionOps ()
{
}

void
TcpCongestionOps::IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);
}

void
TcpCongestionOps::PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked,
                             const Time& rtt)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked << rtt);
}

void
TcpCongestionOps::CongestionStateSet (Ptr<TcpSocketState> tcb,
                                      const TcpSocketState::TcpCongState_t newState)
{
  NS_LOG_FUNCTION (this << tcb << newState);
}

void
TcpCongestionOps::CwndEvent (Ptr<TcpSocketState> tcb,
                             const TcpSocketState::TcpCAEvent_t event)
{
  NS_LOG_FUNCTION (this << tcb << event);
}

bool
TcpCongestionOps::HasCongControl () const
{
  return false;
}

void
TcpCongestionOps::CongControl (Ptr<TcpSocketState> tcb,
                               [[maybe_unused]] const TcpRateOps::TcpRateConnection &rc,
                               [[maybe_unused]] const TcpRateOps::TcpRateSample &rs)
{
  NS_LOG_FUNCTION (this << tcb);
}

// RENO

NS_OBJECT_ENSURE_REGISTERED (TcpNewReno);

TypeId
TcpNewReno::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpNewReno")
    .SetParent<TcpCongestionOps> ()
    .SetGroupName ("Internet")
    .AddConstructor<TcpNewReno> ()
  ;
  return tid;
}

TcpNewReno::TcpNewReno (void) 
  : TcpCongestionOps (),
    congestionTimescale (100000000), /* 100 ms */
    samplingTimescale (25000000), /* 25 ms */
    congestionEncounteredRecently (false),
    qIndex (0),
    taxRate (0.01),
    recentRtt (0),
    congCount (0),
    congNotCount(0) 
{
  NS_LOG_FUNCTION (this);
  zeroTime = Time();

  for (int i = 0; i < NUMBER_OF_SUB_SAMPLES; i++) {
    rttCircularQ[i] = zeroTime;
    rttLogTime[i] = zeroTime;
  }
}

TcpNewReno::TcpNewReno (const TcpNewReno& sock)
  : TcpCongestionOps (sock),
    congestionTimescale (100000000), /* 100 ms */
    samplingTimescale (25000000), /* 25 ms */
    congestionEncounteredRecently (sock.congestionEncounteredRecently),
    qIndex (sock.qIndex),
    taxRate (0.01),
    recentRtt (sock.recentRtt),
    congCount (sock.congCount),
    congNotCount(sock.congNotCount) 
{
  NS_LOG_FUNCTION (this);

  for (int i = 0; i < NUMBER_OF_SUB_SAMPLES; i++) {
    rttCircularQ[i] = zeroTime;
    rttLogTime[i] = zeroTime;
  }
}

TcpNewReno::~TcpNewReno (void)
{
}


/**
 * \brief Tcp NewReno slow start algorithm
 *
 * Defined in RFC 5681 as
 *
 * > During slow start, a TCP increments cwnd by at most SMSS bytes for
 * > each ACK received that cumulatively acknowledges new data.  Slow
 * > start ends when cwnd exceeds ssthresh (or, optionally, when it
 * > reaches it, as noted above) or when congestion is observed.  While
 * > traditionally TCP implementations have increased cwnd by precisely
 * > SMSS bytes upon receipt of an ACK covering new data, we RECOMMEND
 * > that TCP implementations increase cwnd, per:
 * >
 * >    cwnd += min (N, SMSS)                      (2)
 * >
 * > where N is the number of previously unacknowledged bytes acknowledged
 * > in the incoming ACK.
 *
 * The ns-3 implementation respect the RFC definition. Linux does something
 * different:
 * \verbatim
u32 tcp_slow_start(struct tcp_sock *tp, u32 acked)
  {
    u32 cwnd = tp->snd_cwnd + acked;

    if (cwnd > tp->snd_ssthresh)
      cwnd = tp->snd_ssthresh + 1;
    acked -= cwnd - tp->snd_cwnd;
    tp->snd_cwnd = min(cwnd, tp->snd_cwnd_clamp);

    return acked;
  }
  \endverbatim
 *
 * As stated, we want to avoid the case when a cumulative ACK increases cWnd more
 * than a segment size, but we keep count of how many segments we have ignored,
 * and return them.
 *
 * \param tcb internal congestion state
 * \param segmentsAcked count of segments acked
 * \return the number of segments not considered for increasing the cWnd
 */
uint32_t
TcpNewReno::SlowStart (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);

  if (segmentsAcked >= 1)
    {
      tcb->m_cWnd += tcb->m_segmentSize;
      NS_LOG_INFO ("In SlowStart, updated to cwnd " << tcb->m_cWnd << " ssthresh " << tcb->m_ssThresh);
      return segmentsAcked - 1;
    }

  return 0;
}

/**
 * \brief NewReno congestion avoidance
 *
 * During congestion avoidance, cwnd is incremented by roughly 1 full-sized
 * segment per round-trip time (RTT).
 *
 * \param tcb internal congestion state
 * \param segmentsAcked count of segments acked
 */
void
TcpNewReno::CongestionAvoidance (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);

  if (segmentsAcked > 0)
    {
      double adder = static_cast<double> (tcb->m_segmentSize * tcb->m_segmentSize) / tcb->m_cWnd.Get ();
      adder = std::max (1.0, adder);
      tcb->m_cWnd += static_cast<uint32_t> (adder);
      NS_LOG_INFO ("In CongAvoid, updated to cwnd " << tcb->m_cWnd <<
                   " ssthresh " << tcb->m_ssThresh);
    }
}

void
TcpNewReno::CongestionStateSet (Ptr<TcpSocketState> tcb,
                              const TcpSocketState::TcpCongState_t newState)
{
  Time now;

  NS_LOG_FUNCTION (this << tcb << newState);
  if ((newState == TcpSocketState::CA_RECOVERY) || (newState == TcpSocketState::CA_LOSS))
    {
   	now = Simulator::Now ();
  	// std::cout << (now).GetSeconds() << " " << tcb->m_lastAckedSeq << " packet lost" << std::endl;
    }
}

/**
 * For implementing fairness tax, implement tracking of whether 
 * congestion was encountered recently by the flow.
 */
void
TcpNewReno::PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked,
                     const Time& rtt)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked << rtt);

  Time now;
  Time earliestRTTtimestamp;
  Time earliestRTT;



  if (rtt.IsZero ())
    {
      return;
    }

  // Do all of the following for implementing fairness tax
  now = Simulator::Now ();
  // std::cout << "FAIRNESS TAX: time difference: " << (now - rttLogTime[qIndex]).GetInteger() << std::endl;

  // std::cout << "Time: " << rttLogTime[qIndex].GetInteger() << " RTT: " << rtt.GetInteger() << std::endl;
  
  // std::cout << now.GetSeconds() << " " << rtt.GetMilliSeconds() << std::endl;


  if ((now - rttLogTime[qIndex]).GetInteger() > samplingTimescale) { // in nanoseconds
    qIndex = (qIndex+1)%NUMBER_OF_SUB_SAMPLES;
    rttCircularQ[qIndex] = rtt;
    rttLogTime[qIndex] = now;

  }
  
  earliestRTTtimestamp = now;
  earliestRTT = rtt;
  for (int i = 0; i < NUMBER_OF_SUB_SAMPLES; i++) {
    if ((now - rttLogTime[i]).GetInteger() > congestionTimescale) { // in nanoseconds
      rttCircularQ[i] = zeroTime;
      rttLogTime[i] = zeroTime;
    }
    else {
      if (rttLogTime[i] < earliestRTTtimestamp) { 
        earliestRTTtimestamp = rttLogTime[i];
	earliestRTT = rttCircularQ[i];
      }
    }

  }

  if (earliestRTT.GetInteger() > 10) {
    if ((rtt - earliestRTT).GetInteger() > (10*TIMESTAMPING_ERROR_EPSILON) ) { // 1 ms
      congestionEncounteredRecently = true;
      // std::cout << "Congestion encountered: RTT = " << rtt.GetInteger() << " ns,  incRTT = " << (rtt - earliestRTT).GetInteger() << " ns" << std::endl;
    }
    else {
      congestionEncounteredRecently = false;
    }
  }

  recentRtt = rtt;
}


/**
 * \brief Try to increase the cWnd following the NewReno specification
 *
 * \see SlowStart
 * \see CongestionAvoidance
 *
 * \param tcb internal congestion state
 * \param segmentsAcked count of segments acked
 */
void
TcpNewReno::IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);
  
//   uint32_t prevWindow = tcb->m_cWnd;
  uint32_t newWindow;
  double taxRateAdjusted=1;
  Time now;

  if (tcb->m_cWnd < tcb->m_ssThresh)
    {
      segmentsAcked = SlowStart (tcb, segmentsAcked);
    }

  if (tcb->m_cWnd >= tcb->m_ssThresh)
    {
      CongestionAvoidance (tcb, segmentsAcked);
    }

  /* At this point, we could have segmentsAcked != 0. This because RFC says
   * that in slow start, we should increase cWnd by min (N, SMSS); if in
   * slow start we receive a cumulative ACK, it counts only for 1 SMSS of
   * increase, wasting the others.
   *
   * // Incorrect assert, I am sorry
   * NS_ASSERT (segmentsAcked == 0);
   */

  /* Update window to implement Fairness Tax */
  // First check if congestion encountered recently
  if ((Simulator::Now() - rttLogTime[qIndex]).GetInteger() > congestionTimescale) {
    congestionEncounteredRecently = false;
  }
            /* OBSOLETE !!  If tax should be applied to ALL flow, not just 
             * bottlenecked ones, uncomment the following: OBSOLETE!!
             *
             * congestionEncounteredRecently = true;
             */
  // congestionEncounteredRecently = true;

  // To turn OFF FAIRNESS TAX (default to NewReno), uncomment the following: 
  congestionEncounteredRecently = false;

  // PROP_RATE_RTT:
  taxRateAdjusted = tcb->m_cWnd*8*0.075/(recentRtt.GetSeconds()*recentRtt.GetSeconds()*50000000);

  // PROP_RATE^2_RTT:
  // taxRateAdjusted = (tcb->m_cWnd)*(tcb->m_cWnd)*8*12.5/(recentRtt.GetSeconds()*recentRtt.GetSeconds()*recentRtt.GetSeconds()*50000000*50000000);

  
  if (congestionEncounteredRecently == true) {
    newWindow = tcb->m_cWnd;

    // Option 2 -- tax the final new window:
    tcb->m_cWnd = newWindow*(1 - taxRate*taxRateAdjusted);

    // std::cout << "FAIRNESS TAX: newWindow " << newWindow << " reduced to: " << tcb->m_cWnd << std::endl;

    now = Simulator::Now ();

    congCount++;
    if (congCount % 100 == 0) {
      // std::cout << "time: " << now.GetSeconds() << " congCount: " << congCount << " congNotCount: " << congNotCount << " ratio: " << (congNotCount*1.0/congCount) << " lastAckedSeq: " << tcb->m_lastAckedSeq << " tax: " << (taxRate*taxRateAdjusted) << std::endl;
    }
  }
  else {
    congNotCount++;
  }

  
}

std::string
TcpNewReno::GetName () const
{
  return "TcpNewReno";
}

uint32_t
TcpNewReno::GetSsThresh (Ptr<const TcpSocketState> state,
                         uint32_t bytesInFlight)
{
  NS_LOG_FUNCTION (this << state << bytesInFlight);

  return std::max (2 * state->m_segmentSize, bytesInFlight / 2);
}

Ptr<TcpCongestionOps>
TcpNewReno::Fork ()
{
  return CopyObject<TcpNewReno> (this);
}

} // namespace ns3

