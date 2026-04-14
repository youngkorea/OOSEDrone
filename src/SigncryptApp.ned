// Custom application module for ID-based On/Offline Signcryption simulation.
// Replaces UdpBasicApp to inject token retrieval delay (measured at runtime) and
//  OnSigncrypt computation delay before each packet transmission.
// - AOOSE/COOSE scheme: O(1) token retrieval (stack pop, wall-clock measured)
// - Baseline(Sun et al.'s) scheme: O(n) token retrieval (linear search, wall-clock measured) or hashmap lookup with memory consumption
//
//   Final ciphertext sizes (messageLength):
//   AOOSE:    406B  (c_ij 278B + X_ij 128B)
//   COOSE:    406B  (c_ij 278B + X_ij 128B)
//   Baseline: 366B  (c_ij 238B + X_ij 128B)
//

package oosedrone;

import inet.applications.contract.IApp;

simple SigncryptApp like IApp
{
    parameters:
        // --- Network parameters ---
        string destAddresses = default("");         // Space-separated list of receiver node names
        int destPort = default(5000);               // UDP destination port
        int messageLength @unit(B) = default(406B); // Ciphertext size (AOOSE/COOSE=406B, Baseline=366B)
        double sendInterval @unit(s) = default(1s); // Interval between consecutive OnSigncrypt calls

        // --- Signcryption parameters ---
        string schemeType = default("AOOSE");       // "AOOSE", "COOSE", or "Baseline"
        int dbSize = default(1000);                 // Number of offline tokens pre-loaded in DB
        double offSigncryptTime @unit(s) = default(0ms); // OffSigncrypt per-token time (from C benchmark)
        double onlineCompTime @unit(s) = default(0ms);  // OnSigncrypt computation time (from C benchmark)
        double unsigncryptTime @unit(s) = default(0ms); // UnSigncrypt time at receiver (from C benchmark)
        // Note: DB retrieval delay is NOT a parameter — it is measured at runtime
        //       via wall-clock inside processSend() to capture real O(1) vs O(n) difference

        // --- INET internal ---
        string interfaceTableModule;    // Required by INET framework

        @display("i=block/app");        // GUI icon
        @class(::SigncryptApp);         // Links this NED module to the C++ class
        @lifecycleSupport;              // Enables start/stop/crash lifecycle handling
        @signal[offSigncryptTotalTime](type=double);
        @signal[retrievalTime](type=double);
        @signal[totalOnlineTime](type=double);
        @signal[endToEndDelay](type=double);
        @statistic[offSigncryptTotalTime](source=offSigncryptTotalTime; record=vector,mean,max,min,stats);
        @statistic[retrievalTime](source=retrievalTime; record=vector,mean,max,min,stats);
        @statistic[totalOnlineTime](source=totalOnlineTime; record=vector,mean,max,min,stats);
        @statistic[endToEndDelay](source=endToEndDelay; record=vector,mean,max,min,stats);

    gates:
        input socketIn;     // Incoming packets from UDP layer
        output socketOut;   // Outgoing packets to UDP layer
}