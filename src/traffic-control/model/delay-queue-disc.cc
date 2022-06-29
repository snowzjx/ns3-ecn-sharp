#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/delay-queue-disc.h"

namespace ns3 {
  
  NS_LOG_COMPONENT_DEFINE ("DelayQueueDisc");
  NS_OBJECT_ENSURE_REGISTERED (DelayQueueDisc);

  TypeId
  DelayClass::GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::DelayClass")
      .SetParent<Object> ()
      .SetGroupName ("TrafficControl")
      .AddConstructor<DelayClass> ()
      ;

    return tid;
  }

  DelayClass::DelayClass ()
  {
    NS_LOG_FUNCTION (this);
  }


  TypeId
  DelayQueueDisc::GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::DelayQueueDisc")
      .SetParent<QueueDisc> ()
      .SetGroupName ("TrafficControl")
      .AddConstructor<DelayQueueDisc> ()
      ;
    return tid;
  }

  DelayQueueDisc::DelayQueueDisc ()
  {
    NS_LOG_FUNCTION (this);
  }

  DelayQueueDisc::~DelayQueueDisc ()
  {
    NS_LOG_FUNCTION (this);
    m_delayClasses.clear ();
  }

  void
  DelayQueueDisc::AddDelayClass (int32_t cl, Time delay)
  {
    Ptr<DelayClass> delayClass = CreateObject<DelayClass> ();
    delayClass->cl = cl;
    delayClass->delay = delay;
    m_delayClasses[cl] = delayClass;
  }

  void
  DelayQueueDisc::FetchToOutQueue (Ptr<DelayClass> fromClass)
  {
    Ptr<const QueueDiscItem> item = 0;
    item = fromClass->queue.front ();
    fromClass->queue.pop ();

    if (item == 0)
      {
        NS_LOG_ERROR ("Cannot fetch from qdisc, which is impossible to happen!");
        return;
      }

    m_outQueue.push (item);
    NS_LOG_INFO ("Fetch from class: " << fromClass->cl << " to out queue");
  }

  bool
  DelayQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
  {
    NS_LOG_FUNCTION (this << item);
    Ptr<DelayClass> delayClass = 0;

    int32_t cl = Classify (item);

    std::map<int32_t, Ptr<DelayClass> >::iterator itr = m_delayClasses.find (cl);
    if (itr == m_delayClasses.end ())
      {
        NS_LOG_ERROR ("Cannot find class, dropping the packet");
        Drop (item);
        return false;
      }

    delayClass = itr->second;

    delayClass->queue.push (ConstCast<const QueueDiscItem> (item));
    NS_LOG_INFO ("Enqueue to class: " << cl);

    m_event = Simulator::Schedule (delayClass->delay, &DelayQueueDisc::FetchToOutQueue, this, delayClass);

    return true; 
  }

  Ptr<QueueDiscItem>
  DelayQueueDisc::DoDequeue (void)
  {
    NS_LOG_FUNCTION (this);
    Ptr<QueueDiscItem> item = 0;

    if (m_outQueue.empty ())
      {
        NS_LOG_INFO ("Nothing to dequeue, skipping this chance...");
        return 0;
      }

    item = ConstCast<QueueDiscItem> (m_outQueue.front ());
    m_outQueue.pop ();

    return item;
  }

  Ptr<const QueueDiscItem>
  DelayQueueDisc::DoPeek (void) const
  {
    return m_outQueue.front ();
  }

  bool
  DelayQueueDisc::CheckConfig (void)
  {
    NS_LOG_FUNCTION (this);
    return true;
  }

  void 
  DelayQueueDisc::InitializeParams (void)
  {
    NS_LOG_FUNCTION (this);
  }


}
