#include <vector>
#include "models.hpp"
#include <cmath>
#include <algorithm>
#include <complex>
#include <string>
#include <iomanip>
#include <iostream>
#include <fstream>

const double PI = 3.14159265358979323846;

double normalCDF(double x) {
    return 0.5 * std::erfc(-x * std::sqrt(0.5));
}

double blackScholesCall(double S, double K, double T, double r, double q, double sigma) {
    // 1. Teoretisk nedre grense (neddiskontert indre verdi)
    double intrinsic = std::max(S * std::exp(-q * T) - K * std::exp(-r * T), 0.0);
    if (sigma <= 0.0) return intrinsic;

    // 2. d1 og d2justeres med (r - q)
    double d1 = (std::log(S / K) + (r - q + 0.5 * std::pow(sigma, 2)) * T) / (sigma * std::sqrt(T));
    double d2 = d1 - sigma * std::sqrt(T);

    // 3. Både S (med q) og K (med r) diskonteres
    return S * std::exp(-q * T) * normalCDF(d1) - K * std::exp(-r * T) * normalCDF(d2);
}

double blackScholesVega(double S, double K, double T, double r, double q, double sigma) {
    if (sigma <= 0.0) return 0.0;
    double d1 = (std::log(S / K) + (r - q + 0.5 * std::pow(sigma, 2)) * T) / (sigma * std::sqrt(T));
    return S * std::exp(-q * T) * std::sqrt(T) * std::exp(-0.5 * std::pow(d1, 2)) / std::sqrt(2.0 * PI);
}

double getImpliedVol(double S, double K, double T, double r, double q, double target_price) {
    // Riktig diskontert indre verdi og maks teoretisk opsjonsverdi
    double intrinsic = std::max(S * std::exp(-q * T) - K * std::exp(-r * T), 0.0);
    double max_price = S * std::exp(-q * T);
    
    // Ugyldig markedsdata sjekk
    if (target_price <= intrinsic || target_price >= max_price) return 0.0;

    double sigma = 0.25; 
    double low = 0.0001;
    double high = 5.0; 

    for (int i = 0; i < 100; ++i) {
        double price = blackScholesCall(S, K, T, r, q, sigma);
        double diff = price - target_price;
        
        if (std::abs(diff) < 1e-6) return sigma; 

        if (diff > 0) high = sigma;
        else low = sigma;

        double vega = blackScholesVega(S, K, T, r, q, sigma);

        if (vega < 1e-8) {
            sigma = 0.5 * (low + high);
        } else {
            double next_sigma = sigma - diff / vega;
            if (next_sigma <= low || next_sigma >= high) {
                sigma = 0.5 * (low + high);
            } else {
                sigma = next_sigma;
            }
        }
    }
    return sigma;
}

std::vector<double> batchBlackScholesCall(
    const std::vector<double>& S_list,     
    const std::vector<double>& K_list,     
    double T,                              
    double r,                              
    const std::vector<double>& sigma_list  
) {
    std::vector<double> call_prices;
    call_prices.reserve(S_list.size() * K_list.size());

    for (size_t i = 0; i < S_list.size(); ++i) {
        double S = S_list[i];
        double sigma = sigma_list[i];

        for (double K : K_list) {
            double d1 = (std::log(S / K) + (r + 0.5 * std::pow(sigma, 2)) * T) / (sigma * std::sqrt(T));
            double d2 = d1 - sigma * std::sqrt(T);
            double price = S * normalCDF(d1) - K * std::exp(-r * T) * normalCDF(d2);
            call_prices.push_back(price);
        }
    }
    return call_prices; 
}


// 1. Karakteristisk funksjon sender inn (r - q)
std::complex<double> hestonCharFunc(double u, double T, double r_eff, double v0, 
                                    double kappa, double theta, double sigma_v, double rho) {
    using namespace std;
    complex<double> i(0.0, 1.0);
    
    complex<double> d = sqrt(pow(kappa - i * rho * sigma_v * u, 2) + pow(sigma_v, 2) * (i * u + u * u));
    complex<double> g = (kappa - i * rho * sigma_v * u - d) / (kappa - i * rho * sigma_v * u + d);
    
    complex<double> C = (kappa * theta / pow(sigma_v, 2)) * ((kappa - i * rho * sigma_v * u - d) * T - 2.0 * log((1.0 - g * exp(-d * T)) / (1.0 - g)));
    complex<double> D = ((kappa - i * rho * sigma_v * u - d) / pow(sigma_v, 2)) * ((1.0 - exp(-d * T)) / (1.0 - g * exp(-d * T)));
    
    // Bruker r_eff = (r - q)
    return exp(i * u * r_eff * T + C + D * v0);
}

double hestonCOS(double S, double K, double T, double r, double q, double v0, 
                 double kappa, double theta, double sigma_v, double rho) {
    using namespace std;
    int N = 160;    // 160 COS-termer gir god presisjon
    double L = 10;  // Standard COS-trunkering (8-10 fanger opp hele halen)

    double r_eff = r - q;

    // 1. Kumulanter for trunkeringsgrenser [a, b]
    double c1 = r_eff * T + (1.0 - exp(-kappa * T)) * (theta - v0) / (2.0 * kappa) - 0.5 * theta * T;
    
    // Robust estimat for varians c2
    double c2 = (v0 > 0 ? v0 : theta) * T;
    if (sigma_v > 1e-5) {
        c2 = (1.0 / (8.0 * pow(kappa, 3))) * (
            sigma_v * T * kappa * exp(-kappa * T) * (v0 - theta) * (8.0 * kappa - 4.0 * sigma_v * rho) +
            0.5 * theta * sigma_v * sigma_v * (exp(-2.0 * kappa * T) - 1.0) +
            2.0 * theta * kappa * kappa * T +
            4.0 * kappa * (v0 - theta) * (1.0 - exp(-kappa * T))
        );
    }

    double a = c1 - L * sqrt(abs(c2)); 
    double b = c1 + L * sqrt(abs(c2));

    double x = log(S / K); // Log-moneyness

    // Chi-hjelpefunksjon: Integral av exp(y) * cos(o * (y - a)) dy
    auto get_chi = [&](double o, double c, double d) {
        double exp_d = exp(d);
        double exp_c = exp(c);
        double cos_d = cos(o * (d - a));
        double cos_c = cos(o * (c - a));
        double sin_d = sin(o * (d - a));
        double sin_c = sin(o * (c - a));

        return (1.0 / (1.0 + o * o)) * 
               (exp_d * (cos_d + o * sin_d) - exp_c * (cos_c + o * sin_c));
    };

    // Psi-hjelpefunksjon: Integral av cos(o * (y - a)) dy
    auto get_psi = [&](double o, double c, double d) {
        if (abs(o) < 1e-12) return d - c;
        return (sin(o * (d - a)) - sin(o * (c - a))) / o;
    };

    // Integration bounds for Call Payoff: fra -x til b (avkuttet til [a, b])
    double c_bound = max(-x, a);
    double d_bound = b;

    if (c_bound >= d_bound) return 0.0;

    double sum = 0.0;
    
    for (int k = 0; k < N; ++k) {
        double o = k * PI / (b - a);
        complex<double> cf = hestonCharFunc(o, T, r_eff, v0, kappa, theta, sigma_v, rho);
        
        // Riktig V_k med e^x-skalering og dynamisk nedre grense c_bound (-x)
        double chi_val = get_chi(o, c_bound, d_bound);
        double psi_val = get_psi(o, c_bound, d_bound);
        
        double V_k = (2.0 / (b - a)) * K * (exp(x) * chi_val - psi_val);
        
        // COS-summering med korreksjon for a
        double term = (cf * exp(complex<double>(0, -o * a))).real() * V_k;
        
        if (k == 0) sum += 0.5 * term;
        else sum += term;
    }

    return max(0.0, exp(-r * T) * sum);
}