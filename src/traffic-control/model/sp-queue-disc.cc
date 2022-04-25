#include "ns3/log.h"
#include "sp-queue-disc.h"
#include "ns3/ipv4-queue-disc-item.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SPQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (SPQueueDisc);

TypeId
SPClass::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::SPClass")
      .SetParent<Object> ()
      .SetGroupName ("TrafficControl")
      .AddConstructor<SPClass> ()
    ;
    return tid;
}

SPClass::SPClass ()
{
    NS_LOG_FUNCTION (this);
}

TypeId
SPQueueDisc::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::SPQueueDisc")
      .SetParent<Object> ()
      .SetGroupName ("TrafficControl")
      .AddConstructor<SPQueueDisc> ()
    ;
    return tid;
}

SPQueueDisc::SPQueueDisc ()
{
    NS_LOG_FUNCTION (this);
}

SPQueueDisc::~SPQueueDisc ()
{
    NS_LOG_FUNCTION (this);
}

void
SPQueueDisc::AddSPClass (Ptr<QueueDisc> qdisc, int32_t cl, uint32_t priority)
{
    Ptr<SPClass> spClass = CreateObject<SPClass> ();
    spClass->priority = priority;
    spClass->qdisc = qdisc;
    spClass->lengthBytes = 0;
    m_SPs[cl] = spClass;
}

bool
SPQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION (this << item);

    Ptr<SPClass> spClass = 0;

    int32_t cl = Classify (item);

    std::map<int32_t, Ptr<SPClass> >::iterator itr = m_SPs.find (cl);

    if (itr == m_SPs.end ())
    {
        NS_LOG_ERROR ("Cannot find class, dropping the packet");
        Drop (item);
        return false;
    }

    spClass = itr->second;

    NS_LOG_LOGIC ("Found class for the enqueued item: " << cl << " with priority: " << spClass->priority);

    Ptr<Ipv4QueueDiscItem> ipv4Item = DynamicCast<Ipv4QueueDiscItem> (item);
    if (ipv4Item == 0)
    {
        NS_LOG_ERROR ("Cannot convert to the Ipv4QueueDiscItem");
        Drop (item);
        return false;
    }

    if (!spClass->qdisc->Enqueue (item))
    {
        Drop (item);
        return false;
    }
    
    uint32_t length = ipv4Item->GetPacketSize ();
    spClass->lengthBytes += length;

    return true;
}

Ptr<QueueDiscItem>
SPQueueDisc::DoDequeue (void)
{
    NS_LOG_FUNCTION (this);

    int32_t highestPriority = -1;
    Ptr<SPClass> spClassToDequeue = 0;

    std::map<int32_t, Ptr<SPClass> >::const_iterator itr = m_SPs.begin ();

    // Strict priority scheduling
    for (; itr != m_SPs.end (); ++itr)
    {
        Ptr<SPClass> spClass = itr->second;
        if (static_cast<int32_t> (spClass->priority) > highestPriority
                && spClass->lengthBytes > 0)
        {
            highestPriority = static_cast<int32_t> (spClass->priority);
            spClassToDequeue = spClass;
        }
    }

    if (highestPriority == -1)
    {
        NS_LOG_LOGIC ("Cannot find active queue");
        return 0;
    }


    Ptr<const QueueDiscItem> item = spClassToDequeue->qdisc->Peek ();

    if (item == 0)
    {
        NS_LOG_LOGIC ("Cannot peek from the internal queue disc");
        return 0;
    }

    Ptr<const Ipv4QueueDiscItem> ipv4Item = DynamicCast<const Ipv4QueueDiscItem> (item);
    if (ipv4Item == 0)
    {
        NS_LOG_ERROR ("Cannot convert to the Ipv4QueueDiscItem");
        return 0;
    }

    Ptr<QueueDiscItem> retItem = spClassToDequeue->qdisc->Dequeue ();

    if (retItem == 0)
    {
        NS_LOG_ERROR ("Cannot dequeue from the internal queue disc");
        return 0;
    }

    spClassToDequeue->lengthBytes -= ipv4Item->GetPacketSize ();

    return retItem;
}

Ptr<const QueueDiscItem>
SPQueueDisc::DoPeek (void) const
{
    NS_LOG_FUNCTION (this);

    int32_t highestPriority = -1;
    Ptr<SPClass> spClassToPeek = 0;

    std::map<int32_t, Ptr<SPClass> >::const_iterator itr = m_SPs.begin ();

    // Strict priority scheduling
    for (; itr != m_SPs.end (); ++itr)
    {
        Ptr<SPClass> spClass = itr->second;
        if (static_cast<int32_t> (spClass->priority) > highestPriority
                && spClass->lengthBytes > 0)
        {
            highestPriority = static_cast<int32_t> (spClass->priority);
            spClassToPeek = spClass;
        }
    }

    if (highestPriority == -1)
    {
        NS_LOG_LOGIC ("Cannot find active queue");
        return 0;
    }

    return spClassToPeek->qdisc->Peek ();
}

bool
SPQueueDisc::CheckConfig (void)
{
    NS_LOG_FUNCTION (this);
    return true;
}

void
SPQueueDisc::InitializeParams (void)
{
    NS_LOG_FUNCTION (this);

    std::map<int32_t, Ptr<SPClass> >::iterator itr = m_SPs.begin ();
    for ( ; itr != m_SPs.end (); ++itr)
    {
        itr->second->qdisc->Initialize ();
    }

}
}
