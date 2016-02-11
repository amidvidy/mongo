var db_name = "VectoredInsertTest";
var batchSize = 250; // number of documents per vectored insert
var docSize = 512; // Document size + _id field
var testTime = 180; // benchRun test time in seconds
// What are the appropriate number of threads to run this test for? 1,
// 16, 32 were copied from Yen's test.
var threads = [1, 16, 32];
// Threshold for pass fail comparing secondary throughput to primary
var threshold = 0.95;

var testDB = db.getSiblingDB(db_name);

// Make a document of the given size. Actually plus _id size.
function makeDocument(docSize) {
    for (var i=0; i<docSize; i++) {
        var doc = { "fieldName":"" };
        while(Object.bsonsize(doc) < docSize) {
            doc.fieldName += "x";
        }
        return doc;
    }
}
doc = makeDocument(docSize);

// Make the document array to insert
var docs = []
for (var i = 0; i < batchSize; i++) {
    docs.push(doc);
}


function cleanup(d) {
    d.dropDatabase();
}

// Test of oplog with vectored inserts.
// Insert a vector of documents using benchRun.
// Measure the  throughput of benchRun on the primary and secondary.
// Wait for all secondaries to completely catch up, and compute the aggregate throughput.
// The Test passes if the aggregate throughput is within threshold
// percent of the primary's throughput.
function testInsert(d, docs, thread, runTime) {
    d.dropDatabase();
    coll = d.vectored_insert;
    // Make sure that the oplog is caught up 
    // timestamp for the start of the test
    var start = new Date();
    var benchArgs = { ops : [ { ns : coll.getFullName() ,
                                op : "insert" ,
                                doc : docs,
                                writeCmd : true} ],
                      parallel : thread,
                      seconds : runTime,
                      host : "lazarus:27017"};
    res = benchRun(benchArgs);
    // timestamp for benchRun complete, but before waiting for secondaries
    var middle = new Date();

    // if replSet, get secondary stats and compute
    var members = rs.status().members;

    // Get the number of documents on the primary. This assumes we're
    // talking to the primary. Can explicitely get primary if needed,
    // but the benchRun call should have been done against the primary
    var primDocs = d.stats().objects;

    // Find the minimum number of documents processed by any of the
    // secondaries
    var minDocs = primDocs;
    // Go through all members
    for (i in members) {
        // Skip the primary
        if (members[i].state != 1) {
            // Get the number of objects
            // Connect to the secondary and call db.sats()
            var x = new Mongo(members[i].name);
            var mydb = x.getDB(d);
            var numDocs = mydb.stats().objects;
            if (numDocs < minDocs) {
                minDocs = numDocs;
            }
        }
    }

    // for each secondary, wait until it is caught up.
    if ( ! waitOplog(members) ) {
        // error with replSet, quit the test early
        return;
    }
    // All secondaries are caught up now. Test is done. Get the time
    var end = new Date();
    // print out to get lags
    rs.printSlaveReplicationInfo();

    printjson(res);

    //var thpt = res["totalOps/s"]*batchSize; // This throughput isn't comparable to the secondary ones, so not going to use.
    // All following throughputs needs to be scaled by time. The timestamps are in milliseconds.
    var secThpt = primDocs * 1000 / (end - start);
    // Minimum Throughput of a secondary during the benchRun call
    var secThptPrimary = minDocs * 1000 / (middle - start);
    // Throughput of the primary as measured by this script (not benchRun)
    var primThpt = primDocs * 1000 / (middle - start);
    //reportThroughput("insert_vector_primary_benchrun", thpt, {nThread: thread});

    // Did the secondary have 95% of the throughput?
    // During the first phase?
    var second_pass_first_phase = (minDocs > primDocs * threshold);
    // overall?
    var second_pass_overall = ((threshold * (end - start)) < (middle - start));

    print("thread = " + thread + ",   thpt = " + primThpt.toFixed(2));
}

function run_test(d) {
        for (var i=0; i < threads.length; i++) {
            print("Running thread level " + threads[i]);
            db.adminCommand({fsync: 1});
            tcount = threads[i];
            testInsert(d, docs, tcount, testTime);
        }
        cleanup(d);
}

run_test(testDB);
