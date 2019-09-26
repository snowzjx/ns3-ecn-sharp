/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 NITK Surathkal
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
 * Authors: Shravya Ks <shravya.ks0@gmail.com>
 *          Smriti Murali <m.smriti.95@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

/*
 * PORT NOTE: This code was ported from ns-2.36rc1 (queue/pie.cc).
 * Most of the comments are also ported from the same.
 */

/*
 * In this version of PIE for DCN, we made two modifications here.
 * 1. We use ECN instead of dropping
 * 2. The sojourn time is measured directly to address queue capacity problem
 * 3. The burst allowance module is removed
 */

#include "ns3/log.h"
#include "ns3/enum.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/simulator.h"
#include "ns3/abort.h"
#include "ns3/string.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/ipv4-queue-disc-item.h"
#include "pie-queue-disc.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PieQueueDisc");


/**
 * PIE time stamp, used to carry PIE time informations.
 */
class PieTimestampTag : public Tag
{
public:
  PieTimestampTag ();
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;

  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (TagBuffer i) const;
  virtual void Deserialize (TagBuffer i);
  virtual void Print (std::ostream &os) const;

  /**
   * Gets the Tag creation time
   * @return the time object stored in the tag
   */
  Time GetTxTime (void) const;
private:
  uint64_t m_creationTime; //!< Tag creation time
};

PieTimestampTag::PieTimestampTag ()
  : m_creationTime (Simulator::Now ().GetTimeStep ())
{
}

TypeId
PieTimestampTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PieTimestampTag")
    .SetParent<Tag> ()
    .AddConstructor<PieTimestampTag> ()
    .AddAttribute ("CreationTime",
                   "The time at which the timestamp was created",
                   StringValue ("0.0s"),
                   MakeTimeAccessor (&PieTimestampTag::GetTxTime),
                   MakeTimeChecker ())
  ;
  return tid;
}

TypeId
PieTimestampTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint32_t
PieTimestampTag::GetSerializedSize (void) const
{
  return 8;
}
void
PieTimestampTag::Serialize (TagBuffer i) const
{
  i.WriteU64 (m_creationTime);
}
void
PieTimestampTag::Deserialize (TagBuffer i)
{
  m_creationTime = i.ReadU64 ();
}
void
PieTimestampTag::Print (std::ostream &os) const
{
  os << "CreationTime=" << m_creationTime;
}
Time
PieTimestampTag::GetTxTime (void) const
{
  return TimeStep (m_creationTime);
}


NS_OBJECT_ENSURE_REGISTERED (PieQueueDisc);

TypeId PieQueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PieQueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName ("TrafficControl")
    .AddConstructor<PieQueueDisc> ()
    .AddAttribute ("Mode",
                   "Determines unit for QueueLimit",
                   EnumValue (Queue::QUEUE_MODE_PACKETS),
                   MakeEnumAccessor (&PieQueueDisc::SetMode),
                   MakeEnumChecker (Queue::QUEUE_MODE_BYTES, "QUEUE_MODE_BYTES",
                                    Queue::QUEUE_MODE_PACKETS, "QUEUE_MODE_PACKETS"))
    .AddAttribute ("MeanPktSize",
                   "Average of packet size",
                   UintegerValue (1000),
                   MakeUintegerAccessor (&PieQueueDisc::m_meanPktSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("A",
                   "Value of alpha",
                   DoubleValue (0.125),
                   MakeDoubleAccessor (&PieQueueDisc::m_a),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("B",
                   "Value of beta",
                   DoubleValue (1.25),
                   MakeDoubleAccessor (&PieQueueDisc::m_b),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("Tupdate",
                   "Time period to calculate drop probability",
                   TimeValue (Seconds (0.03)),
                   MakeTimeAccessor (&PieQueueDisc::m_tUpdate),
                   MakeTimeChecker ())
    .AddAttribute ("Supdate",
                   "Start time of the update timer",
                   TimeValue (Seconds (0)),
                   MakeTimeAccessor (&PieQueueDisc::m_sUpdate),
                   MakeTimeChecker ())
    .AddAttribute ("QueueLimit",
                   "Queue limit in bytes/packets",
                   UintegerValue (25),
                   MakeUintegerAccessor (&PieQueueDisc::SetQueueLimit),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("DequeueThreshold",
                   "Minimum queue size in bytes before dequeue rate is measured",
                   UintegerValue (10000),
                   MakeUintegerAccessor (&PieQueueDisc::m_dqThreshold),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("QueueDelayReference",
                   "Desired queue delay",
                   TimeValue (Seconds (0.02)),
                   MakeTimeAccessor (&PieQueueDisc::m_qDelayRef),
                   MakeTimeChecker ())
    .AddAttribute ("MaxBurstAllowance",
                   "Current max burst allowance in seconds before random drop",
                   TimeValue (Seconds (0.1)),
                   MakeTimeAccessor (&PieQueueDisc::m_maxBurst),
                   MakeTimeChecker ())
  ;

  return tid;
}

PieQueueDisc::PieQueueDisc ()
  : QueueDisc ()
{
  NS_LOG_FUNCTION (this);
  m_uv = CreateObject<UniformRandomVariable> ();
  m_rtrsEvent = Simulator::Schedule (m_sUpdate, &PieQueueDisc::CalculateP, this);
}

PieQueueDisc::~PieQueueDisc ()
{
  NS_LOG_FUNCTION (this);
}

void
PieQueueDisc::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_uv = 0;
  Simulator::Remove (m_rtrsEvent);
  QueueDisc::DoDispose ();
}

void
PieQueueDisc::SetMode (Queue::QueueMode mode)
{
  NS_LOG_FUNCTION (this << mode);
  m_mode = mode;
}

Queue::QueueMode
PieQueueDisc::GetMode (void)
{
  NS_LOG_FUNCTION (this);
  return m_mode;
}

void
PieQueueDisc::SetQueueLimit (uint32_t lim)
{
  NS_LOG_FUNCTION (this << lim);
  m_queueLimit = lim;
}

uint32_t
PieQueueDisc::GetQueueSize (void)
{
  NS_LOG_FUNCTION (this);
  if (GetMode () == Queue::QUEUE_MODE_BYTES)
    {
      return GetInternalQueue (0)->GetNBytes ();
    }
  else if (GetMode () == Queue::QUEUE_MODE_PACKETS)
    {
      return GetInternalQueue (0)->GetNPackets ();
    }
  else
    {
      NS_ABORT_MSG ("Unknown PIE mode.");
    }
}

PieQueueDisc::Stats
PieQueueDisc::GetStats ()
{
  NS_LOG_FUNCTION (this);
  return m_stats;
}

Time
PieQueueDisc::GetQueueDelay (void)
{
  NS_LOG_FUNCTION (this);
  return m_qDelay;
}

int64_t
PieQueueDisc::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_uv->SetStream (stream);
  return 1;
}

bool
PieQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
  NS_LOG_FUNCTION (this << item);

  uint32_t nQueued = GetQueueSize ();

  if ((GetMode () == Queue::QUEUE_MODE_PACKETS && nQueued >= m_queueLimit)
      || (GetMode () == Queue::QUEUE_MODE_BYTES && nQueued + item->GetPacketSize () > m_queueLimit))
    {
      // Drops due to queue limit: reactive
      Drop (item);
      m_stats.forcedDrop++;
      return false;
    }

  Ptr<Packet> p = item->GetPacket ();
  PieTimestampTag tag;
  p->AddPacketTag (tag);

  if (MarkingEarly (item, nQueued))
    {
      // Early probability drop: proactive
      PieQueueDisc::MarkingECN (item);
      m_stats.unforcedDrop++;
    }

  // No drop
  bool retval = GetInternalQueue (0)->Enqueue (item);

  // If Queue::Enqueue fails, QueueDisc::Drop is called by the internal queue
  // because QueueDisc::AddInternalQueue sets the drop callback

  NS_LOG_LOGIC ("\t bytesInQueue  " << GetInternalQueue (0)->GetNBytes ());
  NS_LOG_LOGIC ("\t packetsInQueue  " << GetInternalQueue (0)->GetNPackets ());

  return retval;
}

void
PieQueueDisc::InitializeParams (void)
{
  // Initially queue is empty so variables are initialize to zero except m_dqCount
  m_inMeasurement = false;
  m_dqCount = -1;
  m_markingProb = 0;
  m_avgDqRate = 0.0;
  m_dqStart = 0;
  m_burstState = NO_BURST;
  m_qDelayOld = Time (Seconds (0));
  m_stats.forcedDrop = 0;
  m_stats.unforcedDrop = 0;
}

bool PieQueueDisc::MarkingEarly (Ptr<QueueDiscItem> item, uint32_t qSize)
{
  NS_LOG_FUNCTION (this << item << qSize);
  if (BURST_ALLOWANCE_MODULE) {
    if (m_burstAllowance.GetSeconds () > 0)
    {
      // If there is still burst_allowance left, skip random early drop.
      return false;
    }

    if (m_burstState == NO_BURST)
    {
      m_burstState = IN_BURST_PROTECTING;
      m_burstAllowance = m_maxBurst;
    }
  }

  double p = m_markingProb;

  uint32_t packetSize = item->GetPacketSize ();

  if (GetMode () == Queue::QUEUE_MODE_BYTES)
    {
      p = p * packetSize / m_meanPktSize;
    }
  bool earlyMarking = true;
  double u =  m_uv->GetValue ();

  if ((m_qDelayOld.GetSeconds () < (0.5 * m_qDelayRef.GetSeconds ())) && (m_markingProb < 0.2))
    {
      return false;
    }
  else if (GetMode () == Queue::QUEUE_MODE_BYTES && qSize <= 2 * m_meanPktSize)
    {
      return false;
    }
  else if (GetMode () == Queue::QUEUE_MODE_PACKETS && qSize <= 2)
    {
      return false;
    }

  if (u > p)
    {
      earlyMarking = false;
    }
  if (!earlyMarking)
    {
      return false;
    }

  return true;
}

void PieQueueDisc::CalculateP ()
{
  NS_LOG_FUNCTION (this);
  Time qDelay;
  double p = 0.0;
  bool missingInitFlag = false;
  /*
  if (m_avgDqRate > 0)
    {
      qDelay = Time (Seconds (GetInternalQueue (0)->GetNBytes () / m_avgDqRate));
    }
  else
    {
      qDelay = Time (Seconds (0));
      missingInitFlag = true;
    }
   */

  // We will directly measure the sojourn time
  Ptr<const QueueDiscItem> item = PieQueueDisc::DoPeek ();
  if (item == 0)
  {
    qDelay = Time (Seconds (0));
    missingInitFlag = true;
  }
  else
  {
    PieTimestampTag timestamp;
    item->GetPacket ()->PeekPacketTag (timestamp);
    qDelay = Simulator::Now () - timestamp.GetTxTime ();
  }

  m_qDelay = qDelay;

  if (BURST_ALLOWANCE_MODULE && m_burstAllowance.GetSeconds () > 0)
    {
      m_markingProb = 0;
    }
  else
    {
      /*
      p = m_a * (qDelay.GetSeconds () - m_qDelayRef.GetSeconds ()) + m_b * (qDelay.GetSeconds () - m_qDelayOld.GetSeconds ());
      if (m_markingProb < 0.001)
        {
          p /= 32;
        }
      else if (m_markingProb < 0.01)
        {
          p /= 8;
        }
      else if (m_markingProb < 0.1)
        {
          p /= 2;
        }
      else if (m_markingProb < 1)
        {
          p /= 0.5;
        }
      else if (m_markingProb < 10)
        {
          p /= 0.125;
        }
      else
        {
          p /= 0.03125;
        }
      if ((m_markingProb >= 0.1) && (p > 0.02))
        {
          p = 0.02;
        }
       */
       // Update to use the algorithm from papers
       double alpha = m_a;
       double beta = m_b;
       if (m_markingProb < 0.01)
       {
           alpha = m_a / 8;
           beta = m_b / 8;
       }
       else if (m_markingProb < 0.1)
       {
           alpha = m_a / 2;
           beta = m_b / 2;
       }
       p = alpha * (qDelay.GetSeconds () - m_qDelayRef.GetSeconds ()) + beta * (qDelay.GetSeconds () - m_qDelayOld.GetSeconds ());
       // Remove this part to speed up the marking
       /*
       if ((m_markingProb >= 0.1) && (p > 0.02))
       {
          p = 0.02;
       }
       */

    }

  p += m_markingProb;

  // For non-linear drop in prob

  if (qDelay.GetSeconds () == 0 && m_qDelayOld.GetSeconds () == 0)
    {
      p *= 0.98;
    }
  else if (qDelay.GetSeconds () > 0.2)
    {
      p += 0.02;
    }

  m_markingProb = (p > 0) ? p : 0;

  if (BURST_ALLOWANCE_MODULE)
  {
    if (m_burstAllowance < m_tUpdate)
    {
      m_burstAllowance =  Time (Seconds (0));
    }
    else
    {
      m_burstAllowance -= m_tUpdate;
    }

    uint32_t burstResetLimit = BURST_RESET_TIMEOUT / m_tUpdate.GetSeconds ();

    if ( (qDelay.GetSeconds () < 0.5 * m_qDelayRef.GetSeconds ()) && (m_qDelayOld.GetSeconds () < (0.5 * m_qDelayRef.GetSeconds ())) && (m_markingProb == 0) && (m_burstAllowance.GetSeconds () == 0))
    {
      if (m_burstState == IN_BURST_PROTECTING)
        {
          m_burstState = IN_BURST;
          m_burstReset = 0;
        }
      else if (m_burstState == IN_BURST)
        {
          m_burstReset++;
          if (m_burstReset > burstResetLimit)
            {
              m_burstReset = 0;
              m_burstState = NO_BURST;
            }
        }
    }
  else if (m_burstState == IN_BURST)
    {
      m_burstReset = 0;
    }
  }

  if ( (qDelay.GetSeconds () < 0.5 * m_qDelayRef.GetSeconds ()) && (m_qDelayOld.GetSeconds () < (0.5 * m_qDelayRef.GetSeconds ())) && (m_markingProb == 0) && !missingInitFlag )
    {
      m_dqCount = -1;
      m_avgDqRate = 0.0;
    }

  m_qDelayOld = qDelay;
  m_rtrsEvent = Simulator::Schedule (m_tUpdate, &PieQueueDisc::CalculateP, this);
}

Ptr<QueueDiscItem>
PieQueueDisc::DoDequeue ()
{
  NS_LOG_FUNCTION (this);

  if (GetInternalQueue (0)->IsEmpty ())
    {
      NS_LOG_LOGIC ("Queue empty");
      return 0;
    }

  Ptr<QueueDiscItem> item = StaticCast<QueueDiscItem> (GetInternalQueue (0)->Dequeue ());
  double now = Simulator::Now ().GetSeconds ();
  uint32_t pktSize = item->GetPacketSize ();

  Ptr<Packet> p = item->GetPacket ();
  PieTimestampTag tag;
  p->RemovePacketTag (tag);

  // if not in a measurement cycle and the queue has built up to dq_threshold,
  // start the measurement cycle

  if ( (GetInternalQueue (0)->GetNBytes () >= m_dqThreshold) && (!m_inMeasurement) )
    {
      m_dqStart = now;
      m_dqCount = 0;
      m_inMeasurement = true;
    }

  if (m_inMeasurement)
    {
      m_dqCount += pktSize;

      // done with a measurement cycle
      if (m_dqCount >= m_dqThreshold)
        {

          double tmp = now - m_dqStart;

          if (tmp > 0)
            {
              if (m_avgDqRate == 0)
                {
                  m_avgDqRate = m_dqCount / tmp;
                }
              else
                {
                  m_avgDqRate = (0.5 * m_avgDqRate) + (0.5 * (m_dqCount / tmp));
                }
            }

          // restart a measurement cycle if there is enough data
          if (GetInternalQueue (0)->GetNBytes () > m_dqThreshold)
            {
              m_dqStart = now;
              m_dqCount = 0;
              m_inMeasurement = true;
            }
          else
            {
              m_dqCount = 0;
              m_inMeasurement = false;
            }
        }
    }

  return item;
}

Ptr<const QueueDiscItem>
PieQueueDisc::DoPeek () const
{
  NS_LOG_FUNCTION (this);
  if (GetInternalQueue (0)->IsEmpty ())
    {
      NS_LOG_LOGIC ("Queue empty");
      return 0;
    }

  Ptr<const QueueDiscItem> item = StaticCast<const QueueDiscItem> (GetInternalQueue (0)->Peek ());

  NS_LOG_LOGIC ("Number packets " << GetInternalQueue (0)->GetNPackets ());
  NS_LOG_LOGIC ("Number bytes " << GetInternalQueue (0)->GetNBytes ());

  return item;
}

bool
PieQueueDisc::CheckConfig (void)
{
  NS_LOG_FUNCTION (this);
  if (GetNQueueDiscClasses () > 0)
    {
      NS_LOG_ERROR ("PieQueueDisc cannot have classes");
      return false;
    }

  if (GetNPacketFilters () > 0)
    {
      NS_LOG_ERROR ("PieQueueDisc cannot have packet filters");
      return false;
    }

  if (GetNInternalQueues () == 0)
    {
      // create a DropTail queue
      Ptr<Queue> queue = CreateObjectWithAttributes<DropTailQueue> ("Mode", EnumValue (m_mode));
      if (m_mode == Queue::QUEUE_MODE_PACKETS)
        {
          queue->SetMaxPackets (m_queueLimit);
        }
      else
        {
          queue->SetMaxBytes (m_queueLimit);
        }
      AddInternalQueue (queue);
    }

  if (GetNInternalQueues () != 1)
    {
      NS_LOG_ERROR ("PieQueueDisc needs 1 internal queue");
      return false;
    }

  if (GetInternalQueue (0)->GetMode () != m_mode)
    {
      NS_LOG_ERROR ("The mode of the provided queue does not match the mode set on the PieQueueDisc");
      return false;
    }

  if ((m_mode ==  Queue::QUEUE_MODE_PACKETS && GetInternalQueue (0)->GetMaxPackets () < m_queueLimit)
      || (m_mode ==  Queue::QUEUE_MODE_BYTES && GetInternalQueue (0)->GetMaxBytes () < m_queueLimit))
    {
      NS_LOG_ERROR ("The size of the internal queue is less than the queue disc limit");
      return false;
    }

  return true;
}

bool
PieQueueDisc::MarkingECN (Ptr<QueueDiscItem> item)
{
  Ptr<Ipv4QueueDiscItem> ipv4Item = DynamicCast<Ipv4QueueDiscItem> (item);
  if (ipv4Item == 0)
  {
    NS_LOG_ERROR ("Cannot convert the queue disc item to ipv4 queue disc item");
    return false;
  }

  Ipv4Header header = ipv4Item -> GetHeader ();

  if (header.GetEcn () != Ipv4Header::ECN_ECT1)
  {
    NS_LOG_ERROR ("Cannot mark because the ECN field is not ECN_ECT1");
    return false;
  }

  header.SetEcn(Ipv4Header::ECN_CE);
  ipv4Item->SetHeader(header);

  return true;

}


} //namespace ns3
