/**
*    Copyright (C) 2012 10gen Inc.
*
*    This program is free software: you can redistribute it and/or  modify
*    it under the terms of the GNU Affero General Public License, version 3,
*    as published by the Free Software Foundation.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU Affero General Public License for more details.
*
*    You should have received a copy of the GNU Affero General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <string>
#include <vector>

#include "mongo/base/disallow_copying.h"
#include "mongo/base/status.h"
#include "mongo/db/auth/action_set.h"
#include "mongo/db/auth/action_type.h"
#include "mongo/db/auth/auth_external_state.h"
#include "mongo/db/auth/principal.h"
#include "mongo/db/auth/principal_name.h"
#include "mongo/db/auth/principal_set.h"
#include "mongo/db/auth/privilege.h"
#include "mongo/db/auth/privilege_set.h"

namespace mongo {

    /**
     * Internal secret key info.
     */
    struct AuthInfo {
        AuthInfo();
        string user;
        string pwd;
    };
    extern AuthInfo internalSecurity; // set at startup and not changed after initialization.

    /**
     * Contains all the authorization logic for a single client connection.  It contains a set of
     * the principals which have been authenticated, as well as a set of privileges that have been
     * granted by those principals to perform various actions.
     * An AuthorizationManager object is present within every mongo::Client object, therefore there
     * is one per thread that corresponds to an incoming client connection.
     */
    class AuthorizationManager {
        MONGO_DISALLOW_COPYING(AuthorizationManager);
    public:

        static const std::string SERVER_RESOURCE_NAME;
        static const std::string CLUSTER_RESOURCE_NAME;

        // Takes ownership of the externalState.
        explicit AuthorizationManager(AuthExternalState* externalState);
        ~AuthorizationManager();

        // Takes ownership of the principal (by putting into _authenticatedPrincipals).
        void addAuthorizedPrincipal(Principal* principal);

        // Returns the authenticated principal with the given name.  Returns NULL
        // if no such user is found.
        // Ownership of the returned Principal remains with _authenticatedPrincipals
        Principal* lookupPrincipal(const PrincipalName& name);

        // Gets an iterator over the names of all authenticated principals stored in this manager.
        PrincipalSet::NameIterator getAuthenticatedPrincipalNames();

        // Removes any authenticated principals whose authorization credentials came from the given
        // database, and revokes any privileges that were granted via that principal.
        void logoutDatabase(const std::string& dbname);

        // Grant this connection the given privilege.
        Status acquirePrivilege(const Privilege& privilege,
                                const PrincipalName& authorizingPrincipal);

        // Adds a new principal with the given principal name and authorizes it with full access.
        // Used to grant internal threads full access.
        void grantInternalAuthorization(const std::string& principalName);

        // Checks if this connection has been authenticated as an internal user.
        bool hasInternalAuthorization();

        // Checks if this connection has the privileges required to perform the given action
        // on the given resource.  Contains all the authorization logic including handling things
        // like the localhost exception.  Returns true if the action may proceed on the resource.
        bool checkAuthorization(const std::string& resource, ActionType action);

        // Same as above but takes an ActionSet instead of a single ActionType.  Returns true if
        // all of the actions may proceed on the resource.
        bool checkAuthorization(const std::string& resource, ActionSet actions);

        // Parses the privilege documents and acquires all privileges that the privilege document
        // grants
        Status acquirePrivilegesFromPrivilegeDocument(const std::string& dbname,
                                                      const PrincipalName& principal,
                                                      const BSONObj& privilegeDocument);

        // Returns the privilege document with the given user name in the given database. Currently
        // this information comes from the system.users collection in that database.
        Status getPrivilegeDocument(const std::string& dbname,
                                    const PrincipalName& userName,
                                    BSONObj* result) {
            return _externalState->getPrivilegeDocument(dbname, userName, result);
        }

        // Checks if this connection has the privileges necessary to perform a query on the given
        // namespace.
        Status checkAuthForQuery(const std::string& ns);

        // Checks if this connection has the privileges necessary to perform an update on the given
        // namespace.
        Status checkAuthForUpdate(const std::string& ns, bool upsert);

        // Checks if this connection has the privileges necessary to perform an insert to the given
        // namespace.
        Status checkAuthForInsert(const std::string& ns);

        // Checks if this connection has the privileges necessary to perform a delete on the given
        // namespace.
        Status checkAuthForDelete(const std::string& ns);

        // Checks if this connection has the privileges necessary to perform a getMore on the given
        // namespace.
        Status checkAuthForGetMore(const std::string& ns);

        // Checks if this connection is authorized for all the given Privileges
        Status checkAuthForPrivileges(const vector<Privilege>& privileges);

        // Given a database name and a readOnly flag return an ActionSet describing all the actions
        // that an old-style user with those attributes should be given.
        static ActionSet getActionsForOldStyleUser(const std::string& dbname, bool readOnly);

        // Parses the privilege document and returns a PrivilegeSet of all the Privileges that
        // the privilege document grants.
        static Status buildPrivilegeSet(const std::string& dbname,
                                        const PrincipalName& principal,
                                        const BSONObj& privilegeDocument,
                                        PrivilegeSet* result);

    private:

        // Parses the old-style (pre 2.4) privilege document and returns a PrivilegeSet of all the
        // Privileges that the privilege document grants.
        static Status _buildPrivilegeSetFromOldStylePrivilegeDocument(
                const std::string& dbname,
                const PrincipalName& principal,
                const BSONObj& privilegeDocument,
                PrivilegeSet* result);

        scoped_ptr<AuthExternalState> _externalState;

        // All the privileges that have been acquired by the authenticated principals.
        PrivilegeSet _acquiredPrivileges;
        // All principals who have been authenticated on this connection
        PrincipalSet _authenticatedPrincipals;
    };

} // namespace mongo
