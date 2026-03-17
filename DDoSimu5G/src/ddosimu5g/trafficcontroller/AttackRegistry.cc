//
// AttackRegistry - Registry for attack module type self-registration
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

#include "AttackRegistry.h"
#include <stdexcept>
#include <iostream>

namespace ddosimu5g {

// Initialize static registry
//std::map<std::string, std::string> AttackRegistry::registry;
std::map<std::string, std::string>& AttackRegistry::getRegistry() {
    static std::map<std::string, std::string> registry;  // Initialized on first use
    return registry;
}

/**
 * @brief Register an attack type with its corresponding module name
 * @param attackType Attack type identifier from JSON config (e.g., "udp_flood")
 * @param moduleName Full OMNeT++ module type name (e.g., "ddosimu5g.apps.adversarialApps.ddos.UdpFloodAttack")
 */
void AttackRegistry::registerAttack(const std::string& attackType, const std::string& moduleName) {
    //registry[attackType] = moduleName;
    std::cout << "[AttackRegistry] Registered: '" << attackType << "' -> '" << moduleName << "'" << std::endl;
    getRegistry()[attackType] = moduleName;
}

/**
 * @brief Retrieve module name for given attack type
 * @param attackType Attack type identifier from JSON config
 * @return Full OMNeT++ module type name
 * @throws std::runtime_error if attack type not registered
 */
std::string AttackRegistry::getModuleName(const std::string& attackType) {
    auto& registry = getRegistry();
    auto it = registry.find(attackType);
    if (it == registry.end()) {
        std::cerr << "[AttackRegistry] ERROR: Attack type '" << attackType << "' not registered!" << std::endl;
        throw std::runtime_error("Unknown attack type: " + attackType);
    }
    return it->second;
}

/**
 * @brief Check if attack type is registered
 * @param attackType Attack type identifier to check
 * @return true if registered, false otherwise
 */
bool AttackRegistry::isRegistered(const std::string& attackType) {
    auto& registry = getRegistry();
    bool found = registry.find(attackType) != registry.end();
    
    std::cout << "[AttackRegistry] Checking if '" << attackType << "' is registered: " 
              << (found ? "YES" : "NO") << std::endl;
    return found;
}

} // namespace ddosimu5g
