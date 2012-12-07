/*    Copyright 2012 10gen Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#pragma once

#include "mongo/dbtests/mock/mock_remote_db_server.h"
#include "mongo/db/repl/rs_config.h"

#include <string>
#include <map>
#include <vector>

namespace mongo {
    /**
     * This is a helper class for managing a replica set consisting of
     * MockRemoteDBServer instances.
     *
     * Warning: Not thread-safe
     */
    class MockReplicaSet {
    public:
        /**
         * Creates a mock replica set and automatically mocks the isMaster
         * and replSetGetStatus commands based on the default replica set
         * configuration.
         *
         * @param setName The name for this replica set
         * @param nodes The initial number of nodes for this replica set
         */
        MockReplicaSet(const std::string& setName, size_t nodes);
        ~MockReplicaSet();

        //
        // getters
        //

        std::string getSetName() const;
        std::string getConnectionString() const;
        std::vector<mongo::HostAndPort> getHosts() const;
        const std::vector<mongo::ReplSetConfig::MemberCfg>& getReplConfig() const;
        std::string getPrimary() const;
        const std::vector<std::string>& getSecondaries() const;

        /**
         * Sets the configuration for this replica sets. This also has a side effect
         * of mocking the ismaster and replSetGetStatus command responses based on
         * the new config.
         */
        void setConfig(const std::vector<mongo::ReplSetConfig::MemberCfg>& newConfig);

        /**
         * @return pointer to the mocked remote server with the given hostName.
         *     NULL if host doesn't exists.
         */
        MockRemoteDBServer* getNode(const std::string& hostName);

        /**
         * Kills a node belonging to this set.
         *
         * @param hostName the name of the replica node to kill.
         */
        void kill(const std::string& hostName);

        /**
         * Kills a set of host belonging to this set.
         *
         * @param hostList the list of host names of the servers to kill.
         */
        void kill(const std::vector<std::string>& hostList);

        /**
         * Reboots a node.
         *
         * @param hostName the name of the host to reboot.
         */
        void restore(const std::string& hostName);

    private:
        /**
         * Mocks the ismaster command based on the information on the current
         * replica set configuration.
         */
        void mockIsMasterCmd();

        /**
         * Mocks the replSetGetStatus command based on the current states of the
         * mocked servers.
         */
        void mockReplSetGetStatusCmd();

        /**
         * @return the replica set state of the given host
         */
        int getState(const std::string& host) const;

        const std::string _setName;
        std::map<std::string, MockRemoteDBServer*> _nodeMap;
        std::vector<mongo::ReplSetConfig::MemberCfg> _replConfig;

        std::string _primaryHost;
        std::vector<std::string> _secondaryHosts;
    };
}
