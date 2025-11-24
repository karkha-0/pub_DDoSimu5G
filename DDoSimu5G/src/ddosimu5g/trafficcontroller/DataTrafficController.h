//
// Class developed by EIT, Lund University, Karim Khalil PhD
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

#ifndef SRC_DATATRAFFICCONTROLLER_H_
#define SRC_DATATRAFFICCONTROLLER_H_

#include <omnetpp.h>
#include "nlohmann/json.hpp"
#include <fstream>

#include "inet/networklayer/common/L3AddressResolver.h"

using namespace omnetpp;
using json = nlohmann::json;

class DataTrafficController : public cSimpleModule {
private:
    nlohmann::json infectionData;

protected:
    omnetpp::simsignal_t infectionTimeSignal_;

    bool enableTrafficMod = false;  // default value
    int trafficMod_pktSize = 512;   // default packet size in bytes
    double trafficMod_dataRate = 1;    // default sampling rate in seconds

    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;

    void parseInfectionData(const std::string &filePath);
    void scheduleInfectionEvents();
    void updateTraffic(int nodeId);
    void ddosTraffic(int nodeId);


public:
    DataTrafficController();
    virtual ~DataTrafficController();
};

#endif
