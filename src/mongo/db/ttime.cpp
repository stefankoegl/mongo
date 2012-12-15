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

    /*
     * Adds query criterion for current document versions
     *
     * Prevents updates/deletes from affected historic versions
     */
    BSONObj addCurrentVersionCriterion(const BSONObj pattern) {

        BSONElement endTimestamp = pattern.getField("transaction_end");
        uassert(999145, "Updating/deleting non-current document versions is not allowed", endTimestamp.eoo() || endTimestamp.isNull() );

        BSONObjBuilder b;
        b.appendNull("transaction_end");
        b.appendElementsUnique(pattern);
        return b.obj();
    }

    /*
     * Sets the transaction_end field when a document version becomes historic
     */
    BSONObj setTransactionEndTimestamp(BSONObj obj) {

        BSONElement endTimestamp = obj.getField("transaction_end");

        uassert(999146, "Can not set transaction_end timestamp for non-existing member", !endTimestamp.eoo() );
        uassert(999147, "Can not set transaction_end timestamp for historic document version", endTimestamp.isNull() );

        obj = obj.replaceTimestamp("transaction_end");
        endTimestamp = obj.getField("transaction_end");
        BSONElementManipulator(endTimestamp).initTimestamp();
        return obj;
    }

    /*
     * Sets the transaction_start timestamp of a new object to the
     * transaction_end timestamp of the now-historic document version
     */
    BSONObj setTransactionStartTimestamp(const BSONObj newObj, const BSONObj prevObj)
    {
        BSONElement endTimestamp = prevObj.getFieldDotted("transaction_end");

        uassert(999148, "Previous document version doesn't have transaction_end timestamp", !endTimestamp.eoo() );
        uassert(999149, "Previous document version has invalid value for transaction_end timestamp", endTimestamp.type() == Timestamp );

        Date_t endTimestampTime = endTimestamp.timestampTime();
        unsigned int endTimestampInc = endTimestamp.timestampInc();

        BSONElement idValue = prevObj.getFieldDotted("_id._id");
        BSONObjBuilder bb;
        bb.append(idValue);
        bb.appendElementsUnique(newObj);
        return wrapObjectId(bb.obj(), endTimestampTime, endTimestampInc);
    }

    /*
     * Adds the from-part of a time-range query
     */
    void addFromCondition(BSONObjBuilder &bb, const BSONElement from)
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

    /*
     * Adds the to-part of a time-range query
     */
    void addToCondition(BSONObjBuilder &bb, BSONElement const to)
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
            return addCurrentVersionCriterion(query);
        }

        /* explicitly requested current documents only */
        BSONElement current = query.getFieldDotted("transaction.current");
        if( !current.eoo() )
        {
            uassert(999150, "\"current\" can only be used with true", current.isBoolean() && current.trueValue() );

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

            massert(1234002, "array must contain at least one non-null element", !fst.isNull() || !snd.isNull());

            BSONObjBuilder bb;

            addFromCondition(bb, fst);
            addToCondition(bb, snd);

            // all other conditions are inserted afterwards
            bb.appendElementsUnique(query);
            query = bb.obj();
            query = query.removeField("transaction");
            return query;
        }

        /* return all document versions */
        BSONElement allElem = query.getFieldDotted("transaction.all");
        if( !allElem.eoo() )
        {
            uassert(999151, "\"all\" can only be used with true", allElem.isBoolean() && allElem.trueValue() );
            return query.removeField("transaction");
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
        uassert(999152, "unknown value for \"transaction\"", false);
    }

    /*
     * Replace a "transaction" by "transaction_end" in the order object
     */
    BSONObj addTemporalOrder(const BSONObj order)
    {
        BSONElement transaction = order.getField("transaction");

        if( transaction.eoo() )
        {
            return order;
        }

        /* replace transaction with transaction_id */
        return order.renameField("transaction", "transaction_end");
    }

    /**
     * Build a TTL query that works on Date and Timestamp objects
     */
    BSONObj getTTLQuery(const char* fieldName, long long expireField)
    {
        BSONObjBuilder b;
        BSONArrayBuilder or_(b.subarrayStart("$or"));
        BSONObjBuilder date(or_.subobjStart());
        BSONObjBuilder dateField(date.subobjStart(fieldName));
        dateField.appendDate( "$lt" , curTimeMillis64() - ( 1000 * expireField ) );
        dateField.done();
        date.done();

        BSONObjBuilder timestamp(or_.subobjStart());
        BSONObjBuilder timestampField(date.subobjStart(fieldName));
        // we don't need a unique timestamp here, so we don't need to lock in OpTime.now()
        timestampField.appendTimestamp( "$tlt", 1000 * (time(0) - expireField), 0 );
        timestampField.done();
        timestamp.done();

        or_.done();

        return b.obj();
    }

    /*
     * Takes an index object, eg {key: { ... }, name: "myindex", ...}
     * and modified the "key" member to include the transaction_end timestamp
     *
     * * "transaction" is replaced by transaction_end
     * * if no "transaction" is given, transaction_end is inserted in the beginning
     * * to disable, pass "transaction: 0"
     */
    BSONObj modifyTransactionTimeIndex(BSONObj idx)
    {
        BSONObj key = idx.getObjectField("key");

        // if a transition_end field has been included explicitly, don't do anything else
        BSONElement transactionEndElem = key.getField("transaction_end");
        if( !transactionEndElem.eoo() )
        {
            return idx;
        }

        /* if the key doesn't include a transaction field, we add
         * the transaction_end as the first member */
        BSONElement transactionElem = key.getField("transaction");
        if( transactionElem.eoo() )
        {
            BSONObjBuilder b;
            b.appendNumber("transaction_end", 1);
            b.appendElements(key);
            idx = idx.replaceField("key", b.obj());
            return idx;
        }

        uassert(999423, "parameter of transaction must be a number", transactionElem.isNumber());

        /* transaction: 0 means that we don't want to include a transaction-timestamp in the index */
        if( transactionElem.number() == 0 )
        {
            return idx.replaceField("key", key.removeField("transaction"));
        }
        else
        {
            key = key.renameField("transaction", "transaction_end");
            return idx.replaceField("key", key);
        }
    }
}

