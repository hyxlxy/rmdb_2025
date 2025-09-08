#include "transaction.h"
#include "snapshot_manager.h"

Transaction::~Transaction()
{
    // 清理快照
    if (snapshot_ != nullptr)
    {
        delete static_cast<Snapshot *>(snapshot_);
        snapshot_ = nullptr;
    }
}
