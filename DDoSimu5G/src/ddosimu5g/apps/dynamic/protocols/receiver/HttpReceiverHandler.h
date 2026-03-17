//
// HttpReceiverHandler.h - HTTP receiver handler
//
// Class developed by EIT, Lund University, Karim Khalil PhD
// Development assisted by AI tools (GitHub Copilot)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

#ifndef __DDOSIMU5G_HTTPRECEIVERHANDLER_H
#define __DDOSIMU5G_HTTPRECEIVERHANDLER_H

#include "ddosimu5g/apps/dynamic/protocols/receiver/TcpReceiverHandler.h"

namespace ddosimu5g {

/**
 * @brief HTTP receiver handler - HTTP server
 * 
 * Extends TcpReceiverHandler with HTTP-specific reply behavior
 * (larger responses, HTTP-specific multiplier)
 */
class HttpReceiverHandler : public TcpReceiverHandler {
public:
    HttpReceiverHandler(DynamicTrafficReceiver* parent, int port);
    virtual ~HttpReceiverHandler() = default;
    
    // ReceiverProtocolHandler interface
    std::string getProtocolName() const override { return "HTTP"; }

protected:
    void sendReply(inet::TcpSocket* socket, inet::Packet* request) override;
};

} // namespace ddosimu5g

#endif
