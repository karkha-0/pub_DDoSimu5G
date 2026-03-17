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

#ifndef __DDOSIMU5G_ATTACKREGISTRY_H_
#define __DDOSIMU5G_ATTACKREGISTRY_H_

#include <map>
#include <string>

namespace ddosimu5g {

/**
 * @brief Registry for attack module types supporting self-registration pattern
 * 
 * This class maintains a mapping between attack type strings (from JSON config files)
 * and OMNeT++ module type names. Attack classes register themselves at static 
 * initialization time, enabling extensibility without modifying DataTrafficController.
 * 
 * Usage:
 *   1. Attack classes call registerAttack() in static initializer
 *   2. BaseAttackApp factory calls getModuleName() to create correct type
 *   3. New attack types require no changes to controller code
 */
class AttackRegistry {
private:
    /**
     * @brief Get the registry map (Meyer's Singleton)
     * @return Reference to the static registry map
     * 
     * This ensures the map is initialized before first use, avoiding
     * static initialization order fiasco.
     */
    static std::map<std::string, std::string>& getRegistry();
    
public:
    /**
     * @brief Register an attack type with its corresponding module name
     * @param attackType Attack type identifier from JSON config (e.g., "udp_flood")
     * @param moduleName Full OMNeT++ module type name (e.g., "ddosimu5g.apps.adversarialApps.ddos.UdpFloodAttack")
     * 
     * Called by attack classes during static initialization to register themselves.
     */
    static void registerAttack(const std::string& attackType, const std::string& moduleName);
    
    /**
     * @brief Retrieve module name for given attack type
     * @param attackType Attack type identifier from JSON config
     * @return Full OMNeT++ module type name
     * @throws std::runtime_error if attack type not registered
     * 
     * Called by BaseAttackApp::createFromProfile() factory method to determine
     * which module type to instantiate.
     */
    static std::string getModuleName(const std::string& attackType);
    
    /**
     * @brief Check if attack type is registered
     * @param attackType Attack type identifier to check
     * @return true if registered, false otherwise
     * 
     * Used to determine whether to use registry or fallback mechanism.
     */
    static bool isRegistered(const std::string& attackType);
};

} // namespace ddosimu5g

#endif // __DDOSIMU5G_ATTACKREGISTRY_H_
