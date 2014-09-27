#ifndef CORE_SERVER_CONNECTOR_CONNECTOR_FRAME_RESPONSE_H_
#define CORE_SERVER_CONNECTOR_CONNECTOR_FRAME_RESPONSE_H_

#include <string>
#include <vector>

#include "core/decision.h"
#include "core/key_set.h"

// TODO(mayah): Should we split this to PipeConnectorFrameResponse and
// HumanConnectorFrameResponse? Or, split using Decision or Key?
// ConnectorFrameResponse is a response that a cpu sends to the server.
class ConnectorFrameResponse {
public:
    static ConnectorFrameResponse parse(const char* str);

    bool isValid() const;

    bool received = false;
    int frameId = -1;
    Decision decision;
    std::string msg;

    // For HumanConnector ReceivedData.
    // Basically the RecievedData of HumanConnector does not have any Decision.
    // Instead, we have keySet.
    KeySet keySet;

    std::string mawashi_area;
    std::string original;
    int usec = 0;
};

#endif
