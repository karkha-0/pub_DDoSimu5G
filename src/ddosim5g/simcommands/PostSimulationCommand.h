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

#ifndef SRC_POSTSIMULATIONCOMMAND_H_
#define SRC_POSTSIMULATIONCOMMAND_H_

#include <cstdlib>  // Include C standard library for system()
#include <omnetpp.h>  // Include OMNeT++ header
#include <sys/stat.h>  // For POSIX stat and mkdir functions
#include <sys/types.h> // For POSIX stat and mkdir functions
#include <cstdlib> // for system()

using namespace omnetpp;  // Use OMNeT++ namespace

class PostSimulationCommand : public cSimpleModule, public cISimulationLifecycleListener  {
    public:
        PostSimulationCommand();
        virtual ~PostSimulationCommand();

    protected:
        virtual void initialize() override;
        virtual void lifecycleEvent(SimulationLifecycleEventType eventType, cObject *details) override;
        virtual void listenerAdded() override {}
        virtual void listenerRemoved() override {}
};

#endif /* SRC_POSTSIMULATIONCOMMAND_H_ */
