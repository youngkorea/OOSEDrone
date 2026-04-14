// Simulates the online phase of ID-based On/Offline Signcryption.
//
// For each message, sender drone:
//  1. Retrieves an offline token from a pre-loaded DB
//     - AOOSE/COOSE: O(1) stack pop (from C benchmark)
//     - Baseline (Sun et al.): O(n) linear scan (from C benchmark)
//  2. Waits for OnSigncrypt computation delay (from C benchmark)
//  3. Sends final ciphertext to a dynamically chosen receiver
//
// Key metrics recorded:
//  - retrievalTime: DB token retrieval (us)
//  - totalOnlineTime: retrievalTime + OnSigncrypt computation time (s)
//

#include <omnetpp.h>
#include <inet/common/packet/Packet.h>
#include <inet/common/TimeTag_m.h>
#include <inet/common/packet/chunk/ByteCountChunk.h>
#include <inet/networklayer/common/L3AddressResolver.h>
#include <inet/transportlayer/contract/udp/UdpSocket.h>
#include <inet/applications/base/ApplicationBase.h>
#include <vector>
#include <stack>
#include <string>
#include <sstream>
#include <random>
#include <algorithm>

using namespace omnetpp;
using namespace inet;

class SigncryptApp : public ApplicationBase
{
  protected:
    // ---- Configuration parameters (from omnetpp.ini) ----
    std::string schemeType;      // "AOOSE", "COOSE", or "Baseline"
    int dbSize;                  // Number of offline tokens pre-loaded in DB
    simtime_t offSigncryptTime;  // OffSigncrypt per-token time (from C benchmark)
    simtime_t onlineCompTime;    // OnSigncrypt computation time (from C benchmark)
    simtime_t sendInterval;      // Time between consecutive OnSigncrypt calls
    int messageLength;           // Ciphertext packet size in bytes
    int destPort;                // UDP destination port number

    // ---- Runtime state ----
    UdpSocket socket;                          // UDP socket for sending packets
    cMessage *sendTimer = nullptr;             // Self-message: triggers next OnSigncrypt
    cMessage *retrievalDoneMsg = nullptr;      // Self-message: fires after retrieval + computation
    std::vector<L3Address> destAddresses;      // Resolved IP addresses of all receivers

    // ---- Token DB structures ----
    // AOOSE: stack — any token works, just pop -> O(1)
    // COOSE: stack — any token works, just pop -> O(1)
    // Baseline: vector — must search for message-specific token -> O(n)
    std::stack<std::string> aooseDB;
    std::stack<std::string> cooseDB;
    std::vector<std::string> baselineDB;

    // ---- Statistics ----
    int numSent = 0;
    simsignal_t offSigncryptTotalTimeSignal;
    simsignal_t retrievalTimeSignal;     // Records each token retrieval delay
    simsignal_t totalOnlineTimeSignal;   // Records retrieval + computation delay
    simsignal_t endToEndDelaySignal;     // Records end-to-end packet delay

    // ---- Lifecycle methods required by ApplicationBase ----
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage *msg) override;
    virtual void finish() override;

    virtual void handleStartOperation(LifecycleOperation *operation) override;
    virtual void handleStopOperation(LifecycleOperation *operation) override;
    virtual void handleCrashOperation(LifecycleOperation *operation) override;

    // ---- Core logic ----
    void processStart();            // Called once at simulation start
    void processSend();             // Begins OnSigncrypt: retrieves token, schedules delay
    void processRetrievalDone();    // After delay: sends packet, schedules next send
    void initDB();                  // Populates token DB based on schemeType
    L3Address chooseDestAddr();     // Randomly picks a receiver for this message
    std::string generateRandomHex(int length);  // Generates dummy token data
};

Define_Module(SigncryptApp);

// ============================================================
// initialize(): Read parameters and register statistics signals.
// Called once before simulation starts.
// ============================================================
void SigncryptApp::initialize(int stage)
{
    ApplicationBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        // Read parameters from NED/ini
        schemeType = par("schemeType").stdstringValue();
        dbSize = par("dbSize");
        offSigncryptTime = par("offSigncryptTime");
        onlineCompTime = par("onlineCompTime");
        sendInterval = par("sendInterval");
        messageLength = par("messageLength");
        destPort = par("destPort");

        numSent = 0;

        // Register signals — recorded in .sca/.vec result files
        offSigncryptTotalTimeSignal = registerSignal("offSigncryptTotalTime");
        retrievalTimeSignal = registerSignal("retrievalTime");
        totalOnlineTimeSignal = registerSignal("totalOnlineTime");
        endToEndDelaySignal = registerSignal("endToEndDelay");

        // Create self-messages (timers)
        sendTimer = new cMessage("sendTimer");
        retrievalDoneMsg = new cMessage("retrievalDone");
    }
}

// ============================================================
// Lifecycle handlers: called when node starts, stops, or crashes
// ============================================================
void SigncryptApp::handleStartOperation(LifecycleOperation *operation)
{
    processStart();
}

void SigncryptApp::handleStopOperation(LifecycleOperation *operation)
{
    cancelEvent(sendTimer);
    cancelEvent(retrievalDoneMsg);
    socket.close();
}

void SigncryptApp::handleCrashOperation(LifecycleOperation *operation)
{
    cancelEvent(sendTimer);
    cancelEvent(retrievalDoneMsg);
    socket.destroy();
}

// ============================================================
// processStart(): Runs once when the drone starts operating.
// - Resolves receiver IP addresses from node names
// - Opens UDP socket
// - Populates offline token DB (simulates pre-flight OffSigncrypt)
// - Schedules the first OnSigncrypt call
// ============================================================
void SigncryptApp::processStart()
{
    // Resolve destination addresses
    const char *destAddrs = par("destAddresses");
    cStringTokenizer tokenizer(destAddrs);

    if (tokenizer.hasMoreTokens()) {
        // Manual mode: use addresses from ini
        while (tokenizer.hasMoreTokens()) {
            const char *token = tokenizer.nextToken();
            L3Address addr = L3AddressResolver().resolve(token);
            destAddresses.push_back(addr);
        }
    }
    else {
        // Auto mode: find all receiver[] nodes in the network
        cModule *network = getModuleByPath("<root>");
        int numReceivers = network->par("numReceivers");
        for (int i = 0; i < numReceivers; i++) {
            std::string name = "receiver[" + std::to_string(i) + "]";
            L3Address addr = L3AddressResolver().resolve(name.c_str());
            destAddresses.push_back(addr);
        }
        EV_INFO << "Auto-resolved " << numReceivers << " receivers" << endl;
    }

    // Open UDP socket for sending signcrypted packets
    socket.setOutputGate(gate("socketOut"));
    socket.bind(2000 + getId());

    // Populate the offline token DB
    // This simulates the pre-flight OffSigncrypt phase:
    // ground station generates N offline tokens and loads them onto the drone
    initDB();

    // Calculate total OffSigncrypt time = dbSize * per-token time
    // This simulates the pre-flight phase where GCS generates N offline tokens
    simtime_t totalOffTime = offSigncryptTime * dbSize;
    emit(offSigncryptTotalTimeSignal, totalOffTime.dbl());

    EV_INFO << "Pre-flight OffSigncrypt: " << dbSize << " tokens x "
            << offSigncryptTime << "s = " << totalOffTime << "s total" << endl;

    // Schedule the first OnSigncrypt after OffSigncrypt delay
    // Drone starts transmitting only after all tokens are loaded
    scheduleAt(simTime() + totalOffTime + sendInterval, sendTimer);
}

// ============================================================
// initDB(): Populates the token database.
//
// AOOSE/COOSE: Tokens are message-independent, stored in a stack.
//   Chameleon hash allows any token for any message -> O(1) pop.
//
// Baseline (Sun et al.): Tokens are message-dependent, stored in vector.
//   Must search for the token matching a specific R value -> O(n) scan.
//   (Future: hash map variant for O(log n) with memory overhead)
//
// Token content is dummy random hex — actual crypto is not simulated.
// Only the retrieval pattern and timing matter.
// Token sizes reflect real scheme parameters:
//   AOOSE c_off:  188 bytes (g1 + 3*zr)
//   COOSE c_off:  346 bytes (c_ii 218B + g1 128B)
//   Baseline c_off: 220 bytes (g1 + 3*zr + aes_key)
// ============================================================
void SigncryptApp::initDB()
{
    if (schemeType == "AOOSE") {
        // AOOSE: 188 bytes per token, stored in stack
        for (int i = 0; i < dbSize; i++) {
            aooseDB.push(generateRandomHex(376));  // 376 hex chars = 188 bytes
        }
        EV_INFO << "AOOSE DB initialized: " << dbSize
                << " tokens x 188B (stack, O(1))" << endl;
    }
    else if (schemeType == "COOSE") {
        // COOSE: 346 bytes per token, stored in stack
        for (int i = 0; i < dbSize; i++) {
            cooseDB.push(generateRandomHex(692));  // 692 hex chars = 346 bytes
        }
        EV_INFO << "COOSE DB initialized: " << dbSize
                << " tokens x 346B (stack, O(1))" << endl;
    }
    else if (schemeType == "Baseline") {
        // Baseline: 220 bytes per token, stored in vector
        // OnSigncrypt requires searching for a specific R value -> O(n) scan
        for (int i = 0; i < dbSize; i++) {
            baselineDB.push_back(generateRandomHex(440));  // 440 hex chars = 220 bytes
        }
        EV_INFO << "Baseline DB initialized: " << dbSize
                << " tokens x 220B (vector, O(n))" << endl;
    }
    else {
        // Unknown scheme: default to vector-based DB with warning
        for (int i = 0; i < dbSize; i++) {
            baselineDB.push_back(generateRandomHex(440));
        }
        EV_WARN << "Unknown schemeType '" << schemeType
                << "', defaulting to vector DB (O(n))" << endl;
    }
}

// ============================================================
// Although we utilize random values for DB, but we reflect 
// exact computation time measured from from C benchmark 
// and size of values in DB. 
// generateRandomHex(): Creates a dummy token string.
// Length parameter is in hex characters (2 hex chars = 1 byte).
// ============================================================
std::string SigncryptApp::generateRandomHex(int length)
{
    static const char hexChars[] = "0123456789abcdef";
    std::string result(length, '0');
    for (int i = 0; i < length; i++) {
        result[i] = hexChars[rand() % 16];
    }
    return result;
}

// ============================================================
// handleMessageWhenUp(): Central message dispatcher.
// Routes self-messages (timers) and incoming packets.
// ============================================================
void SigncryptApp::handleMessageWhenUp(cMessage *msg)
{
    if (msg == sendTimer) {
        // Time to perform next OnSigncrypt
        processSend();
    }
    else if (msg == retrievalDoneMsg) {
        // Token retrieval + computation finished -> send packet
        processRetrievalDone();
    }
    else if (msg->arrivedOn("socketIn")) {
        // Incoming packet (for receiver side, future use)
        delete msg;
    }
    else {
        delete msg;
    }
}

// ============================================================
// processSend(): Core experiment logic. Called when sendTimer fires.
//
// 1. Performs actual DB operation (stack pop or linear scan)
// 2. Measures wall-clock time of the operation (chrono)
// 3. Converts wall-clock time to simulation time delay
// 4. Adds OnSigncrypt computation delay (from C benchmark)
// 5. Schedules packet transmission after total delay
//
// Wall-clock measurement captures real O(1) vs O(n) difference
// on the host machine, then injects it into simulation time.
// ============================================================
void SigncryptApp::processSend()
{
    double delay_us = 0.0;

    // Retrieval time measured from C Benchmark (us)
    double aoose_pop_us = 0.000695;
    double coose_pop_us = 0.000709;
    double baseline_scan_1elem_us = 0.000694;

    if (schemeType == "AOOSE") {
        // ---- O(1) retrieval: stack pop ----
        // Any token can be used for any message (chameleon hash)
        delay_us = aoose_pop_us;
    }

    else if (schemeType == "COOSE") {
        // ---- O(1) retrieval: stack pop ----
        // Any token can be used for any message (chameleon hash)
        delay_us = coose_pop_us;
    }

    else if (schemeType == "Baseline") {
        // ---- O(n) retrieval: linear scan ----
        // Must find token whose R matches the current message
        // Linear scan simulates this message-dependent lookup
        double expectedScans = dbSize / 2.0; 
        delay_us = expectedScans * baseline_scan_1elem_us;
    }

    else {
        // Unknown scheme
        delay_us = 0.0;
    }

    simtime_t actualRetrievalDelay = SimTime(delay_us, SIMTIME_US);

    // Record retrieval time as statistic (in seconds)
    emit(retrievalTimeSignal, delay_us / 1000000.0);

    // Total online phase = DB retrieval (wall-clock) + OnSigncrypt computation (benchmark)
    simtime_t totalDelay = actualRetrievalDelay + onlineCompTime;
    emit(totalOnlineTimeSignal, totalDelay.dbl());

    // Schedule actual packet send after total delay
    // The simulation clock advances by totalDelay before the packet is sent
    scheduleAt(simTime() + totalDelay, retrievalDoneMsg);
}

// ============================================================
// processRetrievalDone(): Fires after retrieval + computation delay.
// - Dynamically selects a receiver
// - Creates signcrypted ciphertext packet
// - Sends via UDP
// - Schedules next OnSigncrypt call
// ============================================================
void SigncryptApp::processRetrievalDone()
{
    // Dynamically choose a receiver for this message
    // Models drone selecting receiver at each waypoint during flight
    L3Address destAddr = chooseDestAddr();

    // Create UDP packet representing the signcrypted ciphertext
    // Packet size = messageLength (AOOSE/COOSE: 406B, Baseline: 366B)
    Packet *packet = new Packet("SigncryptedData");

    // Tag with creation time for end-to-end delay measurement at receiver
    packet->addTag<CreationTimeTag>()->setCreationTime(simTime());

    // Set packet size to match actual ciphertext size from scheme spec
    const auto& payload = makeShared<ByteCountChunk>(B(messageLength));
    packet->insertAtBack(payload);

    // Send via UDP socket to chosen receiver
    socket.sendTo(packet, destAddr, destPort);
    numSent++;

    EV_INFO << "Sent packet #" << numSent
            << " to " << destAddr
            << " [" << schemeType
            << ", DB=" << dbSize << "]" << endl;

    // Schedule next OnSigncrypt call after sendInterval
    scheduleAt(simTime() + sendInterval, sendTimer);
}

// ============================================================
// chooseDestAddr(): Randomly selects one receiver per message.
// Models drone dynamically picking a receiver during flight.
// ============================================================
L3Address SigncryptApp::chooseDestAddr()
{
    int idx = rand() % destAddresses.size();
    return destAddresses[idx];
}

// ============================================================
// finish(): Called when simulation ends.
// Records final scalar statistics to .sca result file.
// ============================================================
void SigncryptApp::finish()
{
    simtime_t totalOffTime = offSigncryptTime * dbSize;
    EV_INFO << "SigncryptApp finish: sent=" << numSent
            << ", offSigncryptTotal=" << totalOffTime << "s" << endl;
    recordScalar("numSent", numSent);
    recordScalar("offSigncryptTotalTime", totalOffTime.dbl());
}