#include "ns3/log.h"
#include "wfq-queue-disc.h"
#include "ns3/ipv4-queue-disc-item.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("WFQQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (WFQQueueDisc);

TypeId
WFQClass::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::WFQClass")
      .SetParent<Object> ()
      .SetGroupName ("TrafficControl")
      .AddConstructor<WFQClass> ()
    ;
    return tid;
}

WFQClass::WFQClass ()
{
    NS_LOG_FUNCTION (this);
}

TypeId
WFQQueueDisc::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::WFQQueueDisc")
      .SetParent<Object> ()
      .SetGroupName ("TrafficControl")
      .AddConstructor<WFQQueueDisc> ()
    ;
    return tid;
}

WFQQueueDisc::WFQQueueDisc ()
{
    NS_LOG_FUNCTION (this);
}

WFQQueueDisc::~WFQQueueDisc ()
{
    NS_LOG_FUNCTION (this);
}

void
WFQQueueDisc::AddWFQClass (Ptr<QueueDisc> qdisc, int32_t cl, uint32_t weight)
{
    WFQQueueDisc::AddWFQClass (qdisc, cl, 0, weight);
}

void
WFQQueueDisc::AddWFQClass (Ptr<QueueDisc> qdisc, int32_t cl, uint32_t priority, uint32_t weight)
{
    Ptr<WFQClass> wfqClass = CreateObject<WFQClass> ();
    wfqClass->priority = priority;
    wfqClass->qdisc = qdisc;
    wfqClass->headFinTime = 0;
    wfqClass->lengthBytes = 0;
    wfqClass->weight = weight;
    m_WFQs[cl] = wfqClass;
}

bool
WFQQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION (this << item);

    Ptr<WFQClass> wfqClass = 0;

    int32_t cl = Classify (item);

    std::map<int32_t, Ptr<WFQClass> >::iterator itr = m_WFQs.find (cl);

    if (itr == m_WFQs.end ())
    {
        NS_LOG_ERROR ("Cannot find class, dropping the packet");
        Drop (item);
        return false;
    }

    wfqClass = itr->second;

    NS_LOG_LOGIC ("Found class for the enqueued item: " << cl << " with priority: " << wfqClass->priority);

    Ptr<Ipv4QueueDiscItem> ipv4Item = DynamicCast<Ipv4QueueDiscItem> (item);
    if (ipv4Item == 0)
    {
        NS_LOG_ERROR ("Cannot convert to the Ipv4QueueDiscItem");
        Drop (item);
        return false;
    }

    if (!wfqClass->qdisc->Enqueue (item))
    {
        Drop (item);
        return false;
    }

    uint32_t length = ipv4Item->GetPacketSize ();

    uint64_t virtualTime = 0;
    std::map<uint32_t, uint64_t>::iterator itr2 = m_virtualTime.find (wfqClass->priority);
    if (itr2 != m_virtualTime.end ())
    {
        virtualTime = itr2->second;
    }

    if (wfqClass->qdisc->GetNPackets () == 1)
    {
        wfqClass->headFinTime = length / wfqClass->weight + virtualTime;
        m_virtualTime[wfqClass->priority] = wfqClass->headFinTime;
    }

    wfqClass->lengthBytes += length;

    return true;
}

Ptr<QueueDiscItem>
WFQQueueDisc::DoDequeue (void)
{
    NS_LOG_FUNCTION (this);

    int32_t highestPriority = -1;
    std::map<int32_t, Ptr<WFQClass> >::const_iterator itr = m_WFQs.begin ();

    // Strict priority scheduling
    for (; itr != m_WFQs.end (); ++itr)
    {
        Ptr<WFQClass> wfqClass = itr->second;
        if (static_cast<int32_t> (wfqClass->priority) > highestPriority
                && wfqClass->lengthBytes > 0)
        {
            highestPriority = static_cast<int32_t> (wfqClass->priority);
        }
    }

    if (highestPriority == -1)
    {
        NS_LOG_LOGIC ("Cannot find active queue");
        return 0;
    }

    // Find the smallest head finish time
    uint64_t smallestHeadFinTime = 0;
    Ptr<WFQClass> wfqClassToDequeue = 0;

    itr = m_WFQs.begin ();
    for (; itr != m_WFQs.end (); ++itr)
    {
        Ptr<WFQClass> wfqClass = itr->second;
        if (static_cast<int32_t> (wfqClass->priority) != highestPriority
                || wfqClass->lengthBytes == 0)
        {
            continue;
        }
        if (wfqClassToDequeue == 0 || wfqClass->headFinTime < smallestHeadFinTime)
        {
            wfqClassToDequeue = wfqClass;
            smallestHeadFinTime = wfqClass->headFinTime;
        }
    }

    Ptr<const QueueDiscItem> item = wfqClassToDequeue->qdisc->Peek ();

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

    Ptr<QueueDiscItem> retItem = wfqClassToDequeue->qdisc->Dequeue ();

    if (retItem == 0)
    {
        NS_LOG_ERROR ("Cannot dequeue from the internal queue disc");
        return 0;
    }

    wfqClassToDequeue->lengthBytes -= ipv4Item->GetPacketSize ();

    if (wfqClassToDequeue->lengthBytes > 0)
    {
        Ptr<const QueueDiscItem> nextItem = wfqClassToDequeue->qdisc->Peek ();
        Ptr<const Ipv4QueueDiscItem> ipv4NextItem = DynamicCast<const Ipv4QueueDiscItem> (nextItem);
        uint32_t nextLength = ipv4NextItem->GetPacketSize ();
        wfqClassToDequeue->headFinTime += nextLength / wfqClassToDequeue->weight;

        uint64_t virtualTime = 0;
        std::map<uint32_t, uint64_t>::iterator itr2 = m_virtualTime.find (wfqClassToDequeue->priority);
        if (itr2 != m_virtualTime.end ())
        {
            virtualTime = itr2->second;
        }

        if (virtualTime < wfqClassToDequeue->headFinTime)
        {
            m_virtualTime[wfqClassToDequeue->priority] = wfqClassToDequeue->headFinTime;
        }
    }

    return retItem;
}

Ptr<const QueueDiscItem>
WFQQueueDisc::DoPeek (void) const
{
    NS_LOG_FUNCTION (this);

    int32_t highestPriority = -1;
    std::map<int32_t, Ptr<WFQClass> >::const_iterator itr = m_WFQs.begin ();

    // Strict priority scheduling
    for (; itr != m_WFQs.end (); ++itr)
    {
        Ptr<WFQClass> wfqClass = itr->second;
        if (static_cast<int32_t> (wfqClass->priority) > highestPriority
                && wfqClass->lengthBytes > 0)
        {
            highestPriority = static_cast<int32_t> (wfqClass->priority);
        }
    }

    if (highestPriority == -1)
    {
        NS_LOG_LOGIC ("Cannot find active queue");
        return 0;
    }

    // Find the smallest head finish time
    uint64_t smallestHeadFinTime = 0;
    Ptr<WFQClass> wfqClassToDequeue = 0;

    itr = m_WFQs.begin ();
    for (; itr != m_WFQs.end (); ++itr)
    {
        Ptr<WFQClass> wfqClass = itr->second;
        if (static_cast<int32_t> (wfqClass->priority) != highestPriority
                || wfqClass->lengthBytes == 0)
        {
            continue;
        }
        if (wfqClassToDequeue == 0 || wfqClass->headFinTime < smallestHeadFinTime)
        {
            wfqClassToDequeue = wfqClass;
            smallestHeadFinTime = wfqClass->headFinTime;
        }
    }

    return wfqClassToDequeue->qdisc->Peek ();
}

bool
WFQQueueDisc::CheckConfig (void)
{
    NS_LOG_FUNCTION (this);
    return true;
}

void
WFQQueueDisc::InitializeParams (void)
{
    NS_LOG_FUNCTION (this);

    std::map<int32_t, Ptr<WFQClass> >::iterator itr = m_WFQs.begin ();
    for ( ; itr != m_WFQs.end (); ++itr)
    {
        itr->second->qdisc->Initialize ();
    }

}


}
