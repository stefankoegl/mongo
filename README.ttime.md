Transaction-time support prototype
==================================

This is a short introduction to the prototype of transaction-time support for
MongoDB. It will guide through the basic use cases using some examples.

Introduction
------------

The term [transaction-time](http://en.wikipedia.org/wiki/Transaction_time)
refers to the time when a record (or document) is valid inside a database.
This is typically combined with keeping previous (historic) versions of data in
the database (annotated with said time) after they has been updated or deleted.

This proptotype implements this behaviour for MongoDB. Please note that --
despite a possibly confusing name -- it does not introduce (multi-document)
transactions.


Creating a collection
---------------------

Transaction-time support is enabled on a per-collection basis. To create a
collection with transaction-time support enabled, execute

    > db.runCommand({create: "test", transactiontime: true})
    { "ok" : 1 }

The behaviour of other collections is not affected by transaction-time support.


Inserting documents
-------------------

When documents are inserted into a collections with transaction-time support,
some transformations are applied automatically (formatting applied for
clarity).

    > db.test.insert({a: 1})
    > db.test.find()
    {
        "_id" : {
            "_id" : ObjectId("50cb78bb5fe03295bf74621e"),
            "transaction_start" : Timestamp(1355511995000, 1)
        },
        "transaction_end" : null,
        "a" : 1
    }


The _id is wrapped into an object which additionally contains a
transaction_start member with a
[Timestamp](http://docs.mongodb.org/manual/core/document/#timestamps) from when
on the document is valid.  An additional transaction_end timestamp is inserted
after the _id which is initially set to null, indicating that the document is
still valid.


Updating documents
------------------

After updating a document, the old version is retained. However, by default
only the latest version is returned by find().

    > db.test.update({a: 1}, {$inc: {a: 1}})
    > db.test.find()
    {
        "_id" : {
            "_id" : ObjectId("1355512030000"),
            "transaction_start" : Timestamp(1355405527000, 1)
        },
        "transaction_end": null,
        "a": 2
    }

However, there are special conditions that can be passed to find in order to
retrieve "historic" documents.

    > db.test.find({transaction: {all: true}})
    {
        "_id" : {
            "_id" : ObjectId("50cb78bb5fe03295bf74621e"),
            "transaction_start" : Timestamp(1355511995000, 1),
        },
        "transaction_end" : Timestamp(1355512030000, 1),
        "a" : 1
    }
    {
        "_id" : {
            "_id" : ObjectId("50cb78bb5fe03295bf74621e"),
            "transaction_start" : Timestamp(1355512030000, 1)
        }, "transaction_end" : null,
        "a" : 2
    }



Deleting documents
------------------

When deleting a document, the transaction_end timestamp is set which marks this
version as no longer valid. A call to find() will give no results, as there is
no current document in the collection anymore.

    > db.test.remove({a: 2})
    > db.test.find()

When querying all versions again, you can see when each document version was
valid:

    > db.test.find({transaction: {all: true}})
    {
        "_id" : {
            "_id" : ObjectId("50cb78bb5fe03295bf74621e"),
            "transaction_start" : Timestamp(1355511995000, 1)   # t1
        },
        "transaction_end" : Timestamp(1355512030000, 1),        # t2
        "a" : 1
    }
    {
        "_id" : {
            "_id" : ObjectId("50cb78bb5fe03295bf74621e"),
            "transaction_start" : Timestamp(1355512030000, 1)   # t2
        },
        "transaction_end" : Timestamp(1355512247000, 2),        # t3
        "a" : 2
    }

The document versions were valid during the following timespans.
* before t1 no document version was valid
* from t1 to t2 (that is [t1, t2) ) the first document version was valid
* from t2 to t3 (that is [t2, t3) ) the second document version was valid
* from t3 on no document version was valid

Or in a more graphical way

    |----- t1 ----- t2 ----- t3 ----- ... (now)
       |        |        |        |
       |        |        |        \- no document
       |        |        \- version 2
       |        \- version 1
      \- no document


Further query options
---------------------

There are ways to query historic document versions more selectively. To get the
document version that was valid at a specific timestamp, execute the following
query. The used timestamp lies between t2 and t3. Hence the query returns the
second document version.

    > db.test.find({transaction: {at: Timestamp(1355512138500, 1)}})
    {
        "_id" : {
            "_id" : ObjectId("50cb78bb5fe03295bf74621e"),
            "transaction_start" : Timestamp(1355512030000, 1)
        },
        "transaction_end" : Timestamp(1355512247000, 2),
        "a" : 2
    }

To get the document versions that were valid within a specific time range, the
following query can be used. The used timestamps lie between t1 and t2, and
after t3. Hence the query returns both document versions.

    > db.test.find({transaction: {inrange:
    ...     [Timestamp(Timestamp(1355512012500, 1),
    ...     Timestamp(1355512355500, 1)]
    ... }})
    {
        "_id" : {
            "_id" : ObjectId("50cb78bb5fe03295bf74621e"),
            "transaction_start" : Timestamp(1355511995000, 1)
        },
        "transaction_end" : Timestamp(1355512030000, 1),
        "a" : 1
    }
    {
        "_id" : {
            "_id" : ObjectId("50cb78bb5fe03295bf74621e"),
            "transaction_start" : Timestamp(1355512030000, 1)
        },
        "transaction_end" : Timestamp(1355512247000, 2),
        "a" : 2
    }

The argument to the inrange parameter must be an array of length two,
containing Timestamp and null values.

The results can also be sorted based on their transaction-time by using the
transaction order parameter in the query:

    > db.test.find({ /* query criteria */ }).sort({transaction: -1})


Null values
-----------

The transaction_end members use null to refer to "the moving point now", which
is always greater than any other value a transaction_end member can have. To
reflect this in queries, additional comparison operators have been introduced.
They behave exactly like their normal counterparts, except that null is always
the largest value.

* $gt => $tgt
* $gte => $tgte
* $lt => $tlt
* $lte => $tlte


Indexing
--------

When creating an index, MongoDB will include the transaction_end in the list of
indexed fields to optimize for temporal queries.

    > db.test.ensureIndex({ "field": 1 })
    > db.test.getIndexes()
    // skipped output
    {
        "v" : 1,
        "key" : {
            "transaction_end" : 1,
            "field" : 1
        },
        "ns" : "test.test",
        "name" : "field_1"
    }

By default the transaction_end timestamp will be included as the first field.
To include the timestamp on a specific position, use "transaction":

    > db.test.ensureIndex({ "field1": 1, "transaction": -1, "field2": -1 })

If the transaction timestamp should not be included at all, use 0 instead.

    > db.test.ensureIndex({ "field1": 1, "transaction": 0, "field2": -1 })


Unique Constraints
------------------

When creating an index with unique constraint, the added transaction_end field
will ensure that the constraint only applied to current documents. All values
of the transaction_end field (except for null) are guaranteed to be unique.
Therefor such a constraint will only cover current documents where
transaction_end is null. As document versions become historic the uniqueness is
no longer enforced but still valid, as they can not be changed anymore.


Purging historic documents
--------------------------

By default historic documents are retained indefinitely, which will increase
disk usage as documents are updated. You can use TTL indexes to purge historic
documents.

    > db.test.ensureIndex({}, {expireAfterSeconds: 3600})

Creating this index will ensure that historic documents will be purged from the
database that have been deleted at least one hour ago. This is independant of
the transaction_start timestamp. Current documents (ie those that have no
transaction_end timestamp set) will not be deleted.



Stille to come
--------------

The following functionality is not yet planned, but is planned to work as
described below.

### Included history

When querying for historic documents using some criteria and a
transaction-time(span), the result contains only those document versions that
satisfied the criteria (within the given time(span)). However it should be possible to

* query for some criteria and time(span) AND
* include some history, regardless of whether they satisfy the criteria or not
