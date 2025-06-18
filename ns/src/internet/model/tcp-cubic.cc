/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014 Natale Patriciello <natale.patriciello@gmail.com>
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

#define NS_LOG_APPEND_CONTEXT \
  { std::clog << Simulator::Now ().GetSeconds () << " "; }

#include "tcp-cubic.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include <iostream>
#include <fstream>


NS_LOG_COMPONENT_DEFINE ("TcpCubic");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (TcpCubic);

TypeId
TcpCubic::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpCubic")
    .SetParent<TcpSocketBase> ()
    .AddConstructor<TcpCubic> ()
    .SetGroupName ("Internet")
    .AddAttribute ("FastConvergence", "Enable (true) or disable (false) fast convergence",
                   BooleanValue (true),
                   MakeBooleanAccessor (&TcpCubic::m_fastConvergence),
                   MakeBooleanChecker ())
    .AddAttribute ("Beta", "Beta for multiplicative decrease",
                   DoubleValue (0.7),
                   MakeDoubleAccessor (&TcpCubic::m_beta),
                   MakeDoubleChecker <double> (0.0))
    .AddAttribute ("HyStart", "Enable (true) or disable (false) hybrid slow start algorithm",
                   BooleanValue (true),
                   MakeBooleanAccessor (&TcpCubic::m_hystart),
                   MakeBooleanChecker ())
    .AddAttribute ("HyStartLowWindow", "Lower bound cWnd for hybrid slow start (segments)",
                   UintegerValue (16),
                   MakeUintegerAccessor (&TcpCubic::m_hystartLowWindow),
                   MakeUintegerChecker <uint32_t> ())
    .AddAttribute ("HyStartDetect", "Hybrid Slow Start detection mechanisms:" \
                   "1: packet train, 2: delay, 3: both",
                   IntegerValue (3),
                   MakeIntegerAccessor (&TcpCubic::m_hystartDetect),
                   MakeIntegerChecker <int> (1,3))
    .AddAttribute ("HyStartMinSamples", "Number of delay samples for detecting the increase of delay",
                   UintegerValue (8),
                   MakeUintegerAccessor (&TcpCubic::m_hystartMinSamples),
                   MakeUintegerChecker <uint8_t> ())
    .AddAttribute ("HyStartAckDelta", "Spacing between ack's indicating train",
                   TimeValue (MilliSeconds (2)),
                   MakeTimeAccessor (&TcpCubic::m_hystartAckDelta),
                   MakeTimeChecker ())
    .AddAttribute ("HyStartDelayMin", "Minimum time for hystart algorithm",
                   TimeValue (MilliSeconds (4)),
                   MakeTimeAccessor (&TcpCubic::m_hystartDelayMin),
                   MakeTimeChecker ())
    .AddAttribute ("HyStartDelayMax", "Maximum time for hystart algorithm",
                   TimeValue (MilliSeconds (1000)),
                   MakeTimeAccessor (&TcpCubic::m_hystartDelayMax),
                   MakeTimeChecker ())
    .AddAttribute ("CubicDelta", "Delta Time to wait after fast recovery before adjusting param",
                   TimeValue (MilliSeconds (10)),
                   MakeTimeAccessor (&TcpCubic::m_cubicDelta),
                   MakeTimeChecker ())
    .AddAttribute ("CntClamp", "Counter value when no losses are detected (counter is used" \
                   " when incrementing cWnd in congestion avoidance, to avoid" \
                   " floating point arithmetic). It is the modulo of the (avoided)" \
                   " division",
                   UintegerValue (20),
                   MakeUintegerAccessor (&TcpCubic::m_cntClamp),
                   MakeUintegerChecker <uint8_t> ())
    .AddAttribute ("C", "Cubic Scaling factor",
                   DoubleValue (0.4),
                   MakeDoubleAccessor (&TcpCubic::m_c),
                   MakeDoubleChecker <double> (0.0))
  ;
  return tid;
}

TcpCubic::TcpCubic ()
  : TcpCongestionOps (),
    m_cWndCnt (0),
    m_lastMaxCwnd (0),
    m_bicOriginPoint (0),
    m_bicK (0.0),
    m_delayMin (Time::Min ()),
    m_epochStart (Time::Min ()),
    m_found (false),
    m_roundStart (Time::Min ()),
    m_endSeq (0),
    m_lastAck (Time::Min ()),
    m_cubicDelta (Time::Min ()),
    m_currRtt (Time::Min ()),
    m_sampleCnt (0),
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

TcpCubic::TcpCubic (const TcpCubic &sock)
  : TcpCongestionOps (sock),
    m_fastConvergence (sock.m_fastConvergence),
    m_beta (sock.m_beta),
    m_hystart (sock.m_hystart),
    m_hystartDetect (sock.m_hystartDetect),
    m_hystartLowWindow (sock.m_hystartLowWindow),
    m_hystartAckDelta (sock.m_hystartAckDelta),
    m_hystartDelayMin (sock.m_hystartDelayMin),
    m_hystartDelayMax (sock.m_hystartDelayMax),
    m_hystartMinSamples (sock.m_hystartMinSamples),
    m_initialCwnd (sock.m_initialCwnd),
    m_cntClamp (sock.m_cntClamp),
    m_c (sock.m_c),
    m_cWndCnt (sock.m_cWndCnt),
    m_lastMaxCwnd (sock.m_lastMaxCwnd),
    m_bicOriginPoint (sock.m_bicOriginPoint),
    m_bicK (sock.m_bicK),
    m_delayMin (sock.m_delayMin),
    m_epochStart (sock.m_epochStart),
    m_found (sock.m_found),
    m_roundStart (sock.m_roundStart),
    m_endSeq (sock.m_endSeq),
    m_lastAck (sock.m_lastAck),
    m_cubicDelta (sock.m_cubicDelta),
    m_currRtt (sock.m_currRtt),
    m_sampleCnt (sock.m_sampleCnt),
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

std::string
TcpCubic::GetName () const
{
  return "TcpCubic";
}

void
TcpCubic::HystartReset (Ptr<const TcpSocketState> tcb)
{
  NS_LOG_FUNCTION (this);

  m_roundStart = m_lastAck = Simulator::Now ();
  m_endSeq = tcb->m_highTxMark;
  m_currRtt = Time::Min ();
  m_sampleCnt = 0;
}


void
TcpCubic::IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);

//   uint32_t prevWindow = tcb->m_cWnd;
  uint32_t newWindow;
  uint32_t originalWindow = tcb->m_cWnd;
  double tax;
  double totalTax;
  // double taxRateAdjusted=1;
  Time now;

  if (tcb->m_cWnd < tcb->m_ssThresh)
    {

      if (m_hystart && tcb->m_lastAckedSeq > m_endSeq)
        {
          HystartReset (tcb);
        }

      // In Linux, the QUICKACK socket option enables the receiver to send
      // immediate acks initially (during slow start) and then transition
      // to delayed acks.  ns-3 does not implement QUICKACK, and if ack
      // counting instead of byte counting is used during slow start window
      // growth, when TcpSocket::DelAckCount==2, then the slow start will
      // not reach as large of an initial window as in Linux.  Therefore,
      // we can approximate the effect of QUICKACK by making this slow
      // start phase perform Appropriate Byte Counting (RFC 3465)
      tcb->m_cWnd += segmentsAcked * tcb->m_segmentSize;
      segmentsAcked = 0;

      NS_LOG_INFO ("In SlowStart, updated to cwnd " << tcb->m_cWnd <<
                   " ssthresh " << tcb->m_ssThresh);
    }

  if (tcb->m_cWnd >= tcb->m_ssThresh && segmentsAcked > 0)
    {
      m_cWndCnt += segmentsAcked;
      uint32_t cnt = Update (tcb);

      /* According to RFC 6356 even once the new cwnd is
       * calculated you must compare this to the number of ACKs received since
       * the last cwnd update. If not enough ACKs have been received then cwnd
       * cannot be updated.
       */
      if (m_cWndCnt >= cnt)
        {
          /* REPLACING THIS LINE (SETTING CONGESTION WINDOW SIZE) WITH THE FAIRNESS TAX BELOW! 
          CONGESTION WINDOW SIZE IS NOW SET INSIDE EACH OPTION
          */
          tcb->m_cWnd += tcb->m_segmentSize;
          m_cWndCnt -= cnt;
          NS_LOG_INFO ("In CongAvoid, updated to cwnd " << tcb->m_cWnd);
        }
      else
        {
          NS_LOG_INFO ("Not enough segments have been ACKed to increment cwnd."
                       "Until now " << m_cWndCnt << " cnd " << cnt);
        }
    }

                 /* Update window to implement Fairness Tax */
          // First check if congestion encountered recently
          // if ((Simulator::Now() - rttLogTime[qIndex]).GetInteger() > congestionTimescale) {
          //   congestionEncounteredRecently = false;
          // }

          congestionEncounteredRecently = false;
          
          if (congestionEncounteredRecently == true) {
            newWindow = tcb->m_cWnd;            

            // Option 1: Similar to Cebinae
            tax = 0.02;
            totalTax = tax * newWindow;
            originalWindow = originalWindow;

            tcb->m_cWnd = newWindow - totalTax;

            // Option 2A
            // tax = 1000.0 / recentRtt.GetMicroSeconds();
            // totalTax = tax * newWindow;
            // originalWindow = originalWindow;

            // if (totalTax / newWindow > 0.05) {
            //   totalTax = 0.05 * newWindow;
            // }

            // tcb->m_cWnd = newWindow - totalTax;

            // Option 2B
            // tax = 10000.0 * newWindow / (recentRtt.GetMicroSeconds() * recentRtt.GetMicroSeconds());
            // totalTax = tax * newWindow;

            // if (totalTax / newWindow > 0.05) {
            //   totalTax = 0.05 * newWindow;
            // }

            // tcb->m_cWnd = newWindow - totalTax;
            // originalWindow = originalWindow;

            // Option 3
            // tax = 4.0;
            // totalTax = tax * (newWindow - originalWindow);

            // if (totalTax / newWindow > 0.05) {
            //   totalTax = 0.05 * newWindow;
            // }

            // tcb->m_cWnd = newWindow - totalTax;


            // Option 4
            // tax = 5000.0 / recentRtt.GetMicroSeconds();
            // totalTax = tax * (newWindow-originalWindow);

            // if (totalTax / newWindow > 0.05) {
            //   totalTax = 0.05 * newWindow;
            // }

            // tcb->m_cWnd = newWindow - totalTax;


            // Option 5B
            // tax =  10000.0 * (newWindow-originalWindow) / (recentRtt.GetMicroSeconds() * recentRtt.GetMicroSeconds());
            // totalTax = tax * (newWindow - originalWindow);
            
            // if (totalTax / newWindow > 0.05) {
            //   totalTax = 0.05 * newWindow;
            // }

            // tcb->m_cWnd = newWindow - totalTax;

            
            // Option 5C
            // tax =  50000.0 * (newWindow-originalWindow) / (recentRtt.GetMicroSeconds() * recentRtt.GetMicroSeconds());
            // totalTax = tax * (newWindow - originalWindow) * (newWindow - originalWindow);
            
            // if (totalTax / newWindow > 0.05) {
            //   totalTax = 0.05 * newWindow;
            // }

            // tcb->m_cWnd = newWindow - totalTax;

            
            // Option 6
            // tax = 0.023;
            // totalTax = tax * newWindow;
            
            // if (newWindow > originalWindow) {
            //   tcb->m_cWnd = newWindow - totalTax;
            // }


            // Option 7
            // tax = 1000.0 / recentRtt.GetMicroSeconds();
            // totalTax = tax * newWindow;

            // if (totalTax / newWindow > 0.05) {
            //   totalTax = 0.05 * newWindow;
            // }

            // if (newWindow > originalWindow) {
            //   tcb->m_cWnd = newWindow - totalTax;
            // }


            // Option 8
            // tax = 1.0;
            // totalTax = tax * (newWindow - originalWindow);

            // if (totalTax / newWindow > 0.05) {
            //   totalTax = 0.05 * newWindow;
            // }
            
            // if (newWindow > originalWindow) {
            //   tcb->m_cWnd = newWindow - totalTax;
            // }

            
            // Option 9
            // tax = 30000.0 / recentRtt.GetMicroSeconds();
            // totalTax = tax * (newWindow-originalWindow);

            // if (totalTax / newWindow > 0.05) {
            //   totalTax = 0.05 * newWindow;
            // }

            // if (newWindow > originalWindow) {
            //   tcb->m_cWnd = newWindow - totalTax;
            // }


            // Option 10B
            // tax =  10000000.0 * (newWindow-originalWindow) / (recentRtt.GetMicroSeconds() * recentRtt.GetMicroSeconds());
            // totalTax = tax * (newWindow - originalWindow);
            
            // if (totalTax / newWindow > 0.05) {
            //   totalTax = 0.05 * newWindow;
            // }

            // if (newWindow > originalWindow) {
            //   tcb->m_cWnd = newWindow - totalTax;
            // }

            now = Simulator::Now ();

            congCount++;

            if (congCount % 100 == 0) {
              // std::cout << "time: " << now.GetSeconds()  << std::endl;
              // std::cout << "time: " << now.GetSeconds() << " congCount: " << congCount << " congNotCount: " << congNotCount << " ratio: " << (congNotCount*1.0/congCount) << " lastAckedSeq: " << tcb->m_lastAckedSeq << " tax: " << (taxRate*taxRateAdjusted) << std::endl;
            }
          }
          else {
            congNotCount++;
          }
  
}

uint32_t
TcpCubic::Update (Ptr<TcpSocketState> tcb)
{
  NS_LOG_FUNCTION (this);
  Time t;
  uint32_t delta, bicTarget, cnt = 0;
  double offs;
  uint32_t segCwnd = tcb->GetCwndInSegments ();

  if (m_epochStart == Time::Min ())
    {
      m_epochStart = Simulator::Now ();   // record the beginning of an epoch

      if (m_lastMaxCwnd <= segCwnd)
        {
          NS_LOG_DEBUG ("lastMaxCwnd <= m_cWnd. K=0 and origin=" << segCwnd);
          m_bicK = 0.0;
          m_bicOriginPoint = segCwnd;
        }
      else
        {
          m_bicK = std::pow ((m_lastMaxCwnd - segCwnd) / m_c, 1 / 3.);
          m_bicOriginPoint = m_lastMaxCwnd;
          NS_LOG_DEBUG ("lastMaxCwnd > m_cWnd. K=" << m_bicK <<
                        " and origin=" << m_lastMaxCwnd);
        }
    }

  t = Simulator::Now () + m_delayMin - m_epochStart;

  if (t.GetSeconds () < m_bicK)       /* t - K */
    {
      offs = m_bicK - t.GetSeconds ();
      NS_LOG_DEBUG ("t=" << t.GetSeconds () << " <k: offs=" << offs);
    }
  else
    {
      offs = t.GetSeconds () - m_bicK;
      NS_LOG_DEBUG ("t=" << t.GetSeconds () << " >= k: offs=" << offs);
    }


  /* Constant value taken from Experimental Evaluation of Cubic Tcp, available at
   * eprints.nuim.ie/1716/1/Hamiltonpfldnet2007_cubic_final.pdf */
  delta = m_c * std::pow (offs, 3);

  NS_LOG_DEBUG ("delta: " << delta);

  if (t.GetSeconds () < m_bicK)
    {
      // below origin
      bicTarget = m_bicOriginPoint - delta;
      NS_LOG_DEBUG ("t < k: Bic Target: " << bicTarget);
    }
  else
    {
      // above origin
      bicTarget = m_bicOriginPoint + delta;
      NS_LOG_DEBUG ("t >= k: Bic Target: " << bicTarget);
    }

  // Next the window target is converted into a cnt or count value. CUBIC will
  // wait until enough new ACKs have arrived that a counter meets or exceeds
  // this cnt value. This is how the CUBIC implementation simulates growing
  // cwnd by values other than 1 segment size.
  if (bicTarget > segCwnd)
    {
      cnt = segCwnd / (bicTarget - segCwnd);
      NS_LOG_DEBUG ("target>cwnd. cnt=" << cnt);
    }
  else
    {
      cnt = 100 * segCwnd;
    }

  if (m_lastMaxCwnd == 0 && cnt > m_cntClamp)
    {
      cnt = m_cntClamp;
    }

  // The maximum rate of cwnd increase CUBIC allows is 1 packet per
  // 2 packets ACKed, meaning cwnd grows at 1.5x per RTT.
  return std::max (cnt, 2U);
}

void
TcpCubic::PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked,
                     const Time &rtt)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked << rtt);

  Time now;
  Time earliestRTTtimestamp;
  Time earliestRTT;


  /* Discard delay samples right after fast recovery */
  if (m_epochStart != Time::Min ()
      && (Simulator::Now () - m_epochStart) < m_cubicDelta)
    {
      return;
    }

  /* first time call or link delay decreases */
  if (m_delayMin == Time::Min () || m_delayMin > rtt)
    {
      m_delayMin = rtt;
    }

  /* hystart triggers when cwnd is larger than some threshold */
  if (m_hystart
      && tcb->m_cWnd <= tcb->m_ssThresh
      && tcb->m_cWnd >= m_hystartLowWindow * tcb->m_segmentSize)
    {
      HystartUpdate (tcb, rtt);
    }

  if (rtt.IsZero ())
    {
      return;
    }

  // Do all of the following for implementing fairness tax
  now = Simulator::Now ();
  // std::cout << "FAIRNESS TAX: time difference: " << (now - rttLogTime[qIndex]).GetInteger() << std::endl;

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

void
TcpCubic::HystartUpdate (Ptr<TcpSocketState> tcb, const Time& delay)
{
  NS_LOG_FUNCTION (this << delay);

  if (!(m_found & m_hystartDetect))
    {
      Time now = Simulator::Now ();

      /* first detection parameter - ack-train detection */
      if ((now - m_lastAck) <= m_hystartAckDelta)
        {
          m_lastAck = now;

          if ((now - m_roundStart) > m_delayMin)
            {
              m_found |= PACKET_TRAIN;
            }
        }

      /* obtain the minimum delay of more than sampling packets */
      if (m_sampleCnt < m_hystartMinSamples)
        {
          if (m_currRtt == Time::Min () || m_currRtt > delay)
            {
              m_currRtt = delay;
            }

          ++m_sampleCnt;
        }
      else
        {
          if (m_currRtt > m_delayMin +
              HystartDelayThresh (m_delayMin))
            {
              m_found |= DELAY;
            }
        }
      /*
       * Either one of two conditions are met,
       * we exit from slow start immediately.
       */
      if (m_found & m_hystartDetect)
        {
          NS_LOG_DEBUG ("Exit from SS, immediately :-)");
          tcb->m_ssThresh = tcb->m_cWnd;
        }
    }
}

Time
TcpCubic::HystartDelayThresh (const Time& t) const
{
  NS_LOG_FUNCTION (this << t);

  Time ret = t;
  if (t > m_hystartDelayMax)
    {
      ret = m_hystartDelayMax;
    }
  else if (t < m_hystartDelayMin)
    {
      ret = m_hystartDelayMin;
    }

  return ret;
}

uint32_t
TcpCubic::GetSsThresh (Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight)
{
  NS_LOG_FUNCTION (this << tcb << bytesInFlight);

  uint32_t segCwnd = tcb->GetCwndInSegments ();
  NS_LOG_DEBUG ("Loss at cWnd=" << segCwnd << " segments in flight=" << bytesInFlight / tcb->m_segmentSize);

  /* Wmax and fast convergence */
  if (segCwnd < m_lastMaxCwnd && m_fastConvergence)
    {
      m_lastMaxCwnd = (segCwnd * (1 + m_beta)) / 2; // Section 4.6 in RFC 8312
    }
  else
    {
      m_lastMaxCwnd = segCwnd;
    }

  m_epochStart = Time::Min ();    // end of epoch

  /* Formula taken from the Linux kernel */
  uint32_t ssThresh = std::max (static_cast<uint32_t> (segCwnd * m_beta ), 2U) * tcb->m_segmentSize;

  NS_LOG_DEBUG ("SsThresh = " << ssThresh);

  return ssThresh;
}

void
TcpCubic::CongestionStateSet (Ptr<TcpSocketState> tcb, const TcpSocketState::TcpCongState_t newState)
{
  NS_LOG_FUNCTION (this << tcb << newState);
  Time now;

  if (newState == TcpSocketState::CA_LOSS)
    {
      CubicReset (tcb);
      HystartReset (tcb);
    }

  if ((newState == TcpSocketState::CA_RECOVERY) || (newState == TcpSocketState::CA_LOSS))
    {
   	now = Simulator::Now ();
  	std::cout << (now).GetInteger() << tcb->m_lastAckedSackedBytes << " packet lost" << std::endl;
    }
}

void
TcpCubic::CubicReset (Ptr<const TcpSocketState> tcb)
{
  NS_LOG_FUNCTION (this << tcb);

  m_lastMaxCwnd = 0;
  m_bicOriginPoint = 0;
  m_bicK = 0;
  m_delayMin = Time::Min ();
  m_found = false;
}

Ptr<TcpCongestionOps>
TcpCubic::Fork (void)
{
  NS_LOG_FUNCTION (this);
  return CopyObject<TcpCubic> (this);
}

}