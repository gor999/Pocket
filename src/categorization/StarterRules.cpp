#include "categorization/StarterRules.h"

namespace pf {

namespace {

using MT = CategorizationRule::MatchType;

// Priority: specific phrases (50) beat category keywords (30) beat vague words (10).
// Confidence: >= 70 is applied automatically; below that the category is only
// suggested and the transaction waits for review.
// clang-format off
const StarterRule kRules[] = {
    // ---- Salary (income) ----
    {"salary", MT::Contains, "salary", 50, 90},
    {"salary", MT::Contains, "payroll", 50, 90},
    {"salary", MT::Contains, "зарплат", 50, 90},
    {"salary", MT::Contains, "заработная плата", 50, 90},
    {"salary", MT::Contains, "աշխատավարձ", 50, 90},

    // ---- Cash (ATM withdrawal) ----
    {"cash", MT::Regex,    R"(\batm\b)", 50, 90},
    {"cash", MT::Contains, "cash withdrawal", 50, 90},
    {"cash", MT::Contains, "cash advance", 50, 90},
    {"cash", MT::Contains, "банкомат", 50, 90},
    {"cash", MT::Contains, "снятие налич", 50, 90},
    {"cash", MT::Contains, "բանկոմատ", 50, 90},
    {"cash", MT::Contains, "կանխիկացում", 50, 90},

    // ---- Groceries (supermarket) ----
    {"groceries", MT::Contains, "supermarket", 30, 85},
    {"groceries", MT::Contains, "hypermarket", 30, 85},
    {"groceries", MT::Contains, "grocery", 30, 85},
    {"groceries", MT::Contains, "minimarket", 30, 80},
    {"groceries", MT::Contains, "супермаркет", 30, 85},
    {"groceries", MT::Contains, "гипермаркет", 30, 85},
    {"groceries", MT::Contains, "продукты", 30, 85},
    {"groceries", MT::Contains, "սուպերմարկետ", 30, 85},
    {"groceries", MT::Contains, "հիպերմարկետ", 30, 85},
    {"groceries", MT::Contains, "մթերք", 30, 85},
    // examples of chains; extend with your own merchant rules
    {"groceries", MT::Regex, R"(\byerevan city\b)", 30, 85},
    {"groceries", MT::Regex, R"(\bsas\b)", 30, 80},
    {"groceries", MT::Regex, R"(\bcarrefour\b)", 30, 85},

    // ---- Restaurants ----
    {"restaurants", MT::Contains, "restaurant", 30, 85},
    {"restaurants", MT::Contains, "cafe", 30, 80},
    {"restaurants", MT::Contains, "café", 30, 80},
    {"restaurants", MT::Contains, "coffee", 30, 80},
    {"restaurants", MT::Contains, "pizza", 30, 80},
    {"restaurants", MT::Contains, "burger", 30, 80},
    {"restaurants", MT::Contains, "sushi", 30, 80},
    {"restaurants", MT::Contains, "bakery", 30, 75},
    {"restaurants", MT::Regex,    R"(\b(kfc|mcdonald\w*|starbucks)\b)", 30, 85},
    {"restaurants", MT::Contains, "ресторан", 30, 85},
    {"restaurants", MT::Contains, "кафе", 30, 80},
    {"restaurants", MT::Contains, "кофе", 30, 80},
    {"restaurants", MT::Contains, "пицц", 30, 80},
    {"restaurants", MT::Contains, "ռեստորան", 30, 85},
    {"restaurants", MT::Contains, "սրճարան", 30, 80},
    {"restaurants", MT::Contains, "կաֆե", 30, 80}, // transliterated "cafe"
    {"restaurants", MT::Contains, "սուրճ", 30, 80},
    {"restaurants", MT::Contains, "պիցց", 30, 80},
    {"restaurants", MT::Regex,    R"(\b(bar|pub)\b)", 10, 60}, // vague: suggestion only
    // Delivery apps carry groceries as well as meals: suggest, never apply silently.
    {"restaurants", MT::Regex,    R"(\b(glovo|wolt|yandex ?food)\b)", 10, 65},

    // ---- Transportation (fuel, taxi, transit, parking) ----
    {"transportation", MT::Contains, "gas station", 30, 85},
    {"transportation", MT::Contains, "petrol", 30, 85},
    {"transportation", MT::Contains, "gasoline", 30, 85},
    {"transportation", MT::Contains, "fuel", 30, 85},
    {"transportation", MT::Regex,    R"(\bazs\b)", 30, 85},
    {"transportation", MT::Contains, "азс", 30, 85},
    {"transportation", MT::Contains, "заправк", 30, 85},
    {"transportation", MT::Contains, "бензин", 30, 85},
    {"transportation", MT::Contains, "топлив", 30, 85},
    {"transportation", MT::Contains, "բենզալցակայան", 30, 85},
    {"transportation", MT::Contains, "վառելիք", 30, 85},
    {"transportation", MT::Contains, "բենզին", 30, 85},
    {"transportation", MT::Contains, "taxi", 30, 85},
    {"transportation", MT::Contains, "такси", 30, 85},
    {"transportation", MT::Contains, "տաքսի", 30, 85},
    {"transportation", MT::Regex,    R"(\b(uber|yandex ?go|bolt)\b)", 30, 85},
    {"transportation", MT::Contains, "metro", 30, 80},
    {"transportation", MT::Contains, "метро", 30, 80},
    {"transportation", MT::Contains, "մետրո", 30, 80},
    {"transportation", MT::Regex,    R"(\b(gazprom ?neft|rompetrol|flash)\b)", 30, 80}, // fuel brands; plain "gazprom" is the gas utility
    {"transportation", MT::Contains, "parking", 30, 80},
    {"transportation", MT::Contains, "парковк", 30, 80},

    // ---- Healthcare (pharmacy, clinic) ----
    {"healthcare", MT::Contains, "pharm", 30, 85},
    {"healthcare", MT::Contains, "drugstore", 30, 85},
    {"healthcare", MT::Contains, "аптек", 30, 85},
    {"healthcare", MT::Contains, "фарм", 30, 85},
    {"healthcare", MT::Contains, "դեղատուն", 30, 85},
    {"healthcare", MT::Contains, "դեղ", 30, 75},
    {"healthcare", MT::Contains, "clinic", 30, 85},
    {"healthcare", MT::Contains, "hospital", 30, 85},
    {"healthcare", MT::Contains, "dental", 30, 85},
    {"healthcare", MT::Contains, "medical", 30, 80},
    {"healthcare", MT::Contains, "клиника", 30, 85},
    {"healthcare", MT::Contains, "больниц", 30, 85},
    {"healthcare", MT::Contains, "стоматолог", 30, 85},
    {"healthcare", MT::Contains, "կլինիկա", 30, 85},
    {"healthcare", MT::Contains, "հիվանդանոց", 30, 85},
    {"healthcare", MT::Contains, "ատամնաբույժ", 30, 85},
    {"healthcare", MT::Contains, "բժշկ", 30, 80},

    // ---- Shopping (online shopping and retail) ----
    {"shopping", MT::Regex,    R"(\b(amazon|aliexpress|ebay|wildberries|ozon|temu|shein)\b)", 30, 90},
    {"shopping", MT::Contains, "online shop", 30, 85},
    {"shopping", MT::Contains, "интернет-магазин", 30, 85},
    {"shopping", MT::Contains, "օնլայն", 30, 75},
    {"shopping", MT::Regex,    R"(\b(ikea|zara)\b)", 30, 85},
    {"shopping", MT::Regex,    R"(\b(shop|store|mall|market|магазин)\b)", 10, 55}, // vague: suggestion only
    {"shopping", MT::Contains, "խանութ", 10, 55},

    // ---- Utilities ----
    {"utilities", MT::Contains, "electric", 30, 80},
    {"utilities", MT::Contains, "электр", 30, 80},
    {"utilities", MT::Contains, "էլեկտրաէներգիա", 30, 80},
    {"utilities", MT::Contains, "internet", 30, 75},
    {"utilities", MT::Contains, "интернет", 20, 75},
    {"utilities", MT::Contains, "ինտերնետ", 30, 75},
    {"utilities", MT::Contains, "telecom", 30, 75},
    {"utilities", MT::Regex,    R"(\b(ucom|beeline|mts)\b)", 30, 80},
    {"utilities", MT::Contains, "water", 20, 70},
    {"utilities", MT::Contains, "водоканал", 30, 80},
    {"utilities", MT::Contains, "ջուր", 20, 65},
    {"utilities", MT::Contains, "գազ", 20, 60}, // vague (gas supply vs fuel): suggestion only

    // ---- Entertainment ----
    {"entertainment", MT::Contains, "cinema", 30, 85},
    {"entertainment", MT::Contains, "кино", 30, 85},
    {"entertainment", MT::Contains, "կինո", 30, 85},
    {"entertainment", MT::Regex,    R"(\b(netflix|spotify|youtube|steam|playstation|xbox|itunes)\b)", 30, 85},
    {"entertainment", MT::Contains, "theatre", 30, 85},
    {"entertainment", MT::Contains, "theater", 30, 85},
    {"entertainment", MT::Contains, "театр", 30, 85},
    {"entertainment", MT::Contains, "թատրոն", 30, 85},
    {"entertainment", MT::Contains, "concert", 30, 85},
    {"entertainment", MT::Contains, "համերգ", 30, 85},

    // ---- Transfers ----
    {"transfers", MT::Contains, "transfer", 20, 75},
    {"transfers", MT::Contains, "p2p", 20, 75},
    {"transfers", MT::Contains, "перевод", 20, 75},
    {"transfers", MT::Contains, "փոխանցում", 20, 75},
};
// clang-format on

} // namespace

const QList<StarterRule> &starterRules()
{
    static const QList<StarterRule> list = [] {
        QList<StarterRule> l;
        for (const StarterRule &r : kRules)
            l.append(r);
        return l;
    }();
    return list;
}

} // namespace pf
