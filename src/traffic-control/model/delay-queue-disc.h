#ifndef DELAY_QUEUE_DISC_H
#define DELAY_QUEUE_DISC_H

#include "ns3/queue-disc.h"
#include "ns3/nstime.h"
#include "ns3/event-id.h"
#include <list>
#include <queue>

namespace ns3 {

  class DelayClass: public Object
  {
  public:
    static TypeId GetTypeId (void);

    DelayClass ();
    
    int32_t cl;
    std::queue<Ptr<const QueueDiscItem> > queue;
    Time delay;
  };

  class DelayQueueDisc: public QueueDisc
  {
  public:
    static TypeId GetTypeId (void);
    
    DelayQueueDisc ();
    virtual ~DelayQueueDisc ();

    void AddDelayClass (int32_t cl, Time delay);

  private:
    virtual bool DoEnqueue (Ptr<QueueDiscItem> item);
    virtual Ptr<QueueDiscItem> DoDequeue (void);
    virtual Ptr<const QueueDiscItem> DoPeek (void) const;
    virtual bool CheckConfig (void);
    virtual void InitializeParams (void);

    void FetchToOutQueue (Ptr<DelayClass> fromClass);

    std::map<int32_t, Ptr<DelayClass> > m_delayClasses;
    std::queue<Ptr<const QueueDiscItem> > m_outQueue;
    EventId m_event;
  };

}

#endif
