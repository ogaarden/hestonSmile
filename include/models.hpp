#ifndef MODELS_HPP
#define MODELS_HPP

#include <vector>

// Deklarasjoner av Black-Scholes og IV-funksjoner (oppdatert med q)
double normalCDF(double x);

double blackScholesCall(double S, double K, double T, double r, double q, double sigma);

double blackScholesVega(double S, double K, double T, double r, double q, double sigma);

double getImpliedVol(double S, double K, double T, double r, double q, double target_price);

std::vector<double> batchBlackScholesCall(
    const std::vector<double>& S_list, 
    const std::vector<double>& K_list, 
    double T, 
    double r, 
    double q,
    const std::vector<double>& sigma_list
);

// Deklarasjon av Heston-funksjonen (oppdatert med q)
double hestonCOS(double S, double K, double T, double r, double q, double v0, 
                 double kappa, double theta, double sigma_v, double rho);

#endif // MODELS_HPP