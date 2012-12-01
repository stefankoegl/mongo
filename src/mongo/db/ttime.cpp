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
     * {_id: {_id: ObjectId(1234), transaction_start: Timestamp(789456, 1)}, transaction_end: null, a: 1}
     */
    BSONObj wrapObjectId(BSONObj obj, unsigned long long int val, unsigned int inc) {

        /* only do all that, if we are not dealing with a temporal object already */
        if ( !obj.getFieldDotted("_id.transaction_start").eoo() ) {
            return obj;
        }

        BSONObjBuilder bb;

        /* move original _id into an transaction-time _id object */
        BSONObjBuilder temporalId(bb.subobjStart("_id"));

        BSONElement idElem = obj.getField("_id");
        if( idElem.eoo() )
        {
            temporalId.appendOID("_id", 0, true );
        }
        else
        {
            temporalId.append(idElem);
        }

        temporalId.appendTimestamp("transaction_start", val, inc);
        temporalId.done();

        /* append transaction_end outside of _id */
        bb.appendNull("transaction_end");
        bb.appendElementsUnique(obj);

        obj = bb.obj();

        BSONElement el = obj.getFieldDotted("_id.transaction_start");
        BSONElementManipulator(el).initTimestamp();

        return obj;
    }

    BSONObj addCurrentVersionCriterion(BSONObj pattern) {
        BSONObjBuilder b;
        b.appendNull("transaction_end");
        b.appendElementsUnique(pattern);
        return b.obj();
    }

    BSONObj setTransactionEndTimestamp(BSONObj obj) {
        obj = obj.replaceTimestamp("transaction_end");
        BSONElement endTimestamp = obj.getField("transaction_end");
        BSONElementManipulator(endTimestamp).initTimestamp();
        return obj;
    }

    BSONObj setTransactionStartTimestamp(BSONObj newObj, BSONObj prevObj)
    {
        Date_t endTimestampTime = prevObj.getFieldDotted("transaction_end").timestampTime();
        unsigned int endTimestampInc = prevObj.getFieldDotted("transaction_end").timestampInc();

        BSONElement idValue = prevObj.getFieldDotted("_id._id");
        BSONObjBuilder bb;
        bb.append(idValue);
        bb.appendElementsUnique(newObj);
        return wrapObjectId(bb.obj(), endTimestampTime, endTimestampInc);
    }

    void addFromCondition(BSONObjBuilder &bb, BSONElement from)
    {
        if( !from.isNull() )
        {
            // and either is still current
            BSONArrayBuilder arr( bb.subarrayStart( "$or" ) );

            BSONObjBuilder upperNull(arr.subobjStart());
            upperNull.appendNull(StringData("transaction_end"));
            upperNull.done();

            // ... or ended after the from-timestamp
            BSONObjBuilder upper(arr.subobjStart());
            BSONObjBuilder upperVal(upper.subobjStart(StringData("transaction_end")));
            upperVal.appendAs(from, "$gte");
            upperVal.done();
            upper.done();

            arr.done();
        }
    }

    void addToCondition(BSONObjBuilder &bb, BSONElement to)
    {
        if( !to.isNull() )
        {
            // the transaction started before the to-timestamp
            BSONObjBuilder startT(bb.subobjStart(StringData("_id.transaction_start")));
            startT.appendAs(to, "$lte");
            startT.done();
        }
    }

    BSONObj addTemporalCriteria(BSONObj query)
    {
        /* no temporal criterion specified, return only current documents */
        if( !query.hasElement("transaction") )
        {
            query = addCurrentVersionCriterion(query);
            query = query.removeField("transaction");
            return query;
        }

        /* explicitly requested current documents only */
        BSONElement current = query.getFieldDotted("transaction.current");
        if( !current.eoo() )
        {
            /* TODO: assert current.trueValue() */
            query = addCurrentVersionCriterion(query);
            query = query.removeField("transaction");
            return query;
        }

        BSONElement inrangeElem = query.getFieldDotted("transaction.inrange");
        if( !inrangeElem.eoo() )
        {
            massert(1234000, "must contain array", inrangeElem.type() == Array);
            vector<BSONElement> elems = inrangeElem.Array();

            massert(1234001, "array must contain two elements", elems.size() == 2);
            BSONElement fst = elems[0];
            BSONElement snd = elems[1];

            massert(1234002, "array must contain at least one non-null element", fst.isNull() && snd.isNull());

            BSONObjBuilder bb;

            addFromCondition(bb, fst);
            addToCondition(bb, snd);

            // all other conditions are inserted afterwards
            bb.appendElementsUnique(query);
            query = query.removeField("transaction");
            query = bb.obj();
            return query;
        }

        /* return all document versions */
        BSONElement allElem = query.getFieldDotted("transaction.all");
        if( !allElem.eoo() )
        {
            /* TODO: assert allElem.trueValue() */
            query = query.removeField("transaction");
            return query;
        }

        /* return document versions that were current at a specific point in time */
        BSONElement atElem = query.getFieldDotted("transaction.at");
        if (!atElem.eoo())
        {
            BSONObjBuilder bb;

            /* from and to is the same timestamp */
            addFromCondition(bb, atElem);
            addToCondition(bb, atElem);

            // all other conditions are inserted afterwards
            bb.appendElementsUnique(query);
            query = bb.obj();
            query = query.removeField("transaction");
            return query;
        }

        /* no supported transaction-time query found */
        /* TODO: assert false with proper error message */
        query = query.removeField("transaction");
        return query;
    }

    BSONObj addTemporalOrder(BSONObj order)
    {
        BSONElement transaction = order.getField("transaction");

        if( !transaction.eoo() )
        {
            /* replace transaction with transaction_id */
            BSONObjBuilder bb;
            bb.appendAs(transaction, "transaction_end");
            bb.appendElementsUnique(order.removeField("transaction"));
            return bb.obj();
        }

        return order;
    }
}

