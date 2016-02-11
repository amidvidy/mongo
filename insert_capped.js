var db_name = "cappedTest";
var d = db.getSiblingDB(db_name);
var capSize = 1024*1024*1024; // 1GB capped collection size
var capCount = 1024*1024; // max doc count that's roughly 1GB
var useCount = false; // default to set the cap by size only
var batchSize = 1000;
var docSize = 1024;
var testTime = 180; 
var threads = [1, 8, 64];

doc_content = '';
for (var i=0; i<docSize; i++) {
    doc_content += 'x'
}

var docs = []
for (var i = 0; i < batchSize; i++) {
    docs.push( {x: doc_content} )
}

function setup(use_count) {
    d.dropDatabase();
    coll = d.capped_coll
    if (use_count)
        d.createCollection("capped_coll", {capped: true, size: capSize, max: capCount});
    else 
        d.createCollection("capped_coll", {capped: true, size: capSize});
    // fill the collection with a little over the cap size to force deletes
    var start_time = Date.now();
    for (var i=0; i < 1.1*capSize/(batchSize*docSize); i++) {
        var bulk = coll.initializeUnorderedBulkOp();
        for (var j=0; j<batchSize; j++)
            bulk.insert( {x: doc_content} );
        bulk.execute();
    }
    var end_time = Date.now();
    elapsed_time = end_time - start_time;
    thpt = 1000*(i*batchSize)/elapsed_time;
    print("Total time to prime the collection: " + elapsed_time/1000 + " seconds");
    print("Throughput: " + thpt + " ops/sec");
}

function cleanup() {
    d.dropDatabase()
}

function testInsert(docs, thread, tTime) {
    var benchArgs = { ops : [ { ns : coll.getFullName() ,
                                op : "insert" ,
                                writeCmd: true,
                                doc : docs} ],
                      parallel : thread,
                      seconds : tTime,
                      host : "lazarus:27017" }
    res = benchRun(benchArgs);
    return res;
}

function run_test() {
        setup(useCount);
        for (var i=0; i < threads.length; i++) {
            db.adminCommand({fsync: 1});
            tcount = threads[i];
            res = testInsert(docs, tcount, testTime);
        }
        cleanup();
}

run_test();
