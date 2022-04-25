#ifndef SP_QUEUE_DISC_H
#define SP_QUEUE_DISC_H

#include "ns3/queue-disc.h"
#include <list>

namespace ns3 {

class SPClass : public Object
{
public:

    static TypeId GetTypeId (void);

    SPClass ();

    uint32_t priority;
    Ptr<QueueDisc> qdisc;
    uint32_t lengthBytes;
};

class SPQueueDisc : public QueueDisc
{
public:

    static TypeId GetTypeId (void);

    SPQueueDisc ();

    virtual ~SPQueueDisc ();

    void AddSPClass (Ptr<QueueDisc> qdisc, int32_t cl, uint32_t priority);

private:
    // Operations offered by multi queue disc should be the same as queue disc
    virtual bool DoEnqueue (Ptr<QueueDiscItem> item);
    virtual Ptr<QueueDiscItem> DoDequeue (void);
    virtual Ptr<const QueueDiscItem> DoPeek (void) const;
    virtual bool CheckConfig (void);
    virtual void InitializeParams (void);

    std::map<int32_t, Ptr<SPClass> > m_SPs;
};

} // namespace ns3

#endif
