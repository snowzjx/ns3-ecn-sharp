#ifndef DWRR_QUEUE_DISC_H
#define DWRR_QUEUE_DISC_H

#include "ns3/queue-disc.h"
#include <list>

namespace ns3 {

class DWRRClass : public Object
{
public:

    static TypeId GetTypeId (void);

    DWRRClass ();

    uint32_t priority;

    Ptr<QueueDisc> qdisc;
    uint32_t quantum;
    uint32_t deficit;
};

class DWRRQueueDisc : public QueueDisc
{
public:

    static TypeId GetTypeId (void);

    DWRRQueueDisc ();

    virtual ~DWRRQueueDisc ();

    void AddDWRRClass (Ptr<QueueDisc> qdisc, int32_t cl, uint32_t quantum);
    void AddDWRRClass (Ptr<QueueDisc> qdisc, int32_t cl, uint32_t priority, uint32_t quantum);

private:
    // Operations offered by multi queue disc should be the same as queue disc
    virtual bool DoEnqueue (Ptr<QueueDiscItem> item);
    virtual Ptr<QueueDiscItem> DoDequeue (void);
    virtual Ptr<const QueueDiscItem> DoPeek (void) const;
    virtual bool CheckConfig (void);
    virtual void InitializeParams (void);

    // The internal DWRR queue discs are first organized in a map
    // with the priority as key and then in a linked list if they are
    // with the same priority
    std::map<uint32_t, std::list<Ptr<DWRRClass> > > m_active;
    std::map<int32_t, Ptr<DWRRClass> > m_DWRRs;
};

} // namespace ns3

#endif
