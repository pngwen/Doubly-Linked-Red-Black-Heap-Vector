// ╔══════════════════════════════════════════════════════════════════════════╗
// ║   THE MINISTRY OF DRACOLOGICAL INCIDENT MANAGEMENT                      ║
// ║   Incident Processing System  ·  v1.0.0  ·  (Possibly Flammable)        ║
// ╚══════════════════════════════════════════════════════════════════════════╝
//
// OVERVIEW
// ════════
// This program simulates the daily operations of the Ministry of Dracological
// Incident Management (hereafter "the Ministry"), an ancient bureaucratic
// institution responsible for tracking, categorising, and processing
// dragon-related conflagrations across the realm.
//
// THE CONTRIVED YET LEGALLY BINDING JUSTIFICATION FOR THIS DATA STRUCTURE
// ════════════════════════════════════════════════════════════════════════
// Under Section 7, Subsection 4(b)(iii) of the "Dracological Incidents
// Management, Assessment, and Remedial Classification Act" (DIMARCIA) of
// 1347 A.D. (as amended by the Notorious Dragon Omnibus of 1612), all
// certified incident processing systems MUST simultaneously maintain:
//
//   (a) A DEQUE view — incidents are processed strictly in arrival order
//       by the Duty Officer (front-to-back), but an arriving Tier-1
//       Conflagration may be preposterously prepended to the front of the
//       queue via emergency executive transmogrification.
//
//   (b) A HEAP view  — the Chief Inspector may, at any moment and without
//       warning, demand to know the single most catastrophically severe
//       ongoing incident regardless of its queue position.  Severity is
//       measured in internationally standardised Dracological Conflagration
//       Units (DCUs, range 1–100).  The heap provides O(1) access to the
//       most egregiously flambe'd incident on record.
//
//   (c) An RB-TREE view — the Regulatory Dragon Census Bureau (RDCB) is
//       entitled to request, at literally any time day or night, a full
//       alphabetically-sorted register of all active incidents by dragon
//       name, for compliance with the Interrealm Dracographic Standards
//       Organisation (IDSO) specification ISO-6666 "Mythological Creature
//       Incident Nomenclature".  They are very serious about this.
//
// Failure to provide all three views simultaneously is punishable by
// severe bureaucratic transmogrification and/or being assigned to work the
// sulphur-fume desk for the remainder of the fiscal year.
//
// HOW THE DEMO FLOWS
// ══════════════════
// 1. MORNING INTAKE    — New incidents arrive and are enqueued at the back.
// 2. EMERGENCY INTAKE  — A Tier-1 Conflagration invokes the Preposterously
//                        Prepended Emergency Procedure (PPEP), jumping the
//                        queue by being shoved onto the front of the deque.
// 3. ESCALATION        — A previously-low-priority incident is retroactively
//                        deemed critical and undergoes Bureaucratic Priority
//                        Elevation via list_move_to_front().
// 4. CHIEF INSPECTOR   — Demands the most severe incident using heap_top(),
//                        then embarks on a dramatic heap-draining severity
//                        audit of all active incidents.
// 5. RDCB COMPLIANCE   — The Census Bureau requests the alphabetical dragon
//                        register via the RB-tree view and a targeted
//                        rb_find() lookup for a specific infamous wyrm.
// 6. DUTY OFFICER      — Processes the queue front-to-back, printing a
//                        gloriously over-formatted incident summary for each.
// 7. SHUTDOWN          — The Ministry closes for the day, noting smugly that
//                        all four data-structure invariants held throughout.

#include "../include/dlrb_heap_vector.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace dlrb;

// ─────────────────────────────────────────────────────────────────────────────
//  The sacred record of a Dracological Incident
// ─────────────────────────────────────────────────────────────────────────────
struct Incident {
    int         report_no;    // Sequential incident number assigned at intake
    std::string dragon;       // Full registered name of offending wyrm
    std::string category;     // Nature of the conflagratory transgression
    std::string location;     // Site of the dracological kerfuffle
    int         severity_dcu; // Dracological Conflagration Units (1–100)
};

// ─────────────────────────────────────────────────────────────────────────────
//  Comparator types
//    RB  tree: alphabetical by dragon name   (ascending — IDSO ISO-6666)
//    Heap    : descending severity DCUs      (max-heap — Chief Inspector)
// ─────────────────────────────────────────────────────────────────────────────
struct ByDragonName {
    bool operator()(const Incident& a, const Incident& b) const {
        return a.dragon < b.dragon;
    }
};
struct BySeverity {
    bool operator()(const Incident& a, const Incident& b) const {
        return a.severity_dcu > b.severity_dcu; // higher DCU wins
    }
};

using IncidentQueue = DLRBHeapVector<Incident, ByDragonName, BySeverity>;

// ─────────────────────────────────────────────────────────────────────────────
//  Formatting helpers — because the Ministry has Standards
// ─────────────────────────────────────────────────────────────────────────────

// Print a rule made of repeated single-char or multi-byte glyph
void rule(const std::string& glyph = "═", int w = 74) {
    for (int i = 0; i < w; ++i) std::cout << glyph;
    std::cout << '\n';
}

// Print a section header in the hallowed Ministry style
void header(const std::string& title) {
    rule();
    std::cout << "  " << title << '\n';
    rule("-");
}

// Print a single incident record in the Ministry's prescribed form MDIM-7(b)
void print_incident(const Incident& inc, const std::string& note = "") {
    // std::right ensures the zero-padded report number isn't left-justified
    // by whatever std::left state the header function left behind.
    std::cout
        << "  [#" << std::right << std::setw(3) << std::setfill('0') << inc.report_no
        << std::setfill(' ')
        << "]  " << std::left << std::setw(28) << inc.dragon
        << std::right << std::setw(4) << inc.severity_dcu << std::left
        << " DCU  " << std::setw(30) << inc.category
        << "  @ " << inc.location;
    if (!note.empty()) std::cout << "  " << note;
    std::cout << '\n';
}

// Print the column header for incident tables
void print_incident_header() {
    std::cout
        << "  " << std::left
        << std::setw(7)  << "REPORT"
        << "  " << std::setw(28) << "DRAGON"
        << std::setw(10) << "SEVERITY"
        << "  " << std::setw(30) << "CATEGORY"
        << "  LOCATION\n";
    rule("-");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Deque convenience wrappers
//  (The Ministry insists on dignified nomenclature for all operations.)
// ─────────────────────────────────────────────────────────────────────────────

// Enqueue at back — standard intake procedure MDIM-1
std::size_t enqueue(IncidentQueue& q, Incident inc) {
    return q.insert(std::move(inc));
}

// Enqueue at front — the Preposterously Prepended Emergency Procedure
std::size_t emergency_enqueue(IncidentQueue& q, Incident inc) {
    return q.insert_front(std::move(inc));
}

// Peek at the front — the Duty Officer's morning dread
const Incident& deque_front(const IncidentQueue& q) {
    return *q.list_begin();
}

// Dequeue from front — processing one incident out of existence
void process_front(IncidentQueue& q) {
    q.erase(q.list_begin().index());
}

// Elevate an incident to the front — Bureaucratic Priority Elevation Order
void bureaucratic_priority_elevation(IncidentQueue& q, std::size_t idx) {
    q.list_move_to_front(idx);
}

// ─────────────────────────────────────────────────────────────────────────────
//  main — a perfectly ordinary day at the Ministry
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    std::cout << '\n';
    rule("╔");
    std::cout << "  MINISTRY OF DRACOLOGICAL INCIDENT MANAGEMENT\n";
    std::cout << "  Incident Processing System  —  Daily Operations Log\n";
    rule("╚");
    std::cout << '\n';

    // Construct the queue with our two custom comparators.
    // ByDragonName governs the RB tree; BySeverity governs the max-heap.
    IncidentQueue queue{ ByDragonName{}, BySeverity{} };

    // ═════════════════════════════════════════════════════════════════════
    // PHASE 1 — MORNING INTAKE
    // Overnight, the realm's Field Correspondents have dutifully filed
    // reports.  Each incident is appended to the back of the Processing
    // Queue per standard intake procedure MDIM-1, "Orderly Sequential
    // Conflagration Cataloguing".
    // ═════════════════════════════════════════════════════════════════════
    header("PHASE 1  ·  MORNING INTAKE  (procedure MDIM-1)");
    std::cout << "  Ingesting overnight incident reports via rear-deque confabulation...\n\n";

    // We store the node index for Guttural Smolderbottom — he will be
    // subject to a Priority Elevation Order later and we need to locate
    // him in O(1) without rifling through the entire queue like a common
    // bureaucrat.  The other indices are not retained; those incidents
    // are, for now, considered administratively self-sufficient.
    enqueue(queue, { 1, "Pyrovex the Incandescent",
                         "Unlicensed Incineration",
                         "Millhaven Granary",          42 });

    auto idx_guttural  = enqueue(queue, { 2, "Guttural Smolderbottom",
                                          "Hoard Expansion (Residential)",
                                          "East Fenwick Village",        17 });

    enqueue(queue, { 3, "The Duchess of Ash",
                         "Maiden Procurement Attempt",
                         "Castle Rotherwick",           55 });

    enqueue(queue, { 4, "Sir Wobbles-A-Lot",
                         "Accidental Tail-Swipe",
                         "Northgate Millpond",           8 });

    enqueue(queue, { 5, "Cinderscale McFlame",
                         "Tax Evasion (Hoard-Based)",
                         "Royal Revenue Office",         31 });

    print_incident_header();
    for (const auto& inc : queue.list_view()) print_incident(inc);
    std::cout << '\n'
              << "  " << queue.size() << " incidents enqueued.\n"
              << "  List order is pure arrival order — the deque is functioning\n"
              << "  with all the orderly grace of a freshly stamped form MDIM-7(b).\n\n";

    // ═════════════════════════════════════════════════════════════════════
    // PHASE 2 — EMERGENCY INTAKE
    // A carrier pigeon, slightly singed, has just arrived bearing news of
    // a Tier-1 Conflagration of frankly unconscionable severity.  Under
    // DIMARCIA Section 7(e), "The Preposterously Prepended Emergency
    // Procedure" (PPEP), this incident bypasses the entire queue and is
    // inserted at the front via insert_front().
    // ═════════════════════════════════════════════════════════════════════
    header("PHASE 2  ·  EMERGENCY INTAKE  (procedure PPEP, Section 7(e))");
    std::cout << "  A singed carrier pigeon has arrived.  Initiating Preposterously\n"
              << "  Prepended Emergency Procedure.  Please stand well back.\n\n";

    // Magmatrix's node index is NOT stored here — in Phase 4 the Inspector
    // drains the entire queue and re-inserts everything, which reclaims and
    // reassigns node slots.  Holding a stale index across a full drain would
    // be an act of bureaucratic recklessness.  Instead, Phase 5 obtains the
    // live index from the rb_find() iterator, which is always authoritative.
    emergency_enqueue(queue, {
        6, "Magmatrix the Unquenchable",
        "Continental Shelf Ignition",
        "The Entire Coastline",
        98   // this is fine
    });

    print_incident_header();
    for (const auto& inc : queue.list_view())
        print_incident(inc,
            inc.report_no == 6 ? "← PPEP — shoved to front with extreme prejudice" : "");
    std::cout << '\n'
              << "  Report #006 now occupies pole position in the Processing Queue.\n"
              << "  The deque has been frontally conflagrated.\n\n";

    // ═════════════════════════════════════════════════════════════════════
    // PHASE 3 — BUREAUCRATIC PRIORITY ELEVATION
    // An eagle-eyed clerk has noticed that incident #002 (Guttural
    // Smolderbottom, Hoard Expansion) has been reassessed by the Rapid
    // Dracological Threat Revaluation Board.  New intelligence suggests
    // the "residential" hoard expansion is in fact a full suburban
    // annexation.  The incident is promoted to the front of the queue
    // by Bureaucratic Priority Elevation Order (BPEO) via
    // list_move_to_front(), which surgically rewires the list links
    // in O(1) without disturbing the RB tree or heap in any way.
    // ═════════════════════════════════════════════════════════════════════
    header("PHASE 3  ·  BUREAUCRATIC PRIORITY ELEVATION ORDER  (BPEO)");
    std::cout << "  The Rapid Dracological Threat Revaluation Board has reconsidered\n"
              << "  incident #002.  Guttural Smolderbottom's hoard is, in fact, a\n"
              << "  FULL SUBURBAN ANNEXATION.  Issuing BPEO forthwith.\n\n";

    // list_move_to_front rewires list_prev/list_next links in O(1).
    // The RB tree (which sorts by dragon name) is completely undisturbed.
    // The heap (which sorts by severity_dcu) is completely undisturbed.
    // Only the linked-list order changes.  Glorious.
    bureaucratic_priority_elevation(queue, idx_guttural);

    std::cout << "  Processing Queue after BPEO:\n";
    print_incident_header();
    for (const auto& inc : queue.list_view())
        print_incident(inc,
            inc.report_no == 2 ? "← elevated by BPEO" : "");
    std::cout << '\n'
              << "  Guttural Smolderbottom is now at the front.  The heap and RB tree\n"
              << "  remain entirely unmolested — only list pointers were transmogrified.\n\n";

    // ═════════════════════════════════════════════════════════════════════
    // PHASE 4 — CHIEF INSPECTOR'S SEVERITY AUDIT
    // Inspector General Barnabas Crumplewick III has materialised from
    // the depths of the Ministry.  He wishes to conduct an unannounced
    // Severity Audit.  He does not care about queue order.  He wants to
    // know about carnage, and he wants it ranked.
    //
    // The heap, which has silently maintained the max-DCU invariant
    // through every insert, move, and deletion, now earns its keep.
    // heap_top() gives O(1) access to the worst incident; successive
    // heap_begin().index() + erase() drains in severity order.
    //
    // IMPORTANT: we drain into a temporary vector and re-insert, because
    // the Inspector is pompous but the Ministry must remain operational.
    // ═════════════════════════════════════════════════════════════════════
    header("PHASE 4  ·  CHIEF INSPECTOR'S UNANNOUNCED SEVERITY AUDIT");
    std::cout << "  Inspector General Barnabas Crumplewick III has materialised.\n"
              << "  He demands a severity ranking.  Resistance is inadvisable.\n\n";

    std::cout << "  Most critically conflagrated incident (heap_top, O(1)):\n";
    print_incident_header();
    print_incident(queue.heap_top(), "← PEAK CARNAGE");
    std::cout << '\n';

    // Drain the heap into a severity-ordered list.
    // Each call to heap_begin().index() retrieves the current worst incident;
    // erase() removes it from all four views simultaneously and the heap
    // restructures itself via sift-down in O(log n).
    std::cout << "  Full severity ranking (heap drain — most catastrophic first):\n";
    print_incident_header();
    std::vector<Incident> severity_order;
    while (!queue.empty()) {
        severity_order.push_back(queue.heap_top());
        queue.erase(queue.heap_begin().index());
    }
    for (const auto& inc : severity_order)
        print_incident(inc);
    std::cout << '\n'
              << "  The heap has spoken.  Carnage has been quantified.\n"
              << "  The Inspector departs, grimly satisfied.\n\n";

    // Re-insert incidents so the Ministry can continue functioning.
    // They re-enter in severity order, so the deque order differs from
    // the original — a small bureaucratic price for the Inspector's audit.
    std::cout << "  Re-instating incidents into the Processing Queue post-audit...\n";
    for (auto& inc : severity_order)
        enqueue(queue, inc);
    std::cout << "  All " << queue.size() << " incidents reinstated.  Normalcy superficially restored.\n\n";

    // ═════════════════════════════════════════════════════════════════════
    // PHASE 5 — RDCB COMPLIANCE: ALPHABETICAL REGISTER
    // The Regulatory Dragon Census Bureau has submitted Request Form
    // RDCB-9 ("Monthly Alphabetical Incident Register") accompanied by
    // a stern covering letter referencing ISO-6666 clause 4.3.1.
    //
    // The RB tree, which has maintained a balanced BST sorted by dragon
    // name through every modification, produces the register by simple
    // in-order traversal in O(n) — no sorting required.  The Census
    // Bureau representative is almost visibly disappointed.
    //
    // A specific lookup is also performed for the notorious Magmatrix
    // the Unquenchable, who is suspected of operating without a valid
    // Continental Conflagration Licence (Form CCL-3).
    // ═════════════════════════════════════════════════════════════════════
    header("PHASE 5  ·  RDCB COMPLIANCE  —  ALPHABETICAL REGISTER (ISO-6666)");
    std::cout << "  Request Form RDCB-9 received.  Generating alphabetical register\n"
              << "  via RB-tree in-order traversal.  The tree has been maintaining\n"
              << "  this order silently since Phase 1.  It is very smug about this.\n\n";

    print_incident_header();
    for (const auto& inc : queue.rb_view())
        print_incident(inc);
    std::cout << '\n';

    // rb_find performs an O(log n) BST search by dragon name.
    // The comparator we provided sorts by the `dragon` field, so
    // rb_find matches on dragon name.
    std::cout << "  Targeted lookup: Magmatrix the Unquenchable (CCL-3 compliance check)\n";
    Incident probe;
    probe.dragon = "Magmatrix the Unquenchable";
    auto found = queue.rb_find(probe);
    if (found != queue.rb_end()) {
        std::cout << "  LOCATED — incident on file:\n";
        print_incident_header();
        print_incident(*found, "← found via rb_find in O(log n)");
        std::cout << '\n'
                  << "  Status: Operating without CCL-3.  Referral issued to the\n"
                  << "  Mythological Licensing Tribunal.  Good luck to them.\n\n";

        // RDCB Regulatory Hold — pursuant to Bylaw 14(f) "Pending Tribunal
        // Incidents Shall Not Jump The Queue", Magmatrix's incident must be
        // demoted to the back of the Processing Queue until the Tribunal
        // responds.  list_move_to_back() rewires list links in O(1).
        // The heap (severity 98 DCU) and RB tree remain gloriously intact;
        // Magmatrix is still the most catastrophic incident on record, now
        // paperwork-bound at the back of the line.
        //
        // We obtain the live node index directly from the rb_find iterator
        // via .index() — no stale bookmarks needed, and it neatly
        // demonstrates that every iterator type provides a stable index.
        std::cout << "  Regulatory Hold invoked (Bylaw 14(f)): demoting Magmatrix\n"
                  << "  to back of Processing Queue pending Tribunal review.\n"
                  << "  The coastline will simply have to wait.\n\n";
        queue.list_move_to_back(found.index());

        std::cout << "  Processing Queue after Regulatory Hold:\n";
        print_incident_header();
        for (const auto& inc : queue.list_view())
            print_incident(inc,
                inc.report_no == 6 ? "← demoted via list_move_to_back (O(1))" : "");
        std::cout << '\n';
    }

    // ═════════════════════════════════════════════════════════════════════
    // PHASE 6 — DUTY OFFICER PROCESSING
    // The Duty Officer, one Percival Snodgrass, now processes the queue
    // from front to back in the traditional manner — dequeue from front,
    // stamp form MDIM-7(b), file in triplicate, repeat.
    //
    // Each call to list_begin().index() identifies the front of the deque;
    // erase() removes it from all four views (list, vector, RB, heap)
    // atomically in O(log n).  The queue shortens.  Percival's headache
    // does not.
    // ═════════════════════════════════════════════════════════════════════
    header("PHASE 6  ·  DUTY OFFICER PROCESSING  (Officer P. Snodgrass)");
    std::cout << "  Officer Snodgrass has located his rubber stamp.\n"
              << "  Processing queue front-to-back.  Incident adjudication commencing.\n\n";

    // Verdicts befitting the gravity (or comedic inadequacy) of each incident.
    auto verdict = [](int severity) -> std::string {
        if (severity >= 90) return "REALM-WIDE EMERGENCY — wake the King";
        if (severity >= 60) return "Critical — deploy the Guild immediately";
        if (severity >= 40) return "Elevated — dispatch two knights and a cleric";
        if (severity >= 20) return "Moderate — send a strongly-worded letter";
        return                     "Negligible — log it and have some tea";
    };

    int processed = 0;
    while (!queue.empty()) {
        // Peek at the front of the deque without removing it
        const Incident& front = deque_front(queue);

        std::cout << "  Processing #"
                  << std::setw(3) << std::setfill('0') << front.report_no
                  << std::setfill(' ') << "  "
                  << std::left << std::setw(28) << front.dragon
                  << "  [" << std::setw(2) << front.severity_dcu << " DCU]  →  "
                  << verdict(front.severity_dcu) << '\n';

        // Dequeue from the front — erase removes the node from all four
        // views in a single O(log n) call.
        process_front(queue);
        ++processed;
    }

    std::cout << '\n'
              << "  " << processed << " incidents processed.\n"
              << "  Officer Snodgrass has run out of rubber-stamp ink.\n\n";

    // ═════════════════════════════════════════════════════════════════════
    // PHASE 7 — END-OF-DAY INTEGRITY REPORT
    // The Ministry is now empty.  A brief moment of reflection is observed
    // before the building inevitably catches fire again tomorrow.
    // ═════════════════════════════════════════════════════════════════════
    rule();
    std::cout << "  END-OF-DAY STATUS REPORT\n";
    rule("-");
    std::cout
        << "  Active incidents remaining : " << queue.size()      << "\n"
        << "  Queue empty                : " << std::boolalpha << queue.empty() << "\n"
        << "  RB tree operational        : true (it is always operational)\n"
        << "  Heap operational           : true (it never complained once)\n"
        << "  Doubly-linked list         : surgically transmogrified as required\n"
        << "  DIMARCIA compliance        : FULL\n"
        << "  ISO-6666 compliance        : FULL\n"
        << "  Ministry on fire           : unknown — requires field assessment\n\n";
    rule();
    std::cout << "  The DLRBHeapVector has performed its quadruple duty with\n"
              << "  distinction.  Four data structures.  One allocation.\n"
              << "  Zero complaints.  Good night.\n";
    rule("╚");
    std::cout << '\n';

    return 0;
}
