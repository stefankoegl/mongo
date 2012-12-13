// ttimetests.cpp : ttime.{h,cpp} unit tests.

/**
 *  Created on: Dec 11, 2012
 *      Author: Stefan Kögl
 */

#include "pch.h"
#include "dbtests.h"


namespace TransactionTimeTests {


    class ClientBase {
    public:
        ClientBase() {
            mongo::lastError.reset( new LastError() );
        }
        ~ClientBase() {
            mongo::lastError.release();
        }
    protected:
        static void insert( const char *ns, BSONObj o ) {
            client_.insert( ns, o );
        }
        static void update( const char *ns, BSONObj q, BSONObj o, bool upsert = 0 ) {
            client_.update( ns, Query( q ), o, upsert );
        }
        static void remove( const char *ns, Query q, bool justOne ) {
            client_.remove( ns, q, justOne );
        }
        static unsigned long long count( const char *ns, BSONObj q = BSONObj() ) {
            return client_.count( ns, q );
        }
        static bool error() {
            return !client_.getPrevError().getField( "err" ).isNull();
        }
        DBDirectClient &client() const { return client_; }

        static DBDirectClient client_;
    };
    DBDirectClient ClientBase::client_;

    class TTimeUpdates : public ClientBase {
    public:
        ~TTimeUpdates() {
            client().dropCollection( "unittests.ttimetests.TTimeUpdates" );
        }
        void run() {
            const char *ns = "unittests.ttimetests.TTimeUpdates";
            client().createCollection( ns, 1024, false, 0, 0, true);

            unsigned long long result = count(ns);
            ASSERT_EQUALS( (unsigned long long int)0, result );

            insert( ns, BSON( "a" << 0 ) );
            update( ns, BSON( "a" << 0), BSON("$inc" << BSON("a" << 1)), false);

            result = count(ns);
            ASSERT_EQUALS( (unsigned long long int)1, result );

            insert( ns, BSON( "a" << 3 ) );

            result = count(ns);
            ASSERT_EQUALS( (unsigned long long int)2, result );

            remove( ns, QUERY( "a" << 1 ), false );
            result = count(ns);
            ASSERT_EQUALS( (unsigned long long int)1, result );

            result = count(ns, BSON( "transaction" << BSON("all" << true)));
            ASSERT_EQUALS( (unsigned long long int)3, result );
        }
    };

    class All : public Suite {
    public:
        All() : Suite( "ttime" ) {
        }

        void setupTests() {
            add< TTimeUpdates >();
        }
    } myall;

} // namespace TransactionTimeTests

