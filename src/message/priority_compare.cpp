#include "../../include/message/message_queue.h"

namespace next_gen {

bool PriorityMessageQueue::PriorityCompare::operator()(
    const std::pair<int, std::unique_ptr<Message>>& a,
    const std::pair<int, std::unique_ptr<Message>>& b
) const {
    // 较高的优先级值意味着较高的优先级（与默认行为相反）
    return a.first < b.first;
}

} // namespace next_gen 