//
// HttpFloodAttack - NED definition
//

package ddosimu5g.apps.adversarialApps.ddos;

import ddosimu5g.apps.adversarialApps.BaseAttackApp;

simple HttpFloodAttack extends BaseAttackApp
{
    parameters:
        @class(ddosimu5g::HttpFloodAttack);
        attackType = default("http_flood");
        
        // HTTP flood specific parameters
        double requestsPerSecond = default(10);
        int numConnections = default(10);
        string httpMethod = default("GET");  // "GET" or "POST"
        string requestPaths = default("/,/index.html,/api/data");  // Comma-separated
        string userAgent = default("Mozilla/5.0 (Botnet)");
        int contentLength @unit(B) = default(512B);  // For POST requests
        bool keepAlive = default(true);
        
        // Interface
        string interfaceTableModule = default("^.^.interfaceTable");
}
