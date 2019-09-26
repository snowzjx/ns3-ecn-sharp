#ifndef WFQ_QUEUE_DISC_H
#define WFQ_QUEUE_DISC_H

#include "ns3/queue-disc.h"
#include <list>

namespace ns3 {

class WFQClass : public Object
{
public:

    static TypeId GetTypeId (void);

    WFQClass ();

    uint32_t priority;

    Ptr<QueueDisc> qdisc;

    uint64_t headFinTime;
    uint32_t lengthBytes;
    uint32_t weight;
};

class WFQQueueDisc : public QueueDisc
{
public:

    static TypeId GetTypeId (void);

    WFQQueueDisc ();

    virtual ~WFQQueueDisc ();

    void AddWFQClass (Ptr<QueueDisc> qdisc, int32_t cl, uint32_t weight);
    void AddWFQClass (Ptr<QueueDisc> qdisc, int32_t cl, uint32_t priority, uint32_t weight);

private:
    // Operations offered by multi queue disc should be the same as queue disc
    virtual bool DoEnqueue (Ptr<QueueDiscItem> item);
    virtual Ptr<QueueDiscItem> DoDequeue (void);
    virtual Ptr<const QueueDiscItem> DoPeek (void) const;
    virtual bool CheckConfig (void);
    virtual void InitializeParams (void);

    std::map<int32_t, Ptr<WFQClass> > m_WFQs;
    std::map<uint32_t, uint64_t> m_virtualTime;

};

} // namespace ns3

#endif
