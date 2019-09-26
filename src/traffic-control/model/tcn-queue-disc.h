#ifndef TCN_QUEUE_DISC_H
#define TCN_QUEUE_DISC_H

#include "ns3/queue-disc.h"
#include "ns3/nstime.h"

namespace ns3 {

class TCNQueueDisc : public QueueDisc
{
public:
    static TypeId GetTypeId (void);

    TCNQueueDisc ();

    virtual ~TCNQueueDisc ();

    bool MarkingECN (Ptr<QueueDiscItem> item);

private:
    // Operations offered by multi queue disc should be the same as queue disc
    virtual bool DoEnqueue (Ptr<QueueDiscItem> item);
    virtual Ptr<QueueDiscItem> DoDequeue (void);
    virtual Ptr<const QueueDiscItem> DoPeek (void) const;
    virtual bool CheckConfig (void);
    virtual void InitializeParams (void);

    uint32_t m_maxPackets;                  //!< Max # of packets accepted by the queue
    uint32_t m_maxBytes;                    //!< Max # of bytes accepted by the queue
    Queue::QueueMode     m_mode;            //!< The operating mode (Bytes or packets)

    Time m_threshold;
};

}

#endif
