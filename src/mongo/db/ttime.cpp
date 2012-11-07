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
    BSONObj wrapObjectId(BSONObj obj, unsigned long long int val, unsigned int inc) {

        /* only do all that, if we are not dealing with a temporal object already */
        if ( !obj.getFieldDotted("_id.transaction_start").eoo() ) {
            return obj;
        }

        BSONElement idField = obj.getField("_id");

        /* move original _id into an transaction-time _id object */
        BSONObjBuilder bb;
        bb.append(idField);
        bb.appendTimestamp("transaction_start", val, inc);
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

    BSONObj setTransactionStartTimestamp(BSONObj newObj, BSONObj prevObj)
    {
        Date_t endTimestampTime = prevObj.getFieldDotted("_id.transaction_end").timestampTime();
        unsigned int endTimestampInc = prevObj.getFieldDotted("_id.transaction_end").timestampInc();

        BSONElement idValue = prevObj.getFieldDotted("_id._id");
        BSONObjBuilder bb;
        bb.append(idValue);
        bb.appendElementsUnique(newObj);
        return wrapObjectId(bb.obj(), endTimestampTime, endTimestampInc);
    }

    BSONObj addTemporalCriteria(BSONObj query)
    {
        if( !query.hasElement("transaction") )
        {
            return addCurrentVersionCriterion(query);
        }

        BSONElement allElem = query.getFieldDotted("transaction.all");
        if( !allElem.eoo() )
        {
            /* TODO: assert allElem.trueValue() */
            return query;
        }

        BSONElement atElem = query.getFieldDotted("transaction.at");
        cout << atElem.type() << endl;
        if (!atElem.eoo())
        {
            BSONObjBuilder bb;

            // the transaction started before the at-timestamp
            BSONObjBuilder startT(bb.subobjStart(StringData("_id.transaction_start")));
            startT.appendAs(atElem, "$lte");
            startT.done();

            // and either is still current
            BSONArrayBuilder arr( bb.subarrayStart( "$or" ) );

            BSONObjBuilder upperNull(arr.subobjStart());
            upperNull.appendNull(StringData("_id.transaction_end"));
            upperNull.done();

            // ... or ended after the at-timestamp
            BSONObjBuilder upper(arr.subobjStart());
            BSONObjBuilder upperVal(upper.subobjStart(StringData("_id.transaction_end")));
            upperVal.appendAs(atElem, "$gte");
            upperVal.done();
            upper.done();

            arr.done();
            bb.done();

            // all other conditions are inserted afterwards
            query = query.removeField("transaction");
            bb.appendElementsUnique(query);
            query = bb.obj();
        }

        return query;
    }
}

