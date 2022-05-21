#include <iomanip>

#include "cebinae-queue-disc.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/log.h"
#include "ns3/object-factory.h"
#include "ns3/simulator.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("CebinaeQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (CebinaeQueueDisc);

TypeId CebinaeQueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::CebinaeQueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName ("TrafficControl")
    .AddConstructor<CebinaeQueueDisc> ()
    .AddAttribute ("MaxSize",
                   "The max queue size",
                   QueueSizeValue (QueueSize ("1000p")),
                   MakeQueueSizeAccessor (&QueueDisc::SetMaxSize,
                                          &QueueDisc::GetMaxSize),
                   MakeQueueSizeChecker ())                
    .AddAttribute ("dT",
                   "dT",
                   TimeValue (NanoSeconds(1048576)),
                   MakeTimeAccessor (&CebinaeQueueDisc::m_dt),
                   MakeTimeChecker())
    .AddAttribute ("vdT",
                   "vdT",
                   TimeValue (NanoSeconds(1024)),
                   MakeTimeAccessor (&CebinaeQueueDisc::m_vdt),
                   MakeTimeChecker())
    .AddAttribute ("L",
                   "m_l",
                   TimeValue (NanoSeconds(65536)),
                   MakeTimeAccessor (&CebinaeQueueDisc::m_l),
                   MakeTimeChecker())                   
    .AddAttribute ("P",
                   "P",
                   UintegerValue (1),
                   MakeUintegerAccessor (&CebinaeQueueDisc::m_p),
                   MakeUintegerChecker<uint32_t> ())                   
    .AddAttribute ("tau",
                   "tau",
                   DoubleValue (0.05),
                   MakeDoubleAccessor (&CebinaeQueueDisc::m_tau),
                   MakeDoubleChecker <double> ())
    .AddAttribute ("delta_port",
                   "delta",
                   DoubleValue (0.05),
                   MakeDoubleAccessor (&CebinaeQueueDisc::m_delta_p),
                   MakeDoubleChecker <double> ())
    .AddAttribute ("delta_flow",
                   "delta",
                   DoubleValue (0.05),
                   MakeDoubleAccessor (&CebinaeQueueDisc::m_delta_f),
                   MakeDoubleChecker <double> ())                   
    .AddAttribute ("DataRate", 
                   "The data rate for the attached NetDevice",
                   DataRateValue (DataRate ("32768b/s")),
                   MakeDataRateAccessor (&CebinaeQueueDisc::m_bps),
                   MakeDataRateChecker ())                   
  ;
  return tid;
}

CebinaeQueueDisc::CebinaeQueueDisc ()
  : QueueDisc (QueueDiscSizePolicy::MULTIPLE_QUEUES)
  // QueueDisc (QueueDiscSizePolicy::SINGLE_INTERNAL_QUEUE)
  // QueueDisc (QueueDiscSizePolicy::NO_LIMITS)
{
  NS_LOG_DEBUG ("Trigger reaction event chain");
  Simulator::Schedule(Seconds(0), &CebinaeQueueDisc::ReactionFSM, this);
}

CebinaeQueueDisc::~CebinaeQueueDisc ()
{
}

void CebinaeQueueDisc::ReactionFSM() {

  NS_LOG_DEBUG ("[" << ns3::Simulator::Now ().GetNanoSeconds() << "]" << CebinaeStateString[m_state]);

  // Single event edge from a state to another, no need to check for event types

  if (m_state == INIT) {
    // Wall clock time origin
    // In hardware implementation, the first rotate packet arriving at ingress pipe sets this virtual origin backwards

    // Stale state issue if put the initialization below to constructor
    m_vb = m_dt.GetNanoSeconds()/m_vdt.GetNanoSeconds();

    m_lbf_bps_top.resize(2);
    m_lbf_bps_top.push_back(0);
    m_lbf_bps_top.push_back(0);
    m_lbf_bps_bot.resize(2);
    m_lbf_bps_bot.push_back(0);
    m_lbf_bps_bot.push_back(0);

    // Initialize rates (unless need to guaratee that traffic arrives long enough after 2P that auto-configures it)
    // Not an issue in hardware as we synthesize traffic long after initialization
    m_lbf_bps_bot[m_headq] = m_bps.GetBitRate();
    m_lbf_bps_bot[m_neg_headq] = m_bps.GetBitRate();    

    m_computed_bps_bot = m_bps.GetBitRate();

    m_enqueue_drop_pkts.resize(2);
    m_enqueue_drop_pkts.push_back(0);
    m_enqueue_drop_pkts.push_back(0);

    m_last_rate_top = 0;
    m_last_rate_bot = m_bps.GetBitRate();

    // FSM moves to ROTATE directly, skipping the RECONFIG during first ROTATE round
    m_high_prio_queue = m_neg_headq;  
    m_recomputation_ctr += 1;

    m_oss_summary << "--- Validate the effective CebinaeQueueDisc params ---\n"
              << "m_maxSize: " << GetMaxSize() << "\n"
              << "GetInternalQueue(0)->GetMaxSize(): " << GetInternalQueue(0)->GetMaxSize() << "\n"
              << "GetInternalQueue(1)->GetMaxSize(): " << GetInternalQueue(1)->GetMaxSize() << "\n"
              << "dT: " << m_dt << "\n"
              << "vdT: " << m_vdt << "\n"
              << "L: " << m_l << "\n"
              << "P: " << m_p << "\n"
              << "tau: " << m_tau << "\n"
              << "delta_port: " << m_delta_p << "\n"
              << "delta_top: " << m_delta_f << "\n"
              << "m_bps: " << m_bps << "\n";

    m_oss_summary << "--- Validate CebinaeQueueDisc initial states ---\n"
              << "m_headq: " << m_headq << "\n"
              << "m_neg_headq: " << m_neg_headq << "\n"
              << "m_vb: " << m_vb << "\n"
              << "------\n";

    // Removed in optimized builds
    NS_ASSERT (m_dt > m_l);
    NS_ASSERT (m_dt - m_vdt > m_l);
    NS_ASSERT (m_p >= 1);
    NS_ASSERT (m_delta_p <= 1);
    NS_ASSERT (m_delta_f <= 1);
    NS_ASSERT (m_tau <= 1);
    NS_ASSERT (m_vb == (m_dt.GetNanoSeconds()/m_vdt.GetNanoSeconds()));
    NS_ASSERT (GetInternalQueue(0)->GetMaxSize().GetUnit() == GetMaxSize().GetUnit());
    NS_ASSERT (GetInternalQueue(1)->GetMaxSize().GetUnit() == GetMaxSize().GetUnit());    
    NS_ASSERT ((GetInternalQueue(0)->GetMaxSize().GetValue()+GetInternalQueue(1)->GetMaxSize().GetValue()) == GetMaxSize().GetValue());

    NS_LOG_DEBUG("Advances to the time point of first ROTATE packet");
    m_state = ROTATE;
    // Stepping back by m_vdt+m_l, additional m_dt due to increment upon ROTATE
    Simulator::Schedule(m_dt+m_vdt+m_l, &CebinaeQueueDisc::ReactionFSM, this);

  } else if (m_state == ROTATE) {

    // --- Control plane operations upon ROTATE packet detection during busy polling (headq != last_headq) ---
    // Implicit busy_sleep(m_dt-m_L) in event schedule

    // --- Data plane operations upon ROTATE packets ---
    // Update bytes count for top and bot, taking saturated subtraction of last dT bytes budget (rate*dT)
    uint32_t budget_top = m_last_rate_top*m_dt.GetSeconds()/8;
    if (m_bytes_top > budget_top) {
      m_bytes_top = m_bytes_top - budget_top;
    } else {
      m_bytes_top = 0;
    }
    uint32_t budget_bot = m_last_rate_bot*m_dt.GetSeconds()/8;
    if (m_bytes_bot > budget_bot) {
      m_bytes_bot = m_bytes_bot - budget_bot;
    } else {
      m_bytes_bot = 0;
    }

    m_base_round_time += m_dt;

    // Flip headq and neg_headq
    m_headq = 1 - m_headq;
    m_neg_headq = 1 - m_neg_headq;

    // No need to drop the virual ROTATE packet in simulation

    // --- Update FSM ---
    m_state = RECONFIG;
    m_num_rotated += 1;
    Simulator::Schedule(m_dt-m_l, &CebinaeQueueDisc::ReactionFSM, this);

  }  else if (m_state == RECONFIG) {

    // headq is empty at RECONFIG

    // In hardware, DP does nothing and CP prepares shadow copies, and DP atomically use the copies upon ROTATE
    // If L is 0, in simulation, equivalent to have in-place replacement of primary copy directly at ROTATE time point
    // Hence, do NULL during RECONFIG phase; here faithfully emulate the delay between rate computation and rotate trigger

    m_high_prio_queue = m_neg_headq;
    NS_LOG_DEBUG ("[" << ns3::Simulator::Now ().GetNanoSeconds() << "]" << "m_high_prio_queue: " << m_high_prio_queue);

    m_recomputation_ctr += 1;  // Pre-increment
    // Recomputes rates every P rounds
    if (m_recomputation_ctr % m_p == 0) {
      m_num_p += 1;
      m_recomputation_ctr = 0;

      // Expected threshold bits during last window (discounted full utilization worth of bits)
      uint64_t threshold_bits = m_bps.GetBitRate()*(m_p*m_dt.GetSeconds())*(1-m_delta_p);

      if (m_port_bytecounts*8 > threshold_bits) {

        NS_LOG_DEBUG("Port is saturated");
        m_num_bottleneck_p += 1;

        auto ret = m_fbd.GetTopFlows(m_delta_f);
        m_bottlenecked_flows_set = ret.first;
        // std::cout << "[------" << std::endl;
        // for (auto tag : m_bottlenecked_flows_set) {
        //   std::cout << tag << std::endl;
        // }
        // std::cout << "------]" << std::endl;
        uint32_t bottleneck_bytes = ret.second;

        // Calculate after taxing
        bottleneck_bytes = bottleneck_bytes * (1-m_tau);
        // std::cout << "bottleneck_bytes: " << bottleneck_bytes << std::endl;

        // Now configure the rates for the physical queue pointed by headq, rather than neg_headq
        m_computed_bps_top = bottleneck_bytes/m_p/m_dt.GetSeconds()*8;
        m_computed_bps_bot = m_bps.GetBitRate()-m_computed_bps_top;
      } else {
        m_num_non_bottleneck_p += 1;
        NS_LOG_DEBUG("Port not bottlenecked, no flows will be considered top in the next periods");
        m_bottlenecked_flows_set.clear();
        // uint64_t DataRate::GetBitRate ()
        // Prepare the rates for the physical queue pointed by headq (which is flipped immediately below)
        m_computed_bps_top = 0;
        m_computed_bps_bot = m_bps.GetBitRate();
      }
      // Flush flow bottleneck monitor (not only during saturated state)
      m_fbd.FlushCache();
      // Flush the bytes per examination, the alternative is to remember last byte count without flush
      m_port_bytecounts = 0;
    }
    
    // Save last rate for data plane reset
    m_last_rate_top = m_lbf_bps_top[m_headq];
    m_last_rate_bot = m_lbf_bps_bot[m_headq];

    // Sets rate every round to prevent flip between oldrate and newrate for next P rounds
    m_lbf_bps_top[m_headq] = m_computed_bps_top;
    m_lbf_bps_bot[m_headq] = m_computed_bps_bot;

    // DEBUG the traces of configured rates and cardinality of bot flow set
    // std::cout << "--- m_num_rotated: " << m_num_rotated << " ---\n"
    //           << "headq: " << m_headq << ", neg_headq: " << m_neg_headq << "\n"
    //           << "m_lbf_bps_top[0]: " << m_lbf_bps_top[0] << ", m_lbf_bps_bot[0]: " << m_lbf_bps_bot[0] << "\n"
    //           << "m_lbf_bps_top[1]: " << m_lbf_bps_top[1] << ", m_lbf_bps_bot[1]: " << m_lbf_bps_bot[1] << "\n"
    //           << "m_bottlenecked_flows_set.size(): " << m_bottlenecked_flows_set.size() << "\n"
    //           << std::endl;

    m_state = ROTATE;
    Simulator::Schedule(m_l, &CebinaeQueueDisc::ReactionFSM, this);
  }
}

bool
CebinaeQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{

  // Process NORMAL packet; note that the processing of ROTATE is embedded in Reaction FSM
  // Regardless of whether the port is saturated or not

  // First check if the packet belongs to top or not
  MySourceIDTag tag;
  item->GetPacket()->FindFirstMatchingByteTag(tag);
  auto got = std::find(m_bottlenecked_flows_set.begin(), m_bottlenecked_flows_set.end(), tag.Get());
  bool is_top = false;
  if (got != m_bottlenecked_flows_set.end()) {
    is_top = true;
  }
  // std::cout << "is_top: " << is_top << std::endl;
  
  // Update round time
  if (Simulator::Now() - m_round_time >= m_vdt) {
    m_round_time = Simulator::Now();
  }
  // Calculate relative round
  uint32_t relative_round = (m_round_time.GetNanoSeconds() - m_base_round_time.GetNanoSeconds())/m_vdt.GetNanoSeconds();

  NS_ASSERT_MSG (relative_round < 2*m_vb, "We've missed a deadline!");
  
  // Now use relative_round to calculate aggregate_size
  uint32_t aggregate_size = 0;
  if (is_top) {
    if (relative_round < m_vb) {
      // relative_round*m_vdt.GetNanoSeconds() is the same as m_round_time.GetNanoSeconds() - m_base_round_time.GetNanoSeconds()
      aggregate_size = m_lbf_bps_top[m_headq]*(m_round_time.GetSeconds() - m_base_round_time.GetSeconds())/8;
    } else if (relative_round < 2*m_vb) {
      aggregate_size = m_lbf_bps_top[m_headq]*m_dt.GetSeconds()/8 + 
                        m_lbf_bps_top[m_neg_headq]*(m_round_time.GetSeconds()-m_base_round_time.GetSeconds()-m_dt.GetSeconds())/8;
    } else {
      std::cout << "relative_round >= 2*m_vb!" << std::endl;
    }
  }
  else {
    if (relative_round < m_vb) {
      aggregate_size = m_lbf_bps_bot[m_headq]*(m_round_time.GetSeconds() - m_base_round_time.GetSeconds())/8;
    } else if (relative_round < 2*m_vb) {
      // (relative_round-m_vb)*m_vdt.GetSeconds() equivalent to m_round_time.GetSeconds()-m_base_round_time.GetSeconds()-m_dt.GetSeconds()
      aggregate_size = m_lbf_bps_bot[m_headq]*m_dt.GetSeconds()/8 + 
                        m_lbf_bps_bot[m_neg_headq]*(m_round_time.GetSeconds()-m_base_round_time.GetSeconds()-m_dt.GetSeconds())/8;
    } else {
      std::cout << "relative_round >= 2*m_vb!" << std::endl;
    }
  }

  // Now calculate the number of bytes passed
  uint32_t past_head = 0;
  uint32_t past_tail = 0;
  uint32_t budget_headq = 0;
  uint32_t budget_neg_headq = 0;
  if (is_top) {
    budget_headq = m_lbf_bps_top[m_headq]*m_dt.GetSeconds()/8;
    budget_neg_headq = m_lbf_bps_top[m_neg_headq]*m_dt.GetSeconds()/8;
    if (m_bytes_top > budget_headq) {
      past_head = m_bytes_top - budget_headq;
      if (past_head > budget_neg_headq) {
        past_tail = past_head - budget_neg_headq;
      }
    }
    // Note that we update bytes later because register processing is delayed by 1 packet
    if (m_bytes_top < aggregate_size) {
      m_bytes_top = aggregate_size;
    }    
    m_bytes_top += item->GetSize();
  } else {
    budget_headq = m_lbf_bps_bot[m_headq]*m_dt.GetSeconds()/8;
    budget_neg_headq = m_lbf_bps_bot[m_neg_headq]*m_dt.GetSeconds()/8;
    if (m_bytes_bot > budget_headq) {
      past_head = m_bytes_bot - budget_headq;
      if (past_head > budget_neg_headq) {
        past_tail = past_head - budget_neg_headq;
      }
    }
    // Note that we update bytes later because register processing is delayed by 1 packet
    if (m_bytes_bot < aggregate_size) {
      m_bytes_bot = aggregate_size;
    }    
    m_bytes_bot += item->GetSize();  
  }

  // TODO Optional ECN bits marking (maybe in appendix)
  // Now execute the queueing decision
  bool retval = false;  // whether the packet succeeds enqueue
  // bool headq_drop = false;
  // bool neg_headq_drop = false;
  if (past_head == 0) {
    m_lbf_past_head_pkts += 1;
    // enqueue(headq)
    if (GetInternalQueue (m_headq)->GetCurrentSize() + item > GetInternalQueue (m_headq)->GetMaxSize ()) {
      // Drop because headq is full
      DropBeforeEnqueue (item, LIMIT_EXCEEDED_DROP);
      m_enqueue_drop_pkts[m_headq] += 1;
      // headq_drop = true;
    } else {
      retval = GetInternalQueue (m_headq)->Enqueue (item);
      NS_ASSERT(retval == true);
    }
  } 
  else if (past_tail == 0) {
    m_lbf_past_tail_pkts += 1;
    // enqueue(neg_headq)
    if (GetInternalQueue (m_neg_headq)->GetCurrentSize() + item > GetInternalQueue (m_neg_headq)->GetMaxSize ()) {
      // Drop because neg_headq is full
      DropBeforeEnqueue (item, LIMIT_EXCEEDED_DROP);
      m_enqueue_drop_pkts[m_neg_headq] += 1;
      // neg_headq_drop = true;
    } else {
      retval = GetInternalQueue (m_neg_headq)->Enqueue (item);
      NS_ASSERT(retval == true); 
    }
  } 
  else {
    // Drop because of LBF policy
    m_lbf_drop_pkts += 1;
    DropBeforeEnqueue (item, LIMIT_EXCEEDED_DROP);
    // std::cout << "m_lbf_drop_pkts/m_arrival_pkts: " << std::to_string(m_lbf_drop_pkts*1.0/m_arrival_pkts) << std::endl;
  }

  // If dropped, reaccounting the bytes 
  // TODO In hardware, it is hard to re-account the bytes due to register constraint
  if (!retval) {
    if (is_top) {
      m_bytes_top -= item->GetSize();
    } else {
      m_bytes_bot -= item->GetSize();
    }
  }

  // Only print dropped packets
  // if (!retval) {
  //   std::cout << "--- m_arrived_pkts (0-index): " << m_arrived_pkts << " ---\n"
  //           << "m_num_rotated: " << m_num_rotated << "\n"
  //           << "packet size: " << item->GetSize() << "\n"
  //           << "is_top: " << is_top << "\n"
  //           << "relative_round: " << relative_round << "\n"
  //           << "budget_headq: " << budget_headq << "\n"
  //           << "budget_neg_headq: " << budget_neg_headq << "\n"
  //           << "past_head: " << past_head << "\n"
  //           << "past_tail: " << past_tail << "\n"
  //           << "headq_drop: " << std::boolalpha << headq_drop << "\n"
  //           << "neg_headq_drop: " << std::boolalpha << neg_headq_drop << "\n";

  //   std::cout << std::boolalpha << "--------------------------------------------> ATENTION: retval: " << retval << "\n";
  // }

  m_arrived_pkts += 1;
  return retval;

  // if (GetCurrentSize () + item > GetMaxSize ())
  //   {
  //     NS_LOG_LOGIC ("Queue full -- dropping pkt");
  //     DropBeforeEnqueue (item, LIMIT_EXCEEDED_DROP);
  //     return false;
  //   }

  // if (GetInternalQueue (0)->GetCurrentSize() + item > GetInternalQueue (0)->GetMaxSize ())
  //   {
  //     // std::cout << "Queue full -- dropping pkt" << std::endl;
  //     DropBeforeEnqueue (item, LIMIT_EXCEEDED_DROP);
  //     return false;
  //   }

  // TODO When is DoDequeue Called by NetDevice? Does NetDevice buffer size impact?

  // bool retval = GetInternalQueue (0)->Enqueue (item);
  // NS_ASSERT(retval == true);

  // If Queue::Enqueue fails, QueueDisc::DropBeforeEnqueue is called by the
  // internal queue because QueueDisc::AddInternalQueue sets the trace callback

  // NS_LOG_LOGIC ("Number packets " << GetInternalQueue (0)->GetNPackets ());
  // NS_LOG_LOGIC ("Number bytes " << GetInternalQueue (0)->GetNBytes ());

  // return retval;
}

std::string 
CebinaeQueueDisc::DumpDebuggingStats() {
  NS_ASSERT (m_num_bottleneck_p+m_num_non_bottleneck_p == m_num_p);
  NS_ASSERT (m_lbf_past_head_pkts + m_lbf_past_tail_pkts + m_lbf_drop_pkts == m_arrived_pkts);
  m_oss_summary << "m_arrived_pkts: " << m_arrived_pkts << "\n"
      << "m_lbf_past_head_pkts: " << m_lbf_past_head_pkts << "\n"
      << "m_lbf_past_tail_pkts: " << m_lbf_past_tail_pkts << "\n"
      << "m_lbf_drop_pkts: " << m_lbf_drop_pkts << "\n"
      << "m_enqueue_drop_pkts[0]: " << m_enqueue_drop_pkts[0] << "\n"
      << "m_enqueue_drop_pkts[1]: " << m_enqueue_drop_pkts[1] << "\n"
      << "m_cebinae_dequeued_succeeded: " << m_cebinae_dequeued_succeeded << "\n"
      << "m_num_p: " << m_num_p << "\n"
      << "m_num_bottleneck_p: " << m_num_bottleneck_p << "\n"
      << "m_num_non_bottleneck_p: " << m_num_non_bottleneck_p << "\n"
      << "m_num_rotated: " << m_num_rotated << "\n";
  m_oss_summary << m_fbd.DumpStats();
  return m_oss_summary.str();
}

Ptr<QueueDiscItem>
CebinaeQueueDisc::DoDequeue (void)
{
  bool got_item = false;

  Ptr<QueueDiscItem> item = GetInternalQueue (m_high_prio_queue)->Dequeue ();
  if (!item) {
    // headq Queue empty, check neg_headq
    item = GetInternalQueue (1-m_high_prio_queue)->Dequeue ();
    if (!item) {
      // neg_headq Queue also empty
    } else {
      got_item = true;
    }
  } else {
    got_item = true;
  }

  if (got_item) {

    m_port_bytecounts += item->GetSize();
    m_fbd.UpdateCache(item->GetPacket());

    m_cebinae_dequeued_succeeded += 1;

    // std::cout << ns3::Simulator::Now ().GetNanoSeconds() << ": " << m_port_saturation_detector.Get() << std::endl;
    // m_fbd.Print(std::cout);
    return item;
  } else {
    return 0;
  }
}

Ptr<const QueueDiscItem>
CebinaeQueueDisc::DoPeek (void)
{

  Ptr<const QueueDiscItem> item = GetInternalQueue (0)->Peek ();

  if (!item)
    {
      NS_LOG_DEBUG ("Queue empty");
      return 0;
    }

  return item;
}

bool
CebinaeQueueDisc::CheckConfig (void)
{
  if (GetNQueueDiscClasses () > 0)
    {
      NS_LOG_ERROR ("User shouldn't configure any classes");
      return false;
    }

  if (GetNPacketFilters () > 0)
    {
      NS_LOG_ERROR ("User shouldn't configure any packet filter");
      return false;
    }

  if (GetNInternalQueues () != 0) {
    NS_LOG_ERROR ("User shouldn't configure any InternalQueues");
    return false;
  }

  if (GetNInternalQueues () == 0) {
      // Unlike PrioQueueDisc, Cebinae directly operates on InternelQueue rather than child QueueDisc
      // ObjectFactory factory;
      // factory.SetTypeId ("ns3::FifoQueueDisc");
      // for (uint8_t i = 0; i < 2; i++) {
      //   Ptr<QueueDisc> qd = factory.Create<QueueDisc> ();
      //   qd->Initialize ();
      //   Ptr<QueueDiscClass> c = CreateObject<QueueDiscClass> ();
      //   c->SetQueueDisc (qd);
      //   AddQueueDiscClass (c);
      // }      

      // CebinaeQueueDisc has two fixed DropTail queue, which is non-configurable for user
      // User configured MaxSize is statically carved in halves for each InternalQueue
      AddInternalQueue (CreateObjectWithAttributes<DropTailQueue<QueueDiscItem> >
                          ("MaxSize", QueueSizeValue (
                            QueueSize(GetMaxSize().GetUnit(), GetMaxSize().GetValue()/2)
                            )));
      AddInternalQueue (CreateObjectWithAttributes<DropTailQueue<QueueDiscItem> >
                          ("MaxSize", QueueSizeValue (
                            QueueSize(GetMaxSize().GetUnit(), GetMaxSize().GetValue()/2)
                            )));                       
    }

  if (GetNInternalQueues () != 2)
    {
      NS_LOG_ERROR ("CebinaeQueueDisc has exactly 2 internal queue");
      return false;
    }

  return true;
}

void
CebinaeQueueDisc::InitializeParams (void)
{
}

} // namespace ns3