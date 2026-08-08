Background



CPG brands face strict labeling and packaging requirements from retailers. When a production error occurs, a brand may find itself with a limited supply of "Correctly Labeled" products and a surplus of "Non-Compliant" inventory. Retailers charge Non-Compliance Fees for every unit that fails to meet their specific labeling threshold. Brands must determine how to allocate their limited "Correct" stock across all Purchase Orders (POs) to minimize the total financial penalty.



For example, say a PO is for 100 units and a total amount of $150 but we only allocate 60 correct units. If Target has a 1% fine rate and an 80% compliance threshold, then we will be fined:



$0.30 = ($1.50/unit × 0.01 × 20 units)


Files



purchase_orders[.csv|.json] — List of POs
compliance_rates[.csv|.json] — Threshold and fine rates for each retailer


Format



Your interviewer will share a series of tasks with you during the session, one at a time. The first few tasks involve writing code; later tasks may be discussion-only. Please complete each task before moving to the next.





Task



Determine the Maximum Potential Fine (i.e we don't ship anything correctly labelled)

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include "nlohmann/json.hpp" // Ensure nlohmann/json is available

using json = nlohmann::json;

// Standard Retailer Rules Engine
struct RetailerRule {
    double compliance_threshold; // e.g., 0.80 for 80%
    double fine_rate;           // e.g., 0.01 for 1%
};

RetailerRule getRetailerRule(const std::string& retailer) {
    if (retailer == "Target") {
        return {0.80, 0.01};  // Target: 80% threshold, 1% penalty
    } else if (retailer == "Walmart") {
        return {0.90, 0.03};  // Walmart (SQEP/OTIF): 90% threshold, 3% penalty
    }
    // Default fallback rule
    return {0.85, 0.02};
}

struct PurchaseOrder {
    std::string retailer;
    int order_number;
    int units;
    double amount;
    std::string date;
    
    // Dynamic rule settings
    double threshold;
    double fine_rate;

    // Helper functions
    int required_compliant_units() const {
        return static_cast<int>(std::ceil(threshold * units));
    }

    double unit_value() const {
        return amount / units;
    }

    // Savings per 1 correct unit allocated (prior to reaching threshold)
    double marginal_savings_per_unit() const {
        return unit_value() * fine_rate;
    }
};

struct AllocationResult {
    int order_number;
    std::string retailer;
    int total_units;
    int allocated_correct_units;
    int non_compliant_units;
    int penalized_units;
    double fine_amount;
};

// Helper function to clean money strings "$130.00" -> 130.00
double parseAmount(std::string amount_str) {
    amount_str.erase(
        std::remove(amount_str.begin(), amount_str.end(), '$'), 
        amount_str.end()
    );
    return std::stod(amount_str);
}

class POOptimizer {
public:
    static std::vector<AllocationResult> runAllocation(
        std::vector<PurchaseOrder> pos, 
        int available_correct_inventory,
        double& total_fine_out
    ) {
        // Sort POs by highest marginal savings rate ($ saved per unit) descending
        std::sort(pos.begin(), pos.end(), [](const PurchaseOrder& a, const PurchaseOrder& b) {
            return a.marginal_savings_per_unit() > b.marginal_savings_per_unit();
        });

        std::vector<AllocationResult> results;
        int remaining_inventory = available_correct_inventory;
        total_fine_out = 0.0;

        for (const auto& po : pos) {
            int needed_for_zero_fine = po.required_compliant_units();
            
            // Greedily allocate as close to the threshold as possible
            int allocated = std::min(remaining_inventory, needed_for_zero_fine);
            remaining_inventory -= allocated;

            int non_compliant = po.units - allocated;
            int penalized_units = std::max(0, needed_for_zero_fine - allocated);
            double fine = penalized_units * po.marginal_savings_per_unit();
            
            total_fine_out += fine;

            results.push_back({
                po.order_number,
                po.retailer,
                po.units,
                allocated,
                non_compliant,
                penalized_units,
                fine
            });
        }

        // Re-sort results by Order Number for clean output display
        std::sort(results.begin(), results.end(), [](const AllocationResult& a, const AllocationResult& b) {
            return a.order_number < b.order_number;
        });

        return results;
    }
};

int main() {
    // Input JSON provided
    std::string raw_json = R"([
        {
            "Retailer": "Target",
            "Order Number": 123,
            "Units": 100,
            "Amount": "$130.00",
            "Date": "1/1"
        },
        {
            "Retailer": "Walmart",
            "Order Number": 124,
            "Units": 101,
            "Amount": "$110.00",
            "Date": "1/1"
        }
    ])";

    // 1. Parse JSON into C++ struct objects
    json parsed = json::parse(raw_json);
    std::vector<PurchaseOrder> pos;

    for (const auto& item : parsed) {
        std::string retailer = item["Retailer"];
        RetailerRule rule = getRetailerRule(retailer);

        pos.push_back({
            retailer,
            item["Order Number"].get<int>(),
            item["Units"].get<int>(),
            parseAmount(item["Amount"].get<std::string>()),
            item["Date"].get<std::string>(),
            rule.compliance_threshold,
            rule.fine_rate
        });
    }

    // 2. Define available "Correctly Labeled" inventory limit
    int available_correct_inventory = 120; // Example supply limit

    // 3. Run Optimization
    double total_fine = 0.0;
    auto results = POOptimizer::runAllocation(pos, available_correct_inventory, total_fine);

    // 4. Output Results Table
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "================ ALLOCATION RESULTS ================\n";
    std::cout << "Available Correct Inventory: " << available_correct_inventory << " units\n\n";

    for (const auto& res : results) {
        std::cout << "PO #" << res.order_number << " (" << res.retailer << "):\n"
                  << "  - Total Units Ordered:   " << res.total_units << "\n"
                  << "  - Correct Units Given:    " << res.allocated_correct_units << "\n"
                  << "  - Non-Compliant Units:    " << res.non_compliant_units << "\n"
                  << "  - Units Penalized:        " << res.penalized_units << "\n"
                  << "  - Fine Incurred:          $" << res.fine_amount << "\n\n";
    }

    std::cout << "----------------------------------------------------\n";
    std::cout << "TOTAL FINE TO PAY: $" << total_fine << "\n";
    std::cout << "====================================================\n";

    return 0;
}
  
