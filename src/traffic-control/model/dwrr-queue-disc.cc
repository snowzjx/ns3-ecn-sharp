#include "ns3/log.h"
#include "dwrr-queue-disc.h"
#include "ns3/ipv4-queue-disc-item.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("DWRRQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (DWRRQueueDisc);

TypeId
DWRRClass::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::DWRRClass")
      .SetParent<Object> ()
      .SetGroupName ("TrafficControl")
      .AddConstructor<DWRRClass> ()
    ;
    return tid;
}

DWRRClass::DWRRClass ()
{
    NS_LOG_FUNCTION (this);
}

TypeId
DWRRQueueDisc::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::DWRRQueueDisc")
      .SetParent<Object> ()
      .SetGroupName ("TrafficControl")
      .AddConstructor<DWRRQueueDisc> ()
    ;
    return tid;
}

DWRRQueueDisc::DWRRQueueDisc ()
{
    NS_LOG_FUNCTION (this);
}

DWRRQueueDisc::~DWRRQueueDisc ()
{
    NS_LOG_FUNCTION (this);
    m_DWRRs.clear ();
}

void
DWRRQueueDisc::AddDWRRClass (Ptr<QueueDisc> qdisc, int32_t cl, uint32_t quantum)
{
    DWRRQueueDisc::AddDWRRClass (qdisc, cl, 0, quantum);
}

void
DWRRQueueDisc::AddDWRRClass (Ptr<QueueDisc> qdisc, int32_t cl, uint32_t priority, uint32_t quantum)
{
    Ptr<DWRRClass> dwrrClass = CreateObject<DWRRClass> ();
    dwrrClass->priority = priority;
    dwrrClass->qdisc = qdisc;
    dwrrClass->quantum = quantum;
    dwrrClass->deficit = 0;
    m_DWRRs[cl] = dwrrClass;
}

bool
DWRRQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION (this << item);

    Ptr<DWRRClass> dwrrClass = 0;

    int32_t cl = Classify (item);


    std::map<int32_t, Ptr<DWRRClass> >::iterator itr = m_DWRRs.find (cl);

    if (itr == m_DWRRs.end ())
    {
        NS_LOG_ERROR ("Cannot find class, dropping the packet");
        Drop (item);
        return false;
    }

    dwrrClass = itr->second;

    NS_LOG_LOGIC ("Found class for the enqueued item: " << cl << " with priority: " << dwrrClass->priority);

    if (!dwrrClass->qdisc->Enqueue (item))
    {
        Drop (item);
        return false;
    }

    if (dwrrClass->qdisc->GetNPackets () == 1)
    {
        m_active[dwrrClass->priority].push_back (dwrrClass);
        dwrrClass->deficit = dwrrClass->quantum;
    }

    return true;
}

Ptr<QueueDiscItem>
DWRRQueueDisc::DoDequeue (void)
{
    NS_LOG_FUNCTION (this);

    Ptr<const QueueDiscItem> item = 0;

    if (m_active.empty ())
    {
        NS_LOG_LOGIC ("Active map is empty");
        return 0;
    }

    int32_t highestPriority = -1;
    std::map<uint32_t, std::list<Ptr<DWRRClass> > >::const_iterator itr = m_active.begin ();
    for (; itr != m_active.end (); ++itr)
    {
        if (static_cast<int32_t>(itr->first) > highestPriority
                && !(itr->second).empty ())
        {
            highestPriority = static_cast<int32_t> (itr->first);
        }
    }

    if (highestPriority == -1)
    {
        NS_LOG_LOGIC ("Cannot find active queue");
        return 0;
    }

    while (true)
    {
        Ptr<DWRRClass> dwrrClass = m_active[highestPriority].front ();

        item = dwrrClass->qdisc->Peek ();
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

        uint32_t length = ipv4Item->GetPacketSize ();

        if (length <= dwrrClass->deficit)
        {
            dwrrClass->deficit -= length;
            Ptr<QueueDiscItem> retItem = dwrrClass->qdisc->Dequeue ();
            if (retItem == 0)
            {
                return 0;
            }
            if (dwrrClass->qdisc->GetNPackets () == 0)
            {
                m_active[highestPriority].pop_front ();
            }
            return retItem;
        }

        dwrrClass->deficit += dwrrClass->quantum;
        m_active[highestPriority].pop_front ();
        m_active[highestPriority].push_back (dwrrClass);
    }

    return 0;
}

Ptr<const QueueDiscItem>
DWRRQueueDisc::DoPeek (void) const
{
    NS_LOG_FUNCTION (this);

    if (m_active.empty ())
    {
        NS_LOG_LOGIC ("Active map is empty");
        return 0;
    }

    int32_t highestPriority = -1;
    std::map<uint32_t, std::list<Ptr<DWRRClass> > >::const_iterator itr = m_active.begin ();
    for (; itr != m_active.end (); ++itr)
    {
        if (static_cast<int32_t>(itr->first) >= highestPriority
                && !(itr->second).empty ())
        {
            highestPriority = static_cast<int32_t>(itr->first);
        }
    }

    if (highestPriority == -1)
    {
        NS_LOG_LOGIC ("Cannot find active queue");
        return 0;
    }

    Ptr<DWRRClass> dwrrClass = m_active.at (highestPriority).front ();

    return dwrrClass->qdisc->Peek ();
}

bool
DWRRQueueDisc::CheckConfig (void)
{
    NS_LOG_FUNCTION (this);
    return true;
}

void
DWRRQueueDisc::InitializeParams (void)
{
    NS_LOG_FUNCTION (this);

    std::map<int32_t, Ptr<DWRRClass> >::iterator itr = m_DWRRs.begin ();
    for ( ; itr != m_DWRRs.end (); ++itr)
    {
        itr->second->qdisc->Initialize ();
    }

}


}
