/*
 * ttime.h
 *
 *  Created on: Oct 22, 2012
 *      Author: Stefan Kögl
 *
 *  Functions for adding transaction-time support
 */

#ifndef TTIME_H_
#define TTIME_H_

#include "mongo/bson/bsonobj.h"
#include "mongo/db/queryutil.h"

namespace mongo {

    BSONObj wrapObjectId(BSONObj obj, unsigned long long int val = 0, unsigned inc = 0);

    BSONObj addCurrentVersionCriterion(BSONObj pattern);

    BSONObj setTransactionEndTimestamp(BSONObj obj);

    BSONObj setTransactionStartTimestamp(BSONObj newObj, BSONObj prevObj);

    BSONObj addTemporalCriteria(BSONObj query);

    BSONObj addTemporalOrder(BSONObj order);

    BSONObj getTTLQuery(const char* fieldName, long long expireField);

    BSONObj modifyTransactionTimeIndex(BSONObj key);

    BSONObj getIncludeHistory(BSONObj query, ParsedQuery &pq);
}

#endif /* TTIME_H_ */
