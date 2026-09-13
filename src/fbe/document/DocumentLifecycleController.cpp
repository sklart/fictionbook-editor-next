#include "stdafx.h"
#include "DocumentLifecycleController.h"

// Transactional creation/loading remains based on PendingDocument.  The frame
// owns MSHTML presentation; this unit owns only lifecycle result vocabulary.
