#include <iostream>
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <filesystem>
#include <numeric>
#include <iomanip>
#include <chrono>

// Kobler til modell- og kalibreringsfilene dine (ligger ett hakk opp)
#include "models.hpp" 
#include "params.hpp"

namespace fs = std::filesystem;

// --- Gjenbrukte hjelpefunksjoner ---

std::string extractExpiryFromFile(const std::string& file) {
    std::ifstream f(file);
    std::string line, header, strike, expiry;
    std::getline(f, header); 
    if (std::getline(f, line)) {
        if (line.empty()) return "";
        std::stringstream ss(line);
        std::getline(ss, strike, ','); 
        std::getline(ss, expiry, ','); 
        return expiry;
    }
    return "";
}

double computeTimeToExpiry(const std::string& expiryStr) {
    if (expiryStr.empty()) return 0.0;
    std::tm expiryDate = {};
    std::istringstream ss(expiryStr);
    ss >> std::get_time(&expiryDate, "%Y-%m-%d"); 
    if (ss.fail()) return 0.0; 
    auto expiryTime = std::chrono::system_clock::from_time_t(std::mktime(&expiryDate));
    auto now = std::chrono::system_clock::now();
    std::chrono::duration<double> diff = expiryTime - now;
    double T = diff.count() / (365.25 * 24 * 3600);
    return (T > 0.0) ? T : 0.0;
}

struct Row { std::string date; double close, high, low, open; long long volume; };

std::vector<std::string> split(const std::string& line, char delimiter) {
    std::vector<std::string> result; std::stringstream ss(line); std::string item;
    while (std::getline(ss, item, delimiter)) result.push_back(item);
    return result;
}

std::vector<Row> readCSV(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) throw std::runtime_error("Kunne ikke åpne filen: " + filename);
    std::string line;
    std::getline(file, line); std::getline(file, line); std::getline(file, line);
    std::vector<Row> data;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        auto cols = split(line, ','); if (cols.size() < 6) continue;
        Row r; r.date = cols[0]; r.close = std::stod(cols[1]); r.high = std::stod(cols[2]);
        r.low = std::stod(cols[3]); r.open = std::stod(cols[4]); r.volume = std::stoll(cols[5]);
        data.push_back(r);
    }
    return data;
}

std::vector<double> calculateLogReturns(const std::vector<double>& prices) {
    std::vector<double> log_returns; if (prices.size() < 2) return log_returns;
    log_returns.reserve(prices.size() - 1);
    for (size_t i = 1; i < prices.size(); ++i) {
        log_returns.push_back((prices[i - 1] > 0.0) ? std::log(prices[i] / prices[i - 1]) : 0.0);
    }
    return log_returns;
}

double calculateStandardDeviation(const std::vector<double>& returns) {
    if (returns.size() < 2) return 0.0;
    double sum = std::accumulate(returns.begin(), returns.end(), 0.0);
    double mean = sum / returns.size();
    double sq_sum = 0.0;
    for (double r : returns) sq_sum += std::pow(r - mean, 2);
    return std::sqrt(sq_sum / (returns.size() - 1));
}

void loadOptionData(const std::string& file, std::vector<double>& strikes, std::vector<double>& market_prices) {
    std::ifstream f(file);
    if (!f.is_open()) return;
    std::string line;
    std::getline(f, line); // Header
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string token, expiry, type;
        double strike, bid, ask, market_price;

        std::getline(ss, token, ','); strike = std::stod(token);
        std::getline(ss, expiry, ',');
        std::getline(ss, type, ',');
        std::getline(ss, token, ','); bid = std::stod(token);
        std::getline(ss, token, ','); ask = std::stod(token);
        std::getline(ss, token, ','); market_price = std::stod(token);

        if (type == "call" && market_price > 0.0) {
            strikes.push_back(strike);
            market_prices.push_back(market_price);
        }
    }
}

int main() {
    std::system("chcp 65001 > nul"); // UTF-8 støtte for Windows

    std::string price_folder = "data/prices";
    std::string strike_root = "data/strikes"; 
    fs::path output_root = "smile_data";
    
    double r = 0.0479; // Risikofri rente (4.5%)
    double q = 0.0074; // Utbytteyield (f.eks. 0.7% for MSFT / AAPL)

    std::string target_ticker;
    std::cout << "Tast inn ticker du vil sjekke (f.eks. AAPL): ";
    std::cin >> target_ticker;

    for (auto &c : target_ticker) c = toupper(c);

    std::string price_file_lower = price_folder + "/" + target_ticker + "_daily.csv";
    std::string price_file_upper = price_folder + "/" + target_ticker + "_DAILY.csv";
    std::string active_price_file = "";

    if (fs::exists(price_file_lower)) active_price_file = price_file_lower;
    else if (fs::exists(price_file_upper)) active_price_file = price_file_upper;

    if (active_price_file.empty()) {
        std::cerr << "FEIL: Fant ikke prisfil for ticker " << target_ticker << std::endl;
        return 1;
    }

    try {
        auto data = readCSV(active_price_file);
        if (data.empty()) return 1;

        double S = data.back().close;
        std::vector<double> prices;
        for (const auto& row : data) prices.push_back(row.close);
        double daily_sigma = calculateStandardDeviation(calculateLogReturns(prices));
        double annual_sigma = daily_sigma * std::sqrt(252.0);

        fs::path ticker_folder = fs::path(strike_root) / target_ticker;
        if (!fs::exists(ticker_folder)) return 1;

        std::vector<fs::path> csv_files;
        for (const auto& entry : fs::directory_iterator(ticker_folder)) {
            if (entry.path().extension() == ".csv") csv_files.push_back(entry.path());
        }
        if (csv_files.empty()) return 1;

        std::sort(csv_files.begin(), csv_files.end());
        fs::path longest_expiry_file = csv_files.back();

        std::string strike_file = longest_expiry_file.string();
        std::vector<double> strikes;
        std::vector<double> market_prices;
        loadOptionData(strike_file, strikes, market_prices);
        
        std::string expiry = extractExpiryFromFile(strike_file);
        double T = computeTimeToExpiry(expiry);

        if (T <= 0 || strikes.empty()) return 1;

        // --- 1. RUN KALIBRERING MED NORMALISERING ---
        std::vector<double> normalized_strikes = strikes;
        std::vector<double> normalized_prices = market_prices;
        for (size_t i = 0; i < strikes.size(); ++i) {
            normalized_strikes[i] = strikes[i] / S;
            normalized_prices[i]  = market_prices[i] / S;
        }

        std::cout << "\n=========================================================================" << std::endl;
        std::cout << " -> STARTER HESTON-KALIBRERING FOR: " << target_ticker << std::endl;
        std::cout << " Kalibrerer over " << strikes.size() << " strikes... ";
        std::cout.flush();

        auto t0 = std::chrono::high_resolution_clock::now();
        
        // Sender inn effektiv drift (r - q) til kalibreringen
        std::vector<double> params = calibrateHeston(1.0, T, r - q, normalized_strikes, normalized_prices);
        
        auto t1 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> dt = t1 - t0;

        double kappa   = params[0];
        double theta   = params[1];
        double sigma_v = params[2];
        double rho     = params[3];
        double v0      = params[4];

        std::cout << "Ferdig! (" << dt.count() << " sek)" << std::endl;
        std::cout << " [Kalibrert] k=" << kappa << " | th=" << theta << " | sig=" << sigma_v << " | rho=" << rho << " | v0=" << v0 << std::endl;
        std::cout << "=========================================================================" << std::endl;

        if (!fs::exists(output_root)) {
            fs::create_directories(output_root);
        }

        fs::path out_filepath = output_root / (target_ticker + "_" + expiry + "_smile.csv");
        std::ofstream out_file(out_filepath.string());
        
        out_file << "Strike,MarketPrice,MarketIV,BSPrice,HestonPrice,HestonIV,HistVol\n";

        for (size_t i = 0; i < strikes.size(); ++i) {
            double K = strikes[i];

            // 1. Black-Scholes pris med utbytte (q)
            double bs_price = blackScholesCall(S, K, T, r, q, annual_sigma);

            // 2. Heston COS pris med utbytte (q)
            double norm_price = hestonCOS(1.0, normalized_strikes[i], T, r, q, v0, kappa, theta, sigma_v, rho);
            double heston_price = norm_price * S;
            
            // 3. Implisitt Volatilitet (IV) beregnet med utbytte (q)
            double heston_iv = getImpliedVol(S, K, T, r, q, heston_price);
            double market_iv = getImpliedVol(S, K, T, r, q, market_prices[i]);

            out_file << K << "," 
                    << market_prices[i] << ","
                    << (market_iv > 0.0001 ? market_iv * 100.0 : 0.0) << ","
                    << bs_price << ","
                    << heston_price << "," 
                    << (heston_iv > 0.0001 ? heston_iv * 100.0 : 0.0) << "," 
                    << (annual_sigma * 100.0) << "\n";
        }
                
        out_file.close();
        std::cout << ">>> Suksess! Optimalisert smile-data lagret til: " << out_filepath.string() << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "FEIL under kjøring: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}