var db_name = "ttlTest";
var expTime = 0; // 0 seconds expireAfterSeconds
var monitorSleep = 5; // 5 second ttlMonitorSleepSecs
var batchSize = 1000;
var docSize = 1024;
var testTime = 180; 
var threads = [1, 16, 32];

var testDB = db.getSiblingDB(db_name);
testDB.adminCommand({setParameter: 1, ttlMonitorSleepSecs: monitorSleep});
print("Setting ttlMonitorSleepSecs to " + monitorSleep);
var docs = []
for (var i = 0; i < batchSize; i++) {
    docs.push( {ttl: {"#CUR_DATE": 0},
                s : docSize > 41 ? new Array(docSize - 41).join('x') : ""} );
    // Make the size of all documents equal to docSize
    // 41 is the size of a document with empty string
}

function cleanup(d) {
    db.adminCommand({setParameter: 1, ttlMonitorSleepSecs: 60});
    print("Setting ttlMonitorSleepSecs to " + 60);
    d.dropDatabase();
}

function testInsert(d, docs, thread, runTime) {
    d.dropDatabase();
    coll = d.ttl_coll;
    coll.ensureIndex({ttl: 1}, {expireAfterSeconds: expTime});
    var benchArgs = { ops : [ { ns : coll.getFullName() ,
                                op : "insert" ,
                                writeCmd : true,
                                doc : docs} ],
                      parallel : thread,
                      seconds : runTime,
                      host : "lazarus:27017" };
    res = benchRun(benchArgs);
    var oldest = coll.find().sort({ttl:1}).limit(1).next().ttl;
    var now = new Date();
    var age = now - oldest;
    var thpt = res["totalOps/s"]*batchSize;
//    reportThroughput("insert_ttl", thpt, {nThread: thread});
//    print("thread = " + thread + ",   thpt = " + thpt.toFixed(2) + ",   oldest_doc_age = " +age);
}

function run_test(d) {
        for (var i=0; i < threads.length; i++) {
            db.adminCommand({fsync: 1});
            tcount = threads[i];
            testInsert(d, docs, tcount, testTime);
        }
        cleanup(d);
}

run_test(testDB);

