//
// AttackTracker.h - Tracks active attacks across the simulation, such as spoofed bots in DNS amplification
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

#ifndef __DDOSIMU5G_ATTACKTRACKER_H
#define __DDOSIMU5G_ATTACKTRACKER_H

#include <set>
#include <string>
#include <omnetpp.h>
#include <inet/networklayer/common/L3Address.h>

namespace ddosimu5g {

/**
 * Singleton to track active attacks across simulation
 */
class AttackTracker {
private:
    inline static AttackTracker* instance = nullptr;
    std::set<std::string> activeSpoofedBots;  // Bots doing DNS amplification
    
    AttackTracker() {}
    
public:
    static AttackTracker* getInstance() {
        if (!instance) {
            instance = new AttackTracker();
        }
        return instance;
    }
    
    void registerDnsAmplificationBot(const inet::L3Address& botAddr) {
        activeSpoofedBots.insert(botAddr.str());
        std::cout << "[AttackTracker] Registered DNS amplification bot: " << botAddr << std::endl;
    }
    
    void unregisterDnsAmplificationBot(const inet::L3Address& botAddr) {
        activeSpoofedBots.erase(botAddr.str());
    }
    
    bool isDnsAmplificationBot(const inet::L3Address& botAddr) const {
        return activeSpoofedBots.find(botAddr.str()) != activeSpoofedBots.end();
    }
};

} // namespace ddosimu5g

#endif