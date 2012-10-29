/*
 * ttime.h
 *
 *  Created on: Oct 22, 2012
 *      Author: Stefan Kögl
 *
 *  Functions for adding transaction-time support
 */

#include "mongo/pch.h"

#include "mongo/db/ttime.h"
#include "mongo/db/jsobjmanipulator.h"

namespace mongo {

    /*
     * wraps the _id field of an object in a new object
     *
     * {_id: ObjectId(1234), a: 1} =>
     * {_id: {_id: ObjectId(1234), transaction_start: Timestamp(789456, 1), transaction_end: null}, a: 1}
     */
    BSONObj wrapObjectId(BSONObj obj) {

        /* only do all that, if we are not dealing with a temporal object already */
        if ( !obj.getFieldDotted("_id.transaction_start").eoo() ) {
            return obj;
        }

        BSONElement idField = obj.getField("_id");

        /* move original _id into an transaction-time _id object */
        BSONObjBuilder bb;
        bb.append(idField);
        bb.appendTimestamp("transaction_start");
        bb.appendNull("transaction_end");
        BSONObj temporalId = bb.obj();

        BSONElementManipulator::lookForTimestamps(temporalId);

        obj = obj.replaceField("_id", temporalId);
        return obj;
    }

    BSONObj addCurrentVersionCriterion(BSONObj pattern) {
        BSONObjBuilder b;
        b.appendNull("_id.transaction_end");
        b.appendElementsUnique(pattern);
        return b.obj();
    }

    BSONObj setTransactionEndTimestamp(BSONObj obj) {
        BSONObj idField = obj.getField("_id").embeddedObject();
        idField = idField.replaceTimestamp("transaction_end");
        BSONElement endTimestamp = idField.getField("transaction_end");
        BSONElementManipulator(endTimestamp).initTimestamp();
        return obj.replaceField("_id", idField);
    }

    void setTransactionStartTimestamp(BSONObj obj) {
        BSONElement startTimestamp = obj.getFieldDotted(
                "_id.transaction_start");
        BSONElementManipulator(startTimestamp).initTimestamp(true);
    }
}

